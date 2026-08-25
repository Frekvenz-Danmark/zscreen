/*
 * zScreen - inverterens tilstand og fejl, oversat til dansk.
 *
 * Inverteren melder sine fejl som bits i nogle registre. Nogle af dem
 * er standard-SunSpec og betyder det samme paa enhver inverter. Andre
 * er Fronius' egne. Vi slaar begge dele op i tabeller der kommer
 * direkte fra kilden, se zs_fronius_codes.h.
 *
 * Kan vi ikke saette ord paa en kode, siger vi det, og fortaeller hvem
 * kunden skal ringe til. Vi gaetter aldrig paa hvad en ukendt kode
 * betyder: en forkert forklaring paa en fejl er vaerre end ingen.
 */

#ifndef ZS_STATUS_H
#define ZS_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "zs_fronius.h"
#include "zs_fronius_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZS_STATUS_MAX   14

typedef struct {
    zs_sev_t sev;
    char     tekst[56];    /* fx "For høj temperatur"              */
    char     detalje[72];  /* fx "Fronius-kode 303, 304, 322"      */
} zs_status_item_t;

typedef struct {
    zs_sev_t         vaerst;      /* hoejeste alvorlighed i listen */
    zs_status_item_t poster[ZS_STATUS_MAX];
    uint8_t          antal;
    bool             afkortet;    /* der var flere end der er plads til */
    bool             har_svar;    /* meldte inverteren overhovedet noget */
} zs_status_list_t;

/* Laver listen ud fra en aflaesning. Rører ikke netvaerket. */
void zs_status_build(zs_status_list_t *ud, const zs_fr_live_t *live);

/* Inverterens driftstilstand som ord. Aldrig NULL. */
const char *zs_status_state_text(int32_t st);

/* En kort saetning der opsummerer hele listen, til overskriften. */
const char *zs_status_summary(const zs_status_list_t *l, const zs_fr_live_t *live);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STATUS_H */
