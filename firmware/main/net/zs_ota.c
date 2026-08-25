#include "zs_ota.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota";

/* GitHubs svar paa "nyeste udgivelse" fylder omkring 3 KB. Vi giver
 * plads til det tidobbelte og afviser alt derover. */
#define MAX_JSON        32768
#define API_TIMEOUT_MS  10000
#define OTA_TIMEOUT_MS  20000

/* GitHub afviser forespoergsler uden. Den siger hvem vi er, og
 * indeholder ikke noget om den enkelte enhed. */
#define USER_AGENT      "zScreen"

typedef struct {
    char  *buf;
    size_t len;
    bool   overloeb;
} hent_t;

static esp_err_t on_api_event(esp_http_client_event_t *e)
{
    hent_t *h = (hent_t *)e->user_data;
    if (h == NULL || h->buf == NULL) {
        return ESP_OK;
    }
    /* Ved en omdirigering skal kroppen af 3xx-svaret ikke blive
     * liggende foran det rigtige svar. Se samme note i zs_price.c. */
    if (e->event_id == HTTP_EVENT_ON_CONNECTED) {
        h->len = 0; h->overloeb = false; h->buf[0] = '\0';
        return ESP_OK;
    }
    if (e->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
    if (h->len + (size_t)e->data_len >= MAX_JSON) {
        h->overloeb = true;
        return ESP_OK;
    }
    memcpy(h->buf + h->len, e->data, (size_t)e->data_len);
    h->len += (size_t)e->data_len;
    h->buf[h->len] = '\0';
    return ESP_OK;
}

/* "v0.2.0" og "0.2.0" er det samme. */
static const char *strip_v(const char *s)
{
    return (s != NULL && (s[0] == 'v' || s[0] == 'V')) ? s + 1 : s;
}

/*
 * Sammenligner to versioner som tal, ikke som tekst.
 *
 * "0.10.0" er nyere end "0.9.0", men staar FOER den alfabetisk. Ville
 * vi bare have sammenlignet strengene, ville skaermen have staaet paa
 * 0.9.0 for evigt.
 *
 * Returnerer 1 hvis a er nyere end b, 0 hvis de er ens, -1 hvis a er
 * aeldre. Kan et af tallene ikke laeses, returneres -1: vi opdaterer
 * hellere ikke end at opdatere til noget vi ikke forstaar.
 */
static int version_cmp(const char *a, const char *b)
{
    unsigned av[3] = {0}, bv[3] = {0};
    if (sscanf(strip_v(a), "%u.%u.%u", &av[0], &av[1], &av[2]) != 3) {
        return -1;
    }
    if (sscanf(strip_v(b), "%u.%u.%u", &bv[0], &bv[1], &bv[2]) != 3) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        if (av[i] > bv[i]) { return 1; }
        if (av[i] < bv[i]) { return -1; }
    }
    return 0;
}

/*
 * Spoerger GitHub om den nyeste udgivelse.
 *
 * Fylder tag og url. Returnerer false ved fejl, og saa staar en dansk
 * forklaring i ud->fejl.
 */
static bool hent_udgivelse(zs_ota_status_t *ud, char *url, size_t url_len)
{
    char api[160];
    snprintf(api, sizeof(api),
             "https://api.github.com/repos/%s/%s/releases/latest",
             ZS_OTA_OWNER, ZS_OTA_REPO);

    hent_t h = { 0 };
    h.buf = heap_caps_malloc(MAX_JSON, MALLOC_CAP_SPIRAM);
    if (h.buf == NULL) {
        h.buf = malloc(MAX_JSON);
    }
    if (h.buf == NULL) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Der er ikke hukommelse nok");
        return false;
    }
    h.buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = api,
        .method = HTTP_METHOD_GET,
        .timeout_ms = API_TIMEOUT_MS,
        .event_handler = on_api_event,
        .user_data = &h,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = false,
        .max_redirection_count = 2,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        free(h.buf);
        snprintf(ud->fejl, sizeof(ud->fejl), "Forbindelsen kunne ikke sættes op");
        return false;
    }
    esp_http_client_set_header(c, "User-Agent", USER_AGENT);
    esp_http_client_set_header(c, "Accept", "application/vnd.github+json");

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    bool ok = false;
    if (err != ESP_OK) {
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "Kunne ikke nå GitHub. Er der internet?");
    } else if (status == 404) {
        /* Der er ingen udgivelser endnu. Ikke en fejl. */
        snprintf(ud->fejl, sizeof(ud->fejl), "Der er ingen udgivelser endnu");
    } else if (status == 403) {
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "GitHub afviste forespørgslen. Prøver igen senere");
    } else if (status != 200) {
        snprintf(ud->fejl, sizeof(ud->fejl), "GitHub svarede %d", status);
    } else if (h.overloeb || h.len == 0) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Svaret fra GitHub var uventet");
    } else {
        cJSON *rod = cJSON_Parse(h.buf);
        if (rod == NULL) {
            snprintf(ud->fejl, sizeof(ud->fejl), "Svaret kunne ikke læses");
        } else {
            cJSON *tag = cJSON_GetObjectItemCaseSensitive(rod, "tag_name");
            cJSON *aktiver = cJSON_GetObjectItemCaseSensitive(rod, "assets");

            if (cJSON_IsString(tag) && tag->valuestring != NULL) {
                snprintf(ud->nyeste, sizeof(ud->nyeste), "%.23s",
                         strip_v(tag->valuestring));
            }
            url[0] = '\0';
            if (cJSON_IsArray(aktiver)) {
                cJSON *a = NULL;
                cJSON_ArrayForEach(a, aktiver) {
                    cJSON *navn = cJSON_GetObjectItemCaseSensitive(a, "name");
                    cJSON *link = cJSON_GetObjectItemCaseSensitive(a, "browser_download_url");
                    if (!cJSON_IsString(navn) || !cJSON_IsString(link)) {
                        continue;
                    }
                    size_t n = strlen(navn->valuestring);
                    if (n > 4 && strcmp(navn->valuestring + n - 4, ".bin") == 0) {
                        snprintf(url, url_len, "%s", link->valuestring);
                        break;
                    }
                }
            }
            if (ud->nyeste[0] == '\0') {
                snprintf(ud->fejl, sizeof(ud->fejl),
                         "Udgivelsen manglede et versionsnummer");
            } else if (url[0] == '\0') {
                snprintf(ud->fejl, sizeof(ud->fejl),
                         "Udgivelsen manglede en firmware-fil");
            } else {
                ok = true;
            }
            cJSON_Delete(rod);
        }
    }

    free(h.buf);
    return ok;
}

