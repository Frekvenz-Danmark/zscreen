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

lv_obj_t *zs_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &s_card, 0);
    lv_obj_add_style(card, &s_card_pressed, LV_STATE_PRESSED);
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

lv_obj_t *zs_row_create(lv_obj_t *parent, const char *icon,
                        const char *title, const char *value, bool chevron)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &s_row, 0);
    lv_obj_add_style(row, &s_row_pressed, LV_STATE_PRESSED);
    lv_obj_set_height(row, ZS_ROW_HEIGHT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    lv_coord_t x = 0;

    if (icon != NULL) {
        lv_obj_t *ic = lv_label_create(row);
        lv_label_set_text(ic, icon);
        lv_obj_set_style_text_font(ic, &zs_icons_20, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);
        x = 20 + 14;   /* ikonets bredde plus luft */
    }

    if (title != NULL) {
        lv_obj_t *tl = lv_label_create(row);
        lv_label_set_text(tl, title);
        zs_style_text(tl, &zs_font_20, ZS_C_TEXT);
        lv_label_set_long_mode(tl, LV_LABEL_LONG_DOT);
        lv_obj_align(tl, LV_ALIGN_LEFT_MID, x, 0);

        /*
         * Bredden skal saettes, ellers vokser etiketten ud over raden og
         * en lang netvaerksnavn skriver hen over vaerdien til hoejre.
         * Vi trraekker fra hvad der er optaget: ikon til venstre, samt
         * vaerdi og pil til hoejre.
         */
        lv_coord_t right = 0;
        if (chevron)      { right += 20 + 10; }
        if (value != NULL) { right += 90 + 10; }
        lv_obj_set_width(tl, ZS_SCR_WIDTH - 2 * ZS_EDGE - 2 * 16 - x - right);
    }

    lv_coord_t rx = 0;
    if (chevron) {
        lv_obj_t *ch = lv_label_create(row);
        lv_label_set_text(ch, ZS_ICON_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(ch, &zs_icons_20, 0);
        lv_obj_set_style_text_color(ch, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(ch, LV_ALIGN_RIGHT_MID, 0, 0);
        rx = -(20 + 10);
    }

    if (value != NULL) {
        lv_obj_t *vl = lv_label_create(row);
        lv_label_set_text(vl, value);
        zs_style_text(vl, &zs_font_16, ZS_C_LABEL);
        lv_obj_align(vl, LV_ALIGN_RIGHT_MID, rx, 0);
    }

    return row;
}
