/*
 * zScreen - beskederne til floedestyringen, som ren tekst.
 *
 * Ligger for sig selv og bruger intet fra ESP-IDF, saa den kan testes
 * paa en almindelig maskine. Det er ikke pynt: indmeldelsen staar og
 * falder paa at et certifikat paa halvanden kilobyte bliver undsluppet
 * rigtigt til JSON. Er ét linjeskift forkert, afviser serveren os, og
 * det ville vi foerst opdage ude hos en kunde.
 */

#ifndef ZS_FLEET_MSG_H
#define ZS_FLEET_MSG_H

#include <stddef.h>

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

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLEET_MSG_H */
