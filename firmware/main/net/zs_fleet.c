/*
 * zScreen - floedestyring. Se zs_fleet.h for forloebet og for de fire
 * faelder der kostede en halv dag at finde.
 */

#include "zs_fleet.h"
#include "zs_config.h"
#include "zs_fleet_msg.h"
#include "../zs_log.h"

#if ZS_FLEET_ENABLED

#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "fleet";

/*
 * Certifikatet og noeglen ligger i lageret, ikke i firmwaren.
 *
 * De er forskellige paa hver enhed, og firmwaren er den samme paa alle.
 * Laa de i firmwaren, ville alle skaerme have samme identitet, og saa
 * kunne den ene skrive i den andens anlaeg.
 *
 * Eget navnerum, saa de ikke fylder i zs_settings_t. Kundens
 * indstillinger og enhedens identitet er to forskellige ting med to
 * forskellige levetider.
 */
#define NS_FLEET        "zsfleet"
#define K_CERT          "cert"
/*
 * Den private noegle. VALGFRI.
 *
 * I indmeldelsen sendes certifikatet som tekst i en JSON-besked, og
 * serveren tjekker at det er signeret af vores CA og at navnet passer.
 * Den beder ALDRIG om bevis paa at vi har den private noegle, saa den
 * bruges ikke til noget i dag.
 *
 * Det betyder noget: certifikatet ALENE er legitimationen. Et
 * certifikat er normalt offentligt, men her skal det behandles som en
 * hemmelighed paa linje med et kodeord. Kan nogen laese det ud af en
 * skaerm, kan de melde sig ind som den skaerm.
 *
 * Noeglen laeses alligevel hvis den er der, saa vi kan skifte til mTLS
 * paa en egen port en dag uden at skulle ud til enhederne igen. Maalt:
 * mTLS virker IKKE gennem deres HAProxy, for den afslutter TLS selv, saa
 * klientcertifikatet naar aldrig brokeren.
 */
#define K_KEY           "key"
#define K_HOST          "host"      /* valgfri, ellers ZS_FLEET_HOST */
/*
 * Serverens eget CA, valgfrit.
 *
 * Er den sat, tjekkes serveren mod DEN i stedet for Mozillas rodliste.
 * Det er ikke en smutvej til test: en selvhostet server kan sagtens
 * have et certifikat fra et privat CA, og saa er det her den rigtige
 * maade. Er den tom, bruges rodlisten, og saa virker et almindeligt
 * Let's Encrypt-certifikat uden videre.
 *
 * Bemaerk at der IKKE findes en mulighed for at springe tjekket over.
 * Et saadant flag ville foer eller siden slippe med i en udgivelse.
 */
#define K_SRV_CA        "srvca"

/* Et certifikat i PEM fylder omkring 1,2 KB, en noegle omkring 1,7. */
#define PEM_MAX         3072

static esp_mqtt_client_handle_t s_klient;
static zs_fleet_state_t         s_state = ZS_FLEET_OFF;
static char   s_unik[32];
static char   s_asset[32];
static char   s_host[64];
static char  *s_cert;              /* PEM, i heapen saa laenge vi koerer */
static char  *s_key;
static char  *s_srv_ca;
static int64_t s_klar_ms;          /* naar vi tidligst maa skrive       */
static bool   s_info_sendt;
static int    s_poll_taeller;

/*
 * Laas om den delte tilstand.
 *
 * To opgaver roerer de samme felter: esp-mqtt skriver dem fra sin egen
 * opgave naar der kommer en haendelse, og hovedopgaven laeser dem naar
 * den sender maalinger. Uden laas kan hovedopgaven laese et enheds-id
 * der er halvt overskrevet, eller se READY med et id der netop blev
 * ryddet. Det ville give et emne der peger paa en anden enhed eller
 * ingen.
 *
 * Laasen holdes i mikrosekunder, kun om kopieringen af nogle faa felter.
 * Der laases aldrig mens vi sender.
 */
static SemaphoreHandle_t s_laas;

#define LAAS()    do { if (s_laas) { xSemaphoreTake(s_laas, portMAX_DELAY); } } while (0)
#define SLIP()    do { if (s_laas) { xSemaphoreGive(s_laas); } } while (0)

