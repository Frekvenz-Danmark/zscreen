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
static bool     s_ready      = false;

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
    s_last_touch_us = esp_timer_get_time();
    ESP_LOGI(TAG, "baglys klar paa GPIO %d, %u %%", BL_GPIO, s_brightness);
}

void zs_display_set_brightness(uint8_t pct)
{
    if (pct < BRIGHTNESS_MIN) { pct = BRIGHTNESS_MIN; }
    if (pct > 100)            { pct = 100; }
    s_brightness = pct;
    /* Skru op med det samme saa brugeren ser hvad skyderen goer,
     * i stedet for at vente paa naeste tick. */
    apply_duty(pct);
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
    if (s_applied < s_brightness) {
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
    apply_duty(want);
}
