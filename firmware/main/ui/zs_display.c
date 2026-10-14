#include "zs_display.h"
#include "zs_config.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <time.h>

static const char *TAG = "display";

/* Baglyset paa SenseCAP Indicator D1. GPIO og opsaetning er den samme
 * som i Seeeds eget eksempel, saa vi ved den passer til hardwaren. */
#define BL_GPIO         45
#define BL_MODE         LEDC_LOW_SPEED_MODE
#define BL_TIMER        LEDC_TIMER_0
#define BL_CHANNEL      LEDC_CHANNEL_0
#define BL_RES          LEDC_TIMER_13_BIT
#define BL_MAX_DUTY     ((1 << 13) - 1)
/* 5 kHz. Hoejt nok til at oejet ikke ser flimmer, lavt nok til at
 * LEDC'en kan levere fuld 13-bit oploesning. */
#define BL_FREQ_HZ      5000

#define BRIGHTNESS_MIN  5

/* Hvor laenge skaermen lyser op efter et tryk om natten. */
#define WAKE_HOLD_MS    60000

static uint8_t  s_brightness = ZS_BRIGHTNESS_DEFAULT;  /* brugerens valg */
static uint8_t  s_applied    = 0;                      /* hvad der faktisk staar paa */
static bool     s_night_dim  = true;
static int64_t  s_last_touch_us = 0;
/*
 * Er der nogensinde blevet TRYKKET paa skaermen, siden den startede.
 *
 * s_last_touch_us primes i zs_display_init, saa natdaempningen ikke
 * slaar til i det foerste minut efter opstart. Det er rigtigt for
 * baglyset - men pulsen laeste den samme vaerdi og kom derfor til at
 * tolke OPSTARTEN som et tryk: en skaerm der taendte ind i en fejl,
 * fordi routeren var vaek, pulserede ikke det foerste minut af sin
 * levetid. Praecis det minut hvor nogen staar og kigger paa den.
 *
 * Derfor et eget flag. Det saettes kun af et rigtigt tryk.
 */
static bool     s_er_beroert = false;
static bool     s_ready      = false;

/*
 * ── Baglyset under en alarm ──
 *
 * s_want er den lysstyrke der SKULLE staa paa lige nu, altsaa resultatet
 * af brugerens valg og natdaempningen. Den regnes ét sted, i
 * zs_display_tick.
 *
 * MENS ALARMEN EJER BAGLYSET, er det fejlsidens animation der saetter
 * det - gennem zs_display_alarm_niveau - og ingen anden rOErer
 * registret: tick, skyderen og et tryk saetter kun MAALET.
 *
 * HER STOD FOER EN ESP_TIMER, og det var én puls for meget. Da fladen
 * selv begyndte at flashe roedt, var der to pulser paa hver sit ur: de
 * startede i takt og gled fra hinanden i loebet af minutter, saa man fik
 * fuldt baglys paa den moerke roed og omvendt. Det ser ud som en fejl i
 * skaermen, ikke som en besked.
 *
 * Nu er der ÉN fase - LVGL-animationens - og den saetter baade farven og
 * lyset. Timeren er vaek, og med den ogsaa det kapløb der var mellem
 * dens egen nedlukning og et nyt set_alarm.
 */
static uint8_t  s_want  = ZS_BRIGHTNESS_DEFAULT;
static bool     s_alarm = false;

static void apply_duty(uint8_t pct)
{
    if (!s_ready) {
        return;
    }
    if (pct < BRIGHTNESS_MIN) { pct = BRIGHTNESS_MIN; }
    if (pct > 100)            { pct = 100; }
    if (pct == s_applied) {
        return;   /* undgaa unoedig I/O paa hvert tick */
    }
    uint32_t duty = (uint32_t)BL_MAX_DUTY * pct / 100u;
    ledc_set_duty(BL_MODE, BL_CHANNEL, duty);
    ledc_update_duty(BL_MODE, BL_CHANNEL);
    s_applied = pct;
}

