/*
 * zScreen - demo-tilstand.
 *
 * Viser hovedskaermen med opdigtede tal, saa man kan se hvordan
 * skaermen opfoerer sig uden at have et anlaeg i naerheden. Et helt
 * doegn gaar paa omkring tre minutter, saa man naar at se solen staa
 * op, batteriet lade op, eksport midt paa dagen og aflading om natten.
 *
 * Tre ting goer den ufarlig:
 *
 *   1. Den gemmes ALDRIG. En genstart slaar den fra. En enhed hos en
 *      kunde kan derfor ikke komme til at starte op i demo, uanset
 *      hvad der er trykket paa foer.
 *
 *   2. Statuslinjen viser DEMO med orange hele tiden. Man kan ikke
 *      komme til at tro at tallene er rigtige.
 *
 *   3. Den kan slaas helt ud af firmwaren med ZS_DEMO_ENABLED i
 *      zs_config.h. Saettes den til 0, findes hverken knappen eller
 *      koden i den byggede fil.
 *
 * Tallene kommer fra den samme model som simulatoren i
 * tools/fronius-sim: forbrug = inverterens AC-effekt plus det der
 * koebes fra nettet, og de tre stoerrelser gaar altid op.
 */

#ifndef ZS_DEMO_H
#define ZS_DEMO_H

#include <stdbool.h>
#include <stdint.h>

#include "zs_fronius.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nulstiller og starter forfra ved middagstid, hvor der sker mest. */
void zs_demo_reset(void);

/*
 * Regner naeste oejeblik ud og fylder live.
 * dt_ms er hvor lang tid der er gaaet siden sidst.
 */
void zs_demo_step(zs_fr_live_t *live, uint32_t dt_ms);

/* Opdigtede oplysninger om anlaegget, saa Detaljer-siden ogsaa
 * har noget at vise. */
void zs_demo_info(zs_fr_info_t *info);

/* Klokkeslaet i demoens egen tid, fx "13:24". */
const char *zs_demo_clock(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_DEMO_H */