bool zs_ota_pending_verify(void)
{
    const esp_partition_t *p = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (p == NULL || esp_ota_get_state_partition(p, &st) != ESP_OK) {
        return false;
    }
    return st == ESP_OTA_IMG_PENDING_VERIFY;
}

void zs_ota_mark_ok(void)
{
    if (!zs_ota_pending_verify()) {
        return;
    }
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        ESP_LOGI(TAG, "den nye firmware er meldt i orden");
    } else {
        ESP_LOGW(TAG, "kunne ikke melde firmwaren i orden");
    }
}

bool zs_ota_check_and_install(zs_ota_status_t *ud)
{
    if (ud == NULL) {
        return false;
    }
    const esp_app_desc_t *koerer = esp_app_get_description();
    /* Praecision, ikke pynt: esp_app_desc_t.version er 32 tegn og
     * vores felt er 24. Uden graensen kan oversaetteren ikke bevise at
     * det passer, og et versionsnummer paa 30 tegn ville blive klippet
     * midt over uden at nogen opdagede det. */
    snprintf(ud->koerende, sizeof(ud->koerende), "%.23s",
             koerer != NULL ? koerer->version : "ukendt");
    ud->fejl[0] = '\0';
    ud->procent = 0;
    ud->state = ZS_OTA_CHECKING;
    ud->har_tjekket = true;

    char url[256];
    if (!hent_udgivelse(ud, url, sizeof(url))) {
        ud->state = ZS_OTA_FAILED;
        ESP_LOGW(TAG, "%s", ud->fejl);
        return false;
    }

    int c = version_cmp(ud->nyeste, ud->koerende);
    if (c <= 0) {
        /*
         * Vi opdaterer KUN opad.
         *
         * Uden det ville en udgivelse der ved et uheld faar et lavere
         * nummer sende hele flaaden tilbage, og hvis den gamle udgave
         * saa opdaterer til den nye igen, ville enhederne skifte frem
         * og tilbage for evigt.
         */
        ud->state = ZS_OTA_UP_TO_DATE;
        ESP_LOGI(TAG, "vi kører %s, nyeste er %s, intet at gøre",
                 ud->koerende, ud->nyeste);
        return false;
    }

    ESP_LOGI(TAG, "opdaterer fra %s til %s", ud->koerende, ud->nyeste);
    ud->state = ZS_OTA_DOWNLOADING;

    esp_http_client_config_t http = {
        .url = url,
        .timeout_ms = OTA_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* GitHub sender filen videre til et andet vaertsnavn, saa
         * omdirigering SKAL foelges her. */
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota = {
        .http_config = &http,
        /* Headeren skal saettes paa hver omdirigering, ellers afviser
         * GitHubs filserver os. */
        .http_client_init_cb = NULL,
    };

    esp_https_ota_handle_t h = NULL;
    esp_err_t err = esp_https_ota_begin(&ota, &h);
    if (err != ESP_OK || h == NULL) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Kunne ikke hente firmwaren");
        ud->state = ZS_OTA_FAILED;
        return false;
    }

    int samlet = esp_https_ota_get_image_size(h);
    while (1) {
        err = esp_https_ota_perform(h);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        if (samlet > 0) {
            int hentet = esp_https_ota_get_image_len_read(h);
            int p = (int)((int64_t)hentet * 100 / samlet);
            ud->procent = (uint8_t)(p < 0 ? 0 : (p > 100 ? 100 : p));
        }
    }

    if (err != ESP_OK) {
        esp_https_ota_abort(h);
        /*
         * Her fanges ogsaa en firmware med forkert eller manglende
         * underskrift. esp_ota_ops tjekker signaturen mod den noegle
         * den KOERENDE firmware baerer, og afviser alt andet.
         */
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "Opdateringen blev afvist. Underskriften passer ikke");
        ud->state = ZS_OTA_FAILED;
        ESP_LOGE(TAG, "hentning faejlede: %s", esp_err_to_name(err));
        return false;
    }
    if (!esp_https_ota_is_complete_data_received(h)) {
        esp_https_ota_abort(h);
        snprintf(ud->fejl, sizeof(ud->fejl), "Filen kom ikke helt frem");
        ud->state = ZS_OTA_FAILED;
        return false;
    }

    err = esp_https_ota_finish(h);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            snprintf(ud->fejl, sizeof(ud->fejl),
                     "Opdateringen blev afvist. Underskriften passer ikke");
        } else {
            snprintf(ud->fejl, sizeof(ud->fejl),
                     "Opdateringen kunne ikke tages i brug");
        }
        ud->state = ZS_OTA_FAILED;
        ESP_LOGE(TAG, "finish faejlede: %s", esp_err_to_name(err));
        return false;
    }

    ud->procent = 100;
    ud->state = ZS_OTA_READY;
    ESP_LOGI(TAG, "%s er hentet og godkendt, genstarter", ud->nyeste);
    return true;
}