/*
 * Naar vi tidligst proever at melde ind igen efter en afvisning.
 *
 * Er certifikatet forkert, hjaelper det ikke at proeve igen om tien
 * sekunder. Uden den her sendte vi det afviste certifikat ved hver
 * genforbindelse, for evigt, og med tres skaerme bliver det stoej paa
 * serveren uden at nogen bliver klogere. Ti minutter er nok til at en
 * rettelse paa serveren bliver opdaget af sig selv, og lidt nok til at
 * ingen skal ud og genstarte en skaerm.
 */
static int64_t s_naeste_forsoeg_ms;
#define AFVIST_PAUSE_MS   (10 * 60 * 1000)

/* ------------------------------------------------------------------ */
/* Smaating                                                            */
/* ------------------------------------------------------------------ */

const char *zs_fleet_unique_id(void)
{
    if (s_unik[0] == '\0') {
        uint8_t mac[6] = {0};
        /* Fabriks-MAC fra eFuse. Kan ikke aendres, og er forskellig paa
         * hver chip. Vi tager den og ikke wifi-MAC'en, for den sidste
         * afhaenger af hvilken grAEnseflade der er oppe. */
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
            snprintf(s_unik, sizeof(s_unik), "zscreen-ukendt");
        } else {
            snprintf(s_unik, sizeof(s_unik), "zscreen-%02x%02x%02x%02x%02x%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
    return s_unik;
}

zs_fleet_state_t zs_fleet_state(void) { return s_state; }
const char *zs_fleet_asset_id(void)   { return s_asset; }

const char *zs_fleet_state_text(void)
{
    switch (s_state) {
    case ZS_FLEET_CONNECTING: return "Forbinder";
    case ZS_FLEET_ENROLLING:  return "Melder sig ind";
    case ZS_FLEET_READY:      return "Sender data";
    case ZS_FLEET_REJECTED:   return "Afvist af serveren";
    case ZS_FLEET_ERROR:      return "Ingen forbindelse";
    default:                  return "Slået fra";
    }
}

/* Laeser en PEM fra lageret. Returnerer NULL hvis den ikke er der. */
static char *laes_pem(nvs_handle_t h, const char *noegle)
{
    size_t n = 0;
    if (nvs_get_str(h, noegle, NULL, &n) != ESP_OK || n == 0 || n > PEM_MAX) {
        return NULL;
    }
    char *buf = malloc(n);
    if (buf == NULL) {
        return NULL;
    }
    if (nvs_get_str(h, noegle, buf, &n) != ESP_OK) {
        free(buf);
        return NULL;
    }
    buf[n - 1] = '\0';   /* NVS lover ikke afslutningen, vi saetter den */
    return buf;
}

/* ------------------------------------------------------------------ */
/* Indmeldelsen                                                        */
/* ------------------------------------------------------------------ */

static void send_indmeldelse(void)
{
    /*
     * Certifikatet sendes som JSON. Vi bygger den i haanden og ikke med
     * cJSON, fordi en PEM er flere kilobyte og cJSON ville lave to
     * kopier mere af den.
     *
     * PEM'en indeholder linjeskift, og de SKAL undslippes i JSON.
     */
    size_t plads = zs_fleet_msg_enroll_size(s_cert);
    char *krop = malloc(plads);
    if (krop == NULL) {
        ZS_LOGE(TAG, "ikke hukommelse nok til indmeldelsen");
        return;
    }
    size_t o = zs_fleet_msg_enroll(krop, plads, s_cert);
    if (o == 0) {
        ZS_LOGE(TAG, "indmeldelsen kunne ikke bygges");
        free(krop);
        s_state = ZS_FLEET_ERROR;
        return;
    }

    char emne[80];
    snprintf(emne, sizeof(emne), "provisioning/%s/request", zs_fleet_unique_id());
    /*
     * Tjenestegrad 0 med vilje.
     *
     * Med 1 gemmer esp-mqtt beskeden og sender den igen efter en
     * genforbindelse. Maalt gav det en indmeldelse der landede paa en
     * lukket forbindelse, og serveren svarede "Skipping provisioning
     * request as connection is now closed". Vi vil hellere melde ind
     * forfra end at gentage en gammel besked.
     */
    int id = esp_mqtt_client_publish(s_klient, emne, krop, (int)o, 0, 0);
    free(krop);

    if (id < 0) {
        ZS_LOGW(TAG, "indmeldelsen kunne ikke sendes");
        s_state = ZS_FLEET_ERROR;
    } else {
        ZS_LOGI(TAG, "sendte certifikat, venter paa svar");
        s_state = ZS_FLEET_ENROLLING;
    }
}

static void laes_svar(const char *data, int len)
{
    cJSON *rod = cJSON_ParseWithLength(data, (size_t)len);
    if (rod == NULL) {
        ZS_LOGW(TAG, "svaret kunne ikke laeses");
        return;
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(rod, "type");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "success") == 0) {
        const cJSON *asset = cJSON_GetObjectItemCaseSensitive(rod, "asset");
        const cJSON *id = asset ? cJSON_GetObjectItemCaseSensitive(asset, "id") : NULL;
        if (cJSON_IsString(id) && id->valuestring[0] != '\0') {
            LAAS();
            snprintf(s_asset, sizeof(s_asset), "%s", id->valuestring);
            /*
             * Vi er godkendt, men der maa ikke skrives endnu. Serveren
             * skal opgradere forbindelsen til servicebrugeren foerst.
             * Se ZS_FLEET_READY_DELAY_MS.
             */
            s_klar_ms = (esp_timer_get_time() / 1000) + ZS_FLEET_READY_DELAY_MS;
            s_info_sendt = false;
            s_state = ZS_FLEET_READY;
            SLIP();
            ZS_LOGI(TAG, "indmeldt som %s", s_asset);
        } else {
            ZS_LOGW(TAG, "svaret havde intet enheds-id");
            s_state = ZS_FLEET_ERROR;
        }
    } else {
        const cJSON *fejl = cJSON_GetObjectItemCaseSensitive(rod, "error");
        ZS_LOGE(TAG, "serveren afviste os: %s",
                cJSON_IsString(fejl) ? fejl->valuestring : "ukendt grund");
        LAAS();
        s_naeste_forsoeg_ms = (esp_timer_get_time() / 1000) + AFVIST_PAUSE_MS;
        /*
         * Afvist er ikke det samme som en netvaerksfejl. Er
         * certifikatet forkert, hjaelper det ikke at proeve igen om et
         * sekund, og vi skal ikke hamre paa serveren. Tilstanden staar,
         * og den kan ses paa Detaljer-siden.
         */
        s_state = ZS_FLEET_REJECTED;
        SLIP();
    }
    cJSON_Delete(rod);
}

/* ------------------------------------------------------------------ */
/* Haendelser fra esp-mqtt                                             */
/* ------------------------------------------------------------------ */

static void paa_haendelse(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED: {
        /*
         * Lyt FOERST, send derefter. Svarer serveren hurtigt, og vi
         * endnu ikke lytter, er svaret vaek.
         */
        char emne[80];
        snprintf(emne, sizeof(emne), "provisioning/%s/response",
                 zs_fleet_unique_id());
        esp_mqtt_client_subscribe(s_klient, emne, 1);
        ZS_LOGI(TAG, "forbundet til %s", s_host);
        break;
    }
    case MQTT_EVENT_SUBSCRIBED: {
        /*
         * Blev vi afvist, venter vi. Se AFVIST_PAUSE_MS: uden den
         * sender vi det samme afviste certifikat ved hver
         * genforbindelse, for evigt.
         */
        LAAS();
        int64_t vent = s_naeste_forsoeg_ms;
        SLIP();
        if (vent > 0 && (esp_timer_get_time() / 1000) < vent) {
            ZS_LOGI(TAG, "vi blev afvist, venter foer vi proever igen");
            break;
        }
        send_indmeldelse();
        break;
    }

    case MQTT_EVENT_DATA:
        laes_svar(e->data, e->data_len);
        break;

    case MQTT_EVENT_DISCONNECTED:
        /*
         * Godkendelsen haenger paa FORBINDELSEN, ikke paa enheden.
         * Derfor skal vi melde ind forfra, og indtil da maa vi ikke
         * sende maalinger. Glemmer man det, bliver hver skrivning
         * nAEgtet og forbindelsen lukket, i en ring.
         */
        LAAS();
        if (s_state != ZS_FLEET_REJECTED) {
            s_state = ZS_FLEET_CONNECTING;
        }
        s_asset[0] = '\0';
        s_klar_ms = 0;
        SLIP();
        ZS_LOGW(TAG, "forbindelsen gik tabt, melder ind igen naar den er tilbage");
        break;

    case MQTT_EVENT_ERROR:
        LAAS();
        if (s_state != ZS_FLEET_REJECTED) {
            s_state = ZS_FLEET_ERROR;
        }
        SLIP();
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Udadtil                                                             */
/* ------------------------------------------------------------------ */

bool zs_fleet_start(void)
{
    if (s_klient != NULL) {
        return true;
    }
    if (s_laas == NULL) {
        s_laas = xSemaphoreCreateMutex();
        if (s_laas == NULL) {
            ZS_LOGE(TAG, "kunne ikke lave laasen");
            return false;
        }
    }

    nvs_handle_t h;
    if (nvs_open(NS_FLEET, NVS_READONLY, &h) != ESP_OK) {
        ZS_LOGI(TAG, "intet certifikat paa enheden, floedestyring er ikke sat op");
        s_state = ZS_FLEET_OFF;
        return false;
    }
    s_cert = laes_pem(h, K_CERT);
    s_key  = laes_pem(h, K_KEY);
    s_srv_ca = laes_pem(h, K_SRV_CA);
    size_t n = sizeof(s_host);
    if (nvs_get_str(h, K_HOST, s_host, &n) != ESP_OK || s_host[0] == '\0') {
        snprintf(s_host, sizeof(s_host), "%s", ZS_FLEET_HOST);
    }
    nvs_close(h);

    if (s_cert == NULL) {
        ZS_LOGI(TAG, "intet certifikat, floedestyring springes over");
        free(s_cert); free(s_key); free(s_srv_ca);
        s_cert = s_key = s_srv_ca = NULL;
        s_state = ZS_FLEET_OFF;
        return false;
    }

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address = {
                .hostname  = s_host,
                .transport = MQTT_TRANSPORT_OVER_SSL,
                .port      = ZS_FLEET_PORT,
            },
            /*
             * Serveren tjekkes mod Mozillas rodliste, den samme vi
             * bruger til priser og opdateringer. Saa virker det med et
             * almindeligt Let's Encrypt-certifikat uden at vi skal
             * laegge noget ind i firmwaren.
             */
            /*
             * Er der et CA i lageret, bruges det. Ellers Mozillas
             * rodliste, den samme vi bruger til priser og opdateringer.
             * Begge veje tjekker serveren, der er ingen tredje.
             */
            .verification = {
                .certificate = s_srv_ca,
                .crt_bundle_attach = (s_srv_ca == NULL)
                                     ? esp_crt_bundle_attach : NULL,
            },
        },
        .credentials = {
            /*
             * Det RENE id som klientnavn.
             *
             * Saetter man ps-{id}, altsaa servicebrugerens navn, lukker
             * serveren forbindelsen foer indmeldelsen er behandlet.
             * Maalt, og det er ikke til at gennemskue fra loggen uden at
             * man ved det.
             */
            .client_id = zs_fleet_unique_id(),
        },
        .session = {
            .keepalive = 60,
            /* Ren forbindelse hver gang. Vi skal melde ind forfra
             * alligevel, saa en gemt session giver intet. */
            .disable_clean_session = false,
        },
        .network = {
            .reconnect_timeout_ms = 10000,
            .timeout_ms = 10000,
        },
        .task = {
            /* Under hovedopgaven. En langsom server maa aldrig
             * forsinke aflaesningen fra inverteren. */
            .priority = 4,
            .stack_size = 6144,
        },
    };

    s_klient = esp_mqtt_client_init(&cfg);
    if (s_klient == NULL) {
        ZS_LOGE(TAG, "klienten kunne ikke laves");
        /* Giv hukommelsen tilbage. Uden det ville et nyt forsoeg laese
         * certifikaterne igen og laegge endnu et saet i heapen. */
        zs_fleet_stop();
        s_state = ZS_FLEET_ERROR;
        return false;
    }
    esp_mqtt_client_register_event(s_klient, ESP_EVENT_ANY_ID, paa_haendelse, NULL);
    if (esp_mqtt_client_start(s_klient) != ESP_OK) {
        ZS_LOGE(TAG, "klienten kunne ikke startes");
        zs_fleet_stop();
        s_state = ZS_FLEET_ERROR;
        return false;
    }
    s_state = ZS_FLEET_CONNECTING;
    ZS_LOGI(TAG, "floedestyring startet, id %s, server %s:%d",
            zs_fleet_unique_id(), s_host, ZS_FLEET_PORT);
    return true;
}

