#include "zs_wifi.h"
#include "zs_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi";

#define BIT_GOT_IP        BIT0
#define BIT_DISCONNECTED  BIT1

static EventGroupHandle_t s_events;
static esp_netif_t       *s_netif;
static zs_wifi_state_t    s_state = ZS_WIFI_OFF;
static char               s_error[96] = "";
static bool               s_inited = false;

/*
 * Vi forsoeger IKKE selv at forbinde igen inde i haendelseshaandteringen.
 *
 * Goer man det, faar man en loekke der proever i det uendelige saa
 * hurtigt den kan, ogsaa naar kodeordet er forkert. Skaermen ville staa
 * og hamre paa routeren uden nogensinde at fortaelle brugeren hvad der
 * er galt. Genforsoeg styres i stedet fra zs_app.c, med en pause der
 * bliver laengere for hvert forsoeg.
 */
static bool s_want_connected = false;

/* Fejlteksterne er skrevet til den der staar med skaermen i haanden,
 * ikke til den der har skrevet koden. */
static const char *reason_text(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_EXPIRE:
        return "Kodeordet passer ikke";
    case WIFI_REASON_NO_AP_FOUND:
        return "Netværket blev ikke fundet";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_NOT_ASSOCED:
        return "Netværket afviste forbindelsen";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "Signalet forsvandt";
    case WIFI_REASON_CONNECTION_FAIL:
        return "Der kunne ikke oprettes forbindelse";
    default:
        return "Kunne ikke forbinde til netværket";
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    (void)arg; (void)base;

    if (id == WIFI_EVENT_STA_START) {
        if (s_want_connected) {
            esp_wifi_connect();
        }
        return;
    }
    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        snprintf(s_error, sizeof(s_error), "%s", reason_text(d->reason));
        ESP_LOGW(TAG, "afbrudt, aarsag %u: %s", d->reason, s_error);
        s_state = ZS_WIFI_FAILED;
        xEventGroupSetBits(s_events, BIT_DISCONNECTED);
        return;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base,
                        int32_t id, void *data)
{
    (void)arg; (void)base;

    if (id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "forbundet, IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_error[0] = '\0';
        s_state = ZS_WIFI_CONNECTED;
        xEventGroupSetBits(s_events, BIT_GOT_IP);
    }
}

bool zs_wifi_init(void)
{
    if (s_inited) {
        return true;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        snprintf(s_error, sizeof(s_error), "Der er ikke hukommelse nok");
        return false;
    }

    if (esp_netif_init() != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "Netværksdelen kunne ikke startes");
        return false;
    }
    /* Haendelsesloekken kan allerede vaere lavet af noget andet. Det er
     * ikke en fejl, saa vi accepterer ESP_ERR_INVALID_STATE. */
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        snprintf(s_error, sizeof(s_error), "Netværksdelen kunne ikke startes");
        return false;
    }

    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == NULL) {
        snprintf(s_error, sizeof(s_error), "Wifi kunne ikke startes");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "Wifi kunne ikke startes");
        return false;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip_event, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* Indstillingerne gemmes i vores eget lager, ikke i wifi-stakkens.
     * Ét sted at kigge, og ét sted at slette ved fabriksnulstilling. */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_inited = true;
    s_state = ZS_WIFI_OFF;
    ESP_LOGI(TAG, "wifi klar");
    return true;
}

/* Sorterer staerkeste signal foerst. */
static int cmp_rssi(const void *a, const void *b)
{
    const zs_ap_t *x = a, *y = b;
    return (y->rssi > x->rssi) - (y->rssi < x->rssi);
}

int zs_wifi_scan(zs_ap_t *out, size_t max)
{
    if (!s_inited || out == NULL || max == 0) {
        return -1;
    }
    s_state = ZS_WIFI_SCANNING;

    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        /* 120 ms pr. kanal. Med 13 kanaler bliver det omkring halvandet
         * sekund. Kortere, og svage netvaerk naar ikke at svare. */
        .scan_time.active = { .min = 60, .max = 120 },
    };

    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    if (err != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "Der kunne ikke søges efter netværk");
        s_state = ZS_WIFI_FAILED;
        return -1;
    }

    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    if (found == 0) {
        esp_wifi_clear_ap_list();
        s_state = ZS_WIFI_OFF;
        return 0;
    }

    /* Vi henter flere end vi viser, saa der er noget at vaelge imellem
     * efter at gengangere er sorteret fra. */
    uint16_t want = found;
    if (want > ZS_WIFI_MAX_APS * 3) {
        want = ZS_WIFI_MAX_APS * 3;
    }
    wifi_ap_record_t *recs = calloc(want, sizeof(wifi_ap_record_t));
    if (recs == NULL) {
        esp_wifi_clear_ap_list();
        snprintf(s_error, sizeof(s_error), "Der er ikke hukommelse nok");
        s_state = ZS_WIFI_FAILED;
        return -1;
    }
    esp_wifi_scan_get_ap_records(&want, recs);

    size_t n = 0;
    for (uint16_t i = 0; i < want && n < max; i++) {
        if (recs[i].ssid[0] == '\0') {
            continue;   /* skjult netvaerk, kan ikke vaelges paa en liste */
        }

        /*
         * Et mesh-netvaerk sender det samme navn fra flere kasser. Uden
         * dette ville brugeren se "Stuen" fire gange og ikke vide
         * hvilken der var den rigtige. Vi beholder den med det bedste
         * signal.
         */
        bool dupe = false;
        for (size_t j = 0; j < n; j++) {
            if (strcmp(out[j].ssid, (const char *)recs[i].ssid) == 0) {
                if (recs[i].rssi > out[j].rssi) {
                    out[j].rssi = recs[i].rssi;
                }
                dupe = true;
                break;
            }
        }
        if (dupe) {
            continue;
        }

        snprintf(out[n].ssid, sizeof(out[n].ssid), "%s", (const char *)recs[i].ssid);
        out[n].rssi = recs[i].rssi;
        out[n].secured = (recs[i].authmode != WIFI_AUTH_OPEN);
        n++;
    }

    free(recs);
    esp_wifi_clear_ap_list();

    qsort(out, n, sizeof(zs_ap_t), cmp_rssi);

    s_state = ZS_WIFI_OFF;
    ESP_LOGI(TAG, "fandt %u netværk, viser %u", (unsigned)found, (unsigned)n);
    return (int)n;
}