void zs_display_set_alarm(bool on)
{
    if (!s_ready || on == s_alarm) {
        return;
    }
    s_alarm = on;
    if (on) {
        ESP_LOGI(TAG, "alarmen overtager baglyset");
        return;
    }
    /*
     * Alarmen er slut, og baglyset afleveres HER. Gjorde vi det ikke,
     * stod det paa pulsens sidste niveau indtil tick() naaede forbi -
     * op mod et sekund paa en tilfaeldig lysstyrke.
     */
    ESP_LOGI(TAG, "alarmen slipper baglyset");
    apply_duty(s_want);
}

void zs_display_alarm_niveau(uint8_t hoejde)
{
    if (!s_ready || !s_alarm) {
        return;
    }

    /*
     * ROERER NOGEN VED SKAERMEN, HOLDER BAGLYSET PAUSE.
     *
     * Pulsen er der for at FAA nogen hen til skaermen. Staar hun foran
     * den og laeser beskeden eller trykker paa knappen, har den gjort
     * sit arbejde, og saa er en flade der bliver ved med at skifte
     * lysstyrke bare i vejen for det hun er kommet for at goere.
     *
     * Samme pause som baglyset bruger efter et tryk om natten, altsaa
     * WAKE_HOLD_MS. Gaar hun fra skaermen uden at fejlen er rettet,
     * begynder pulsen igen af sig selv et minut senere.
     *
     * Kun BAGLYSET holder pause. Fladen bliver ved at flashe roedt:
     * beskeden skal ikke forsvinde fordi nogen rOErte skaermen.
     */
    if (s_er_beroert
        && (esp_timer_get_time() - s_last_touch_us) / 1000 < WAKE_HOLD_MS) {
        apply_duty(s_want);
        return;
    }

    /* 0 nederst, 255 paa kundens egen lysstyrke. */
    uint32_t top  = s_want;
    uint32_t bund = top * ZS_ALARM_DIM_PCT / 100u;
    apply_duty((uint8_t)(bund + (top - bund) * hoejde / 255u));
}

void zs_display_init(uint8_t start_pct)
{
    if (start_pct < BRIGHTNESS_MIN) { start_pct = BRIGHTNESS_MIN; }
    if (start_pct > 100)            { start_pct = 100; }
    s_brightness = start_pct;

    ledc_timer_config_t timer = {
        .speed_mode      = BL_MODE,
        .timer_num       = BL_TIMER,
        .duty_resolution = BL_RES,
        .freq_hz         = BL_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&timer) != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke saette PWM-timer op til baglyset");
        return;
    }

    ledc_channel_config_t ch = {
        .speed_mode = BL_MODE,
        .channel    = BL_CHANNEL,
        .timer_sel  = BL_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = BL_GPIO,
        .duty       = (uint32_t)BL_MAX_DUTY * start_pct / 100u,
        .hpoint     = 0,
    };
    if (ledc_channel_config(&ch) != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke saette PWM-kanal op til baglyset");
        return;
    }

    s_ready   = true;
    s_applied = start_pct;
    /* Pulsens loft fra foerste oejeblik. Stod den paa standardvaerdien
     * indtil det foerste tick, ville en alarm i det foerste sekund
     * pulsere op til 80 selvom kunden havde valgt 20. */
    s_want    = start_pct;
    s_last_touch_us = esp_timer_get_time();
    ESP_LOGI(TAG, "baglys klar paa GPIO %d, %u %%", BL_GPIO, s_brightness);
}

void zs_display_set_brightness(uint8_t pct)
{
    if (pct < BRIGHTNESS_MIN) { pct = BRIGHTNESS_MIN; }
    if (pct > 100)            { pct = 100; }
    s_brightness = pct;
    /*
     * Skru op med det samme saa brugeren ser hvad skyderen goer,
     * i stedet for at vente paa naeste tick.
     *
     * Pulsen faar sit nye loft med, men skal selv saette lysstyrken:
     * trak vi i registret her mens den koerer, ville skaermen rykke to
     * steder fra paa én gang. Kunden ser aendringen inden for 40 ms.
     */
    s_want = pct;
    if (!s_alarm) {
        apply_duty(pct);
    }
    s_last_touch_us = esp_timer_get_time();
}

