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
    lv_img_set_src(sb->logo, &zs_img_zmark);
    lv_obj_set_pos(sb->logo, 14, (ZS_BAR_HEIGHT - 26) / 2);

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

void zs_statusbar_set_link(zs_statusbar_t *sb, zs_link_state_t state, int rssi)
{
    if (sb == NULL || sb->status_icon == NULL) {
        return;
    }
    sb->state = state;

    const char *icon;
    uint32_t    color;

    switch (state) {
    case ZS_LINK_OK:
        /*
         * Signalstyrke i tre trin. Graenserne er de samme som resten af
         * branchen bruger:
         *   over -60 dBm  godt
         *   -60 til -75   brugbart
         *   under -75     svagt, her begynder forbindelsen at hakke
         */
        if (rssi >= -60)      { icon = ZS_ICON_WIFI; }
        else if (rssi >= -75) { icon = ZS_ICON_WIFI_HIGH; }
        else                  { icon = ZS_ICON_WIFI_LOW; }
        color = ZS_C_LABEL;
        break;

    case ZS_LINK_CONNECTING:
        icon = ZS_ICON_SPINNER;
        color = ZS_C_LABEL;
        break;

    case ZS_LINK_NO_INVERTER:
        /* Wifi er der, men inverteren svarer ikke. Gul, ikke roed:
         * det er som regel noget der retter sig selv. */
        icon = ZS_ICON_ALERT;
        color = ZS_C_WARN;
        break;

    case ZS_LINK_NO_WIFI:
    default:
        icon = ZS_ICON_WIFI_OFF;
        color = ZS_C_BAD;
        break;
    }

    lv_label_set_text(sb->status_icon, icon);
    lv_obj_set_style_text_color(sb->status_icon, lv_color_hex(color), 0);
}
