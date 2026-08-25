#include "zs_tile.h"
#include "zs_theme.h"
#include "zs_format.h"

#include <stdio.h>
#include <string.h>

/*
 * Enheden skal staa paa samme grundlinje som tallet.
 *
 * De to skrifttyper har hver sin stoerrelse, saa hvis man bare saetter
 * dem ved siden af hinanden og stiller dem op efter overkanten, kommer
 * "kW" til at svaeve. LVGL fortaeller hvor grundlinjen ligger i hver
 * skrifttype, saa vi regner forskellen ud i stedet for at skrive et
 * tal ind der holder op med at passe naar en stoerrelse aendres.
 */
static lv_coord_t baseline_offset(const lv_font_t *big, const lv_font_t *small)
{
    lv_coord_t b_big   = big->line_height   - big->base_line;
    lv_coord_t b_small = small->line_height - small->base_line;
    lv_coord_t d = b_big - b_small;
    return d > 0 ? d : 0;
}

void zs_tile_create(zs_tile_t *t, lv_obj_t *parent, int col, int row,
                    const char *label, const char *icon)
{
    memset(t, 0, sizeof(*t));

    /* false: de fire kasser er til at LAESE, ikke til at trykke paa. */
    t->card = zs_card_create(parent, false);
    lv_obj_set_size(t->card, ZS_CARD_WIDTH, ZS_CARD_HEIGHT);
    /* Placeringen er i forhold til SIDEN, ikke til skaermen. Siden
     * ligger allerede under statuslinjen, saa den maa ikke laegges til
     * her ogsaa. */
    lv_obj_set_pos(t->card,
                   ZS_EDGE + col * (ZS_CARD_WIDTH + ZS_GRID_GAP),
                   ZS_EDGE + row * (ZS_CARD_HEIGHT + ZS_GRID_GAP));

    /* ── overskrift ── */
    t->head_label = zs_label_create(t->card, label);
    lv_obj_align(t->head_label, LV_ALIGN_TOP_LEFT, 0, ZS_CARD_HEAD_Y + 2);

    t->head_icon = lv_label_create(t->card);
    lv_label_set_text(t->head_icon, icon != NULL ? icon : "");
    lv_obj_set_style_text_font(t->head_icon, &zs_icons_20, 0);
    lv_obj_set_style_text_color(t->head_icon, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_align(t->head_icon, LV_ALIGN_TOP_RIGHT, 0, ZS_CARD_HEAD_Y);

    /* ── stort tal og enhed ── */
    t->value_box = lv_obj_create(t->card);
    lv_obj_remove_style_all(t->value_box);
    lv_obj_set_size(t->value_box, ZS_CARD_IN_WIDTH, ZS_CARD_VALUE_HEIGHT);
    lv_obj_align(t->value_box, LV_ALIGN_TOP_LEFT, 0, ZS_CARD_VALUE_Y);
    lv_obj_clear_flag(t->value_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(t->value_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(t->value_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t->value_box, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(t->value_box, ZS_UNIT_GAP, 0);

    t->value = lv_label_create(t->value_box);
    lv_label_set_text(t->value, "-");
    zs_style_text(t->value, &zs_font_num_64, ZS_C_TEXT);

    t->unit = lv_label_create(t->value_box);
    lv_label_set_text(t->unit, "");
    zs_style_text(t->unit, &zs_font_28, ZS_C_LABEL);
    lv_obj_set_style_translate_y(t->unit,
                                 baseline_offset(&zs_font_num_64, &zs_font_28), 0);

    /* ── undertekst ── */
    t->sub_icon = lv_label_create(t->card);
    lv_label_set_text(t->sub_icon, "");
    lv_obj_set_style_text_font(t->sub_icon, &zs_icons_20, 0);
    lv_obj_set_style_text_color(t->sub_icon, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_align(t->sub_icon, LV_ALIGN_TOP_LEFT, 0, ZS_CARD_SUB_Y - 1);

    t->sub_text = lv_label_create(t->card);
    lv_label_set_text(t->sub_text, "");
    zs_style_text(t->sub_text, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(t->sub_text, LV_LABEL_LONG_DOT);
    lv_obj_align(t->sub_text, LV_ALIGN_TOP_LEFT, 0, ZS_CARD_SUB_Y);
    lv_obj_set_width(t->sub_text, ZS_CARD_IN_WIDTH);
}

/* Faelles for de to saettere: skriv tal og enhed, og vaelg farven. */
static void apply_value(zs_tile_t *t, const zs_num_t *n, bool active)
{
    lv_label_set_text(t->value, n->value);
    lv_label_set_text(t->unit, n->unit);

    /* ZS_C_VALUE, ikke ZS_C_ACCENT. I moerkt tema er de den samme
     * orange. I lyst tema er tallet moerkegroent og accenten orange,
     * fordi orangen ikke kan laeses mod hvid. */
    uint32_t col = t->stale ? ZS_C_STALE
                 : active   ? ZS_C_VALUE
                            : ZS_C_TEXT;
    lv_obj_set_style_text_color(t->value, lv_color_hex(col), 0);
    lv_obj_set_style_text_color(t->unit,
        lv_color_hex(t->stale ? ZS_C_STALE : ZS_C_LABEL), 0);
}

void zs_tile_set_power(zs_tile_t *t, zs_val_t watt, bool active)
{
    zs_num_t n;
    if (!watt.ok) {
        zs_fmt_none(&n);
        active = false;
    } else {
        zs_fmt_power(watt.v, &n);
    }
    apply_value(t, &n, active);
}

void zs_tile_set_percent(zs_tile_t *t, zs_val_t pct, bool active)
{
    zs_num_t n;
    if (!pct.ok) {
        zs_fmt_none(&n);
        active = false;
    } else {
        zs_fmt_percent(pct.v, &n);
    }
    apply_value(t, &n, active);
}

void zs_tile_set_none(zs_tile_t *t, const char *why)
{
    zs_num_t n;
    zs_fmt_none(&n);
    apply_value(t, &n, false);
    zs_tile_set_sub(t, NULL, why, ZS_C_LABEL);
}

void zs_tile_set_sub(zs_tile_t *t, const char *icon, const char *text,
                     uint32_t color)
{
    uint32_t c = t->stale ? ZS_C_STALE : color;

    lv_label_set_text(t->sub_icon, icon != NULL ? icon : "");
    lv_obj_set_style_text_color(t->sub_icon, lv_color_hex(c), 0);

    lv_label_set_text(t->sub_text, text != NULL ? text : "");
    lv_obj_set_style_text_color(t->sub_text, lv_color_hex(c), 0);

    /* Teksten rykker til side for ikonet naar der er ét, og helt ud til
     * kanten naar der ikke er. Ellers ville alle underteksterne staa
     * med et hul foran sig paa de kort der ikke bruger ikonet. */
    lv_coord_t x = (icon != NULL && icon[0] != '\0') ? (20 + 6) : 0;
    lv_obj_align(t->sub_text, LV_ALIGN_TOP_LEFT, x, ZS_CARD_SUB_Y);
    lv_obj_set_width(t->sub_text, ZS_CARD_IN_WIDTH - x);
}

void zs_tile_set_stale(zs_tile_t *t, bool stale)
{
    if (t->stale == stale) {
        return;
    }
    t->stale = stale;

    uint32_t val_col = stale ? ZS_C_STALE : ZS_C_TEXT;
    lv_obj_set_style_text_color(t->value, lv_color_hex(val_col), 0);
    lv_obj_set_style_text_color(t->unit,
        lv_color_hex(stale ? ZS_C_STALE : ZS_C_LABEL), 0);
    lv_obj_set_style_text_color(t->head_label,
        lv_color_hex(stale ? ZS_C_STALE : ZS_C_LABEL), 0);
    lv_obj_set_style_text_color(t->head_icon,
        lv_color_hex(stale ? ZS_C_STALE : ZS_C_LABEL), 0);
    lv_obj_set_style_text_color(t->sub_text, lv_color_hex(ZS_C_STALE), 0);
    lv_obj_set_style_text_color(t->sub_icon, lv_color_hex(ZS_C_STALE), 0);
}
