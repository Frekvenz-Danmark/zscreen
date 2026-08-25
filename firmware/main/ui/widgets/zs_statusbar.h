/*
 * zScreen - statuslinjen foroven.
 *
 *     ┌────────────────────────────────────────────┐
 *     │ [Z]                        14:32   ᯤ   ⚙   │  44 px
 *     └────────────────────────────────────────────┘
 *       14                        388   400   430
 *
 * Ét statusikon, ikke to. Er der ingen wifi, er der heller ingen
 * inverter, saa to lamper der siger det samme ville bare fylde.
 * Ikonet viser signalstyrken naar alt er godt, og skifter til en
 * advarsel naar noget mangler.
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
    lv_obj_t *demo;
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

/* rssi er wifi-signalet i dBm. Bruges kun naar tilstanden er ZS_LINK_OK. */
void zs_statusbar_set_link(zs_statusbar_t *sb, zs_link_state_t state, int rssi);

/*
 * Viser eller skjuler DEMO-maerket ved siden af logoet.
 *
 * Det staar med orange og fylder. Meningen er at ingen skal kunne
 * komme til at tro at tallene paa skaermen er rigtige maalinger.
 */
void zs_statusbar_set_demo(zs_statusbar_t *sb, bool demo);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STATUSBAR_H */
