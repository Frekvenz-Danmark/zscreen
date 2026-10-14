/*
 * zScreen - floedestyring. Se zs_fleet.h for forloebet og for de fire
 * faelder der kostede en halv dag at finde.
 */

#include "zs_fleet.h"
#include "zs_tilmeld.h"
#include "zs_tekst.h"
#include "zs_config.h"
#include "zs_fleet_msg.h"
#include "zs_wifi.h"
#include "zs_ladeordre.h"
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
 * Er esp_mqtt_client_start kaldt? Spredningsuret og
 * zs_fleet_skynd_paa_indmeldelsen kan begge naa frem til starten, og
 * esp-mqtt svarer fejl hvis man starter en klient to gange. Flaget
 * laeses og saettes under laasen, saa de to ikke kan slippe forbi
 * hinanden.
 */
static bool s_startet;

/* Hvor laenge der ventes foer vi kigger efter en IP igen. Se
 * start_klienten: uret gaar efter tid siden opstart, og nettet kommer
 * naar det kommer. */
#define IP_VENT_MS   2000

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

/*
 * Navnet er praefiks plus tolv hextegn plus nul. Med en for lille
 * buffer ville snprintf afkorte, og to skaerme kunne faa SAMME navn
 * uden at nogen saa en fejl. Derfor et tjek ved bygningen.
 */
_Static_assert(sizeof(ZS_FLEET_NAVN_PREFIX) + 12 <= sizeof(s_unik),
               "s_unik er for lille til praefikset plus MAC'en");

