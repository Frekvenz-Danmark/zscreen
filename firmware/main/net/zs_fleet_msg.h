/*
 * zScreen - den del af floedestyringen der kan staa alene: beskederne
 * som ren tekst, og beslutningen om hvornaar vi melder ind igen.
 *
 * Bruger intet fra ESP-IDF, saa den kan testes paa en almindelig
 * maskine. Det er ikke pynt. Indmeldelsen staar og falder paa at et
 * certifikat paa halvanden kilobyte bliver undsluppet rigtigt til JSON:
 * er ét linjeskift forkert, afviser serveren os, og det ville vi foerst
 * opdage ude hos en kunde. Og beslutningen om at proeve igen gik
 * EN GANG galt netop fordi den laa inde i haendelseshaandteringen hvor
 * den ikke kunne proeves af.
 */

#ifndef ZS_FLEET_MSG_H
#define ZS_FLEET_MSG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bygger indmeldelsen: {"type":"x509","cert":"..."}
 *
 * Linjeskift i PEM'en bliver \n, gaasefoedder og bagstreg undsluppet,
 * og vognretur smidt vaek. Serveren vil have PEM'en som ÉN JSON-streng.
 *
 * Returnerer antal skrevne tegn uden afslutningen, eller 0 hvis der
 * ikke var plads. Ved 0 er ud sat til en tom streng, saa en kalder der
 * glemmer at tjekke ikke sender affald.
 */
size_t zs_fleet_msg_enroll(char *ud, size_t ud_len, const char *cert_pem);

/* Hvor stor en buffer skal der til for det certifikat. Inklusive
 * afslutningen, saa den kan gives direkte til malloc. */
size_t zs_fleet_msg_enroll_size(const char *cert_pem);

/*
 * Bygger emnet der skrives til:
 *   {realm}/{unik}/writeattributevalue/{felt}/{enhedsid}
 *
 * Returnerer antal tegn, eller 0 hvis noget manglede eller der ikke var
 * plads. Ved 0 er ud en tom streng.
 */
size_t zs_fleet_msg_topic(char *ud, size_t ud_len, const char *realm,
                          const char *unik, const char *felt,
                          const char *asset_id);

/*
 * Emnet vi LYTTER paa, altsaa den anden vej. Samme form, men uden
 * "write": serveren sender paa attributevalue naar et felt aendrer sig.
 *
 *   {realm}/{unik}/attributevalue/{felt}/{enheds-id}
 *
 * Bruges til maalversionen, se ZS_FLEET_TARGET_FELT. Samme tjek som
 * ovenfor: returnerer 0 og en tom streng hvis noget mangler eller ikke
 * kan vaere der.
 */
size_t zs_fleet_msg_lyt_topic(char *ud, size_t ud_len, const char *realm,
                              const char *unik, const char *felt,
                              const char *asset_id);

/*
 * Skal vi melde ind igen nu?
 *
 * ÉN regel for alle de maader en indmeldelse kan gaa skaevt paa: en
 * afvisning, en afsendelse der ikke gik igennem, og et svar der aldrig
 * kom. Alle tre ender med at vi sidder paa en aaben forbindelse uden at
 * vaere indmeldt.
 *
 *   abonneret           er vi forbundet og lytter paa svaret
 *   har_asset           har vi et enheds-id, altsaa er vi inde
 *   naeste_forsoeg_ms   hvornaar vi tidligst maa, nul = aldrig
 *   nu_ms               nu
 *
 * Nul i naeste_forsoeg_ms betyder "ingen plan", ikke "med det samme".
 * Ellers ville en nystartet skaerm melde ind fra den rene tilstand uden
 * at vaere forbundet.
 */
bool zs_fleet_enroll_due(bool abonneret, bool har_asset,
                         int64_t naeste_forsoeg_ms, int64_t nu_ms);

/* ------------------------------------------------------------------ */
/* Beskeder der kommer i stykker                                       */
/* ------------------------------------------------------------------ */

