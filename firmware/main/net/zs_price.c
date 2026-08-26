#include "zs_price.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <time.h>

static const char *TAG = "price";

/*
 * Svaret er omkring 3,4 KB for et doegn. Vi giver plads til det
 * firedobbelte og afviser alt derover.
 *
 * Graensen er ikke smaalig, den er en sikkerhedsgraense: uden den
 * kunne en server der svarer med en uendelig stroem spise al vores
 * hukommelse. Bufferen ligger i PSRAM, hvor der er 6 MB.
 */
#define MAX_BODY      16384
/*
 * Otte sekunder, og hoejst én omdirigering.
 *
 * Hentningen koerer i sin egen opgave, saa den blokerer ikke skaermen.
 * Men den skal stadig give op i rimelig tid: sker der noget, proever
 * vi igen om ti minutter i stedet for at staa og vente.
 */
#define HTTP_TIMEOUT_MS   8000
#define MAX_REDIRECTS        1

typedef struct {
    char  *buf;
    size_t len;
    bool   overloeb;
} hent_t;

static esp_err_t on_http_event(esp_http_client_event_t *e)
{
    hent_t *h = (hent_t *)e->user_data;
    if (h == NULL || h->buf == NULL) {
        return ESP_OK;
    }

    /*
     * Bufferen ryddes ved hver ny forbindelse OG ved et Location-svar.
     *
     * Uden det ville kroppen af et omdirigeringssvar blive lagt FORAN
     * priserne, og en hentning der egentlig lykkedes ville ende som
     * "priserne kunne ikke laeses". Location-headeren findes kun i et
     * 3xx-svar, saa den er et sikkert tegn paa at det der kommer
     * bagefter er den rigtige krop.
     */
    if (e->event_id == HTTP_EVENT_ON_CONNECTED) {
        h->len = 0;
        h->overloeb = false;
        h->buf[0] = '\0';
        return ESP_OK;
    }
    if (e->event_id == HTTP_EVENT_ON_HEADER) {
        if (e->header_key != NULL && strcasecmp(e->header_key, "Location") == 0) {
            h->len = 0;
            h->overloeb = false;
            h->buf[0] = '\0';
        }
        return ESP_OK;
    }
    if (e->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
    if (h->len + (size_t)e->data_len >= MAX_BODY) {
        h->overloeb = true;
        return ESP_OK;      /* vi laeser resten og smider det vaek */
    }
    memcpy(h->buf + h->len, e->data, (size_t)e->data_len);
    h->len += (size_t)e->data_len;
    h->buf[h->len] = '\0';
    return ESP_OK;
}

/* Timetallet ud af "2026-08-25T13:00:00+02:00". Vi tager den LOKALE
 * time direkte fra teksten. Saa passer den ogsaa de to naetter om
 * aaret hvor doegnet er 23 eller 25 timer langt. */
static int hour_of(const char *ts)
{
    if (ts == NULL || strlen(ts) < 13 || ts[10] != 'T') {
        return -1;
    }
    if (ts[11] < '0' || ts[11] > '9' || ts[12] < '0' || ts[12] > '9') {
        return -1;
    }
    int h = (ts[11] - '0') * 10 + (ts[12] - '0');
    return (h >= 0 && h <= 23) ? h : -1;
}

static bool parse(const char *json, zs_price_day_t *ud)
{
    cJSON *rod = cJSON_Parse(json);
    if (rod == NULL) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Priserne kunne ikke læses");
        return false;
    }
    if (!cJSON_IsArray(rod)) {
        cJSON_Delete(rod);
        snprintf(ud->fejl, sizeof(ud->fejl), "Priserne havde et uventet format");
        return false;
    }

    ud->antal = 0;
    float sum = 0.0f;

    cJSON *post = NULL;
    cJSON_ArrayForEach(post, rod) {
        if (ud->antal >= ZS_PRICE_MAX_HOURS) {
            break;
        }
        cJSON *pris = cJSON_GetObjectItemCaseSensitive(post, "DKK_per_kWh");
        cJSON *start = cJSON_GetObjectItemCaseSensitive(post, "time_start");
        if (!cJSON_IsNumber(pris) || !cJSON_IsString(start)) {
            continue;
        }
        int h = hour_of(start->valuestring);
        if (h < 0) {
            continue;
        }
        ud->timer[ud->antal].hour = (uint8_t)h;
        ud->timer[ud->antal].dkk  = (float)pris->valuedouble;
        sum += ud->timer[ud->antal].dkk;
        ud->antal++;
    }
    cJSON_Delete(rod);

    if (ud->antal == 0) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Der var ingen priser i svaret");
        return false;
    }

    ud->gennemsnit = sum / (float)ud->antal;
    ud->billigst = 0;
    ud->dyrest = 0;
    for (uint8_t i = 1; i < ud->antal; i++) {
        if (ud->timer[i].dkk < ud->timer[ud->billigst].dkk) { ud->billigst = i; }
        if (ud->timer[i].dkk > ud->timer[ud->dyrest].dkk)   { ud->dyrest = i; }
    }
    return true;
}