void zs_fleet_stop(void)
{
    if (s_klient != NULL) {
        esp_mqtt_client_stop(s_klient);
        esp_mqtt_client_destroy(s_klient);
        s_klient = NULL;
    }
    free(s_cert); free(s_key); free(s_srv_ca);
    s_cert = s_key = s_srv_ca = NULL;
    s_asset[0] = '\0';
    s_state = ZS_FLEET_OFF;
}

/* Sender ét felt. Fejler den, siger vi ikke fra: naeste runde er om to
 * sekunder, og en tabt maaling er ikke noget at raabe op om. */
static void send_tal(const char *felt, const char *asset, float vaerdi)
{
    char emne[160], krop[32];
    if (zs_fleet_msg_topic(emne, sizeof(emne), ZS_FLEET_REALM,
                           zs_fleet_unique_id(), felt, asset) == 0) {
        return;
    }
    /* Ét decimal er rigeligt for watt og procent, og det halverer
     * beskeden sammenlignet med en fuld float. */
    snprintf(krop, sizeof(krop), "%.1f", (double)vaerdi);
    esp_mqtt_client_publish(s_klient, emne, krop, 0, 0, 0);
}

static void send_tekst(const char *felt, const char *asset, const char *vaerdi)
{
    if (vaerdi == NULL || vaerdi[0] == '\0') {
        return;
    }
    char emne[160], krop[72];
    if (zs_fleet_msg_topic(emne, sizeof(emne), ZS_FLEET_REALM,
                           zs_fleet_unique_id(), felt, asset) == 0) {
        return;
    }
    snprintf(krop, sizeof(krop), "\"%.64s\"", vaerdi);
    esp_mqtt_client_publish(s_klient, emne, krop, 0, 0, 0);
}