uint8_t zs_display_get_brightness(void)
{
    return s_brightness;
}

void zs_display_set_night_dimming(bool enabled)
{
    s_night_dim = enabled;
}

bool zs_display_get_night_dimming(void)
{
    return s_night_dim;
}

void zs_display_touch_wake(void)
{
    s_last_touch_us = esp_timer_get_time();
    s_er_beroert = true;
    /*
     * MAALET SAETTES, OG PULSEN FAAR LOV AT SKRIVE DET.
     *
     * Her stod kun apply_duty, og det var en anden skriver i registret
     * end den der ejede det. Mens pulsen koerer, roerte tick() med
     * vilje ikke lysstyrken - men den her gjorde, og uden at rette
     * s_want. Et tryk om natten midt i en alarm gav derfor et glimt af
     * fuld styrke, hvorefter pulsens naeste trin 40 ms senere lagde den
     * tilbage paa det gamle, daempede maal - indtil tick() et sekund
     * senere opdagede trykket. Et dyk man kan se, lige efter at man har
     * rOErt skaermen.
     *
     * Nu saettes MAALET, som baade tick og pulsen regner af, og
     * registret roeres kun hvis pulsen ikke ejer det.
     */
    s_want = s_brightness;
    if (!s_alarm && s_applied < s_brightness) {
        apply_duty(s_brightness);
    }
}

/* Er klokken inde i natperioden? Haandterer at perioden gaar over
 * midnat, altsaa at 22 til 6 er otte timer og ikke minus seksten. */
static bool is_night_now(void)
{
    time_t now = time(NULL);
    /* Uret er ikke sat foer NTP har svaret. Vi daemper ikke paa et ur
     * der staar paa 1970, for saa ville skaermen vaere moerk fra den
     * gaar i gang og indtil den finder internet. */
    if (now < 1700000000) {   /* nogenlunde november 2023 */
        return false;
    }
    struct tm lt;
    localtime_r(&now, &lt);
    int h = lt.tm_hour;

    if (ZS_NIGHT_START_HOUR == ZS_NIGHT_END_HOUR) {
        return false;
    }
    if (ZS_NIGHT_START_HOUR < ZS_NIGHT_END_HOUR) {
        return h >= ZS_NIGHT_START_HOUR && h < ZS_NIGHT_END_HOUR;
    }
    return h >= ZS_NIGHT_START_HOUR || h < ZS_NIGHT_END_HOUR;
}

bool zs_display_er_nat(void)
{
    return is_night_now();
}

void zs_display_tick(void)
{
    if (!s_ready) {
        return;
    }
    uint8_t want = s_brightness;

    if (s_night_dim && is_night_now()) {
        int64_t since_touch_ms = (esp_timer_get_time() - s_last_touch_us) / 1000;
        if (since_touch_ms > WAKE_HOLD_MS) {
            /* Natniveauet er en OEVRE graense, ikke en fast vaerdi. Har
             * brugeren allerede skruet ned til 15, skal vi ikke skrue
             * OP til 25 fordi klokken blev 22. */
            want = (s_brightness < ZS_BRIGHTNESS_NIGHT)
                 ? s_brightness : ZS_BRIGHTNESS_NIGHT;
        }
    }
    /*
     * Gem den, OGSAA naar pulsen koerer. Den er pulsens loft, saa en
     * kunde der skruer ned mens skaermen blinker, faar en daempet puls
     * med det samme i stedet for naar fejlen er ovre.
     */
    s_want = want;

    if (s_alarm) {
        /* Alarmen ejer registret lige nu. Se noten ved s_alarm. */
        return;
    }
    apply_duty(want);
}