bool zs_price_is_stale(const zs_price_day_t *d)
{
    if (d == NULL || !d->ok) {
        return true;
    }
    time_t t = time(NULL);
    if (t < 1700000000) {
        return false;   /* uden ur kan vi ikke vide det, saa lad vaere */
    }
    struct tm lt;
    localtime_r(&t, &lt);
    char i_dag[11];
    /*
     * Modulo, ikke pynt.
     *
     * tm_year er en fortegnet int, saa oversaetteren kan hverken vide
     * at aarstallet er fire cifre eller at dagen er positiv. Et
     * negativt tal ville fylde ét tegn mere paa grund af minusset.
     *
     * Med unsigned og modulo er graensen bevislig: 0 til 9999 og 0 til
     * 99, altsaa praecis fire og to tegn. En enhed med et vildt ur
     * skriver saa en forkert dato i stedet for uden for bufferen.
     */
    snprintf(i_dag, sizeof(i_dag), "%04u-%02u-%02u",
             (unsigned)(lt.tm_year + 1900) % 10000u,
             (unsigned)(lt.tm_mon + 1) % 100u,
             (unsigned)lt.tm_mday % 100u);
    return strcmp(i_dag, d->dato) != 0;
}

bool zs_price_fetch(const char *zone, zs_price_day_t *ud)
{
    if (ud == NULL) {
        return false;
    }
    memset(ud, 0, sizeof(*ud));
    ud->nu = -1;
    snprintf(ud->zone, sizeof(ud->zone), "%.3s",
             (zone != NULL && zone[0] != '\0') ? zone : "DK2");

    time_t t = time(NULL);
    if (t < 1700000000) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Uret er ikke sat endnu");
        return false;
    }
    struct tm lt;
    localtime_r(&t, &lt);
    /* Se noten om unsigned og modulo i zs_price_is_stale. */
    unsigned aar = (unsigned)(lt.tm_year + 1900) % 10000u;
    unsigned mdr = (unsigned)(lt.tm_mon + 1) % 100u;
    unsigned dag = (unsigned)lt.tm_mday % 100u;
    snprintf(ud->dato, sizeof(ud->dato), "%04u-%02u-%02u", aar, mdr, dag);

    char url[128];
    snprintf(url, sizeof(url),
             "https://www.elprisenligenu.dk/api/v1/prices/%04u/%02u-%02u_%.3s.json",
             aar, mdr, dag, ud->zone);

    /* Bufferen ligger i PSRAM. Den interne hukommelse er knap, og 16 KB
     * ville vaere en maerkbar bid af den. */
    hent_t h = { 0 };
    h.buf = heap_caps_malloc(MAX_BODY, MALLOC_CAP_SPIRAM);
    if (h.buf == NULL) {
        h.buf = malloc(MAX_BODY);
    }
    if (h.buf == NULL) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Der er ikke hukommelse nok");
        return false;
    }
    h.buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = on_http_event,
        .user_data = &h,
        /* Mozillas rodcertifikater, som foelger med ESP-IDF. Uden dem
         * ville vi enten skulle laegge et certifikat i firmwaren, som
         * udloeber, eller springe kontrollen over, som ingen af delene
         * duer. */
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = false,
        .max_redirection_count = MAX_REDIRECTS,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        free(h.buf);
        snprintf(ud->fejl, sizeof(ud->fejl), "Forbindelsen kunne ikke sættes op");
        return false;
    }

    bool ok = false;
    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);

    if (err != ESP_OK) {
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "Kunne ikke hente priserne. Er der internet?");
    } else if (status == 404) {
        /* Priserne for i morgen kommer foerst omkring middag. Ligger
         * dagens fil ikke der, er det som regel fordi enheden staar
         * lige efter midnat og filen endnu ikke er lagt op. */
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "Der er endnu ingen priser for i dag");
    } else if (status != 200) {
        snprintf(ud->fejl, sizeof(ud->fejl),
                 "Prisserveren svarede %d", status);
    } else if (h.overloeb) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Svaret var uventet stort");
    } else if (h.len == 0) {
        snprintf(ud->fejl, sizeof(ud->fejl), "Svaret var tomt");
    } else {
        ok = parse(h.buf, ud);
    }

    esp_http_client_cleanup(c);
    free(h.buf);

    ud->ok = ok;
    if (ok) {
        zs_price_update_now(ud);
        ESP_LOGI(TAG, "%s %s: %u timer, billigst %.2f kr, dyrest %.2f kr",
                 ud->zone, ud->dato, (unsigned)ud->antal,
                 (double)ud->timer[ud->billigst].dkk,
                 (double)ud->timer[ud->dyrest].dkk);
    } else {
        ESP_LOGW(TAG, "%s", ud->fejl);
    }
    return ok;
}
