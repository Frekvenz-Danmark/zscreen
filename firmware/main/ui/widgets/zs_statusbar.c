#include "zs_statusbar.h"
#include "zs_theme.h"

#include <string.h>

/* Vandrette pladser, regnet fra hoejre kant paa en 480 px skaerm.
 *     tandhjul   430 .. 474   (44 x 44, hele fingerfladen)
 *     statusikon 400 .. 420
 *     klokkeslaet slutter paa 388
 * Se zs_statusbar.h for tegningen. */
#define GEAR_X      430
#define ICON_X      400
#define TIME_RIGHT  (ZS_SCR_WIDTH - 92)

void zs_statusbar_create(zs_statusbar_t *sb, lv_obj_t *parent,
                         lv_event_cb_t gear_cb, void *user_data)
{
    memset(sb, 0, sizeof(*sb));

    sb->bar = lv_obj_create(parent);
    lv_obj_remove_style_all(sb->bar);
    lv_obj_set_size(sb->bar, ZS_SCR_WIDTH, ZS_BAR_HEIGHT);
    lv_obj_set_pos(sb->bar, 0, 0);
    lv_obj_clear_flag(sb->bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(sb->bar, LV_SCROLLBAR_MODE_OFF);

    /* Z-maerket. 26 px hoejt, midt i den 44 px hoeje linje. */
    sb->logo = lv_img_create(sb->bar);
    lv_img_set_src(sb->logo, zs_logo_zmark());
    lv_obj_set_pos(sb->logo, 14, (ZS_BAR_HEIGHT - 26) / 2);

    /*
     * Maerket lige efter logoet.
     *
     * Hoejden er 22: 13 px skrift giver 16 px linje, plus 3 px luft
     * over og under. Det staar lodret midt i den 44 px hoeje linje.
     */
    sb->badge = lv_label_create(sb->bar);
    lv_label_set_text(sb->badge, "");
    zs_style_text(sb->badge, &zs_font_13, ZS_C_BG);
    lv_obj_set_style_text_letter_space(sb->badge, 1, 0);
    lv_obj_set_style_bg_opa(sb->badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sb->badge, 6, 0);
    lv_obj_set_style_pad_hor(sb->badge, 8, 0);
    lv_obj_set_style_pad_ver(sb->badge, 3, 0);
    lv_obj_set_pos(sb->badge, 14 + 26 + 10, (ZS_BAR_HEIGHT - 22) / 2);

    sb->time = lv_label_create(sb->bar);
    lv_label_set_text(sb->time, "");
    zs_style_text(sb->time, &zs_font_20, ZS_C_TEXT_DIM);
    lv_obj_align(sb->time, LV_ALIGN_TOP_RIGHT, -(ZS_SCR_WIDTH - TIME_RIGHT), 10);

    sb->status_icon = lv_label_create(sb->bar);
    lv_label_set_text(sb->status_icon, ZS_ICON_WIFI_OFF);
    lv_obj_set_style_text_font(sb->status_icon, &zs_icons_20, 0);
    lv_obj_set_style_text_color(sb->status_icon, lv_color_hex(ZS_C_STALE), 0);
    lv_obj_set_pos(sb->status_icon, ICON_X, (ZS_BAR_HEIGHT - 20) / 2);

    if (gear_cb != NULL) {
        /* Hele knappen er 44 x 44 selvom tandhjulet kun er 20 px.
         * Et ikon paa 20 px er omkring 3,5 mm paa denne skaerm, og det
         * kan man ikke ramme med en tommelfinger. */
        sb->gear = lv_btn_create(sb->bar);
        lv_obj_remove_style_all(sb->gear);
        lv_obj_set_size(sb->gear, ZS_TOUCH_MIN, ZS_TOUCH_MIN);
        lv_obj_set_pos(sb->gear, GEAR_X, 0);
        lv_obj_add_event_cb(sb->gear, gear_cb, LV_EVENT_CLICKED, user_data);

        lv_obj_t *ic = lv_label_create(sb->gear);
        lv_label_set_text(ic, ZS_ICON_SETTINGS);
        lv_obj_set_style_text_font(ic, &zs_icons_20, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_center(ic);
    }

    sb->state = ZS_LINK_NO_WIFI;
}

void zs_statusbar_set_time(zs_statusbar_t *sb, const char *hhmm)
{
    if (sb == NULL || sb->time == NULL) {
        return;
    }
    lv_label_set_text(sb->time, hhmm != NULL ? hhmm : "");
}

/*
 * Hvad hver tilstand hedder og hvilken farve den har. ÉN tabel.
 *
 * Teksten er den kunden laeser, saa den er paa dansk og siger hvad der
 * ER, ikke hvad der mangler i koden. Farven paa maerket er baggrunden,
 * og teksten staar altid i sidens moerke bund, saa den kan laeses paa
 * baade groen, gul og roed.
 */
typedef struct {
    const char *tekst;
    uint32_t    farve;
} badge_t;

static badge_t badge_for(zs_link_state_t state, bool demo)
{
    if (demo) {
        /* Demo overtrumfer alt. Ingen maa kunne komme til at tro at
         * opdigtede tal er rigtige maalinger. */
        return (badge_t){ "DEMO", ZS_C_ACCENT };
    }
    switch (state) {
    case ZS_LINK_OK:           return (badge_t){ "FORBUNDET",      ZS_C_GOOD };
    case ZS_LINK_CONNECTING:   return (badge_t){ "FORBINDER",      ZS_C_LABEL };
    case ZS_LINK_NO_INVERTER:  return (badge_t){ "INGEN INVERTER", ZS_C_WARN };
    case ZS_LINK_NO_WIFI:
    default:                   return (badge_t){ "INTET NETVÆRK",  ZS_C_BAD };
    }
}

void zs_statusbar_set_link(zs_statusbar_t *sb, zs_link_state_t state,
                           int rssi, bool demo)
{
    if (sb == NULL || sb->badge == NULL || sb->status_icon == NULL) {
        return;
    }
    sb->state = state;

    badge_t b = badge_for(state, demo);
    lv_label_set_text(sb->badge, b.tekst);
    lv_obj_set_style_bg_color(sb->badge, lv_color_hex(b.farve), 0);

    /*
     * Ikonet viser wifi-signalet. Er der ingen forbindelse, viser vi
     * wifi-off, altsaa wifi-buerne med en streg over, i samme roede som
     * maerket. Vi skjuler det ikke, for en tom plads laeser man som at
     * skaermen ikke har opdaget noget, og det er lige praecis det
     * modsatte af hvad vi vil sige.
     *
     * Alle fire ikoner er Lucide og har samme adv_w paa 20 px, saa
     * labelet er lige bredt uanset hvilket der staar i det. Ikonet
     * flytter sig ikke naar signalet skifter.
     *
     * Graenserne er de samme som resten af branchen bruger:
     *   over -60 dBm  godt
     *   -60 til -75   brugbart
     *   under -75     svagt, her begynder forbindelsen at hakke
     *
     * Demo er den ene undtagelse. Der er intet rigtigt signal at vise,
     * og maerket siger allerede DEMO med ord.
     */
    if (demo) {
        lv_obj_add_flag(sb->status_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(sb->status_icon, LV_OBJ_FLAG_HIDDEN);

    const char *icon;
    uint32_t    farve;
    if (state == ZS_LINK_NO_WIFI || rssi == 0) {
        icon  = ZS_ICON_WIFI_OFF;
        farve = (state == ZS_LINK_NO_WIFI) ? ZS_C_BAD : ZS_C_STALE;
    } else if (rssi >= -60) {
        icon  = ZS_ICON_WIFI;
        farve = ZS_C_LABEL;
    } else if (rssi >= -75) {
        icon  = ZS_ICON_WIFI_HIGH;
        farve = ZS_C_LABEL;
    } else {
        icon  = ZS_ICON_WIFI_LOW;
        farve = ZS_C_LABEL;
    }

    lv_label_set_text(sb->status_icon, icon);
    lv_obj_set_style_text_color(sb->status_icon, lv_color_hex(farve), 0);
}
