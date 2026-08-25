/*
 * zScreen - den ene opgave der laver alt det der tager tid.
 *
 * Tilstandene:
 *
 *     OPSAETNING     brugeren er i gang. Vi laver kun det brugerfladen
 *                    beder om, og venter ellers.
 *     FORBINDER      der er gemte indstillinger. Vi kobler paa wifi og
 *                    finder inverteren.
 *     KOERER         vi laeser hvert andet sekund og opdaterer kortene.
 *
 * Der er ingen loekke uden en udgang. Hvert eneste sted der ventes,
 * ventes der med en graense: en skaerm der haenger paa en vaeg har ingen
 * til at trykke reset.
 */

#include "zs_app.h"
#include "zs_selftest.h"
#include "zs_config.h"
#include "zs_ui.h"
#include "zs_wifi.h"
#include "zs_discovery.h"
#include "zs_fronius.h"
#include "zs_nvs.h"
#include "zs_display.h"
#include "zs_demo.h"
#include "zs_price.h"
#include "zs_screen_setup.h"
#include "zs_ota.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_sntp.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "app";

typedef enum {
    ST_SETUP = 0,
    ST_CONNECTING,
    ST_RUNNING,
    ST_DEMO,        /* opdigtede tal, ingen wifi og ingen Modbus */
} app_state_t;

static QueueHandle_t   s_queue;
static zs_settings_t   s_cfg;

/* Sat naar indstillingerne er laest. Se zs_app_load_settings. */
static bool            s_cfg_laest;
static bool            s_cfg_configured;

/* Saettes naar siderne er bygget om, fx efter et temaskift. Naeste
 * gennemgang fylder dem med det vi allerede har, i stedet for at vente
 * paa naeste aflaesning. */
static bool            s_ui_genopfrisk;
static app_state_t     s_state = ST_SETUP;
static zs_fr_t         s_fr;
static zs_home_data_t  s_home;
static zs_found_t      s_found[ZS_DISCOVERY_MAX];
static zs_ap_t         s_aps[ZS_WIFI_MAX_APS];
static char            s_time_text[8];
static bool            s_clock_ok;

/* Hvornaar vi sidst fik et helt sæt tal, i millisekunder siden start. */
static int64_t s_last_good_ms;

/*
 * Demo gemmes ALDRIG.
 *
 * Flaget lever kun i hukommelsen, saa en genstart altid slaar det fra.
 * En enhed hos en kunde kan derfor ikke komme til at starte op med
 * opdigtede tal, uanset hvad der er trykket paa foer.
 */
/*
 * ÉN opgave til alt hvad der bruger internettet.
 *
 * Baade elpriser og firmwareopdatering bruger TLS, og et TLS-haandtryk
 * med certifikatbundtet er den tungeste ting hele programmet laver.
 * To opgaver ville betyde to store stakke der staar reserveret hele
 * tiden for noget der sker én gang i doegnet, og de kunne ramme
 * hinanden hvis de kom til at koere samtidig.
 *
 * Med én opgave og en koe koerer de efter hinanden, og der er kun ét
 * sted der skal have plads nok.
 *
 * Opgaven roerer ALDRIG skaermen. Den skriver sit resultat i en
 * struct og saetter et flag til sidst, og hovedopgaven samler det op.
 */
typedef enum {
    JOB_PRICE = 0,
    JOB_OTA,
} net_job_t;

static QueueHandle_t  s_net_queue;

static zs_price_day_t s_price;          /* det skaermen viser        */
static zs_price_day_t s_price_hentet;   /* netvaerksopgavens resultat */
static char           s_price_zone[4];
static volatile bool  s_price_klar;

static zs_ota_status_t s_ota;           /* det skaermen viser        */
static zs_ota_status_t s_ota_hentet;
static volatile bool   s_ota_klar;
static volatile bool   s_ota_genstart;  /* ny firmware er klar       */

static volatile bool  s_net_igang;

/* Naar vi tidligst proever igen. Et mislykket forsoeg maa ikke
 * gentages hvert sekund. */
static int64_t s_price_next_try;

static bool s_demo;
static bool s_demo_restart;
static zs_fr_info_t s_demo_info;

/* Ventetid foer naeste forsoeg, vokser for hvert fejlslagent. */
static uint32_t s_backoff_ms = ZS_RECONNECT_MIN_MS;

