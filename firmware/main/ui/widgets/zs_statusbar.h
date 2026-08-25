/*
 * zScreen - statuslinjen foroven.
 *
 *     ┌────────────────────────────────────────────┐
 *     │ [Z]                        14:32   ᯤ   ⚙   │  44 px
 *     └────────────────────────────────────────────┘
 *       14                        388   400   430
 *
 * Maerket til venstre siger med ORD hvordan det gaar. Ikonet til
 * hoejre viser kun signalstyrken, og kun naar der ER en forbindelse.
 *
 *     DEMO             orange, opdigtede tal
 *     Forbundet        groen, alt virker
 *     Forbinder        graa, undervejs
 *     Ingen inverter   gul, wifi virker men inverteren svarer ikke
 *     Intet netvaerk   roed, ingen forbindelse
 *
 * Der staar ét sted hvad hver tilstand hedder og hvilken farve den
 * har. To steder ville med sikkerhed komme ud af trit.
 *
 * Klokkeslaettet kommer fra NTP. Er der intet internet, skjules det
 * helt i stedet for at vise et forkert klokkeslaet. Resten af skaermen
 * virker som normalt: uret er pynt, ikke en forudsaetning.
 */

#ifndef ZS_STATUSBAR_H
#define ZS_STATUSBAR_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZS_LINK_OK = 0,       /* wifi og inverter svarer begge          */
    ZS_LINK_CONNECTING,   /* vi er i gang med at forbinde           */
    ZS_LINK_NO_INVERTER,  /* wifi er oppe, inverteren svarer ikke   */
    ZS_LINK_NO_WIFI,      /* der er ingen netvaerksforbindelse      */
} zs_link_state_t;

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *logo;
    lv_obj_t *badge;
    lv_obj_t *time;
    lv_obj_t *status_icon;
    lv_obj_t *gear;
    zs_link_state_t state;
} zs_statusbar_t;

/* gear_cb kaldes naar der trykkes paa tandhjulet. Maa vaere NULL,
 * og saa tegnes tandhjulet slet ikke. */
void zs_statusbar_create(zs_statusbar_t *sb, lv_obj_t *parent,
                         lv_event_cb_t gear_cb, void *user_data);

/* hhmm er fx "14:32". NULL skjuler klokkeslaettet. */
void zs_statusbar_set_time(zs_statusbar_t *sb, const char *hhmm);

/*
 * Saetter baade maerket og ikonet.
 *
 * ÉT kald, saa de to aldrig kan komme til at sige hver sit.
 * rssi bruges kun naar tilstanden er ZS_LINK_OK.
 * demo overtrumfer alt: opdigtede tal skal altid kunne ses som saadan.
 */
void zs_statusbar_set_link(zs_statusbar_t *sb, zs_link_state_t state,
                           int rssi, bool demo);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STATUSBAR_H */
