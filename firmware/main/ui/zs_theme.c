/*
 * zScreen - designsystem. Se zs_theme.h for reglerne.
 */

#include "zs_theme.h"

#include <string.h>

/*
 * Stilarterne er static og bliver genbrugt.
 *
 * LVGL kopierer ikke en stil naar man saetter den paa et objekt, den
 * gemmer en pegepind. Derfor skal de leve lige saa laenge som skaermen,
 * og derfor bliver de sat op én gang i stedet for at hvert kort bygger
 * sin egen. Med fire kort, en liste med tyve netvaerk og et tastatur
 * med tredive taster ville det ellers blive til mange hundrede
 * ens kopier i hukommelsen.
 */
static lv_style_t s_card;
static lv_style_t s_card_pressed;
static lv_style_t s_btn_primary;
static lv_style_t s_btn_primary_pressed;
static lv_style_t s_btn_secondary;
static lv_style_t s_btn_secondary_pressed;
static lv_style_t s_row;
static lv_style_t s_row_pressed;

static bool s_inited = false;

void zs_style_text(lv_obj_t *obj, const lv_font_t *font, uint32_t color)
{
    if (font != NULL) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

/* Faellestraek for alt der ligner et kort eller en knap: ingen skygge,
 * intet forloeb, ingen kant vi ikke selv har bedt om. */
static void base_surface(lv_style_t *st, uint32_t bg, uint32_t border,
                         lv_coord_t radius, lv_coord_t border_w)
{
    lv_style_init(st);
    lv_style_set_bg_color(st, lv_color_hex(bg));
    lv_style_set_bg_opa(st, LV_OPA_COVER);
    lv_style_set_radius(st, radius);
    lv_style_set_border_width(st, border_w);
    if (border_w > 0) {
        lv_style_set_border_color(st, lv_color_hex(border));
        lv_style_set_border_opa(st, LV_OPA_COVER);
    }
    /* Ingen skygge og intet forloeb. Det er en bevidst beslutning, ikke
     * en forglemmelse: begge dele er de foerste ting der faar en flade
     * til at ligne noget der er genereret frem for tegnet. */
    lv_style_set_shadow_width(st, 0);
    lv_style_set_bg_grad_dir(st, LV_GRAD_DIR_NONE);
    lv_style_set_outline_width(st, 0);
}

void zs_theme_init(void)
{
    if (s_inited) {
        return;
    }
    s_inited = true;

    base_surface(&s_card, ZS_C_CARD, ZS_C_BORDER, ZS_CARD_RADIUS, 1);
    lv_style_set_pad_all(&s_card, ZS_CARD_PAD);

    lv_style_init(&s_card_pressed);
    lv_style_set_bg_color(&s_card_pressed, lv_color_hex(ZS_C_CARD_PRESSED));

    base_surface(&s_btn_primary, ZS_C_ACCENT, 0, 14, 0);
    lv_style_set_pad_hor(&s_btn_primary, 22);
    lv_style_set_text_color(&s_btn_primary, lv_color_hex(ZS_C_BG));
    lv_style_set_text_font(&s_btn_primary, &zs_font_20);

    lv_style_init(&s_btn_primary_pressed);
    /* Nedtonet i stedet for at skifte farve. Et tryk skal ses, ikke
     * fejres, og en knap der skifter kuloer virker som en anden knap. */
    lv_style_set_bg_opa(&s_btn_primary_pressed, LV_OPA_80);

    base_surface(&s_btn_secondary, ZS_C_BG, ZS_C_BORDER, 14, 1);
    lv_style_set_bg_opa(&s_btn_secondary, LV_OPA_TRANSP);
    lv_style_set_pad_hor(&s_btn_secondary, 22);
    lv_style_set_text_color(&s_btn_secondary, lv_color_hex(ZS_C_TEXT));
    lv_style_set_text_font(&s_btn_secondary, &zs_font_20);

    lv_style_init(&s_btn_secondary_pressed);
    lv_style_set_bg_color(&s_btn_secondary_pressed, lv_color_hex(ZS_C_CARD));
    lv_style_set_bg_opa(&s_btn_secondary_pressed, LV_OPA_COVER);

    base_surface(&s_row, ZS_C_CARD, ZS_C_BORDER, 14, 1);
    lv_style_set_pad_hor(&s_row, 16);
    lv_style_set_pad_ver(&s_row, 0);

    lv_style_init(&s_row_pressed);
    lv_style_set_bg_color(&s_row_pressed, lv_color_hex(ZS_C_CARD_PRESSED));

    /* Skaermens bund. Saettes her saa ingen skaerm glemmer det og
     * efterlader en sort kant der ikke passer til resten. */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

lv_obj_t *zs_card_create(lv_obj_t *parent, bool pressable)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &s_card, 0);
    if (pressable) {
        lv_obj_add_style(card, &s_card_pressed, LV_STATE_PRESSED);
    } else {
        /* Ikke bare uden tryk-stil: kortet maa heller ikke TAGE imod
         * tryk. Ellers spiser det bevaegelsen fra fingeren, og siden
         * kan ikke traekkes til side ovenpaa et kort. */
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    }
    /* Kortene skal ikke kunne rulles. Uden dette kan en finger der
     * glider en anelse under et tryk faa hele kortet til at vippe. */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    return card;
}