bool zs_wifi_connect(const char *ssid, const char *pass, uint32_t timeout_ms)
{
    if (!s_inited || ssid == NULL || ssid[0] == '\0') {
        snprintf(s_error, sizeof(s_error), "Der er ikke valgt et netværk");
        return false;
    }
    if (timeout_ms == 0) {
        timeout_ms = ZS_WIFI_CONNECT_TIMEOUT_MS;
    }

    s_error[0] = '\0';
    s_state = ZS_WIFI_CONNECTING;
    s_want_connected = true;
    xEventGroupClearBits(s_events, BIT_GOT_IP | BIT_DISCONNECTED);

    wifi_config_t cfg = { 0 };
    snprintf((char *)cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid);
    if (pass != NULL && pass[0] != '\0') {
        snprintf((char *)cfg.sta.password, sizeof(cfg.sta.password), "%s", pass);
        /* WPA2 som mindstekrav. WEP og aabne netvaerk med kodeord er
         * ikke noget vi vil forbinde til: de er brudt, og en skaerm der
         * gaar paa dem giver kunden en falsk tryghed. */
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    /* WPA3 hvis routeren kan. Falder selv tilbage til WPA2. */
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;
    cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "Netværket kunne ikke sættes op");
        s_state = ZS_WIFI_FAILED;
        return false;
    }

    esp_wifi_disconnect();
    if (esp_wifi_connect() != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "Der kunne ikke oprettes forbindelse");
        s_state = ZS_WIFI_FAILED;
        return false;
    }

    /*
     * Vent paa enten en IP eller en afvisning, men aldrig laengere end
     * timeout_ms. Uden den graense kunne vi staa her for altid hvis
     * routeren hverken svarer eller afviser, og skaermen ville se
     * frossen ud uden at vaere det.
     */
    EventBits_t bits = xEventGroupWaitBits(
        s_events, BIT_GOT_IP | BIT_DISCONNECTED,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    if (bits & BIT_GOT_IP) {
        s_state = ZS_WIFI_CONNECTED;
        return true;
    }
    if (bits & BIT_DISCONNECTED) {
        /* s_error er allerede sat af haendelseshaandteringen. */
        s_state = ZS_WIFI_FAILED;
        s_want_connected = false;
        esp_wifi_disconnect();
        return false;
    }

    snprintf(s_error, sizeof(s_error),
             "Netværket svarede ikke. Prøv igen, eller flyt skærmen tættere på routeren");
    s_state = ZS_WIFI_FAILED;
    s_want_connected = false;
    esp_wifi_disconnect();
    return false;
}

void zs_wifi_disconnect(void)
{
    if (!s_inited) {
        return;
    }
    s_want_connected = false;
    esp_wifi_disconnect();
    s_state = ZS_WIFI_OFF;
}

zs_wifi_state_t zs_wifi_state(void)
{
    return s_state;
}

int zs_wifi_rssi(void)
{
    if (s_state != ZS_WIFI_CONNECTED) {
        return 0;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return 0;
    }
    return ap.rssi;
}

bool zs_wifi_get_ip(char *buf, size_t len)
{
    if (buf == NULL || len < 8) {
        return false;
    }
    buf[0] = '\0';
    if (s_netif == NULL || s_state != ZS_WIFI_CONNECTED) {
        return false;
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(s_netif, &info) != ESP_OK || info.ip.addr == 0) {
        return false;
    }
    snprintf(buf, len, IPSTR, IP2STR(&info.ip));
    return true;
}

bool zs_wifi_get_subnet(char *buf, size_t len, uint8_t *prefix_bits)
{
    if (buf == NULL || len < 8) {
        return false;
    }
    buf[0] = '\0';
    if (s_netif == NULL || s_state != ZS_WIFI_CONNECTED) {
        return false;
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(s_netif, &info) != ESP_OK || info.ip.addr == 0) {
        return false;
    }

    uint32_t ip   = ntohl(info.ip.addr);
    uint32_t mask = ntohl(info.netmask.addr);
    uint32_t net  = ip & mask;

    if (prefix_bits != NULL) {
        uint8_t bits = 0;
        for (int i = 31; i >= 0; i--) {
            if (mask & (1u << i)) { bits++; } else { break; }
        }
        *prefix_bits = bits;
    }

    snprintf(buf, len, "%u.%u.%u.%u",
             (unsigned)((net >> 24) & 0xFF), (unsigned)((net >> 16) & 0xFF),
             (unsigned)((net >> 8) & 0xFF),  (unsigned)(net & 0xFF));
    return true;
}

const char *zs_wifi_last_error(void)
{
    return s_error[0] != '\0' ? s_error : "Ukendt fejl";
}
