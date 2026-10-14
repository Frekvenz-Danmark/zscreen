/*
 * zScreen - siden der kommer frem naar noget er vaek.
 *
 * HVORFOR DEN FINDES.
 *
 * Mister skaermen wifi, mister den ALT: anlaegget naas over det samme
 * netvaerk, saa der kommer hverken nye maalinger eller priser. FOer den
 * her side blev hovedsiden staaende med de sidste tal den havde. De
 * saa levende ud, for der staar ikke et tidspunkt paa dem, og en kunde
 * kunne kigge paa en time gamle tal uden at vide det.
 *
 * Derfor tager den her side over, og derfor blinker den. Et stille ikon
 * i et hjoerne er ikke nok: skaermen haenger paa en vaeg og bliver set
 * paa afstand, og det eneste der faar nogen til at gaa derhen, er at
 * der sker noget.
 *
 * DEN HED FOER zs_screen_nonet OG KUNNE ÉN TING: wifi. Men wifi er kun
 * den ene af to maader tallene kan holde op med at komme paa, og den
 * anden - at anlaegget ikke svarer - havde kun et lille gult maerke i
 * toplinjen. Det ser ingen paa afstand. Nu er det den samme side, og
 * den faar at vide HVAD der er galt, saa baade teksten og knappen peger
 * det rigtige sted hen. Reglen for hvad der er galt staar i
 * app/zs_fejl.h, ikke her: siden tegner, den beslutter ikke.
 *
 * KNAPPEN FOERER FORSKELLIGE STEDER HEN, og det er hele pointen. Er det
 * netvaerket, skal kunden vaelge et netvaerk. Er det anlaegget, er der
 * ingen grund til at bede hende om et wifi-kodeord hun ikke har brug
 * for - saa skal skaermen lede efter anlaegget. FOer gik begge veje til
 * netvaerkslisten.
 */

#ifndef ZS_SCREEN_FEJL_H
#define ZS_SCREEN_FEJL_H

#include <stdbool.h>

#include "lvgl.h"
#include "zs_fejl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bygger siden. Kaldes én gang, som de andre sider. */
void zs_fejl_page_create(void);

/* Sidens rodobjekt, til zs_ui.c. NULL foer den er bygget. */
lv_obj_t *zs_fejl_page_root(void);

/*
 * Skriver beskeden om.
 *
 * tid er klokkeslaettet for den sidste maaling, fx "14:05", eller NULL
 * hvis uret ikke er sat. Staar der et tidspunkt, kommer det med i
 * forklaringen: "Tallene paa forsiden er fra kl. 14:05 og staar
 * stille." Det er den eneste linje paa siden der fortaeller kunden hvor
 * gammelt det hun kiggede paa, faktisk er.
 *
 * blink kommer fra zs_fejl_tik og besluttes IKKE her. En inverter der
 * sover skal ikke blinke, og den regel skal kun staa ét sted.
 *
 * Skal kaldes med LVGL-laasen taget, som de andre update-funktioner.
 */
void zs_fejl_page_update(zs_fejl_type_t type, const char *tid, bool blink);

/*
 * Siger om siden vises lige nu.
 *
 * Det er IKKE nok at bygge siden med en animation der altid koerer: en
 * animation paa en side ingen ser, koster baade stroem og en
 * genoptegning i hvert billede. Derfor taendes den naar siden vises og
 * slukkes naar den forlades, og zs_ui.c gOEr det.
 */
void zs_fejl_page_set_active(bool aktiv);

/* River siden ned. Se noten ved zs_screen_home_destroy: farver der
 * sidder paa hvert objekt skal tegnes forfra naar temaet skifter. */
void zs_fejl_page_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_FEJL_H */
