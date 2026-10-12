/*
 * zScreen - siden der vises naar skaermen har mistet netvaerket.
 *
 * HVORFOR DEN FINDES.
 *
 * Mister skaermen wifi, mister den ALT: inverteren naas over det samme
 * netvaerk, saa der kommer hverken nye maalinger eller priser. Foer den
 * her side blev hovedsiden staaende med de sidste tal den havde. De
 * saa levende ud, for der staar ikke et tidspunkt paa dem, og en kunde
 * kunne kigge paa en time gamle tal uden at vide det.
 *
 * Derfor tager den her side over, og derfor blinker den. Et stille
 * ikon i et hjoerne er ikke nok: skaermen haenger paa en vaeg og bliver
 * set paa afstand, og det eneste der faar nogen til at gaa derhen, er
 * at der sker noget.
 *
 * Siden har ÉN knap, og den foerer til opsaetningens netvaerkstrin.
 * Derfra er der en vej hjem igen: tilbageknappen dér sender
 * ZS_CMD_SETUP_CANCEL, og app'en bestemmer selv hvor kunden skal hen.
 */

#ifndef ZS_SCREEN_NONET_H
#define ZS_SCREEN_NONET_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bygger siden. Kaldes én gang, som de andre sider. */
void zs_nonet_page_create(void);

/* Sidens rodobjekt, til zs_ui.c. NULL foer den er bygget. */
lv_obj_t *zs_nonet_page_root(void);

/*
 * Starter og stopper blinket.
 *
 * Det er IKKE nok at bygge siden med en animation der altid koerer: en
 * LVGL-timer der tikker videre paa en side ingen ser, koster baade
 * stroem og en genoptegning hvert halve sekund. Derfor taendes den naar
 * siden vises og slukkes naar den forlades, og zs_ui.c goer det.
 */
void zs_nonet_page_set_active(bool aktiv);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_NONET_H */
