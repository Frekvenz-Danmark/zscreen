#include "zs_screen_settings.h"
#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_app.h"
#include "zs_config.h"
#include "zs_format.h"
#include "zs_config.h"

#include <stdio.h>
#include <string.h>

static zs_page_t s_set, s_det;

static zs_row_t  s_row_wifi;
static zs_row_t  s_row_inv;
static lv_obj_t *s_slider;
static lv_obj_t *s_slider_val;
static lv_obj_t *s_sw_night;
static lv_obj_t *s_sw_meter;
static lv_obj_t *s_det_col;
static zs_row_t  s_row_demo;

/* ------------------------------------------------------------------ */
/* Smaa byggeklodser                                                   */
/* ------------------------------------------------------------------ */

static void send_simple(zs_cmd_type_t t)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = t;
    zs_app_send(&c);
}

/* En raekke med en kontakt til hoejre. */
static void make_switch_row(lv_obj_t *parent, const char *icon,
                            const char *title, const char *hint,
                            lv_event_cb_t cb, lv_obj_t **out_sw)
{
    zs_row_t r;
    zs_row_create(&r, parent, icon, title, NULL, false);
    lv_obj_t *row = r.row;
    lv_obj_set_width(row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_set_height(row, hint != NULL ? 76 : ZS_ROW_HEIGHT);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 52, 30);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_ACCENT),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_BG), LV_PART_KNOB);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (out_sw != NULL) {
        *out_sw = sw;
    }

    if (hint != NULL) {
        if (r.title != NULL) {
            lv_obj_align(r.title, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, -12);
        }
        lv_obj_t *h = lv_label_create(row);
        lv_label_set_text(h, hint);
        zs_style_text(h, &zs_font_16, ZS_C_LABEL);
        lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
        /* Bredden er raden minus luft, ikonet og kontakten til hoejre. */
        lv_obj_set_width(h, ZS_SCR_WIDTH - 2 * ZS_EDGE - 2 * 16
                            - ZS_ROW_ICON_W - 62);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, 14);
    }
}

/* En linje paa Detaljer-siden: etiket til venstre, vaerdi til hoejre. */
static void detail_line(lv_obj_t *parent, const char *label, const char *value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    zs_style_text(l, &zs_font_16, ZS_C_LABEL);
    lv_obj_set_width(l, 170);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, (value != NULL && value[0] != '\0') ? value : "-");
    zs_style_text(v, &zs_font_16, ZS_C_TEXT);
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(v, ZS_SCR_WIDTH - 2 * ZS_EDGE - 178);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, 178, 0);
}

static void detail_heading(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = zs_label_create(parent, text);
    lv_obj_set_style_pad_top(l, 14, 0);
}

/* ------------------------------------------------------------------ */
/* Indstillinger                                                       */
/* ------------------------------------------------------------------ */

static void on_set_back(lv_event_t *e)  { (void)e; zs_ui_show(ZS_SCREEN_HOME); }
static void on_det_back(lv_event_t *e)  { (void)e; zs_ui_show(ZS_SCREEN_SETTINGS); }

static void on_open_details(lv_event_t *e) { (void)e; zs_ui_show(ZS_SCREEN_DETAILS); }

static void on_change_wifi(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_SETUP_RESTART);
}

static void on_change_inverter(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_INVERTER_SCAN);
    send_simple(ZS_CMD_INVERTER_SCAN);
}

static void on_brightness(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int v = (int)lv_slider_get_value(sl);

    char t[8];
    snprintf(t, sizeof(t), "%d %%", v);
    lv_label_set_text(s_slider_val, t);

    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_BRIGHTNESS;
    c.u8 = (uint8_t)v;
    zs_app_send(&c);
}

static void on_night(lv_event_t *e)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_NIGHT_DIM;
    c.flag = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    zs_app_send(&c);
}

static void on_meter_sign(lv_event_t *e)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_METER_SIGN;
    /* Kontakten hedder "Byt køb og salg". Er den slaaet TIL, skal
     * fortegnet vendes, altsaa er positiv IKKE laengere køb. */
    c.flag = !lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    zs_app_send(&c);
}