const char *zs_version(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    return (d != NULL && d->version[0] != '\0') ? d->version : "ukendt";
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* ------------------------------------------------------------------ */
/* Elprisen                                                            */
/* ------------------------------------------------------------------ */

static void net_task(void *arg)
{
    (void)arg;
    for (;;) {
        net_job_t job;
        if (xQueueReceive(s_net_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        s_net_igang = true;

        switch (job) {
        case JOB_PRICE:
            zs_price_fetch(s_price_zone, &s_price_hentet);
            /* Resultatet skal vaere skrevet FOER flaget saettes,
             * ellers kan hovedopgaven naa at laese en halv struct. */
            s_price_klar = true;
            break;

        case JOB_OTA:
            if (zs_ota_check_and_install(&s_ota_hentet)) {
                s_ota_hentet.state = ZS_OTA_READY;
                s_ota_genstart = true;
            }
            s_ota_klar = true;
            break;
        }
        s_net_igang = false;
    }
}

/* Laegger et stykke arbejde i koen. Er der allerede noget i gang eller
 * i koe, sker der ingenting: begge dele maa gerne vente. */
static void net_job(net_job_t job)
{
    if (s_net_queue == NULL || s_net_igang) {
        return;
    }
    xQueueSend(s_net_queue, &job, 0);
}

/* ------------------------------------------------------------------ */
/* Uret                                                                */
/* ------------------------------------------------------------------ */

static void clock_start(void)
{
    if (esp_sntp_enabled()) {
        return;
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ZS_NTP_SERVER_1);
    esp_sntp_setservername(1, ZS_NTP_SERVER_2);
    esp_sntp_init();

    setenv("TZ", ZS_TIMEZONE, 1);
    tzset();
    ESP_LOGI(TAG, "henter tid fra %s", ZS_NTP_SERVER_1);
}

/* Fylder s_time_text. Uret er pynt: er der intet internet, skjuler vi
 * klokkeslaettet i stedet for at vise et forkert et. */
static void clock_update(void)
{
    time_t t = time(NULL);
    /* Cirka november 2023. Alt foer det betyder at uret aldrig er sat. */
    if (t < 1700000000) {
        s_clock_ok = false;
        s_time_text[0] = '\0';
        return;
    }
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(s_time_text, sizeof(s_time_text), "%02d:%02d", lt.tm_hour, lt.tm_min);
    s_clock_ok = true;
}

/* ------------------------------------------------------------------ */
/* Aflaesning                                                          */
/* ------------------------------------------------------------------ */

static void publish_home(void)
{
    if (s_demo) {
        /*
         * Demoen gaar gennem PRAECIS den samme vej som rigtige tal.
         * Brugerfladen har ikke én eneste demo-gren: den faar en
         * zs_home_data_t og tegner den. Det er hele pointen med at
         * samle det her ét sted.
         */
        s_home.have_data   = true;
        s_home.stale       = false;
        s_home.has_meter   = true;
        s_home.has_battery = true;
        s_home.link        = ZS_LINK_OK;
        s_home.rssi        = -55;
        s_home.time_text   = zs_demo_clock();
        s_home.demo        = true;
        zs_ui_set_home(&s_home);
        return;
    }
    s_home.demo = false;

    int64_t age = now_ms() - s_last_good_ms;

    s_home.stale = (s_last_good_ms == 0) || (age > ZS_STALE_AFTER_MS);
    s_home.has_meter   = s_fr.info.has_meter;
    s_home.has_battery = s_fr.info.has_battery;
    s_home.rssi = zs_wifi_rssi();
    s_home.time_text = s_clock_ok ? s_time_text : NULL;

    if (zs_wifi_state() != ZS_WIFI_CONNECTED) {
        s_home.link = ZS_LINK_NO_WIFI;
    } else if (!zs_fr_is_connected(&s_fr)) {
        s_home.link = ZS_LINK_NO_INVERTER;
    } else if (s_home.stale) {
        s_home.link = ZS_LINK_CONNECTING;
    } else {
        s_home.link = ZS_LINK_OK;
    }

    zs_ui_set_home(&s_home);
}

static void poll_once(void)
{
    zs_fr_live_t live;
    if (zs_fr_poll(&s_fr, &live)) {
        s_home.live = live;
        s_home.have_data = true;
        s_last_good_ms = now_ms();
        s_backoff_ms = ZS_RECONNECT_MIN_MS;
    }
    publish_home();
}

/* ------------------------------------------------------------------ */
/* Forbindelse til inverteren                                          */
/* ------------------------------------------------------------------ */

static bool inverter_connect(void)
{
    if (s_cfg.inverter_ip[0] == '\0') {
        return false;
    }
    /*
     * Flere hurtige forsoeg foer vi venter.
     *
     * En Fronius Gen24 holder sin side af en afbrudt forbindelse aaben
     * i op til et kvarter og afviser nye forsoeg imens. I praksis
     * kommer man som regel igennem i andet eller tredje forsoeg, saa
     * det er dumt at vente et minut efter det foerste.
     */
    for (int i = 1; i <= ZS_RECONNECT_BURST; i++) {
        if (zs_fr_connect(&s_fr, s_cfg.inverter_ip, s_cfg.inverter_port,
                          s_cfg.inverter_unit)) {
            s_fr.meter_import_positive = s_cfg.meter_import_positive;
            if (i > 1) {
                ESP_LOGI(TAG, "forbundet i forsøg %d", i);
            }
            return true;
        }
        if (i < ZS_RECONNECT_BURST) {
            vTaskDelay(pdMS_TO_TICKS(ZS_RECONNECT_BURST_MS));
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Kommandoer fra brugerfladen                                         */
/* ------------------------------------------------------------------ */

static void scan_progress(void *ctx, int done, int total, int found)
{
    (void)ctx;
    zs_ui_set_scan_progress(done, total, found);
}

static void do_inverter_scan(void)
{
    char subnet[16];
    if (!zs_wifi_get_subnet(subnet, sizeof(subnet), NULL)) {
        zs_ui_set_inverter_list(NULL, 0);
        zs_ui_show(ZS_SCREEN_INVERTER_LIST);
        return;
    }
    zs_ui_show(ZS_SCREEN_INVERTER_SCAN);
    zs_ui_set_scan_progress(0, 254, 0);

    const char *prefer = s_cfg.inverter_ip[0] ? s_cfg.inverter_ip : NULL;
    int n = zs_discovery_scan(subnet, prefer, s_found, ZS_DISCOVERY_MAX,
                              scan_progress, NULL);
    if (zs_discovery_was_aborted()) {
        /* Brugeren trykkede tilbage mens vi soegte. Vi viser IKKE
         * resultatet: saa ville skaermen rive dem tilbage til listen
         * over invertere et halvt sekund efter de gik derfra. */
        ESP_LOGI(TAG, "søgningen blev afbrudt, viser ikke resultatet");
        return;
    }
    if (n < 0) {
        n = 0;
    }
    zs_ui_set_inverter_list(s_found, n);
    zs_ui_show(ZS_SCREEN_INVERTER_LIST);
}

static void handle_cmd(const zs_cmd_t *c)
{
    switch (c->type) {

    case ZS_CMD_SETUP_CONTINUE: {
        /*
         * "Kom i gang" er trykket.
         *
         * Har vi et netvaerk og et kodeord fra sidst, proever vi det
         * foerst. Ellers, og hvis det ikke lykkes, viser vi listen.
         * Det sparer brugeren for at taste et kodeord de allerede har
         * tastet én gang.
         */
        if (s_cfg.wifi_ssid[0] != '\0' &&
            zs_wifi_state() != ZS_WIFI_CONNECTED) {
            char t[96];
            snprintf(t, sizeof(t), "Forbinder til %s ...", s_cfg.wifi_ssid);
            zs_ui_set_connect_status(t, false, false);
            zs_ui_show(ZS_SCREEN_CONNECTING);

            if (zs_wifi_connect(s_cfg.wifi_ssid, s_cfg.wifi_pass,
                                ZS_WIFI_CONNECT_TIMEOUT_MS)) {
                clock_start();
                do_inverter_scan();
                break;
            }
            ESP_LOGW(TAG, "det gemte netværk svarede ikke: %s",
                     zs_wifi_last_error());
        }
        if (zs_wifi_state() == ZS_WIFI_CONNECTED) {
            do_inverter_scan();
            break;
        }
        /* Ingen gemte oplysninger, eller de virkede ikke. Vis listen. */
        zs_ui_show(ZS_SCREEN_WIFI_LIST);
        zs_ui_set_wifi_scanning(true);
        {
            int n = zs_wifi_scan(s_aps, ZS_WIFI_MAX_APS);
            zs_ui_set_wifi_scanning(false);
            zs_ui_set_wifi_list(s_aps, n < 0 ? 0 : n);
        }
        break;
    }

    case ZS_CMD_WIFI_SCAN: {
        zs_ui_set_wifi_scanning(true);
        int n = zs_wifi_scan(s_aps, ZS_WIFI_MAX_APS);
        zs_ui_set_wifi_scanning(false);
        zs_ui_set_wifi_list(s_aps, n < 0 ? 0 : n);
        break;
    }

    case ZS_CMD_WIFI_CONNECT: {
        zs_ui_set_connect_status("Forbinder til netværket ...", false, false);
        if (!zs_wifi_connect(c->ssid, c->pass, ZS_WIFI_CONNECT_TIMEOUT_MS)) {
            zs_ui_set_connect_status(zs_wifi_last_error(), true, true);
            break;
        }
        /* Gem foerst naar det virkede. Ellers ville et forkert kodeord
         * blive husket, og skaermen ville proeve det igen ved hver
         * opstart uden nogensinde at komme videre. */
        snprintf(s_cfg.wifi_ssid, sizeof(s_cfg.wifi_ssid), "%s", c->ssid);
        snprintf(s_cfg.wifi_pass, sizeof(s_cfg.wifi_pass), "%s", c->pass);
        /*
         * Gem med det samme, ogsaa selvom der endnu ikke er valgt en
         * inverter. "Sat op"-flaget bliver foerst sat naar der ER én,
         * saa skaermen starter stadig i opsaetningen. Men kodeordet
         * skal ikke tastes igen bare fordi stroemmen gik mens man
         * ledte efter inverteren.
         */
        if (!zs_nvs_save(&s_cfg)) {
            ESP_LOGW(TAG, "netværket kunne ikke gemmes");
        }

        clock_start();
        zs_ui_set_connect_status("Forbundet", false, false);
        do_inverter_scan();
        break;
    }

    case ZS_CMD_INVERTER_SCAN:
        do_inverter_scan();
        break;

    case ZS_CMD_INVERTER_SELECT: {
        snprintf(s_cfg.inverter_ip, sizeof(s_cfg.inverter_ip), "%s", c->ip);
        s_cfg.inverter_port = 502;
        s_cfg.inverter_unit = 1;
        s_cfg.configured = true;
        if (!zs_nvs_save(&s_cfg)) {
            /* Skaermen virker videre, men den har glemt valget naar
             * stroemmen har vaeret af. Det skal staa i loggen, ikke
             * forsvinde i stilhed. */
            ESP_LOGE(TAG, "valget kunne ikke gemmes. Skærmen virker nu, "
                          "men skal sættes op igen efter en genstart");
        }

        s_last_good_ms = 0;
        s_home.have_data = false;
        s_state = ST_CONNECTING;
        /*
         * ÉT sted bestemmer hvor brugeren skal hen. Mangler der et
         * prisomraade, er opsaetningen ikke faerdig endnu.
         */
        if (s_cfg.price_zone[0] == '\0') {
            zs_setup_zone_set_return(ZS_SCREEN_INVERTER_LIST);
            zs_ui_show(ZS_SCREEN_PRICE_ZONE);
        } else {
            zs_ui_show(ZS_SCREEN_HOME);
        }
        break;
    }

    case ZS_CMD_SETUP_RESTART:
        zs_fr_disconnect(&s_fr);
        s_state = ST_SETUP;
        zs_ui_show(ZS_SCREEN_WIFI_LIST);
        zs_ui_set_wifi_scanning(true);
        {
            int n = zs_wifi_scan(s_aps, ZS_WIFI_MAX_APS);
            zs_ui_set_wifi_scanning(false);
            zs_ui_set_wifi_list(s_aps, n < 0 ? 0 : n);
        }
        break;

    case ZS_CMD_SET_BRIGHTNESS:
        s_cfg.brightness = c->u8;
        zs_display_set_brightness(c->u8);
        zs_nvs_save(&s_cfg);
        break;

    case ZS_CMD_SET_NIGHT_DIM:
        s_cfg.night_dimming = c->flag;
        zs_display_set_night_dimming(c->flag);
        zs_nvs_save(&s_cfg);
        break;

    case ZS_CMD_SET_THEME:
        if (c->u8 >= ZS_THEME_COUNT) {
            break;
        }
        if (c->u8 == s_cfg.theme) {
            break;
        }
        s_cfg.theme = c->u8;
        zs_ui_set_theme((zs_theme_mode_t)c->u8);
        if (!zs_nvs_save(&s_cfg)) {
            ESP_LOGW(TAG, "temaet kunne ikke gemmes");
        }
        /* Siderne er nybyggede og tomme. Faa alt ind i dem paa naeste
         * gennemgang, som er hoejst 200 ms vaek. */
        s_ui_genopfrisk = true;
        break;

    case ZS_CMD_SET_METER_SIGN:
        s_cfg.meter_import_positive = c->flag;
        s_fr.meter_import_positive = c->flag;
        /* Taelleren nulstilles: den gamle vaerdi sagde noget om den
         * gamle indstilling og ville forvirre paa Detaljer-siden. */
        s_fr.negative_house_count = 0;
        zs_nvs_save(&s_cfg);
        break;

#if ZS_DEMO_ENABLED
    case ZS_CMD_DEMO_START:
        ESP_LOGI(TAG, "demo startet");
        /* Slip inverteren og wifi. Demoen skal kunne koere paa et bord
         * uden netvaerk overhovedet. */
        zs_fr_disconnect(&s_fr);
        zs_demo_reset();
        zs_demo_info(&s_demo_info);
        s_demo = true;
        s_state = ST_DEMO;
        s_demo_restart = true;
        /* Er der ikke valgt et prisomraade, viser vi opdigtede priser
         * saa siden ogsaa kan ses. Er der ét, bruger vi de rigtige:
         * priserne kommer fra internettet og har intet med inverteren
         * at goere, saa de er lige saa rigtige i demo. */
        if (s_cfg.price_zone[0] == '\0') {
            zs_demo_price(&s_price);
            zs_ui_set_price(&s_price);
        }
        zs_ui_set_demo(true);
        zs_ui_show(ZS_SCREEN_HOME);
        break;

    case ZS_CMD_DEMO_STOP:
        ESP_LOGI(TAG, "demo afsluttet");
        s_demo = false;
        zs_ui_set_demo(false);
        memset(&s_home, 0, sizeof(s_home));
        s_last_good_ms = 0;
        /*
         * Ryd ogsaa de opdigtede priser.
         *
         * Uden det ville demoens prisekurve blive staaende og se ud
         * som rigtige priser, for der er intet der overskriver den
         * naar der ikke er valgt et prisomraade.
         */
        memset(&s_price, 0, sizeof(s_price));
        s_price_next_try = 0;
        if (s_cfg.price_zone[0] == '\0') {
            snprintf(s_price.fejl, sizeof(s_price.fejl),
                     "Vælg dit prisområde under Indstillinger");
        }
        zs_ui_set_price(&s_price);
        /* Tilbage til det skaermen ellers ville have lavet: drift hvis
         * den er sat op, ellers opsaetningen forfra. */
        if (s_cfg.configured) {
            s_state = ST_CONNECTING;
            zs_ui_show(ZS_SCREEN_HOME);
        } else {
            s_state = ST_SETUP;
            zs_ui_show(ZS_SCREEN_WELCOME);
        }
        break;
#else
    case ZS_CMD_DEMO_START:
    case ZS_CMD_DEMO_STOP:
        /* Demoen er slaaet fra i denne udgave. Se ZS_DEMO_ENABLED. */
        break;
#endif

    case ZS_CMD_SET_PRICE_ZONE: {
        const char *z = c->ssid;
        if (strcmp(z, "DK1") != 0 && strcmp(z, "DK2") != 0) {
            break;
        }
        /* Praecis tre tegn. Feltet er 4 bytes, og z kommer fra en
         * besked hvor strengen kan vaere op til 32. */
        snprintf(s_cfg.price_zone, sizeof(s_cfg.price_zone), "%.3s", z);
        if (!zs_nvs_save(&s_cfg)) {
            ESP_LOGW(TAG, "prisområdet kunne ikke gemmes");
        }
        ESP_LOGI(TAG, "prisområde sat til %s", z);
        /* Hent med det samme, saa siden ikke staar tom mens kunden
         * kigger paa den. */
        s_price_next_try = 0;
        memset(&s_price, 0, sizeof(s_price));
        zs_ui_set_price(&s_price);
        /* Opsaetningen er faerdig naar der ogsaa er en inverter. */
        if (s_cfg.configured) {
            zs_ui_show(ZS_SCREEN_HOME);
        }
        break;
    }

    case ZS_CMD_FACTORY_RESET:
        ESP_LOGW(TAG, "fabriksnulstilling");
        zs_nvs_factory_reset();
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
        break;

    case ZS_CMD_CHECK_UPDATE:
        if (zs_wifi_state() != ZS_WIFI_CONNECTED) {
            s_ota.state = ZS_OTA_FAILED;
            snprintf(s_ota.fejl, sizeof(s_ota.fejl), "Der er ingen netværksforbindelse");
            zs_ui_set_ota(&s_ota);
            break;
        }
        s_ota.state = ZS_OTA_CHECKING;
        zs_ui_set_ota(&s_ota);
        s_ota_klar = false;
        net_job(JOB_OTA);
        break;

    case ZS_CMD_REBOOT:
        ESP_LOGI(TAG, "genstarter");
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Opgaven                                                             */
/* ------------------------------------------------------------------ */

/*
 * Laeser de gemte indstillinger, én gang.
 *
 * Kaldes fra app_main FOER skaermen og brugerfladen bygges. Ellers
 * ville skaermen taende med standardlysstyrken og i moerkt tema, for
 * saa at rette sig selv et oejeblik senere. Det ser ud som en fejl,
 * ogsaa selvom det ender rigtigt.
 *
 * Kaldes den to gange, sker der ingenting anden gang.
 */
void zs_app_load_settings(void)
{
    if (s_cfg_laest) {
        return;
    }
    s_cfg_laest      = true;
    s_cfg_configured = zs_nvs_load(&s_cfg);
    zs_theme_set_mode((zs_theme_mode_t)s_cfg.theme);
}

uint8_t zs_app_saved_brightness(void)
{
    return s_cfg_laest ? s_cfg.brightness : ZS_BRIGHTNESS_DEFAULT;
}

static void app_task(void *arg)
{
    (void)arg;

    zs_fr_init(&s_fr);
    memset(&s_home, 0, sizeof(s_home));

    zs_app_load_settings();
    bool configured = s_cfg_configured;
    zs_display_set_brightness(s_cfg.brightness);
    zs_display_set_night_dimming(s_cfg.night_dimming);
    s_fr.meter_import_positive = s_cfg.meter_import_positive;

    /*
     * Skriv hvad der rent faktisk blev taget i brug, ikke bare hvad der
     * stod i lageret. Ellers kan man ikke se forskel paa en indstilling
     * der blev laest og en der blev laest og derefter ignoreret, og det
     * er praecis den slags der forsvinder i stilhed.
     */
    ESP_LOGI(TAG, "i brug: lys %u %%, natdæmpning %s, prisområde %s, "
                  "elmåler %s, inverter %s:%u enhed %u",
             s_cfg.brightness,
             s_cfg.night_dimming ? "til" : "fra",
             s_cfg.price_zone[0] != '\0' ? s_cfg.price_zone : "ikke valgt",
             s_cfg.meter_import_positive ? "import positiv" : "import negativ",
             s_cfg.inverter_ip[0] != '\0' ? s_cfg.inverter_ip : "ingen",
             s_cfg.inverter_port, s_cfg.inverter_unit);

    if (!zs_wifi_init()) {
        ESP_LOGE(TAG, "wifi kunne ikke startes: %s", zs_wifi_last_error());
    }

    /* Netvaerksopgaven. Se noten ved net_task om hvorfor der kun er én.
     * 10 KB stak: mbedTLS' haandtryk med certifikatbundtet er det
     * tungeste vi laver. Prioritet 4, under hovedopgaven, saa en
     * langsom server aldrig forsinker aflaesningen fra inverteren. */
    s_net_queue = xQueueCreate(4, sizeof(net_job_t));
    if (s_net_queue == NULL ||
        xTaskCreate(net_task, "zs_net", 10240, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "netværksopgaven kunne ikke startes");
    }

    /*
     * Prissiden skal have besked fra starten, ogsaa naar der ikke er
     * noget at vise. Ellers staar den med de tomme pladsholdere fra
     * da den blev bygget, og brugeren faar ingen forklaring paa
     * hvorfor der ikke er priser.
     */
    if (s_cfg.price_zone[0] == '\0') {
        snprintf(s_price.fejl, sizeof(s_price.fejl),
                 "Vælg dit prisområde under Indstillinger");
    } else {
        snprintf(s_price.fejl, sizeof(s_price.fejl), "Henter priser ...");
    }
    zs_ui_set_price(&s_price);

    if (configured) {
        s_state = ST_CONNECTING;
        zs_ui_show(ZS_SCREEN_HOME);
    } else {
        s_state = ST_SETUP;
        zs_ui_show(ZS_SCREEN_WELCOME);
    }

#if ZS_SELFTEST
    zs_selftest_run();
#endif

    int64_t next_poll = 0;
    int64_t next_retry = 0;
    int64_t next_tick = 0;
    int64_t next_detail = 0;
    int64_t next_demo_step = 0;
    int64_t last_demo_ms = 0;
    /*
     * Foerste tjek efter et halvt minut, saa wifi og ur naar at komme
     * op foerst. Derefter hver halve time.
     */
    int64_t next_ota_check = now_ms() + 30 * 1000;
    /*
     * Naar den koerende firmware maa meldes i orden.
     *
     * To minutter uden nedbrud. Meldte vi den i orden med det samme,
     * ville hele vaernet vaere vaek: en firmware der gaar ned efter et
     * minut ville vaere godkendt paa det foerste sekund, og saa er der
     * ingen vej tilbage for en skaerm paa en vaeg.
     */
    int64_t ota_ok_at = zs_ota_pending_verify() ? (now_ms() + 120 * 1000) : 0;
    if (ota_ok_at != 0) {
        ESP_LOGI(TAG, "ny firmware, meldes i orden om to minutter hvis alt går godt");
    }
    /* Vi starter med en vaerdi der ikke er en rigtig skaerm, saa den
     * foerste gennemgang altid taeller som "siden blev skiftet". */
    zs_screen_id_t last_screen = (zs_screen_id_t)-1;

    for (;;) {
        /*
         * Vent paa en besked, men aldrig laengere end 200 ms.
         *
         * Det er hjerteslaget: uden det ville en skaerm der ikke bliver
         * roert ved aldrig komme videre til at laese fra inverteren.
         * Med det bliver alt nedenfor kigget efter mindst fem gange i
         * sekundet, ogsaa naar der ikke sker noget.
         */
        zs_cmd_t cmd;
        if (xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
            handle_cmd(&cmd);
            continue;
        }

        int64_t t = now_ms();

        if (s_ui_genopfrisk) {
            s_ui_genopfrisk = false;
            /* Tving Indstillinger og Detaljer til at blive fyldt igen. */
            last_screen = (zs_screen_id_t)-1;
            next_detail = 0;
            zs_ui_set_home(&s_home);
            zs_ui_set_price(&s_price);
            zs_ui_set_demo(s_demo);
        }

        /*
         * Indstillinger og Detaljer fyldes uanset hvilken tilstand vi
         * er i.
         *
         * Det stod tidligere NEDERST i loekken, efter de steder hvor vi
         * springer videre. I demo og under opsaetning naaede vi aldrig
         * derned, og saa stod lysstyrke-skyderen paa nul selvom
         * skaermen lyste. Den kom foerst til at passe naar man rykkede
         * i den.
         *
         * Indstillinger fyldes ÉN gang, naar man kommer ind paa siden.
         * Gjorde vi det bliver ved, ville skyderen faa sat sin vaerdi
         * fem gange i sekundet mens brugeren traekker i den, og saa
         * ville den hoppe tilbage under fingeren.
         *
         * Detaljer bygges helt om hver gang, saa den faar én gang i
         * sekundet.
         */
        {
            zs_screen_id_t scr = zs_ui_current();
            char ip[16];

            if (scr != last_screen) {
                last_screen = scr;
                next_detail = 0;
                if (scr == ZS_SCREEN_SETTINGS) {
                    zs_wifi_get_ip(ip, sizeof(ip));
                    zs_ui_set_settings(&s_cfg, ip);
                    zs_ui_set_ota(&s_ota);
                }
            }
            if (scr == ZS_SCREEN_DETAILS && t >= next_detail) {
                next_detail = t + 1000;
                if (s_demo) {
                    static zs_fr_t vis;
                    zs_fr_init(&vis);
                    vis.info = s_demo_info;
                    snprintf(vis.host, sizeof(vis.host), "demo");
                    vis.port = 502;
                    zs_ui_set_details(&vis, "demo", -55);
                } else {
                    zs_wifi_get_ip(ip, sizeof(ip));
                    zs_ui_set_details(&s_fr, ip, zs_wifi_rssi());
                }
            }
        }

        /* Uret og lysstyrken, ét sekund ad gangen. */
        if (t >= next_tick) {
            next_tick = t + 1000;
            clock_update();
            zs_display_tick();

            /*
             * Elprisen.
             *
             * Den staar for sig selv: den kommer fra internettet og
             * ikke fra inverteren, den skifter én gang i doegnet, og
             * den skal virke ogsaa selvom inverteren er nede.
             *
             * Vi henter naar der er et prisomraade, et ur og wifi, og
             * enten ikke har priser eller har priser fra i gaar. Ved
             * fejl venter vi ti minutter foer naeste forsoeg, saa en
             * server der er nede ikke bliver spurgt hvert sekund.
             */
            /* Meld den nye firmware i orden naar den har koert et
             * stykke tid uden at gaa ned. */
            if (ota_ok_at != 0 && t >= ota_ok_at) {
                ota_ok_at = 0;
                zs_ota_mark_ok();
            }

            /* Vis hvor langt hentningen er naaet, mens den koerer. */
            if (s_net_igang && s_ota_hentet.state == ZS_OTA_DOWNLOADING &&
                zs_ui_current() == ZS_SCREEN_SETTINGS) {
                zs_ui_set_ota(&s_ota_hentet);
            }

            /* Er der hentet en opdatering? */
            if (s_ota_klar) {
                s_ota_klar = false;
                s_ota = s_ota_hentet;
                zs_ui_set_ota(&s_ota);
                if (s_ota_genstart) {
                    ESP_LOGI(TAG, "genstarter for at tage %s i brug", s_ota.nyeste);
                    /* Kort pause saa loggen naar ud af porten. */
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
            }

            /*
             * Se efter en opdatering.
             *
             * Kun naar der er netvaerk, og aldrig mens demoen koerer:
             * en genstart midt i en fremvisning er ikke rart. Aldrig
             * heller mens opsaetningen staar paa, hvor brugeren er i
             * gang med at taste.
             */
            if (t >= next_ota_check && !s_net_igang && !s_demo &&
                s_state != ST_SETUP &&
                zs_wifi_state() == ZS_WIFI_CONNECTED) {
                next_ota_check = t + ZS_OTA_CHECK_INTERVAL_MS;
                s_ota_klar = false;
                net_job(JOB_OTA);
            }

            /* Er hente-opgaven blevet faerdig? */
            if (s_price_klar) {
                s_price_klar = false;
                s_price = s_price_hentet;
                s_price_next_try = s_price.ok ? 0 : (now_ms() + 10 * 60 * 1000);
                zs_ui_set_price(&s_price);
            }

            bool demo_priser = s_demo && s_cfg.price_zone[0] == '\0';

            if (!demo_priser && s_cfg.price_zone[0] != '\0' && s_clock_ok &&
                zs_wifi_state() == ZS_WIFI_CONNECTED &&
                !s_net_igang && t >= s_price_next_try &&
                (!s_price.ok || zs_price_is_stale(&s_price))) {

                snprintf(s_price_zone, sizeof(s_price_zone), "%.3s",
                         s_cfg.price_zone);
                s_price_klar = false;
                net_job(JOB_PRICE);
            } else if (!demo_priser && s_price.ok) {
                /* Flyt den fremhaevede soejle naar klokken skifter
                 * time. Koster ingen netvaerkstrafik. */
                int8_t foer = s_price.nu;
                zs_price_update_now(&s_price);
                if (s_price.nu != foer) {
                    zs_ui_set_price(&s_price);
                }
            }
        }

        if (s_state == ST_DEMO) {
            if (s_demo_restart) {
                /* Foerste skridt med det samme, saa skaermen ikke staar
                 * tom i to sekunder efter tryk paa "Se demo". */
                s_demo_restart = false;
                next_demo_step = 0;
                last_demo_ms = 0;
            }
            /* Demoen har sin egen takt. Den roerer hverken wifi eller
             * Modbus, saa resten af loekken springes over. */
            if (t >= next_demo_step) {
                /*
                 * Vi maaler tiden siden sidste skridt direkte i stedet
                 * for at regne den ud af den planlagte tid. Skifter man
                 * til demo mens en anden takt allerede var i gang, ville
                 * den udregning give et spring paa flere minutter i
                 * demoens ur.
                 *
                 * Foerste skridt efter start faar den normale takt, og
                 * et langt ophold klippes til ét sekund: skaermen har
                 * vaeret optaget af noget andet, ikke rejst i tiden.
                 */
                uint32_t dt = (last_demo_ms == 0) ? ZS_POLL_INTERVAL_MS
                                                  : (uint32_t)(t - last_demo_ms);
                if (dt > 5000u) {
                    dt = 1000u;
                }
                last_demo_ms = t;
                next_demo_step = t + ZS_POLL_INTERVAL_MS;
                zs_demo_step(&s_home.live, dt);
                publish_home();

                /* Flyt den fremhaevede prissoejle med demoens ur. */
                if (s_cfg.price_zone[0] == '\0') {
                    int8_t foer = s_price.nu;
                    zs_demo_price(&s_price);
                    if (s_price.nu != foer) {
                        zs_ui_set_price(&s_price);
                    }
                }
            }
            continue;
        }

        if (s_state == ST_SETUP) {
            continue;
        }

        /* ── forbind hvis vi ikke er forbundet ── */
        if (zs_wifi_state() != ZS_WIFI_CONNECTED) {
            if (t >= next_retry) {
                ESP_LOGI(TAG, "prøver at komme på netværket igen");
                if (zs_wifi_connect(s_cfg.wifi_ssid, s_cfg.wifi_pass,
                                    ZS_WIFI_CONNECT_TIMEOUT_MS)) {
                    clock_start();
                    s_backoff_ms = ZS_RECONNECT_MIN_MS;
                } else {
                    /*
                     * Ventetiden fordobles for hvert forsoeg, op til et
                     * minut. Uden det ville skaermen hamre paa en
                     * router der er slukket, og baade bruge stroem og
                     * fylde luften med forespoergsler.
                     */
                    s_backoff_ms *= 2;
                    if (s_backoff_ms > ZS_RECONNECT_MAX_MS) {
                        s_backoff_ms = ZS_RECONNECT_MAX_MS;
                    }
                }
                next_retry = now_ms() + s_backoff_ms;
            }
            publish_home();
            continue;
        }

        if (!zs_fr_is_connected(&s_fr)) {
            if (t >= next_retry) {
                if (inverter_connect()) {
                    s_state = ST_RUNNING;
                    s_backoff_ms = ZS_RECONNECT_MIN_MS;
                    s_fr.reconnect_count++;
                } else {
                    ESP_LOGW(TAG, "inverteren på %s svarer ikke",
                             s_cfg.inverter_ip);
                    s_backoff_ms *= 2;
                    if (s_backoff_ms > ZS_RECONNECT_MAX_MS) {
                        s_backoff_ms = ZS_RECONNECT_MAX_MS;
                    }
                }
                next_retry = now_ms() + s_backoff_ms;
            }
            publish_home();
            continue;
        }

        /* ── vi er forbundet, saa laes ── */
        if (t >= next_poll) {
            next_poll = t + ZS_POLL_INTERVAL_MS;
            poll_once();

            /*
             * Har vi ikke faaet et brugbart svar laenge, saa luk og
             * forbind forfra. En Modbus-forbindelse kan sagtens se
             * aaben ud uden at der kommer noget igennem, og saa ville
             * vi staa og vise gamle tal for evigt.
             */
            if (s_last_good_ms > 0 &&
                now_ms() - s_last_good_ms > ZS_RECONNECT_AFTER_MS) {
                ESP_LOGW(TAG, "ingen brugbare tal i %d sekunder, forbinder forfra",
                         ZS_RECONNECT_AFTER_MS / 1000);
                zs_fr_disconnect(&s_fr);
                next_retry = now_ms();
            }
        }

    }
}

/* ------------------------------------------------------------------ */

bool zs_app_start(void)
{
    /* Otte pladser er rigeligt: brugeren kan ikke trykke hurtigere end
     * opgaven arbejder, og bliver koen alligevel fuld, er den foerste
     * besked allerede paa vej. */
    s_queue = xQueueCreate(8, sizeof(zs_cmd_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "kunne ikke lave beskedkøen");
        return false;
    }

    /*
     * 8 KB stak. Netvaerksscanningen har et bundt sockets og en
     * SunSpec-vandring i gang samtidig, og lwIP bruger ogsaa noget.
     * Prioritet 5 ligger over tomgang men under LVGL's tegning, saa
     * skaermen bliver ved med at foeles levende mens der scannes.
     */
    BaseType_t ok = xTaskCreate(app_task, "zs_app", 8192, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "kunne ikke starte opgaven");
        return false;
    }
    return true;
}

bool zs_app_send(const zs_cmd_t *cmd)
{
    if (s_queue == NULL || cmd == NULL) {
        return false;
    }
    /* Aldrig vente. Kaldes fra LVGL's opgave, og den maa ikke staa
     * stille fordi netvaerksopgaven er optaget. */
    return xQueueSend(s_queue, cmd, 0) == pdTRUE;
}