void zs_fleet_publish(const zs_fr_live_t *live, const zs_fr_info_t *info)
{
    if (s_klient == NULL || live == NULL) {
        return;
    }
    /* Takten foelger aflaesningen, se ZS_FLEET_PUBLISH_EVERY_N_POLLS. */
    if (++s_poll_taeller < ZS_FLEET_PUBLISH_EVERY_N_POLLS) {
        return;
    }
    s_poll_taeller = 0;

    /*
     * Tag en KOPI af det vi skal bruge, under laas, og send derefter
     * uden. Saa kan MQTT-opgaven rydde tilstanden midt i vores
     * afsendelse uden at vi bygger et emne der peger paa ingenting.
     */
    char asset[sizeof(s_asset)];
    bool send_info;
    LAAS();
    bool klar = (s_state == ZS_FLEET_READY) && (s_asset[0] != '\0')
             && ((esp_timer_get_time() / 1000) >= s_klar_ms);
    snprintf(asset, sizeof(asset), "%s", s_asset);
    send_info = klar && !s_info_sendt;
    if (send_info) {
        /* Saettes HER, under laasen, saa to runder ikke kan sende
         * oplysningerne to gange hvis de overlapper. */
        s_info_sendt = true;
    }
    SLIP();

    if (!klar) {
        return;
    }

    /* Kun det vi faktisk har maalt. En tom maaling skal ikke blive til
     * et nul paa en graf. */
    if (live->solar_w.ok)       { send_tal("solarPower", asset,   live->solar_w.v); }
    if (live->house_w.ok)       { send_tal("housePower", asset,   live->house_w.v); }
    if (live->battery_w.ok)     { send_tal("batteryPower", asset, live->battery_w.v); }
    if (live->grid_w.ok)        { send_tal("gridPower", asset,    live->grid_w.v); }
    if (live->soc_pct.ok)       { send_tal("batteryLevel", asset, live->soc_pct.v); }

    /* Anlaeggets oplysninger ÉN gang per indmeldelse. De skifter ikke,
     * og at sende dem hvert andet sekund ville fylde databasen med det
     * samme svar. */
    if (send_info && info != NULL && info->has_inverter) {
        send_tekst("inverterModel", asset,  info->model);
        send_tekst("inverterSerial", asset, info->serial);
        send_tal("ratedPower", asset,      info->inverter_rated_kw);
        send_tal("batteryCapacity", asset, info->battery_capacity_kwh);
        ZS_LOGI(TAG, "sendte anlaeggets oplysninger");
    }
}

#else  /* ZS_FLEET_ENABLED == 0 */

bool zs_fleet_start(void) { return false; }
void zs_fleet_stop(void) {}
void zs_fleet_publish(const zs_fr_live_t *live, const zs_fr_info_t *info)
{ (void)live; (void)info; }
zs_fleet_state_t zs_fleet_state(void) { return ZS_FLEET_OFF; }
const char *zs_fleet_state_text(void) { return "Slået fra"; }
const char *zs_fleet_asset_id(void) { return ""; }
const char *zs_fleet_unique_id(void) { return ""; }

#endif /* ZS_FLEET_ENABLED */
