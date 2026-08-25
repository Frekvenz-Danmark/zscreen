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
#include "zs_config.h"
#include "zs_ui.h"
#include "zs_wifi.h"
#include "zs_discovery.h"
#include "zs_fronius.h"
#include "zs_nvs.h"
#include "zs_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_sntp.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "app";

typedef enum {
    ST_SETUP = 0,
    ST_CONNECTING,
    ST_RUNNING,
} app_state_t;

static QueueHandle_t   s_queue;
static zs_settings_t   s_cfg;
static app_state_t     s_state = ST_SETUP;
static zs_fr_t         s_fr;
static zs_home_data_t  s_home;
static zs_found_t      s_found[ZS_DISCOVERY_MAX];
static zs_ap_t         s_aps[ZS_WIFI_MAX_APS];
static char            s_time_text[8];
static bool            s_clock_ok;

/* Hvornaar vi sidst fik et helt sæt tal, i millisekunder siden start. */
static int64_t s_last_good_ms;

/* Ventetid foer naeste forsoeg, vokser for hvert fejlslagent. */
static uint32_t s_backoff_ms = ZS_RECONNECT_MIN_MS;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
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
    if (n < 0) {
        n = 0;
    }
    zs_ui_set_inverter_list(s_found, n);
    zs_ui_show(ZS_SCREEN_INVERTER_LIST);
}

static void handle_cmd(const zs_cmd_t *c)
{
    switch (c->type) {

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
        zs_ui_show(ZS_SCREEN_HOME);
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

    case ZS_CMD_SET_METER_SIGN:
        s_cfg.meter_import_positive = c->flag;
        s_fr.meter_import_positive = c->flag;
        /* Taelleren nulstilles: den gamle vaerdi sagde noget om den
         * gamle indstilling og ville forvirre paa Detaljer-siden. */
        s_fr.negative_house_count = 0;
        zs_nvs_save(&s_cfg);
        break;

    case ZS_CMD_FACTORY_RESET:
        ESP_LOGW(TAG, "fabriksnulstilling");
        zs_nvs_factory_reset();
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
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

static void app_task(void *arg)
{
    (void)arg;

    zs_fr_init(&s_fr);
    memset(&s_home, 0, sizeof(s_home));

    bool configured = zs_nvs_load(&s_cfg);
    zs_display_set_brightness(s_cfg.brightness);
    zs_display_set_night_dimming(s_cfg.night_dimming);
    s_fr.meter_import_positive = s_cfg.meter_import_positive;

    if (!zs_wifi_init()) {
        ESP_LOGE(TAG, "wifi kunne ikke startes: %s", zs_wifi_last_error());
    }

    if (configured) {
        s_state = ST_CONNECTING;
        zs_ui_show(ZS_SCREEN_HOME);
    } else {
        s_state = ST_SETUP;
        zs_ui_show(ZS_SCREEN_WELCOME);
    }

    int64_t next_poll = 0;
    int64_t next_retry = 0;
    int64_t next_tick = 0;
    int64_t next_detail = 0;
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

        /* Uret og lysstyrken, ét sekund ad gangen. */
        if (t >= next_tick) {
            next_tick = t + 1000;
            clock_update();
            zs_display_tick();
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

        /*
         * Indstillinger og Detaljer opdateres kun naar de er fremme,
         * og med maade.
         *
         * Indstillinger fyldes ÉN gang, naar man kommer ind paa siden.
         * Gjorde vi det bliver ved, ville lysstyrke-skyderen faa sat
         * sin vaerdi fem gange i sekundet mens brugeren traekker i
         * den, og saa ville den hoppe tilbage under fingeren.
         *
         * Detaljer bygges helt om hver gang, saa den faar én gang i
         * sekundet. Fem gange i sekundet ville baade flimre og bruge
         * tid paa noget ingen naar at laese.
         */
        zs_screen_id_t scr = zs_ui_current();
        char ip[16];

        if (scr != last_screen) {
            last_screen = scr;
            next_detail = 0;
            if (scr == ZS_SCREEN_SETTINGS) {
                zs_wifi_get_ip(ip, sizeof(ip));
                zs_ui_set_settings(&s_cfg, ip);
            }
        }
        if (scr == ZS_SCREEN_DETAILS && t >= next_detail) {
            next_detail = t + 1000;
            zs_wifi_get_ip(ip, sizeof(ip));
            zs_ui_set_details(&s_fr, ip, zs_wifi_rssi());
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