const char *zs_fleet_unique_id(void)
{
    if (s_unik[0] == '\0') {
        uint8_t mac[6] = {0};
        /* Fabriks-MAC fra eFuse. Kan ikke aendres, og er forskellig paa
         * hver chip. Vi tager den og ikke wifi-MAC'en, for den sidste
         * afhaenger af hvilken grAEnseflade der er oppe. */
        /* Hvilken MAC-type der kan laeses, afhaenger af chippen. Se
         * ZS_MAC_TYPE i zs_config.h: paa ESP32-P4 findes der ingen
         * wifi-MAC, fordi radioen er en anden chip. */
        if (esp_read_mac(mac, ZS_MAC_TYPE) != ESP_OK) {
            /*
             * Falder vi her, har ALLE skaerme af den slags samme navn.
             * Det er derfor det staar som en fejl og ikke en advarsel:
             * den skal findes paa den foerste enhed, ikke paa den
             * tiende hos en kunde.
             */
            ZS_LOGE(TAG, "MAC'en kunne ikke laeses. Skaermen har intet "
                         "entydigt navn paa flåden");
            snprintf(s_unik, sizeof(s_unik),
                     ZS_FLEET_NAVN_PREFIX "ukendt");
        } else {
            snprintf(s_unik, sizeof(s_unik),
                     ZS_FLEET_NAVN_PREFIX "%02x%02x%02x%02x%02x%02x",
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
/*
 * LADEORDREN FRA PLATFORMEN.
 *
 * Den ligger her og ikke i zs_app, fordi den kommer ind her: felterne
 * ankommer som to separate beskeder, og zs_ladeordre samler dem. Alt
 * der roerer den, gor det bag LAAS() - MQTT-opgaven skriver, og enhver
 * anden opgave kan laese.
 *
 * INGEN HANDLER PAA DEN ENDNU. Den bliver samlet, efterset og sat paa
 * tid, og saa ligger den. Vejen ud til inverteren findes ikke:
 * zs_fronius er skrivebeskyttet, og registeradresserne er aldrig
 * fundet. Det staar her saa ingen tror batteriet er styret.
 */
static zs_ladeordre_t s_ordre;

/*
 * Det MONOTONE ur, i millisekunder siden opstart. Samme kilde som resten
 * af filen bruger til sine pauser.
 *
 * Og det er ikke tilfaeldigt: doedmandsknappen regnes af den her, ikke
 * af vaeguret. Et NTP-spring flytter vaeguret med timer, og saa ville
 * knappen slaa til for tidligt eller aldrig. Se zs_ladeordre.h.
 */
static int64_t lade_nu_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void saet_ordre_tilstand(const char *navn)
{
    LAAS();
    bool godt = zs_ladeordre_tilstand(&s_ordre, navn, lade_nu_ms());
    zs_ordre_stand_t stand = zs_ladeordre_stand(&s_ordre, lade_nu_ms());
    SLIP();
    if (godt) {
        ZS_LOGI(TAG, "ladeplan: tilstand %s, ordren er nu %s",
                navn, zs_ladeordre_stand_navn(stand));
    } else {
        /*
         * Et navn vi ikke kender roerer ikke den ordre der staar. Det
         * skal ses i loggen: symptomet ellers er "Platformen satte en
         * plan og skaermen gjorde noget andet".
         */
        ZS_LOGW(TAG, "ladeplan: tilstanden \"%.24s\" kendes ikke, "
                     "ordren staar uroert", navn ? navn : "(null)");
    }
}

static void saet_ordre_sekunder(int32_t sek)
{
    LAAS();
    bool godt = zs_ladeordre_sekunder(&s_ordre, sek, lade_nu_ms());
    zs_ordre_stand_t stand = zs_ladeordre_stand(&s_ordre, lade_nu_ms());
    SLIP();
    if (godt) {
        ZS_LOGI(TAG, "ladeplan: timer %d s, ordren er nu %s",
                (int)sek, zs_ladeordre_stand_navn(stand));
    } else {
        ZS_LOGW(TAG, "ladeplan: timeren %d s er uden for 1..%d, "
                     "ordren staar uroert",
                (int)sek, ZS_LADE_DOEDMAND_MAKS_S);
    }
}

/*
 * Laeser ét af ladeplanens felter ud af en besked eller et svar.
 *
 * Den samme afkodning bruges begge steder: beskeden naar feltet aendres,
 * og svaret ved indmelding. Stod den to steder, kunne de komme ud af
 * trit - og saa ville en plan sat mens skaermen var slukket blive laest
 * anderledes end en sat mens den koerte.
 */
static void brug_lade_vaerdi(const char *felt, const cJSON *vaerdi)
{
    if (felt == NULL || vaerdi == NULL || cJSON_IsNull(vaerdi)) {
        return;             /* ryddet paa serveren: lad ordren udloebe */
    }
    if (strcmp(felt, ZS_FLEET_LADE_TILSTAND_FELT) == 0) {
        if (cJSON_IsString(vaerdi)) {
            saet_ordre_tilstand(vaerdi->valuestring);
        } else {
            ZS_LOGW(TAG, "ladeplan: tilstanden var ikke tekst");
        }
    } else if (strcmp(felt, ZS_FLEET_LADE_SEKUNDER_FELT) == 0) {
        if (cJSON_IsNumber(vaerdi)) {
            /*
             * Afkortning og ikke afrunding: et halvt sekund i en
             * doedmandstimer er ikke noget vi runder OP. Og
             * zs_ladeordre_sekunder afviser selv 0 og det der er over
             * loftet, saa graensen staar ét sted.
             */
            saet_ordre_sekunder((int32_t)vaerdi->valuedouble);
        } else {
            ZS_LOGW(TAG, "ladeplan: timeren var ikke et tal");
        }
    }
}

/* Ét af ladeplanens felter er aendret paa serveren. */
static void laes_ladeplan(const char *felt, const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    cJSON *rod = cJSON_ParseWithLength(data, (size_t)len);
    if (rod == NULL) {
        ZS_LOGW(TAG, "ladeplanens %s kunne ikke laeses", felt);
        return;
    }
    brug_lade_vaerdi(felt, rod);
    cJSON_Delete(rod);
}

/*
 * PRISOMRAADET FRA SERVEREN.
 *
 * Vaerdien laegges til side her, og app'en henter den i sit tik med
 * zs_fleet_zone_ny. Den bliver IKKE skrevet i flashen herfra: det er
 * app'en der ejer konfigurationen, og to steder der gemmer den samme
 * ting, er to steder der kan komme ud af trit.
 *
 * Kun DK1 og DK2 godtages. Serveren kan tage fejl - et felt et menneske
 * har tastet i - og en zone skaermen ikke kender, ville ellers gaa med
 * ud i den adresse der hentes priser fra, hvor den giver et svar ingen
 * kan bruge. Afvises den, bliver den gamle staaende, og det er det
 * rigtige: en kendt zone er bedre end ingen.
 */
static char s_zone[4];          /* under LAAS */
static bool s_zone_ny;          /* under LAAS */

/*
 * Samme afkodning bruges BEGGE steder: beskeden naar feltet aendres, og
 * svaret ved indmeldingen. Se noten ved brug_lade_vaerdi - stod den to
 * steder, kunne de komme ud af trit, og saa ville en vaerdi sat mens
 * skaermen var slukket blive laest anderledes end en sat mens den koerte.
 */
static void brug_zone_vaerdi(const cJSON *rod)
{
    if (rod == NULL) {
        /* Feltet stod slet ikke i svaret. Serveren er bagud - felter.py
         * har ikke lagt det paa - og vi har ingenting at gaa efter. Vi
         * beholder det vi har, og siger det. */
        ZS_LOGW(TAG, "prisområdet findes ikke på vores asset, "
                     "så serveren har ikke fået feltet endnu");
        return;
    }
    if (cJSON_IsString(rod) && rod->valuestring != NULL
        && (strcmp(rod->valuestring, "DK1") == 0
            || strcmp(rod->valuestring, "DK2") == 0)) {
        LAAS();
        if (strcmp(s_zone, rod->valuestring) != 0) {
            snprintf(s_zone, sizeof(s_zone), "%.3s", rod->valuestring);
            s_zone_ny = true;
            ZS_LOGI(TAG, "serveren satte prisområdet til %s", s_zone);
        }
        SLIP();
    } else if (cJSON_IsNull(rod)) {
        /* Ryddet paa serveren. Vi beholder den vi har: en tom zone
         * ville faa prisopslaget til at falde tilbage til DK2, og saa
         * kunne en rydning flytte en skaerm paa Sjaelland til Jylland. */
        ZS_LOGI(TAG, "prisområdet blev ryddet på serveren, vi beholder "
                     "det vi har");
    } else {
        ZS_LOGW(TAG, "prisområdet var ikke DK1 eller DK2, det bruges ikke");
    }
}

static void laes_zone(const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    cJSON *rod = cJSON_ParseWithLength(data, (size_t)len);
    if (rod == NULL) {
        ZS_LOGW(TAG, "prisområdet kunne ikke læses");
        return;
    }
    brug_zone_vaerdi(rod);
    cJSON_Delete(rod);
}

/*
 * KUNDENS NAVN, altsaa beskeden om at tilmeldingen lykkedes.
 *
 * Serveren skriver det paa vores eget asset naar anlaegget er oprettet.
 * Kommer der et navn, ER vi tilmeldt. Se ZS_FLEET_KUNDE_FELT.
 *
 * Navnet gemmes ikke i flashen. Det er serverens oplysning, ikke vores,
 * og den kommer igen ved hver indmeldelse - se brug_kunde_vaerdi, der
 * ogsaa kaldes med vaerdien ud af indmeldingssvaret. Den saetning stod
 * her foer, men INGEN laeste feltet af svaret, saa den var ikke sand.
 */
static char s_kunde[ZS_FLEET_KUNDE_MAKS];   /* under LAAS */
static bool s_kunde_ny;                     /* under LAAS */

/*
 * Vaerdien af ét felt i indmeldingssvarets attributter, eller NULL.
 *
 * Tre opslag skrevet forfra tre gange er tre chancer for at stave
 * "value" forkert ét af stederne, og saa ville netop det felt tavst
 * blive NULL.
 */
static const cJSON *vaerdi_af(const cJSON *attrs, const char *felt)
{
    const cJSON *at = attrs
        ? cJSON_GetObjectItemCaseSensitive(attrs, felt) : NULL;
    return at ? cJSON_GetObjectItemCaseSensitive(at, "value") : NULL;
}

/* Samme afkodning begge steder. Se noten ved brug_zone_vaerdi. */
static void brug_kunde_vaerdi(const cJSON *rod)
{
    if (rod == NULL) {
        /* Feltet stod slet ikke i svaret. Det er ikke "ikke tilmeldt" -
         * det er en server der mangler feltet, se felter.py. Vi roerer
         * ikke det vi har, og vi siger det. */
        ZS_LOGW(TAG, "kundeNavn findes ikke på vores asset, så serveren "
                     "har ikke fået feltet endnu");
        return;
    }
    if (cJSON_IsString(rod) && rod->valuestring != NULL
        && rod->valuestring[0] != '\0') {
        LAAS();
        if (strcmp(s_kunde, rod->valuestring) != 0) {
            /*
             * zs_tekst_kopi og ikke snprintf: navnet kommer UDEFRA og
             * maa fylde hele bufferen, og en klipning midt i et æ ville
             * staa paa vaeggen som et hul der ikke kan tegnes. Se noten
             * ved funktionen.
             */
            (void)zs_tekst_kopi(s_kunde, sizeof(s_kunde), rod->valuestring);
            s_kunde_ny = true;
            ZS_LOGI(TAG, "tilmeldt til %s", s_kunde);
        }
        SLIP();
    } else if (cJSON_IsNull(rod)
               || (cJSON_IsString(rod) && rod->valuestring != NULL
                   && rod->valuestring[0] == '\0')) {
        /*
         * Ryddet paa serveren. Det sker hvis nogen fjerner tilmeldingen,
         * og saa skal skaermen kunne tilmeldes igen - altsaa tilbage til
         * at vise en kode.
         *
         * OG EN TOM STRENG RYDDER OGSAA, ikke kun null.
         *
         * Her stod der kun null, og saa faldt "" igennem uden at rydde
         * og uden at sige noget. Men serveren rydder netop et felt ved
         * at skrive en tom streng - det er det braend() gor med koden,
         * og maalt 12. oktober 2026 staar feltet bagefter som '' og ikke
         * som null. Saa ville skaermen have staaet paa "Anlægget er
         * tilmeldt" for evigt, med en tilmelding der var fjernet, og den
         * kunne ikke tilmeldes igen uden at nogen slettede hele assetet.
         */
        LAAS();
        if (s_kunde[0] != '\0') {
            s_kunde[0] = '\0';
            s_kunde_ny = true;
            ZS_LOGI(TAG, "tilmeldingen er fjernet på serveren");
        }
        SLIP();
    } else {
        /* Hverken en tekst eller null. Siges der fra om, som ved
         * prisomraadet: et felt der tavst bliver kastet vaek, er det
         * samme som et felt der ikke findes. */
        ZS_LOGW(TAG, "kundenavnet var hverken tekst eller tomt");
    }
}

static void laes_kunde(const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    cJSON *rod = cJSON_ParseWithLength(data, (size_t)len);
    if (rod == NULL) {
        ZS_LOGW(TAG, "kundenavnet kunne ikke læses");
        return;
    }
    brug_kunde_vaerdi(rod);
    cJSON_Delete(rod);
}

bool zs_fleet_kunde_ny(char *ud, size_t ud_len)
{
    if (ud == NULL || ud_len == 0) {
        return false;
    }
    bool ny = false;
    LAAS();
    if (s_kunde_ny) {
        snprintf(ud, ud_len, "%s", s_kunde);
        s_kunde_ny = false;
        ny = true;
    }
    SLIP();
    return ny;
}

void zs_fleet_kunde(char *ud, size_t ud_len)
{
    if (ud == NULL || ud_len == 0) {
        return;
    }
    LAAS();
    snprintf(ud, ud_len, "%s", s_kunde);
    SLIP();
}

bool zs_fleet_zone_ny(char *ud, size_t ud_len)
{
    if (ud == NULL || ud_len < 4) {
        return false;
    }
    bool ny = false;
    LAAS();
    if (s_zone_ny) {
        snprintf(ud, ud_len, "%.3s", s_zone);
        s_zone_ny = false;
        ny = true;
    }
    SLIP();
    return ny;
}

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
            /*
             * Ét abonnement PER FELT, fra listen i zs_config.h. Og IKKE
             * et jokertegn i asset-positionen: maalt 10. oktober 2026
             * svarer serveren "Granted QoS 0" paa attributevalue/+/+ og
             * sender saa INGENTING. Tre runder, nul beskeder. Det ligner
             * ét abonnement til alt, og det er det ikke.
             */
            char lyt[160];
            static const char *const NED[] = { ZS_FLEET_NED_FELTER };
            int lyttede = 0;
            for (size_t lf = 0; lf < sizeof(NED) / sizeof(NED[0]); lf++) {
                if (zs_fleet_msg_lyt_topic(lyt, sizeof(lyt), ZS_FLEET_REALM,
                                           zs_fleet_unique_id(),
                                           NED[lf],
                                           id->valuestring) > 0) {
                    esp_mqtt_client_subscribe(s_klient, lyt, 0);
                    lyttede++;
                }
            }
            ZS_LOGI(TAG, "lytter paa %d felter ned i skaermen", lyttede);

            /*
             * OG TAG MAALVERSIONEN MED FRA SVARET HER.
             *
             * EN ORDRE SAT MENS SKAERMEN VAR SLUKKET BLEV TABT. Et
             * abonnement giver kun beskeder mens man lytter, og
             * forbindelsen er "clean session", saa serveren gemmer
             * ingenting. Saetter man en maalversion paa en skaerm der er
             * slukket, stod vaerdien paa serveren, men skaermen spurgte
             * aldrig efter den. Opdateringen skete aldrig, og man kunne
             * tro den var sat.
             *
             * Maalt 8. oktober 2026: skaermen fik INGENTING ved at komme
             * tilbage, mens serveren stadig havde vaerdien.
             *
             * Men svaret her indeholder den allerede. Maalt: svaret er
             * 4218 bytes og har alle nitten felter MED vaerdier, ogsaa
             * targetVersion. Der skal altsaa ikke et ekstra kald til,
             * kun at vi laeser det vi faar.
             *
             * zs_ota_set_target efterser selv teksten, og den udloeser
             * kun et tjek hvis maalet er ANDERLEDES end det vi havde.
             * Derfor koster en genforbindelse ingenting.
             */
            const cJSON *attrs =
                cJSON_GetObjectItemCaseSensitive(asset, "attributes");
            const cJSON *tv = attrs
                ? cJSON_GetObjectItemCaseSensitive(attrs,
                                                   ZS_FLEET_TARGET_FELT)
                : NULL;
            const cJSON *tv_val = tv
                ? cJSON_GetObjectItemCaseSensitive(tv, "value") : NULL;
            if (cJSON_IsString(tv_val)) {
                ZS_LOGI(TAG, "svaret havde en målversion: %.23s",
                        tv_val->valuestring);
                zs_ota_set_target(tv_val->valuestring);
            } else if (tv_val == NULL || cJSON_IsNull(tv_val)) {
                /* Intet maal paa serveren. Saa skal vi heller ikke have
                 * et: er det ryddet i dashboardet mens vi var vaek, er
                 * det ogsaa en ordre. */
                zs_ota_set_target("");
            }

            /*
             * OG LADEPLANEN MED FRA SVARET, af samme grund som
             * maalversionen: et abonnement giver kun beskeder mens man
             * lytter, og forbindelsen er "clean session". En plan sat
             * mens skaermen var slukket ville ellers aldrig naa frem,
             * mens serveren stadig havde den.
             *
             * Maalt 8. oktober 2026 paa maalversionen: skaermen fik
             * INGENTING ved at komme tilbage. Svaret her har alle
             * felterne MED vaerdier, saa der skal ikke et ekstra kald
             * til.
             *
             * TILSTANDEN FOERST. Den nulstiller de andre felter naar den
             * er ny, saa kom timeren foerst, ville den blive kastet vaek
             * igen - og ordren ville staa ufuld til naeste besked.
             */
            const char *lade_raekke[] = {
                ZS_FLEET_LADE_TILSTAND_FELT,
                ZS_FLEET_LADE_SEKUNDER_FELT,
            };
            for (size_t lf = 0;
                 lf < sizeof(lade_raekke) / sizeof(lade_raekke[0]); lf++) {
                const cJSON *at = attrs
                    ? cJSON_GetObjectItemCaseSensitive(attrs, lade_raekke[lf])
                    : NULL;
                const cJSON *v = at
                    ? cJSON_GetObjectItemCaseSensitive(at, "value") : NULL;
                brug_lade_vaerdi(lade_raekke[lf], v);
            }

            /*
             * OG PRISOMRAADET OG KUNDENS NAVN, af NOEJAGTIG samme grund.
             *
             * De to kom til 12. oktober 2026 og blev kun hentet gennem
             * abonnementet. Og et abonnement leverer kun mens man
             * lytter: forbindelsen er "clean session", saa en vaerdi sat
             * mens skaermen var slukket naar ALDRIG frem - heller ikke
             * naar den kommer tilbage.
             *
             * Maalt 13. oktober 2026 paa den rigtige skaerm: navnet
             * "Hansen" blev skrevet kl. 21:35 i aftes, skaermen meldte
             * sig ind kl. 09:44, og den viste stadig en kode. Serveren
             * havde navnet hele tiden. Noten ved s_kunde lovede at
             * "den kommer igen ved hver indmeldelse", og det gjorde den
             * ikke, for ingen laeste den af svaret.
             *
             * Det er den SAMME fejl der blev maalt 8. oktober paa
             * maalversionen, og den stod allerede rettet ti linjer
             * herover. Derfor staar de to nu samme sted: naeste felt
             * nedad skal ikke kunne glemme det igen.
             *
             * STAAR FELTET SLET IKKE i svaret, roerer vi ikke det vi
             * har - saa siger brug_*_vaerdi det i loggen. Det betyder
             * at serveren mangler feltet, ikke at vaerdien er tom.
             */
            brug_zone_vaerdi(vaerdi_af(attrs, ZS_FLEET_ZONE_FELT));
            brug_kunde_vaerdi(vaerdi_af(attrs, ZS_FLEET_KUNDE_FELT));
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
        /*
         * NULSTIL FOER ABONNEMENTET. s_abonneret afgoer nu om det NAESTE
         * SUBSCRIBED er det foerste, og dermed om indmeldelsen sendes.
         *
         * Den nulstilles ogsaa ved DISCONNECTED, men esp-mqtt lover ikke
         * at der KOMMER en disconnect foer en ny connect. Stod den
         * tilbage som true, ville vi aldrig melde ind paa den nye
         * forbindelse - og det ville se ud som en skaerm der tier.
         */
        LAAS();
        s_abonneret = false;
        SLIP();

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
        /*
         * KUN DET FOERSTE ABONNEMENT SENDER INDMELDELSEN.
         *
         * MQTT_EVENT_SUBSCRIBED fyrer én gang PER ABONNEMENT. Da der
         * kun var ét - svaremnet - var "abonneret" og "klar til at
         * melde ind" det samme. Det er det ikke laengere: efter
         * indmeldelsen abonnerer vi paa ZS_FLEET_NED_FELTER, og hvert
         * af dem fyrer sin egen haendelse.
         *
         * Uden vagten her sendte vi indmeldelsen igen for hvert af dem,
         * og serveren afviste de gentagne. Saa ryddede afvisningen
         * s_asset, vi meldte ind igen, abonnerede igen, og sad fast i
         * en ring. MAALT PAA RIGTIG HARDWARE 11. oktober 2026 - og det
         * er netop den slags der ikke kan ses andre steder end dér:
         * haendelsesloekken i esp-mqtt koeres ikke af nogen proeve.
         */
        LAAS();
        int64_t vent = s_naeste_forsoeg_ms;
        bool foerste = !s_abonneret;
        s_abonneret = true;
        SLIP();
        if (!foerste) {
            break;      /* et af de senere abonnementer. Intet at goere. */
        }
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
        /*
         * HVILKET FELT HANDLER BESKEDEN OM? Det staar i det FJERDE led,
         * og det bliver hentet praecis derfra - ikke ved at lede efter
         * navnet et vilkaarligt sted i emnet. Et asset-id der
         * tilfaeldigvis heder det samme som et felt, skal ikke kunne
         * afgoere hvor beskeden lander. Se zs_fleet_msg_felt.
         */
        char felt[48];
        bool har_felt = zs_fleet_msg_felt(s_saml.emne, felt,
                                          sizeof(felt)) > 0;
        if (har_felt && strcmp(felt, ZS_FLEET_TARGET_FELT) == 0) {
            laes_maalversion(s_saml.buf, (int)s_saml.har);
        } else if (har_felt
                   && (strcmp(felt, ZS_FLEET_LADE_TILSTAND_FELT) == 0
                       || strcmp(felt, ZS_FLEET_LADE_SEKUNDER_FELT) == 0)) {
            laes_ladeplan(felt, s_saml.buf, (int)s_saml.har);
        } else if (har_felt && strcmp(felt, ZS_FLEET_ZONE_FELT) == 0) {
            laes_zone(s_saml.buf, (int)s_saml.har);
        } else if (har_felt && strcmp(felt, ZS_FLEET_KUNDE_FELT) == 0) {
            laes_kunde(s_saml.buf, (int)s_saml.har);
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
    if (s_startet) {
        SLIP();                 /* uret og et tryk naaede frem begge to */
        return;
    }
    /*
     * ER DER OVERHOVEDET ET NET ENDNU?
     *
     * Spredningsuret gaar efter tid siden opstart, og nettet kommer naar
     * det kommer. Maalt paa jern 11. oktober 2026: uret gik kl. 17,5
     * sekunder, IP'en kom kl. 17,96, og de 460 millisekunder kostede et
     * "connect() error: Host is unreachable" i roedt og fjorten sekunder
     * ekstra foer anden forsoeg. Fjorten sekunder hvor montoeren stod og
     * ventede paa en kode.
     *
     * Saa vi venter paa IP'en i stedet. Spredningen er ikke brudt af
     * det: er nettet der allerede, naar uret gaar, sker der praecis det
     * samme som foer.
     */
    if (zs_wifi_state() != ZS_WIFI_CONNECTED) {
        bool igen = (s_spred_ur != NULL)
                    && (esp_timer_start_once(s_spred_ur,
                                             (uint64_t)IP_VENT_MS * 1000)
                        == ESP_OK);
        SLIP();
        if (igen) {
            /* Ingen log her. Den ville komme hvert andet sekund hele
             * vejen gennem en wifi-opsaetning der tager minutter. */
            return;
        }
        /*
         * Kan vi ikke faa uret igen, er det bedre at proeve nu end
         * aldrig: esp-mqtt proever selv videre bagefter.
         */
        ZS_LOGW(TAG, "intet net endnu og intet ur, proever alligevel");
        LAAS();
        if (s_klient == NULL || s_startet) {
            SLIP();
            return;
        }
    }
    esp_err_t r = esp_mqtt_client_start(s_klient);
    if (r != ESP_OK) {
        s_state = ZS_FLEET_ERROR;
    } else {
        s_startet = true;
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
            /*
             * TYVE SEKUNDER, OG DET ER DET TAL DER AFGOER HVOR LAENGE EN
             * MONTOER STAAR OG VENTER.
             *
             * Her stod 60 uden en note. MQTT 3.1.1 siger at serveren
             * holder en session i 1,5 x keepalive efter at enheden er
             * vaek, og saa afviser den en NY forbindelse med samme
             * klient-id - uden en fejl man kan se. Maalt paa jern
             * 11. oktober 2026 kl. 23:12: serveren lukkede fire gange i
             * traek, og indmeldelsen var foerst i hus efter 64 sekunder.
             *
             * 60 gav altsaa et doedt vindue paa 90 sekunder. 20 giver
             * 30. Det er den ventetid montoeren staar i efter en
             * genstart, og den eneste maade at goere den kortere.
             *
             * PRISEN ER MAALT OP: et PINGREQ og et PINGRESP er fire
             * bytes. Tre gange saa ofte er for tusind skaerme omkring
             * 200 bytes i sekundet i alt. Serveren taalte sytten
             * INDMELDELSER i sekundet, og en indmeldelse er et
             * certifikat paa halvandet kilobyte plus en TLS-opsaetning.
             *
             * Og det er en gevinst et sted mere: boxOnline falder nu
             * efter 30 sekunder i stedet for 90, saa flaadeoversigten
             * ser en doed skaerm tre gange hurtigere.
             */
            .keepalive = 20,
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

/*
 * SPRING RESTEN AF SPREDNINGEN OVER.
 *
 * Spredningen findes fordi tres enheder der meldte ind inden for fem
 * sekunder, vaeltede serverens Keycloak - det er maalt, se headeren. Men
 * den maaling handler om en FLOK: en gade der faar stroemmen tilbage, et
 * hus hvor sikringen slaar ind igen.
 *
 * Her er der ét menneske der har aabnet tilmeldingen paa ÉN skaerm. Der
 * er ingen flok at sprede, og den ventetid er ren spildtid for en
 * montoer der staar paa en stige. Tusind montoerer trykker ikke i samme
 * sekund, og skulle de, er det stadig én indmeldelse per tryk.
 *
 * Kaldet er uden virkning hvis klienten allerede er startet, eller hvis
 * floedestyringen er slaaet fra. Er nettet ikke klart endnu, venter
 * start_klienten paa IP'en som den altid har gjort.
 */
void zs_fleet_skynd_paa_indmeldelsen(void)
{
    LAAS();
    bool noget_at_goere = (s_klient != NULL) && !s_startet;
    SLIP();
    if (!noget_at_goere) {
        return;
    }
    /*
     * Uret stoppes foer vi starter selv. Naar det ikke at gaa alligevel,
     * er det harmloest: start_klienten siger nej til den anden af dem,
     * fordi s_startet laeses under laasen.
     *
     * Handtaget laeses uden laas, praecis som i zs_fleet_stop, og det er
     * i orden fordi den ene kalder af zs_fleet_stop ligger inde i
     * zs_fleet_start FOER uret bliver lavet. Der er altsaa ingen der kan
     * slette uret under os.
     */
    if (s_spred_ur != NULL) {
        esp_timer_stop(s_spred_ur);     /* ikke i gang: harmloes fejl */
    }
    ZS_LOGI(TAG, "tilmeldingen er aabnet, melder ind nu i stedet for at vente");
    start_klienten(NULL);
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
    s_startet = false;
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

void zs_fleet_publish_energi(const float *wh, const bool *har, int8_t time)
{
    if (wh == NULL || har == NULL) {
        return;
    }
    char asset[sizeof(s_asset)];
    LAAS();
    bool klar = (s_state == ZS_FLEET_READY) && (s_asset[0] != '\0')
             && ((esp_timer_get_time() / 1000) >= s_klar_ms);
    snprintf(asset, sizeof(asset), "%s", s_asset);
    SLIP();
    if (!klar) {
        /*
         * Ikke indmeldt endnu. Timen gaar tabt, og det er med vilje det
         * rigtige: vi kan ikke gemme den til senere, for om en time
         * kommer den naeste, og saa ville de hobe sig op.
         */
        ZS_LOGW(TAG, "time %d kunne ikke sendes, vi er ikke indmeldt endnu",
                time);
        return;
    }

    /*
     * I KILOWATTTIMER, ikke wattimer.
     *
     * Det er den enhed der staar paa en elregning, og den kunden kan
     * genkende. Feltet paa serveren faar samme enhed, saa ingen skal
     * regne om.
     */
    for (size_t i = 0; i < ZS_E_ANTAL; i++) {
        if (!har[i]) {
            continue;       /* vi opfinder ikke et felt vi ikke har */
        }
        send_tal(zs_energi_felt_navn((zs_energi_felt_t)i), asset,
                 wh[i] / ZS_ENERGI_WH_PR_KWH);
    }

    if (zs_energi_time_ok(time)) {
        send_tal(ZS_ENERGI_TIME_FELT, asset, (float)time);
    }
}

/*
 * Feltnavnene i SAMME raekkefoelge som zs_kv_felt_t i zs_kvarter.h.
 *
 * Raekkefoelgen er ÉN sandhed. Byttes to om her, lander solen i forbruget
 * og der kommer ingen fejl nogen steder: begge er tal, begge er watt, og
 * serveren tager imod. Derfor et tjek ved bygningen paa at der er praecis
 * ét navn per felt.
 */
static const char *const KV_NAVNE[ZS_KV_ANTAL] = {
    ZS_FLEET_SOL_KV_FELT,
    ZS_FLEET_FORBRUG_KV_FELT,
    ZS_FLEET_BATTERI_KV_FELT,
    ZS_FLEET_NET_KV_FELT,
    ZS_FLEET_SOC_KV_FELT,
};
_Static_assert(sizeof(KV_NAVNE) / sizeof(KV_NAVNE[0]) == ZS_KV_ANTAL,
               "der skal vaere praecis ét feltnavn per kvartersfelt");

void zs_fleet_publish_kvarter(const zs_kvarter_ud_t *ud)
{
    if (s_klient == NULL || ud == NULL || !ud->klar) {
        return;
    }
    LAAS();
    bool klar = (s_abonneret && s_asset[0] != '\0');
    char asset[sizeof(s_asset)];
    snprintf(asset, sizeof(asset), "%s", s_asset);
    SLIP();
    if (!klar) {
        return;
    }

    int sendt = 0;
    for (int i = 0; i < ZS_KV_ANTAL; i++) {
        if (!ud->har[i]) {
            continue;       /* vi sender ikke et felt vi ikke har */
        }
        send_tal(KV_NAVNE[i], asset, ud->vaerdi[i]);
        sendt++;
    }
    /* Kvarteret sendes KUN hvis der ogsaa kom et tal. Et kvarternummer
     * alene siger ingenting, og det ville se ud som en maaling. */
    if (sendt > 0) {
        send_tal(ZS_FLEET_KVARTER_FELT, asset, (float)ud->kvarter);
        ESP_LOGI(TAG, "kvarter %d sendt, %d felter", (int)ud->kvarter, sendt);
    }
}

void zs_fleet_publish_tilmeld(const char *kode)
{
    /* Som ved inverterfeltet: tilstanden ligger STATISK her, saa der kun
     * er ét sted der ved hvad serveren er blevet fortalt. */
    static zs_tilmeld_sendt_t s_sendt;

    if (s_klient == NULL || kode == NULL) {
        return;
    }
    LAAS();
    bool klar = (s_abonneret && s_asset[0] != '\0');
    char asset[sizeof(s_asset)];
    snprintf(asset, sizeof(asset), "%s", s_asset);
    SLIP();
    if (!klar) {
        /* Ikke indmeldt. Glem hvad vi har sagt, saa koden bliver sagt
         * forfra naar vi kommer ind: serveren kan have faaet enheden
         * oprettet forfra, og saa staar feltet tomt dér. */
        zs_tilmeld_sendt_nulstil(&s_sendt);
        return;
    }
    if (!zs_tilmeld_skal_sende(&s_sendt, kode)) {
        return;
    }

    char emne[160];
    if (zs_fleet_msg_topic(emne, sizeof(emne), ZS_FLEET_REALM,
                           zs_fleet_unique_id(), ZS_FLEET_TILMELD_FELT,
                           asset) == 0) {
        /* Kunne vi ikke bygge emnet, har vi ikke sagt noget. */
        zs_tilmeld_sendt_nulstil(&s_sendt);
        return;
    }
    /*
     * Kroppen bygges her og IKKE med send_tekst.
     *
     * send_tekst springer ud paa en tom streng, og det er rigtigt for
     * modelnavne: et tomt modelnavn er ingenting at sige. Men her er en
     * tom streng selve beskeden - "koden gaelder ikke laengere" - og
     * sprang vi ud, ville koden blive ved med at virke paa serveren
     * efter skaermen var holdt op med at vise den.
     */
    char krop[ZS_TILMELD_KODE_LEN + 3];
    snprintf(krop, sizeof(krop), "\"%s\"", kode);
    esp_mqtt_client_publish(s_klient, emne, krop, 0, 0, 0);
    if (kode[0] == '\0') {
        ESP_LOGI(TAG, "tilmeldingskoden er ryddet");
    } else {
        ESP_LOGI(TAG, "tilmeldingskoden er sendt");   /* ALDRIG koden selv */
    }
}

void zs_fleet_publish_inverter(bool online)
{
    /* Tilstanden ligger STATISK her og ikke hos kalderen, saa der kun er
     * ét sted der ved hvad serveren er blevet fortalt. */
    static zs_inv_status_t s_inv;

    if (s_klient == NULL) {
        return;
    }
    LAAS();
    bool klar = (s_abonneret && s_asset[0] != '\0');
    char asset[sizeof(s_asset)];
    snprintf(asset, sizeof(asset), "%s", s_asset);
    SLIP();
    if (!klar) {
        /*
         * Ikke indmeldt endnu. Vi nulstiller OGSAA tilstanden, saa
         * vaerdien bliver sagt forfra naar vi kommer ind. Ellers ville en
         * skaerm der mistede forbindelsen midt i en uaendret tilstand
         * aldrig sige den igen, og feltet stod tomt paa serveren.
         */
        s_inv.sagt = false;
        return;
    }
    if (!zs_fleet_inv_skal_sende(&s_inv, online)) {
        return;
    }

    char emne[160];
    if (zs_fleet_msg_topic(emne, sizeof(emne), ZS_FLEET_REALM,
                           zs_fleet_unique_id(), ZS_FLEET_INVERTER_FELT,
                           asset) == 0) {
        /* Kunne vi ikke bygge emnet, har vi ikke sagt noget. Saa maa den
         * ikke staa som sagt, ellers bliver den aldrig forsoegt igen. */
        s_inv.sagt = false;
        return;
    }
    /* SMAA bogstaver. Maalt mod serveren: "True" med stort bliver TAVST
     * ingenting, feltet staar tomt og der kommer ingen fejl nogen steder. */
    const char *krop = online ? "true" : "false";
    esp_mqtt_client_publish(s_klient, emne, krop, 0, 0, 0);
    ESP_LOGI(TAG, "inverteren er %s", online ? "online" : "offline");
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
    if (live->solar_w.ok)       { send_tal(ZS_FLEET_SOL_FELT, asset,   live->solar_w.v); }
    if (live->house_w.ok)       { send_tal(ZS_FLEET_FORBRUG_FELT, asset,   live->house_w.v); }
    if (live->battery_w.ok)     { send_tal(ZS_FLEET_BATTERI_FELT, asset, live->battery_w.v); }
    if (live->grid_w.ok)        { send_tal(ZS_FLEET_NET_FELT, asset,    live->grid_w.v); }
    if (live->soc_pct.ok)       { send_tal(ZS_FLEET_SOC_FELT, asset, live->soc_pct.v); }

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
        send_tekst(ZS_FLEET_MODEL_FELT, asset,  info->model);
        send_tekst(ZS_FLEET_SERIENR_FELT, asset, info->serial);
        send_tal(ZS_FLEET_EFFEKT_FELT, asset,      info->inverter_rated_kw);
        send_tal(ZS_FLEET_KAPACITET_FELT, asset, info->battery_capacity_kwh);
        ZS_LOGI(TAG, "sendte anlaeggets oplysninger");
    }
}

#else  /* ZS_FLEET_ENABLED == 0 */

bool zs_fleet_start(void) { return false; }
void zs_fleet_stop(void) {}
void zs_fleet_skynd_paa_indmeldelsen(void) {}
void zs_fleet_publish(const zs_fr_live_t *live, const zs_fr_info_t *info)
{ (void)live; (void)info; }
void zs_fleet_publish_energi(const float *wh, const bool *har, int8_t time)
{ (void)wh; (void)har; (void)time; }
void zs_fleet_publish_inverter(bool online) { (void)online; }
void zs_fleet_publish_tilmeld(const char *kode) { (void)kode; }
void zs_fleet_publish_kvarter(const zs_kvarter_ud_t *ud) { (void)ud; }
zs_fleet_state_t zs_fleet_state(void) { return ZS_FLEET_OFF; }
const char *zs_fleet_state_text(void) { return "Slået fra"; }
void zs_fleet_asset_id(char *ud, size_t ud_len)
{ zs_tekst_kopi(ud, ud_len, ""); }
const char *zs_fleet_unique_id(void) { return ""; }

#endif /* ZS_FLEET_ENABLED */
