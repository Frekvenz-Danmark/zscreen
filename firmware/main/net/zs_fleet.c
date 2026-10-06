/*
 * zScreen - floedestyring. Se zs_fleet.h for forloebet og for de fire
 * faelder der kostede en halv dag at finde.
 */

#include "zs_fleet.h"
#include "zs_config.h"
#include "zs_fleet_msg.h"
#include "zs_ota.h"
#include "esp_app_desc.h"
#include "../zs_log.h"

#if ZS_FLEET_ENABLED

#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
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
/*
 * Plads til at samle et svar der kommer i stykker, og samleren selv.
 *
 * Se ZS_FLEET_SVAR_MAX for hvorfor den findes: svaret paa en indmeldelse
 * er maalt til 2604 bytes mod esp-mqtt's modtagebuffer paa 1024, saa det
 * kommer i tre stykker. Pladsen tages i PSRAM, for otte kilobyte er
 * mange i den interne hukommelse og ingenting i PSRAM.
 */
static char  *s_svar_plads;
static zs_fleet_saml_t s_saml;
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
 * Naar vi tidligst maa proeve at melde ind igen. Nul betyder med det
 * samme.
 *
 * ÉT ur til alle de maader en indmeldelse kan gaa skaevt paa, saa der
 * ikke er tre halve loesninger der skal passe sammen:
 *
 *   afvist af serveren     AFVIST_PAUSE_MS, ti minutter
 *   afsendelsen fejlede    ZS_FLEET_ENROLL_RETRY_MS
 *   svaret kom aldrig      ZS_FLEET_ENROLL_RETRY_MS
 *   indmeldt               nul, der er intet at proeve
 *
 * Er certifikatet forkert, hjaelper det ikke at proeve igen om ti
 * sekunder. Uden pausen sendte vi det afviste certifikat ved hver
 * genforbindelse, for evigt, og med tres skaerme bliver det stoej paa
 * serveren uden at nogen bliver klogere. Ti minutter er nok til at en
 * rettelse paa serveren bliver opdaget af sig selv, og lidt nok til at
 * ingen skal ud og genstarte en skaerm.
 */
static int64_t s_naeste_forsoeg_ms;
#define AFVIST_PAUSE_MS   (10 * 60 * 1000)

/*
 * Engangsuret til spredningen. Se ZS_FLEET_START_SPREAD_MS.
 *
 * Et ur og ikke en opgave der sover: en opgave ville staa stille i op
 * til et minut og alligevel optage sin stak hele tiden.
 */
static esp_timer_handle_t s_spred_ur;

/*
 * Er vi forbundet OG abonneret paa svaret?
 *
 * Skal vaere sandt foer vi tOErr melde ind, for svaret kommer paa det
 * abonnement. Bruges af den tidsstyrede genopmelding i
 * zs_fleet_publish.
 */
static bool s_abonneret;

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

zs_fleet_state_t zs_fleet_state(void)
{
    LAAS();
    zs_fleet_state_t t = s_state;
    SLIP();
    return t;
}

void zs_fleet_asset_id(char *ud, size_t ud_len)
{
    if (ud == NULL || ud_len == 0) {
        return;
    }
    LAAS();
    snprintf(ud, ud_len, "%s", s_asset);
    SLIP();
}

