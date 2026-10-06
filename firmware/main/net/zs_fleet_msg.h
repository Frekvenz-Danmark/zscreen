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

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLEET_MSG_H */