static void on_reset_confirmed(lv_event_t *e)
{
    lv_obj_t *box = lv_event_get_current_target(e);
    uint16_t btn = lv_msgbox_get_active_btn(box);
    /* 0 er "Nulstil", 1 er "Fortryd". Se raekkefoelgen nedenfor. */
    if (btn == 0) {
        send_simple(ZS_CMD_FACTORY_RESET);
    }
    lv_msgbox_close(box);
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    /* En fabriksnulstilling sletter kundens wifi-kode og valgte
     * inverter. Den maa aldrig kunne ske ved et enkelt fejltryk. */
    static const char *btns[] = { "Nulstil", "Fortryd", "" };
    lv_obj_t *box = lv_msgbox_create(NULL, "Nulstil skærmen",
        "Alt bliver slettet, også wifi og den valgte inverter.\n\n"
        "Skærmen skal sættes op forfra bagefter.", btns, false);

    lv_obj_set_style_bg_color(box, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 16, 0);
    lv_obj_set_style_text_color(box, lv_color_hex(ZS_C_TEXT), 0);
    lv_obj_set_style_text_font(box, &zs_font_16, 0);
    lv_obj_set_width(box, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_center(box);

    lv_obj_t *bl = lv_msgbox_get_btns(box);
    lv_obj_set_style_bg_color(bl, lv_color_hex(ZS_C_BG), LV_PART_ITEMS);
    lv_obj_set_style_text_color(bl, lv_color_hex(ZS_C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(bl, &zs_font_20, LV_PART_ITEMS);
    lv_obj_set_style_radius(bl, 12, LV_PART_ITEMS);
    lv_obj_set_height(bl, 52);

    lv_obj_add_event_cb(box, on_reset_confirmed, LV_EVENT_VALUE_CHANGED, NULL);
}

#if ZS_DEMO_ENABLED
static void on_demo_stop(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_DEMO_STOP);
}
#endif

static void on_reboot(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_REBOOT);
}

static void build_settings(void)
{
    zs_page_create(&s_set, "Indstillinger", on_set_back, NULL, false);
    lv_obj_t *col = zs_column_create(s_set.content, 8);

    detail_heading(col, "ANLÆG");

    zs_row_create(&s_row_wifi, col, ZS_ICON_WIFI, "Netværk", "", true);
    lv_obj_set_width(s_row_wifi.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(s_row_wifi.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_wifi.row, on_change_wifi, LV_EVENT_CLICKED, NULL);

    zs_row_create(&s_row_inv, col, ZS_ICON_PLUG, "Inverter", "", true);
    lv_obj_set_width(s_row_inv.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(s_row_inv.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_inv.row, on_change_inverter, LV_EVENT_CLICKED, NULL);

    zs_row_t det;
    zs_row_create(&det, col, ZS_ICON_INFO, "Detaljer om anlægget", NULL, true);
    lv_obj_set_width(det.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(det.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(det.row, on_open_details, LV_EVENT_CLICKED, NULL);

    detail_heading(col, "SKÆRM");

    /* Lysstyrke. Skyderen er 40 px hoej og hele raden 76, saa den kan
     * rammes uden at sigte. */
    lv_obj_t *brow = lv_obj_create(col);
    lv_obj_remove_style_all(brow);
    lv_obj_set_size(brow, ZS_SCR_WIDTH - 2 * ZS_EDGE, 90);
    lv_obj_clear_flag(brow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(brow, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(brow, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(brow, 14, 0);
    lv_obj_set_style_border_width(brow, 1, 0);
    lv_obj_set_style_border_color(brow, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_pad_all(brow, 16, 0);

    lv_obj_t *bl = lv_label_create(brow);
    lv_label_set_text(bl, "Lysstyrke");
    zs_style_text(bl, &zs_font_20, ZS_C_TEXT);
    lv_obj_align(bl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_slider_val = lv_label_create(brow);
    lv_label_set_text(s_slider_val, "80 %");
    zs_style_text(s_slider_val, &zs_font_16, ZS_C_LABEL);
    lv_obj_align(s_slider_val, LV_ALIGN_TOP_RIGHT, 0, 2);

    s_slider = lv_slider_create(brow);
    lv_obj_set_size(s_slider, ZS_SCR_WIDTH - 2 * ZS_EDGE - 32, 10);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_LEFT, 0, -6);
    lv_slider_set_range(s_slider, 5, 100);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 10, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    make_switch_row(col, ZS_ICON_POWER, "Dæmp om natten",
                    "Skærmen lyser mindre mellem 22 og 6", on_night, &s_sw_night);

    detail_heading(col, "ELMÅLER");

    make_switch_row(col, ZS_ICON_ZAP, "Byt køb og salg",
                    "Slå til hvis NETTET viser køb når du sælger",
                    on_meter_sign, &s_sw_meter);

    detail_heading(col, "SKÆRMEN");

#if ZS_DEMO_ENABLED
    /* Kun synlig mens demoen koerer. Se zs_settings_set_demo. */
    zs_row_create(&s_row_demo, col, ZS_ICON_CLOSE, "Afslut demo", NULL, true);
    lv_obj_set_width(s_row_demo.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_demo.row, on_demo_stop, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    if (s_row_demo.title != NULL) {
        lv_obj_set_style_text_color(s_row_demo.title, lv_color_hex(ZS_C_ACCENT), 0);
    }
#endif

    zs_row_t rb;
    zs_row_create(&rb, col, ZS_ICON_REFRESH, "Genstart", NULL, true);
    lv_obj_set_width(rb.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(rb.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rb.row, on_reboot, LV_EVENT_CLICKED, NULL);

    zs_row_t fr;
    zs_row_create(&fr, col, ZS_ICON_ROTATE,
                  "Nulstil til fabriksindstillinger", NULL, true);
    lv_obj_set_width(fr.row, ZS_SCR_WIDTH - 2 * ZS_EDGE);
    lv_obj_add_flag(fr.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fr.row, on_reset, LV_EVENT_CLICKED, NULL);
    /* Den eneste røde tekst i hele fladen. Den skal skille sig ud. */
    if (fr.title != NULL) {
        lv_obj_set_style_text_color(fr.title, lv_color_hex(ZS_C_BAD), 0);
    }

    lv_obj_t *ver = lv_label_create(col);
    lv_label_set_text(ver, ZS_PRODUCT_NAME " " ZS_VERSION);
    zs_style_text(ver, &zs_font_16, ZS_C_STALE);
    lv_obj_set_style_pad_top(ver, 16, 0);
    lv_obj_set_style_pad_bottom(ver, 16, 0);
}

/* ------------------------------------------------------------------ */
/* Detaljer                                                            */
/* ------------------------------------------------------------------ */

static void build_details(void)
{
    zs_page_create(&s_det, "Detaljer", on_det_back, NULL, false);
    s_det_col = zs_column_create(s_det.content, 6);
}

/* ------------------------------------------------------------------ */

void zs_settings_create(void)
{
    build_settings();
    build_details();
}

lv_obj_t *zs_settings_root(void) { return s_set.root; }
lv_obj_t *zs_details_root(void)  { return s_det.root; }

void zs_settings_update(const zs_settings_t *s, const char *ip)
{
    if (s == NULL) {
        return;
    }
    if (s_row_wifi.value != NULL) {
        lv_label_set_text(s_row_wifi.value,
                          s->wifi_ssid[0] ? s->wifi_ssid : "Ikke valgt");
    }
    if (s_row_inv.value != NULL) {
        lv_label_set_text(s_row_inv.value,
                          s->inverter_ip[0] ? s->inverter_ip : "Ikke valgt");
    }

    lv_slider_set_value(s_slider, s->brightness, LV_ANIM_OFF);
    char t[8];
    snprintf(t, sizeof(t), "%u %%", (unsigned)s->brightness);
    lv_label_set_text(s_slider_val, t);

    if (s->night_dimming) {
        lv_obj_add_state(s_sw_night, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_night, LV_STATE_CHECKED);
    }
    /* Kontakten hedder "Byt køb og salg", saa den er slaaet TIL naar
     * fortegnet IKKE er standard. */
    if (!s->meter_import_positive) {
        lv_obj_add_state(s_sw_meter, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_meter, LV_STATE_CHECKED);
    }
    (void)ip;
}

void zs_settings_set_demo(bool demo)
{
#if ZS_DEMO_ENABLED
    if (s_row_demo.row == NULL) {
        return;
    }
    if (demo) {
        lv_obj_clear_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    }
#else
    (void)demo;
#endif
}

void zs_details_update(const zs_fr_t *fr, const char *own_ip, int rssi)
{
    if (s_det_col == NULL) {
        return;
    }
    lv_obj_clean(s_det_col);

    char buf[96];

    detail_heading(s_det_col, "INVERTER");
    if (fr != NULL && fr->info.manufacturer[0] != '\0') {
        detail_line(s_det_col, "Fabrikat", fr->info.manufacturer);
        detail_line(s_det_col, "Model", fr->info.model);
        detail_line(s_det_col, "Firmware", fr->info.version);
        detail_line(s_det_col, "Serienummer", fr->info.serial);

        snprintf(buf, sizeof(buf), "%s:%u", fr->host, fr->port);
        detail_line(s_det_col, "Adresse", buf);

        snprintf(buf, sizeof(buf), "%u", fr->inverter_unit);
        detail_line(s_det_col, "Modbus-enhed", buf);

        snprintf(buf, sizeof(buf), "%u%s", fr->info.inverter_model_id,
                 fr->info.inverter_model_id >= 111 ? " (flydende tal)"
                                                   : " (heltal)");
        detail_line(s_det_col, "SunSpec-model", buf);

        if (fr->info.inverter_rated_kw > 0.0f) {
            snprintf(buf, sizeof(buf), "%.1f kW",
                     (double)fr->info.inverter_rated_kw);
            zs_fmt_da_decimal(buf);
            detail_line(s_det_col, "Mærkeeffekt", buf);
        }
    } else {
        detail_line(s_det_col, "Status", "Ikke forbundet");
    }

    if (fr != NULL) {
        detail_heading(s_det_col, "ELMÅLER");
        if (fr->info.has_meter) {
            snprintf(buf, sizeof(buf), "Model %u på enhed %u",
                     fr->info.meter_model_id, fr->info.meter_unit);
            detail_line(s_det_col, "Fundet", buf);
            detail_line(s_det_col, "Placering",
                        fr->meter_in_inverter_chain ? "I inverterens egen liste"
                                                    : "Egen Modbus-enhed");
            detail_line(s_det_col, "Fortegn",
                        fr->meter_import_positive ? "Positiv betyder køb"
                                                  : "Positiv betyder salg");
            if (fr->negative_house_count > 0) {
                snprintf(buf, sizeof(buf), "%u gange", (unsigned)fr->negative_house_count);
                detail_line(s_det_col, "Negativt forbrug", buf);
                lv_obj_t *w = lv_label_create(s_det_col);
                lv_label_set_text(w,
                    "Forbruget er blevet regnet negativt. Det kan ikke lade sig "
                    "gøre, så elmålerens fortegn er sandsynligvis vendt. "
                    "Prøv at slå \"Byt køb og salg\" til under Indstillinger.");
                zs_style_text(w, &zs_font_16, ZS_C_WARN);
                lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(w, ZS_SCR_WIDTH - 2 * ZS_EDGE);
            }
        } else {
            detail_line(s_det_col, "Fundet", "Ingen");
            lv_obj_t *w = lv_label_create(s_det_col);
            lv_label_set_text(w,
                "Uden elmåler kan skærmen ikke vise forbrug eller køb og salg. "
                "Solceller og batteri virker som normalt.");
            zs_style_text(w, &zs_font_16, ZS_C_LABEL);
            lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(w, ZS_SCR_WIDTH - 2 * ZS_EDGE);
        }

        detail_heading(s_det_col, "BATTERI");
        if (fr->info.has_battery) {
            if (fr->info.battery_capacity_kwh > 0.0f) {
                snprintf(buf, sizeof(buf), "%.2f kWh",
                         (double)fr->info.battery_capacity_kwh);
                zs_fmt_da_decimal(buf);
                detail_line(s_det_col, "Kapacitet", buf);
            }
            detail_line(s_det_col, "Kanalnavne",
                        fr->info.labels_usable
                            ? "Inverteren navngiver sine kanaler"
                            : "Uden navne, opdelt efter Fronius' mønster");
        } else {
            detail_line(s_det_col, "Fundet", "Intet batteri");
        }

        detail_heading(s_det_col, "SUNSPEC");
        snprintf(buf, sizeof(buf), "%u", fr->inv_map.base);
        detail_line(s_det_col, "Startadresse", buf);

        size_t p = 0;
        buf[0] = '\0';
        for (uint8_t i = 0; i < fr->inv_map.count && p + 8 < sizeof(buf); i++) {
            p += (size_t)snprintf(buf + p, sizeof(buf) - p, "%s%u",
                                  i ? ", " : "", fr->inv_map.models[i].id);
        }
        detail_line(s_det_col, "Modeller", buf);
        if (fr->inv_map.truncated) {
            detail_line(s_det_col, "Bemærk", "Listen kunne ikke læses færdig");
        }

        detail_heading(s_det_col, "AFLÆSNING");
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->poll_count);
        detail_line(s_det_col, "Aflæsninger", buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->poll_error_count);
        detail_line(s_det_col, "Fejlede", buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->reconnect_count);
        detail_line(s_det_col, "Genforbindelser", buf);
        snprintf(buf, sizeof(buf), "%u sendt, %u fejl",
                 (unsigned)fr->mb.stat_requests, (unsigned)fr->mb.stat_errors);
        detail_line(s_det_col, "Modbus", buf);
    }

    detail_heading(s_det_col, "SKÆRMEN");
    detail_line(s_det_col, "IP-adresse", own_ip);
    if (rssi != 0) {
        snprintf(buf, sizeof(buf), "%d dBm", rssi);
        detail_line(s_det_col, "Wifi-signal", buf);
    }
    detail_line(s_det_col, "Version", ZS_PRODUCT_NAME " " ZS_VERSION);

    /* Lidt luft i bunden, saa den sidste linje ikke klistrer til kanten
     * naar man har rullet helt ned. */
    lv_obj_t *pad = lv_obj_create(s_det_col);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, 10, 24);
}
