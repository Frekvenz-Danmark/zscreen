#include "zs_nvs.h"
#include "zs_config.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "nvs";

/* Alt ligger i ét navnerum. Noeglerne er korte fordi NVS hoejst tager
 * 15 tegn, hvilket er nemt at glemme indtil det pludselig fejler. */
#define NS          "zscreen"
#define K_SSID      "ssid"
#define K_PASS      "pass"
#define K_IP        "inv_ip"
#define K_PORT      "inv_port"
#define K_UNIT      "inv_unit"
#define K_METERSIGN "meter_pos"
#define K_ZONE      "pris_zone"
#define K_BRIGHT    "bright"
#define K_NIGHT     "night"
#define K_CONF      "configured"

void zs_nvs_defaults(zs_settings_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->inverter_port = 502;
    s->inverter_unit = 1;
    s->meter_import_positive = true;
    s->brightness = ZS_BRIGHTNESS_DEFAULT;
    s->night_dimming = true;
    s->configured = false;
}

/* Laeser en streng. Efterlader dst tom hvis noeglen ikke findes. */
static void get_str(nvs_handle_t h, const char *key, char *dst, size_t len)
{
    size_t n = len;
    if (nvs_get_str(h, key, dst, &n) != ESP_OK) {
        dst[0] = '\0';
        return;
    }
    /* NVS lover ikke en afsluttende nulbyte hvis noget er gaaet galt
     * under skrivningen. Vi saetter den selv, ellers kan en beskadiget
     * post loebe ud over bufferen naar den senere bruges. */
    dst[len - 1] = '\0';
}

static void get_u8(nvs_handle_t h, const char *key, uint8_t *dst)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) {
        *dst = v;
    }
}

static void get_u16(nvs_handle_t h, const char *key, uint16_t *dst)
{
    uint16_t v;
    if (nvs_get_u16(h, key, &v) == ESP_OK) {
        *dst = v;
    }
}

static void get_bool(nvs_handle_t h, const char *key, bool *dst)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) {
        *dst = (v != 0);
    }
}

bool zs_nvs_load(zs_settings_t *s)
{
    if (s == NULL) {
        return false;
    }
    zs_nvs_defaults(s);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Foerste opstart efter flashning. Ikke en fejl. */
        ESP_LOGI(TAG, "ingen gemte indstillinger, skaermen skal saettes op");
        return false;
    }

    get_str(h, K_SSID, s->wifi_ssid, sizeof(s->wifi_ssid));
    get_str(h, K_PASS, s->wifi_pass, sizeof(s->wifi_pass));
    get_str(h, K_IP,   s->inverter_ip, sizeof(s->inverter_ip));
    get_u16(h, K_PORT, &s->inverter_port);
    get_u8(h, K_UNIT,  &s->inverter_unit);
    get_bool(h, K_METERSIGN, &s->meter_import_positive);
    get_str(h, K_ZONE, s->price_zone, sizeof(s->price_zone));
    get_u8(h, K_BRIGHT, &s->brightness);
    get_bool(h, K_NIGHT, &s->night_dimming);
    get_bool(h, K_CONF,  &s->configured);

    nvs_close(h);

    /*
     * Tjek at det vi laeste kan bruges til noget.
     *
     * En halvskrevet post, fx efter en stroemafbrydelse midt i en
     * gemning, kan give et tomt netvaerksnavn og en flaget der siger
     * "sat op". Saa ville skaermen gaa direkte til hovedskaermen og
     * staa og vente paa en forbindelse der aldrig kommer, uden at
     * fortaelle brugeren hvorfor.
     */
    if (s->configured) {
        if (s->wifi_ssid[0] == '\0' || s->inverter_ip[0] == '\0') {
            ESP_LOGW(TAG, "gemte indstillinger er ufuldstaendige, "
                          "skaermen skal saettes op igen");
            zs_nvs_defaults(s);
            return false;
        }
    }
    if (s->inverter_port == 0) {
        s->inverter_port = 502;
    }
    if (s->inverter_unit == 0) {
        s->inverter_unit = 1;
    }
    if (s->brightness < 5 || s->brightness > 100) {
        s->brightness = ZS_BRIGHTNESS_DEFAULT;
    }

    ESP_LOGI(TAG, "indstillinger laest: netvaerk \"%s\", inverter %s:%u",
             s->wifi_ssid, s->inverter_ip, s->inverter_port);
    return s->configured;
}

bool zs_nvs_save(const zs_settings_t *s)
{
    if (s == NULL) {
        return false;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke aabne lageret: %s", esp_err_to_name(err));
        return false;
    }

    bool ok = true;

    /*
     * Er skaermen IKKE sat op, ryddes flaget FOERST og forpligtes for
     * sig. Saa kan en stroemafbrydelse midt i resten ikke efterlade et
     * flag der siger "sat op" oven paa halvt skrevne vaerdier.
     *
     * I dag saetter ingen sti flaget tilbage til false, men den dag
     * nogen tilfoejer "skift inverter", skal det virke uden at nogen
     * skal huske det her.
     */
    if (!s->configured) {
        ok &= nvs_set_u8(h, K_CONF, 0) == ESP_OK;
        ok &= nvs_commit(h) == ESP_OK;
    }

    ok &= nvs_set_str(h, K_SSID, s->wifi_ssid) == ESP_OK;
    ok &= nvs_set_str(h, K_PASS, s->wifi_pass) == ESP_OK;
    ok &= nvs_set_str(h, K_IP,   s->inverter_ip) == ESP_OK;
    ok &= nvs_set_u16(h, K_PORT, s->inverter_port) == ESP_OK;
    ok &= nvs_set_u8(h,  K_UNIT, s->inverter_unit) == ESP_OK;
    ok &= nvs_set_u8(h,  K_METERSIGN, s->meter_import_positive ? 1 : 0) == ESP_OK;
    ok &= nvs_set_str(h, K_ZONE, s->price_zone) == ESP_OK;
    ok &= nvs_set_u8(h,  K_BRIGHT, s->brightness) == ESP_OK;
    ok &= nvs_set_u8(h,  K_NIGHT, s->night_dimming ? 1 : 0) == ESP_OK;

    /*
     * "Sat op"-flaget skrives SIDST og forpligtes i sin egen omgang.
     *
     * NVS skriver foerst rigtigt naar man kalder commit. Ryger stroemmen
     * midt i, er det bedre at flaget mangler end at det staar der uden
     * at resten naaede med: saa beder skaermen om at blive sat op igen,
     * i stedet for at staa og vente paa en inverter den ikke kender
     * adressen paa.
     */
    if (ok) {
        ok &= nvs_commit(h) == ESP_OK;
    }
    if (ok && s->configured) {
        ok &= nvs_set_u8(h, K_CONF, 1) == ESP_OK;
        ok &= nvs_commit(h) == ESP_OK;
    }

    nvs_close(h);

    if (!ok) {
        ESP_LOGE(TAG, "indstillingerne kunne ikke gemmes");
    } else {
        ESP_LOGI(TAG, "indstillinger gemt");
    }
    return ok;
}

bool zs_nvs_factory_reset(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = (nvs_erase_all(h) == ESP_OK);
    ok &= (nvs_commit(h) == ESP_OK);
    nvs_close(h);

    if (ok) {
        ESP_LOGW(TAG, "alle indstillinger er slettet");
    }
    return ok;
}