const char *zs_fleet_state_text(void)
{
    /* Teksterne er faste strenge, saa den returnerede pegepind kan
     * ikke blive revet vaek under laeseren. Kun tilstanden skal laeses
     * under laas. */
    switch (zs_fleet_state()) {
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
        LAAS(); s_state = ZS_FLEET_ERROR; SLIP();
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

    /*
     * Laas om tilstanden. Den her funktion kaldes nu fra TO opgaver:
     * MQTT-opgaven naar abonnementet er paa plads, og hovedopgaven naar
     * pausen efter en afvisning er gaaet.
     */
    LAAS();
    if (id < 0) {
        s_state = ZS_FLEET_ERROR;
    } else {
        s_state = ZS_FLEET_ENROLLING;
    }
    /*
     * Saet hvornaar vi maa proeve igen, ogsaa naar afsendelsen lykkedes.
     * Kommer der aldrig et svar, sidder vi ellers og venter for evigt
     * paa en forbindelse der er helt i orden. Svaret nulstiller den.
     */
    s_naeste_forsoeg_ms = (esp_timer_get_time() / 1000) + ZS_FLEET_ENROLL_RETRY_MS;
    SLIP();
    if (id < 0) {
        ZS_LOGW(TAG, "indmeldelsen kunne ikke sendes");
    } else {
        ZS_LOGI(TAG, "sendte certifikat, venter paa svar");
    }
}


/*
 * Serveren har sat en maalversion. Se ZS_FLEET_TARGET_FELT og zs_ota.h.
 *
 * Kroppen er JSON. Som tekst kommer den i gaasefoedder, og ryddes feltet
 * i dashboardet kommer der null. Begge dele skal forstaas: null betyder
 * "foelg nyeste igen", og det er den vej tilbage til normal drift.
 */
static void laes_maalversion(const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    cJSON *rod = cJSON_ParseWithLength(data, (size_t)len);
    if (rod == NULL) {
        ZS_LOGW(TAG, "målversionen kunne ikke læses");
        return;
    }
    if (cJSON_IsNull(rod)) {
        zs_ota_set_target("");
    } else if (cJSON_IsString(rod)) {
        /* Tom streng betyder ogsaa "foelg nyeste". zs_ota_set_target
         * efterser resten og forkaster det der ikke giver mening. */
        zs_ota_set_target(rod->valuestring);
    } else {
        ZS_LOGW(TAG, "målversionen var ikke tekst, den bruges ikke");
    }
    cJSON_Delete(rod);
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
            s_naeste_forsoeg_ms = 0;        /* vi er inde, intet at proeve */
            SLIP();
            ZS_LOGI(TAG, "indmeldt som %s", s_asset);

            /*
             * Lyt efter en maalversion.
             *
             * SKAL ske her og ikke ved forbindelsen: emnet indeholder
             * enhedens id, og det kender vi foerst nu. Og det skal ske
             * ved HVER indmeldelse, for abonnementet haenger paa
             * forbindelsen praecis som godkendelsen gOEr.
             *
             * Maalt: serveren naegter abonnementet i stilhed hvis feltet
             * ikke er markeret laesbart for en begraenset bruger. Der
             * kommer ingen fejl, der kommer bare aldrig noget.
             */
            char lyt[160];
            if (zs_fleet_msg_lyt_topic(lyt, sizeof(lyt), ZS_FLEET_REALM,
                                       zs_fleet_unique_id(),
                                       ZS_FLEET_TARGET_FELT,
                                       id->valuestring) > 0) {
                esp_mqtt_client_subscribe(s_klient, lyt, 0);
                ZS_LOGI(TAG, "lytter efter målversion");
            }
        } else {
            ZS_LOGW(TAG, "svaret havde intet enheds-id");
            LAAS();
            s_state = ZS_FLEET_ERROR;
            s_naeste_forsoeg_ms = (esp_timer_get_time() / 1000)
                                + ZS_FLEET_ENROLL_RETRY_MS;
            SLIP();
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
        /*
         * Hvor meget af stakken der var tilbage efter TLS-haandtrykket.
         *
         * Vi koerer paa MQTT-opgaven lige her, saa NULL er den selv.
         * Haandtrykket er det tungeste der sker paa den, saa tallet her
         * er det taetteste vi kommer paa sandheden. Staar der faa hundrede
         * bytes, er ZS_FLEET_TASK_STACK for lille.
         */
        ZS_LOGI(TAG, "forbundet til %s, %u bytes stak tilbage", s_host,
                (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
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
        s_abonneret = true;
        SLIP();
        if (vent > 0 && (esp_timer_get_time() / 1000) < vent) {
            ZS_LOGI(TAG, "vi blev afvist, venter foer vi proever igen");
            break;
        }
        send_indmeldelse();
        break;
    }

    case MQTT_EVENT_DATA: {
        /*
         * SAML FOERST, laes bagefter.
         *
         * En besked der er stoerre end esp-mqtt's modtagebuffer kommer i
         * flere stykker, og kun det FOERSTE har et emne paa sig. Foer blev
         * hvert stykke laest som om det var en hel besked, og svaret paa
         * en indmeldelse er maalt til 2604 bytes mod en buffer paa 1024.
         * Altsaa tre stykker, ingen af dem gyldig JSON, alle tre
         * forkastet, og skaermen fik aldrig sit enheds-id. Se
         * ZS_FLEET_SVAR_MAX.
         */
        if (s_svar_plads == NULL) {
            break;              /* ingen plads, intet at samle i */
        }
        zs_saml_t r = zs_fleet_saml_tag(&s_saml, e->topic, e->topic_len,
                                        e->data, e->data_len,
                                        e->current_data_offset,
                                        e->total_data_len);
        if (r == ZS_SAML_FOR_STOR) {
            /* Kun paa det foerste stykke, resten tier samleren om. */
            if (e->current_data_offset == 0) {
                ZS_LOGW(TAG, "en besked paa %d bytes er for stor, den springes over",
                        e->total_data_len);
            }
            break;
        }
        if (r == ZS_SAML_USAMMENHAENG) {
            ZS_LOGW(TAG, "en besked kom i stykker der ikke hang sammen");
            break;
        }
        if (r != ZS_SAML_KLAR) {
            break;              /* der mangler mere endnu */
        }

        /*
         * Hel besked, og emnet er det fra det foerste stykke.
         *
         * Vi kender beskeden paa hvad den ER, ikke paa hvad den ikke er.
         * Foer var reglen "alt der ikke indeholder targetVersion er et
         * indmeldelsessvar", og saa ville en tredje slags besked en dag
         * blive laest som et svar.
         */
        if (zs_fleet_emne_har_led(s_saml.emne, ZS_FLEET_TARGET_FELT)) {
            laes_maalversion(s_saml.buf, (int)s_saml.har);
        } else if (zs_fleet_emne_har_led(s_saml.emne, "response")) {
            laes_svar(s_saml.buf, (int)s_saml.har);
        } else {
            ZS_LOGW(TAG, "en besked paa et emne vi ikke kender: %s",
                    s_saml.emne);
        }
        break;
    }

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
        s_abonneret = false;
        if (s_state != ZS_FLEET_REJECTED) {
            /* En ny forbindelse skal melde ind med det samme. Pausen
             * gaelder kun en afvisning, og den overlever med vilje. */
            s_naeste_forsoeg_ms = 0;
        }
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

/*
 * Starter klienten naar spredningen er gaaet. Kaldes fra uret, eller
 * direkte hvis spredningen er slaaet fra.
 *
 * Laasen holdes hele vejen gennem starten. Det er den ene undtagelse
 * fra reglen om at vi aldrig laaser mens vi taler med serveren, og den
 * er efterset i esp-mqtt: esp_mqtt_client_start laver en opgave og
 * vender tilbage med det samme, den kalder ikke vores
 * haendelseshaandtering undervejs. Saa er der ingen vej til en laas der
 * venter paa sig selv, og til gengaeld kan zs_fleet_stop ikke rive
 * klienten ned midt i starten.
 */
static void start_klienten(void *arg)
{
    (void) arg;

    LAAS();
    if (s_klient == NULL) {
        SLIP();                 /* stoppet imens, der er intet at starte */
        return;
    }
    esp_err_t r = esp_mqtt_client_start(s_klient);
    if (r != ESP_OK) {
        s_state = ZS_FLEET_ERROR;
    }
    SLIP();

    if (r != ESP_OK) {
        ZS_LOGE(TAG, "klienten kunne ikke startes");
        return;
    }
    ZS_LOGI(TAG, "melder ind som %s hos %s:%d",
            zs_fleet_unique_id(), s_host, ZS_FLEET_PORT);
}

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

    /*
     * Plads til at samle et svar der kommer i stykker.
     *
     * PSRAM foerst, intern hukommelse hvis der ikke er PSRAM: otte
     * kilobyte er mange af de godt tre hundrede interne og ingenting af
     * de otte megabyte i PSRAM. Lykkes ingen af dem, koerer vi videre
     * uden floedestyring frem for at vaelte: skaermen skal vise
     * solcellerne selv om serveren ikke kan naas.
     */
    if (s_svar_plads == NULL) {
        s_svar_plads = heap_caps_malloc(ZS_FLEET_SVAR_MAX, MALLOC_CAP_SPIRAM);
        if (s_svar_plads == NULL) {
            s_svar_plads = malloc(ZS_FLEET_SVAR_MAX);
        }
    }
    if (s_svar_plads == NULL) {
        ZS_LOGE(TAG, "ikke plads til at samle svaret, floedestyring springes over");
        free(s_cert); free(s_key); free(s_srv_ca);
        s_cert = s_key = s_srv_ca = NULL;
        s_state = ZS_FLEET_OFF;
        return false;
    }
    zs_fleet_saml_init(&s_saml, s_svar_plads, ZS_FLEET_SVAR_MAX);

    /*
     * Eget genforbindelsesinterval per skaerm, se
     * ZS_FLEET_RECONNECT_SPREAD_MS. Traekkes én gang ved opstart og
     * bliver ved, saa skaermen ligger ude af trit med naboen for altid.
     */
    uint32_t genforbind_ms = ZS_FLEET_RECONNECT_MS;
    if (ZS_FLEET_RECONNECT_SPREAD_MS > 0) {
        genforbind_ms += esp_random() % (uint32_t) ZS_FLEET_RECONNECT_SPREAD_MS;
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
            .reconnect_timeout_ms = (int) genforbind_ms,
            .timeout_ms = 10000,
        },
        .task = {
            /* Under hovedopgaven. En langsom server maa aldrig
             * forsinke aflaesningen fra inverteren. */
            .priority = 4,
            .stack_size = ZS_FLEET_TASK_STACK,
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
    s_state = ZS_FLEET_CONNECTING;

    /*
     * Vent et tilfaeldigt stykke tid foer vi melder ind, saa en hel gade
     * der faar stroem tilbage samtidig ikke rammer serveren i samme
     * sekund. Tallet kommer fra hardwarens stoejkilde, saa to skaerme
     * med samme firmware ikke lander paa samme ventetid.
     */
    uint32_t vent_ms = 0;
    if (ZS_FLEET_START_SPREAD_MS > 0) {
        vent_ms = esp_random() % (uint32_t) ZS_FLEET_START_SPREAD_MS;
    }
    if (vent_ms == 0) {
        start_klienten(NULL);
        return true;
    }

    const esp_timer_create_args_t ur = {
        .callback = start_klienten,
        .name     = "fleet-spred",
    };
    if (esp_timer_create(&ur, &s_spred_ur) != ESP_OK
        || esp_timer_start_once(s_spred_ur, (uint64_t) vent_ms * 1000) != ESP_OK) {
        /* Kan vi ikke faa et ur, er spredningen det mindste af to onder.
         * Saa melder vi ind med det samme i stedet for aldrig. */
        ZS_LOGW(TAG, "intet ur til spredning, melder ind straks");
        start_klienten(NULL);
        return true;
    }
    ZS_LOGI(TAG, "floedestyring klar som %s, melder ind om %u ms",
            zs_fleet_unique_id(), (unsigned) vent_ms);
    return true;
}

void zs_fleet_stop(void)
{
    /*
     * Tag handtaget ud under laasen foerst.
     *
     * Saa kan start_klienten ikke vaere midt i at starte en klient vi er
     * ved at rive ned: den holder laasen hele vejen gennem starten, saa
     * de to udelukker hinanden. Derefter slipper vi laasen IGEN foer vi
     * venter paa mqtt-opgaven. Holdt vi den, kunne opgaven staa og
     * vente paa den samme laas inde i vores haendelseshaandtering, og
     * saa ventede de to paa hinanden for evigt.
     */
    LAAS();
    esp_mqtt_client_handle_t klient = s_klient;
    s_klient = NULL;
    SLIP();

    if (s_spred_ur != NULL) {
        esp_timer_stop(s_spred_ur);       /* ikke startet: harmloes fejl */
        esp_timer_delete(s_spred_ur);
        s_spred_ur = NULL;
    }
    if (klient != NULL) {
        esp_mqtt_client_stop(klient);
        esp_mqtt_client_destroy(klient);
    }
    free(s_cert); free(s_key); free(s_srv_ca);
    s_cert = s_key = s_srv_ca = NULL;
    /* Samleren peger ind i pladsen, saa den skal nulstilles FOER
     * pladsen gives tilbage. Ellers staar der en peger til hukommelse
     * der ikke er vores laengere. */
    memset(&s_saml, 0, sizeof(s_saml));
    free(s_svar_plads);
    s_svar_plads = NULL;

    LAAS();
    s_asset[0] = '\0';
    s_state = ZS_FLEET_OFF;
    SLIP();
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

    /*
     * Proev at melde ind igen. Styret af TIDEN, ikke af en haendelse.
     *
     * Hvorfor det skal vaere tiden: indmeldelsen blev foer kun forsoegt
     * naar abonnementet var nyt, altsaa én gang per forbindelse. Og
     * maalt mod serveren holder den forbindelsen AABEN efter en
     * afvisning, den svarer
     * {"type":"error","error":"UNAUTHORIZED"} og lader den ligge. Saa
     * kom der aldrig et nyt abonnement, og skaermen var afvist for
     * evigt, ogsaa efter at fejlen var rettet paa serveren, indtil
     * nogen tog stroemmen. Det samme gjaldt en afsendelse der ikke gik
     * igennem og et svar der aldrig kom.
     *
     * Reglen er den samme for alle tre: er vi abonneret, har vi intet
     * enheds-id, og er uret gaaet, saa proever vi igen. Se
     * s_naeste_forsoeg_ms.
     *
     * Hovedopgaven kommer forbi her ved hver aflaesning, og det er nok
     * til at holde oeje med uret. Ingen ekstra opgave, intet ekstra ur.
     */
    bool proev_igen = false;
    LAAS();
    if (zs_fleet_enroll_due(s_abonneret, s_asset[0] != '\0',
                            s_naeste_forsoeg_ms,
                            esp_timer_get_time() / 1000)) {
        /* Nulstil FOER vi sender, under laasen, saa to runder ikke kan
         * sende to indmeldelser hvis de overlapper. Svaret saetter den
         * igen hvis vi bliver afvist en gang mere. */
        s_naeste_forsoeg_ms = 0;
        proev_igen = true;
    }
    SLIP();
    if (proev_igen) {
        ZS_LOGI(TAG, "pausen er gaaet, proever at melde ind igen");
        send_indmeldelse();
        return;                 /* svaret skal ind foerst */
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
        /*
         * Hvilken udgave vi faktisk koerer.
         *
         * Uden den kan vi ikke se om en udrulning gik godt, og saa er en
         * maalversion ikke meget vaerd: man kan saette den, men ikke se
         * om skaermen rent faktisk naaede frem. Den sendes sammen med
         * anlaeggets oplysninger, altsaa én gang per indmeldelse, og en
         * ny indmeldelse sker netop efter en genstart. Saa staar der
         * altid det rigtige kort efter en opdatering.
         */
        const esp_app_desc_t *mig = esp_app_get_description();
        if (mig != NULL) {
            send_tekst(ZS_FLEET_VERSION_FELT, asset, mig->version);
        }
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
void zs_fleet_asset_id(char *ud, size_t ud_len)
{ if (ud != NULL && ud_len > 0) { ud[0] = '\0'; } }
const char *zs_fleet_unique_id(void) { return ""; }

#endif /* ZS_FLEET_ENABLED */
