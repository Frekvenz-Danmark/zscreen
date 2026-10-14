/*
 * zScreen - ikonnavne.
 *
 * Ikonerne kommer fra Lucide, den samme pakke som Zbox-webfladen
 * bruger. Saa ser en advarselstrekant ens ud paa skaermen og i
 * browseren, og vi behoever ikke tegne noget selv.
 *
 * Lucide laegger hvert ikon paa et kodepunkt i Unicodes private
 * omraade. Her staar de som UTF-8-strenge, saa de kan saettes direkte
 * ind i en LVGL-etiket:
 *
 *     lv_label_set_text(lbl, ZS_ICON_SUN);
 *     lv_obj_set_style_text_font(lbl, &zs_icons_20, 0);
 *
 * FILEN ER GENERERET. Ret ikke i den i haanden.
 * Listen staar i tools/icons.py, og den samme liste bestemmer ogsaa
 * hvilke ikoner der kommer med i skrifttypen. Skriv du et nyt ikon
 * ind her i stedet, ville navnet findes uden at tegningen fulgte med,
 * og feltet ville staa tomt paa skaermen uden en eneste fejl i loggen.
 *
 * Byg igen med:  ./tools/build-assets.sh --force
 */

#ifndef ZS_ICONS_H
#define ZS_ICONS_H

#include "lvgl.h"

LV_FONT_DECLARE(zs_icons_20)
LV_FONT_DECLARE(zs_icons_28)

/* sun              U+E178  solceller */
#define ZS_ICON_SUN            "\xEE\x85\xB8"
/* house            U+E0F5  forbrug i huset */
#define ZS_ICON_HOUSE          "\xEE\x83\xB5"
/* battery-charging U+E054  batteri der lader eller aflader */
#define ZS_ICON_BATTERY_CHARGE "\xEE\x81\x94"
/* battery          U+E053  batteri i hvile */
#define ZS_ICON_BATTERY        "\xEE\x81\x93"
/* zap              U+E1B4  elnettet */
#define ZS_ICON_ZAP            "\xEE\x86\xB4"
/* wifi             U+E1AE  godt signal */
#define ZS_ICON_WIFI           "\xEE\x86\xAE"
/* wifi-high        U+E5F7  middel signal */
#define ZS_ICON_WIFI_HIGH      "\xEE\x97\xB7"
/* wifi-low         U+E5F8  svagt signal */
#define ZS_ICON_WIFI_LOW       "\xEE\x97\xB8"
/* wifi-off         U+E1AF  ingen forbindelse */
#define ZS_ICON_WIFI_OFF       "\xEE\x86\xAF"
/* settings         U+E154  indstillinger */
#define ZS_ICON_SETTINGS       "\xEE\x85\x94"
/* arrow-up         U+E04A  ud af huset, saelger */
#define ZS_ICON_ARROW_UP       "\xEE\x81\x8A"
/* arrow-down       U+E042  ind i huset, koeber */
#define ZS_ICON_ARROW_DOWN     "\xEE\x81\x82"
/* arrow-left       U+E048  tilbage */
#define ZS_ICON_ARROW_LEFT     "\xEE\x81\x88"
/* arrow-right      U+E049  videre */
#define ZS_ICON_ARROW_RIGHT    "\xEE\x81\x89"
/* triangle-alert   U+E193  advarsel */
#define ZS_ICON_ALERT          "\xEE\x86\x93"
/* loader-circle    U+E10A  arbejder */
#define ZS_ICON_SPINNER        "\xEE\x84\x8A"
/* check            U+E06C  valgt eller faerdig */
#define ZS_ICON_CHECK          "\xEE\x81\xAC"
/* chevron-left     U+E06E  tilbage i en liste */
#define ZS_ICON_CHEVRON_LEFT   "\xEE\x81\xAE"
/* chevron-right    U+E06F  ind i en liste */
#define ZS_ICON_CHEVRON_RIGHT  "\xEE\x81\xAF"
/* chevron-down     U+E06D  fold ud */
#define ZS_ICON_CHEVRON_DOWN   "\xEE\x81\xAD"
/* lock             U+E10B  netvaerk med kodeord */
#define ZS_ICON_LOCK           "\xEE\x84\x8B"
/* refresh-cw       U+E145  soeg igen */
#define ZS_ICON_REFRESH        "\xEE\x85\x85"
/* plug             U+E37F  inverter */
#define ZS_ICON_PLUG           "\xEE\x8D\xBF"
/* x                U+E1B2  luk */
#define ZS_ICON_CLOSE          "\xEE\x86\xB2"
/* eye              U+E0BA  vis kodeordet */
#define ZS_ICON_EYE            "\xEE\x82\xBA"
/* eye-off          U+E0BB  skjul kodeordet */
#define ZS_ICON_EYE_OFF        "\xEE\x82\xBB"
/* circle-check     U+E226  det lykkedes */
#define ZS_ICON_CIRCLE_CHECK   "\xEE\x88\xA6"
/* circle-alert     U+E077  der er noget galt */
#define ZS_ICON_CIRCLE_ALERT   "\xEE\x81\xB7"
/* power            U+E140  genstart */
#define ZS_ICON_POWER          "\xEE\x85\x80"
/* info             U+E0F9  detaljer om anlaegget */
#define ZS_ICON_INFO           "\xEE\x83\xB9"
/* search           U+E151  soeg efter inverter */
#define ZS_ICON_SEARCH         "\xEE\x85\x91"
/* rotate-ccw       U+E148  nulstil */
#define ZS_ICON_ROTATE         "\xEE\x85\x88"
/* trash-2          U+E18E  slet */
#define ZS_ICON_TRASH          "\xEE\x86\x8E"
/* delete           U+E0AE  slet et tegn */
#define ZS_ICON_BACKSPACE      "\xEE\x82\xAE"

#endif /* ZS_ICONS_H */
