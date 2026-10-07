#include "zs_ota.h"
#include "zs_version.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota";

/* ------------------------------------------------------------------ */
/* Maalversionen. Se zs_ota.h for hvorfor den findes.                   */
/* ------------------------------------------------------------------ */

/* "v" + tre tal a hoejst fem cifre + to punktummer + afslutning. Rigeligt,
 * og kort nok til at et uendeligt langt svar fra serveren ikke kan fylde
 * noget op. */
#define TARGET_MAX  ZS_VERSION_MAX

static char             s_target[TARGET_MAX];
static SemaphoreHandle_t s_target_laas;


static void target_laas_klar(void)
{
    if (s_target_laas == NULL) {
        s_target_laas = xSemaphoreCreateMutex();
    }
}

void zs_ota_set_target(const char *version)
{
    target_laas_klar();
    if (s_target_laas == NULL) {
        return;
    }
    xSemaphoreTake(s_target_laas, portMAX_DELAY);
    if (version == NULL || version[0] == '\0') {
        s_target[0] = '\0';
        xSemaphoreGive(s_target_laas);
        ESP_LOGI(TAG, "ingen målversion, vi følger nyeste igen");
        return;
    }
    if (!zs_version_tag_ok(version)) {
        xSemaphoreGive(s_target_laas);
        /* Vi beholder det gamle maal. At falde tilbage til "nyeste"
         * fordi nogen tastede forkert ville vaere en overraskelse. */
        ESP_LOGW(TAG, "målversionen \"%.23s\" giver ikke mening, den bruges ikke",
                 version);
        return;
    }
    /* Gem UDEN v. Maerket paa GitHub har v foran, og det saettes paa igen
     * naar vi slaar op, saa der kun er ét sted der ved det. */
    const char *p = (*version == 'v' || *version == 'V') ? version + 1 : version;
    snprintf(s_target, sizeof(s_target), "%.*s", (int)(TARGET_MAX - 1), p);
    xSemaphoreGive(s_target_laas);
    ESP_LOGI(TAG, "målversion sat til %s", p);
}

void zs_ota_get_target(char *ud, size_t ud_len)
{
    if (ud == NULL || ud_len == 0) {
        return;
    }
    target_laas_klar();
    if (s_target_laas == NULL) {
        ud[0] = '\0';
        return;
    }
    xSemaphoreTake(s_target_laas, portMAX_DELAY);
    snprintf(ud, ud_len, "%s", s_target);
    xSemaphoreGive(s_target_laas);
}

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

/*
 * Spoerger GitHub om den nyeste udgivelse.
 *
 * Fylder tag og url. Returnerer false ved fejl, og saa staar en dansk
 * forklaring i ud->fejl.
 */
static bool hent_udgivelse(zs_ota_status_t *ud, char *url, size_t url_len,
                           const char *maal)
{
    char api[200];
    if (maal != NULL && maal[0] != '\0') {
        /*
         * Et bestemt maerke. Maalet er efterset af zs_ota_target_ok foer
         * det blev gemt, saa der kan kun staa cifre og punktummer her.
         * Vi saetter v'et paa ét sted, nemlig her.
         */
        snprintf(api, sizeof(api),
                 "https://api.github.com/repos/%s/%s/releases/tags/v%s",
                 ZS_OTA_OWNER, ZS_OTA_REPO, maal);
    } else {
        snprintf(api, sizeof(api),
                 "https://api.github.com/repos/%s/%s/releases/latest",
                 ZS_OTA_OWNER, ZS_OTA_REPO);
    }

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
                         zs_version_strip_v(tag->valuestring));
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

    char maal[24];
    zs_ota_get_target(maal, sizeof(maal));

    /*
     * Er maalet det vi allerede koerer, er vi faerdige med det samme.
     * Saa spoerger vi ikke engang GitHub, og en skaerm der er sat fast
     * paa sin version bruger ingen trafik paa det.
     */
    if (maal[0] != '\0' && zs_version_cmp(maal, ud->koerende) == 0) {
        snprintf(ud->nyeste, sizeof(ud->nyeste), "%.23s", maal);
        ud->state = ZS_OTA_UP_TO_DATE;
        ESP_LOGI(TAG, "vi kører %s, som er målet", ud->koerende);
        return false;
    }

    char url[256];
    if (!hent_udgivelse(ud, url, sizeof(url), maal)) {
        ud->state = ZS_OTA_FAILED;
        ESP_LOGW(TAG, "%s", ud->fejl);
        return false;
    }

    int c = zs_version_cmp(ud->nyeste, ud->koerende);
    if (maal[0] != '\0') {
        /*
         * MED et maal gaar vi begge veje. Det er hele pointen: kan vi
         * ikke gaa ned igen, er en daarlig udgivelse ikke til at komme
         * af med uden at hente skaermene hjem.
         *
         * Der er ingen ring i det: naar den koerende udgave er lig
         * maalet, stopper vi ovenfor, og maalet skifter kun naar vi
         * selv skriver et nyt.
         */
        if (c == 0) {
            ud->state = ZS_OTA_UP_TO_DATE;
            ESP_LOGI(TAG, "vi kører %s, som er målet", ud->koerende);
            return false;
        }
        ESP_LOGI(TAG, "%s til målversion %s",
                 c > 0 ? "opdaterer" : "går TILBAGE", ud->nyeste);
    } else if (c <= 0) {
        /*
         * UDEN maal opdaterer vi KUN opad.
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
        /*
         * GitHub sender filen videre til et andet vaertsnavn.
         * Hentelinket svarer 302 med en Location paa
         * release-assets.githubusercontent.com, hvor selve filen ligger
         * bag en tidsbegraenset underskrift i adressen.
         *
         * Begge linjer herunder er ESP-IDF's standard i forvejen. De
         * staar der alligevel, fordi det her er det eneste sted hvor en
         * aendret standard ville betyde at ingen skaerm nogensinde fik
         * en opdatering, og fejlen ville se ud som om GitHub var nede.
         * To er nok: ét hop til filserveren, og ét i reserve. MAALT
         * 7. oktober 2026 paa en rigtig udgivelse: praecis ét hop.
         *
         * OG DET HER ER VAERD AT VIDE, saa ingen "retter" det paa et
         * gaet: adressen vi bliver sendt videre til er 915 TEGN lang,
         * fordi filen ligger bag en tidsbegraenset underskrift. Den er
         * altsaa naesten dobbelt saa lang som esp_http_client's
         * standardbuffer paa 512 bytes (DEFAULT_HTTP_BUF_SIZE).
         *
         * Det ser ud som den fejl vi havde i floedestyringen, hvor en
         * besked stoerre end bufferen kom i stykker. Men det er det
         * ikke: esp_http_client laegger header-vaerdien til i heapen med
         * http_utils_append_string, stykke for stykke, saa en lang
         * Location klarer sig uanset bufferens stoerrelse. Efterset i
         * deres kilde, http_on_header_value.
         *
         * Derfor saetter vi IKKE buffer_size her. Gjorde vi det, ville
         * det se ud som om det var noedvendigt, og saa ville den naeste
         * gaette paa at det var derfor.
         */
        .disable_auto_redirect = false,
        .max_redirection_count = 2,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota = {
        .http_config = &http,
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