/*
 * HVORFOR DEN FINDES.
 *
 * esp-mqtt laeser ind i en buffer paa 1024 bytes. Er en besked stoerre,
 * faar vi den i FLERE haendelser, og kun den FOERSTE har et emne paa sig.
 * De naeste har topic_len nul.
 *
 * Foer blev hvert stykke laest som om det var en hel besked. MAALT mod
 * vores egen server: svaret paa en indmeldelse er 2604 bytes, altsaa tre
 * stykker, og ingen af dem er gyldig JSON alene. Skaermen fik derfor
 * aldrig sit enheds-id og maatte aldrig skrive en maaling.
 *
 * Her samles de, og emnet fra det foerste stykke huskes, saa beskeden
 * kan sendes det rigtige sted hen naar den er hel.
 */

/* Laengste emne vi kan faa. Det laengste vi selv laver er
 * lytte-emnet: realm + unikt id + "attributevalue" + felt + enheds-id. */
#define ZS_FLEET_EMNE_MAX   160

typedef struct {
    char  *buf;                      /* kalderens plads, ejes udefra  */
    size_t buf_len;
    char   emne[ZS_FLEET_EMNE_MAX];  /* fra FOERSTE stykke            */
    size_t har;                      /* hvor meget vi har samlet      */
    size_t venter;                   /* hvor meget der skal komme     */
    bool   dropper;                  /* for stor, resten smides vaek  */
} zs_fleet_saml_t;

typedef enum {
    ZS_SAML_VENTER = 0,   /* der mangler flere stykker                */
    ZS_SAML_KLAR,         /* hele beskeden staar i buf, emne er sat   */
    ZS_SAML_FOR_STOR,     /* den kunne ikke vaere der, droppet        */
    ZS_SAML_USAMMENHAENG  /* stykkerne hang ikke sammen, alt smidt ud */
} zs_saml_t;

/* Gør samleren klar. buf skal leve lige saa laenge som samleren. */
void zs_fleet_saml_init(zs_fleet_saml_t *s, char *buf, size_t buf_len);

/*
 * Tager ét stykke ind. Felterne er dem esp-mqtt giver os:
 *
 *   emne/emne_len    kun paa det foerste stykke, ellers tom
 *   data/data_len    stykket
 *   offset           current_data_offset, nul paa det foerste
 *   total            total_data_len, altsaa hele beskedens laengde
 *
 * Nul i offset starter forfra, ogsaa hvis det forrige aldrig blev helt.
 * Saadan er virkeligheden: en forbindelse der falder midt i en besked
 * efterlader os med en halv, og den maa ikke blandes med den naeste.
 *
 * Ved ZS_SAML_KLAR staar hele beskeden i buf med en afslutning, og
 * s->emne er emnet. Ellers maa buf ikke bruges.
 */
zs_saml_t zs_fleet_saml_tag(zs_fleet_saml_t *s,
                            const char *emne, int emne_len,
                            const char *data, int data_len,
                            int offset, int total);

/*
 * Staar ordet som et helt LED i emnet, altsaa mellem to skraastreger?
 *
 * Et MQTT-emne er led adskilt af skraastreger:
 *
 *     provisioning/zscreen-44b176b06c60/response
 *     master/zscreen-44b176b06c60/attributevalue/targetVersion/59EUfm
 *
 * Foer blev der ledt efter ordet HVOR SOM HELST i emnet, og beskeden
 * blev sendt videre efter reglen "alt der ikke indeholder targetVersion
 * er et indmeldelsessvar". Det er den forkerte vej rundt: en besked skal
 * kendes paa hvad den ER, ikke paa hvad den ikke er. Kommer der en
 * tredje slags besked en dag, bliver den ellers laest som et svar.
 *
 * Og halen kan ikke bruges i stedet: lytte-emnet SLUTTER paa enhedens
 * id, saa "targetVersion" staar i midten. Derfor led og ikke hale.
 */
bool zs_fleet_emne_har_led(const char *emne, const char *led);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLEET_MSG_H */