lv_obj_t *zs_label_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text != NULL ? text : "");
    zs_style_text(lbl, &zs_font_13, ZS_C_LABEL);
    /* Lidt luft mellem bogstaverne. Versaler i smaa stoerrelser bliver
     * ellers til en klump man skal laese to gange. */
    lv_obj_set_style_text_letter_space(lbl, 1, 0);
    return lbl;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                             lv_style_t *base, lv_style_t *pressed)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, base, 0);
    lv_obj_add_style(btn, pressed, LV_STATE_PRESSED);
    lv_obj_set_height(btn, ZS_BTN_HEIGHT);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text != NULL ? text : "");
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *zs_btn_primary_create(lv_obj_t *parent, const char *text)
{
    return make_button(parent, text, &s_btn_primary, &s_btn_primary_pressed);
}

lv_obj_t *zs_btn_secondary_create(lv_obj_t *parent, const char *text)
{
    return make_button(parent, text, &s_btn_secondary, &s_btn_secondary_pressed);
}

lv_obj_t *zs_column_create(lv_obj_t *parent, lv_coord_t gap)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, ZS_CONTENT_WIDTH);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, gap, 0);
    lv_obj_set_style_pad_ver(col, ZS_EDGE, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

void zs_row_create(zs_row_t *out, lv_obj_t *parent, const char *icon,
                   const char *title, const char *value, bool chevron)
{
    memset(out, 0, sizeof(*out));

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &s_row, 0);
    lv_obj_add_style(row, &s_row_pressed, LV_STATE_PRESSED);
    lv_obj_set_height(row, ZS_ROW_HEIGHT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    out->row = row;

    lv_coord_t x = 0;

    if (icon != NULL) {
        out->icon = lv_label_create(row);
        lv_label_set_text(out->icon, icon);
        lv_obj_set_style_text_font(out->icon, &zs_icons_20, 0);
        lv_obj_set_style_text_color(out->icon, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(out->icon, LV_ALIGN_LEFT_MID, 0, 0);
        x = ZS_ROW_ICON_W;
    }

    if (title != NULL) {
        out->title = lv_label_create(row);
        lv_label_set_text(out->title, title);
        zs_style_text(out->title, &zs_font_20, ZS_C_TEXT);
        lv_label_set_long_mode(out->title, LV_LABEL_LONG_DOT);
        lv_obj_align(out->title, LV_ALIGN_LEFT_MID, x, 0);

        /*
         * Bredden skal saettes, ellers vokser etiketten ud over raden
         * og et langt netvaerksnavn skriver hen over vaerdien til
         * hoejre. Vi traekker fra hvad der er optaget: ikonet til
         * venstre, og vaerdien og pilen til hoejre.
         */
        lv_coord_t right = 0;
        if (chevron)       { right += 30; }
        if (value != NULL) { right += 100; }
        lv_obj_set_width(out->title,
                         ZS_CONTENT_WIDTH - 2 * 16 - x - right);
    }

    lv_coord_t rx = 0;
    if (chevron) {
        out->chevron = lv_label_create(row);
        lv_label_set_text(out->chevron, ZS_ICON_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(out->chevron, &zs_icons_20, 0);
        lv_obj_set_style_text_color(out->chevron, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(out->chevron, LV_ALIGN_RIGHT_MID, 0, 0);
        rx = -30;
    }

    if (value != NULL) {
        out->value = lv_label_create(row);
        lv_label_set_text(out->value, value);
        zs_style_text(out->value, &zs_font_16, ZS_C_LABEL);
        lv_obj_align(out->value, LV_ALIGN_RIGHT_MID, rx, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Sidens ramme                                                        */
/* ------------------------------------------------------------------ */

void zs_page_create(zs_page_t *p, const char *title,
                    lv_event_cb_t back_cb, void *user_data,
                    bool with_footer)
{
    memset(p, 0, sizeof(*p));

    p->root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(p->root);
    lv_obj_set_size(p->root, ZS_SCR_WIDTH, ZS_SCR_HEIGHT);
    lv_obj_set_pos(p->root, 0, 0);
    lv_obj_clear_flag(p->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(p->root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(p->root, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(p->root, LV_OPA_COVER, 0);

    /* ── hoved ── */
    p->head = lv_obj_create(p->root);
    lv_obj_remove_style_all(p->head);
    lv_obj_set_size(p->head, ZS_SCR_WIDTH, ZS_PAGE_HEAD_HEIGHT);
    lv_obj_set_pos(p->head, 0, 0);
    lv_obj_clear_flag(p->head, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t title_x = ZS_EDGE + 4;

    if (back_cb != NULL) {
        /* Hele knappen er 44 x 44 selvom pilen er 20 px. Se noten om
         * fingerflader i zs_theme.h. */
        p->back = lv_btn_create(p->head);
        lv_obj_remove_style_all(p->back);
        lv_obj_set_size(p->back, ZS_TOUCH_MIN, ZS_TOUCH_MIN);
        lv_obj_set_pos(p->back, 4, (ZS_PAGE_HEAD_HEIGHT - ZS_TOUCH_MIN) / 2);
        lv_obj_add_event_cb(p->back, back_cb, LV_EVENT_CLICKED, user_data);

        lv_obj_t *ic = lv_label_create(p->back);
        lv_label_set_text(ic, ZS_ICON_ARROW_LEFT);
        lv_obj_set_style_text_font(ic, &zs_icons_20, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(ZS_C_TEXT), 0);
        lv_obj_center(ic);

        title_x = 4 + ZS_TOUCH_MIN + 8;
    }

    p->title = lv_label_create(p->head);
    lv_label_set_text(p->title, title != NULL ? title : "");
    zs_style_text(p->title, &zs_font_28, ZS_C_TEXT);
    lv_label_set_long_mode(p->title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(p->title, ZS_SCR_WIDTH - title_x - ZS_EDGE);
    lv_obj_align(p->title, LV_ALIGN_LEFT_MID, title_x, 0);

    /* ── fod ── */
    lv_coord_t foot_h = with_footer ? ZS_PAGE_FOOT_HEIGHT : 0;
    if (with_footer) {
        p->footer = lv_obj_create(p->root);
        lv_obj_remove_style_all(p->footer);
        lv_obj_set_size(p->footer, ZS_SCR_WIDTH, ZS_PAGE_FOOT_HEIGHT);
        lv_obj_set_pos(p->footer, 0, ZS_SCR_HEIGHT - ZS_PAGE_FOOT_HEIGHT);
        lv_obj_clear_flag(p->footer, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(p->footer, ZS_EDGE, 0);
    }

    /* ── indhold ── */
    p->content = lv_obj_create(p->root);
    lv_obj_remove_style_all(p->content);
    lv_obj_set_size(p->content, ZS_SCR_WIDTH,
                    ZS_SCR_HEIGHT - ZS_PAGE_HEAD_HEIGHT - foot_h);
    lv_obj_set_pos(p->content, 0, ZS_PAGE_HEAD_HEIGHT);
    /* Ingen luft i siderne. Se noten ved ZS_CONTENT_WIDTH: alt paa en
     * side bruger skaermens egne koordinater, saa der kun er ét sted at
     * regne fra. */
    lv_obj_set_style_pad_hor(p->content, 0, 0);
    lv_obj_set_style_pad_ver(p->content, 0, 0);
    /* Kun lodret rulning. Vandret ville betyde at et uheldigt strejf
     * kunne skubbe hele indholdet ud til siden. */
    lv_obj_set_scroll_dir(p->content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p->content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(p->content, LV_OPA_TRANSP, 0);

    /* Rullebjaelken skal kunne ses paa den moerke bund uden at fylde. */
    lv_obj_set_style_bg_color(p->content, lv_color_hex(ZS_C_BORDER), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(p->content, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(p->content, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(p->content, 2, LV_PART_SCROLLBAR);
}

void zs_page_set_hidden(zs_page_t *p, bool hidden)
{
    if (p == NULL || p->root == NULL) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    }
}
