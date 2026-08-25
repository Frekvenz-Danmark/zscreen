#include "zs_status_page.h"
#include "zs_theme.h"
#include "zs_status.h"

#include <stdio.h>
#include <string.h>

/*
 * Maal. Siden er 480 x 408.
 *
 *   feltet foroven   y  12 .. 148   (136 hoejt)
 *   listen           y 160 .. 396   (236 hoejt, kan rulles)
 */
#define HERO_Y        12
#define HERO_H        136
#define LIST_Y        160
#define LIST_H        236

/* Farve og ikon efter hvor slemt det er. */
static uint32_t sev_farve(zs_sev_t s)
{
    switch (s) {
    case ZS_SEV_FAULT: return ZS_C_BAD;
    case ZS_SEV_WARN:  return ZS_C_WARN;
    case ZS_SEV_INFO:  return ZS_C_LABEL;
    case ZS_SEV_OK:
    default:           return ZS_C_GOOD;
    }
}

static const char *sev_ikon(zs_sev_t s)
{
    switch (s) {
    case ZS_SEV_FAULT: return ZS_ICON_CIRCLE_ALERT;
    case ZS_SEV_WARN:  return ZS_ICON_ALERT;
    case ZS_SEV_INFO:  return ZS_ICON_INFO;
    case ZS_SEV_OK:
    default:           return ZS_ICON_CIRCLE_CHECK;
    }
}

void zs_status_page_create(zs_status_page_t *p, lv_obj_t *parent)
{
    memset(p, 0, sizeof(*p));
    p->page = parent;

    /* ── feltet foroven ── */
    p->hero = zs_card_create(parent, false);
    lv_obj_set_size(p->hero, ZS_CONTENT_WIDTH, HERO_H);
    lv_obj_set_pos(p->hero, ZS_EDGE, HERO_Y);

    p->hero_icon = lv_label_create(p->hero);
    lv_label_set_text(p->hero_icon, ZS_ICON_CIRCLE_CHECK);
    lv_obj_set_style_text_font(p->hero_icon, &zs_icons_28, 0);
    lv_obj_set_style_text_color(p->hero_icon, lv_color_hex(ZS_C_GOOD), 0);
    lv_obj_align(p->hero_icon, LV_ALIGN_TOP_MID, 0, 6);

    p->hero_title = lv_label_create(p->hero);
    lv_label_set_text(p->hero_title, "");
    zs_style_text(p->hero_title, &zs_font_28, ZS_C_TEXT);
    lv_obj_set_style_text_align(p->hero_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(p->hero_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(p->hero_title, ZS_CONTENT_WIDTH - 2 * ZS_CARD_PAD);
    lv_obj_align(p->hero_title, LV_ALIGN_TOP_MID, 0, 44);

    p->hero_sub = lv_label_create(p->hero);
    lv_label_set_text(p->hero_sub, "");
    zs_style_text(p->hero_sub, &zs_font_16, ZS_C_LABEL);
    lv_obj_set_style_text_align(p->hero_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(p->hero_sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p->hero_sub, ZS_CONTENT_WIDTH - 2 * ZS_CARD_PAD);
    lv_obj_align(p->hero_sub, LV_ALIGN_TOP_MID, 0, 82);

    /* ── listen ── */
    p->list = lv_obj_create(parent);
    lv_obj_remove_style_all(p->list);
    lv_obj_set_size(p->list, ZS_SCR_WIDTH, LIST_H);
    lv_obj_set_pos(p->list, 0, LIST_Y);
    lv_obj_set_style_pad_hor(p->list, ZS_EDGE, 0);
    lv_obj_set_flex_flow(p->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p->list, 8, 0);
    /* Kun lodret rulning. Vandret ville staa i vejen for at traekke
     * mellem siderne. */
    lv_obj_set_scroll_dir(p->list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p->list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(p->list, lv_color_hex(ZS_C_BORDER), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(p->list, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(p->list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(p->list, 2, LV_PART_SCROLLBAR);
}

/* Én fejl i listen: ikon, tekst, og den raa kode nedenunder. */
static void tilfoej_raekke(lv_obj_t *parent, const zs_status_item_t *it)
{
    lv_obj_t *row = zs_card_create(parent, false);
    lv_obj_set_width(row, ZS_CONTENT_WIDTH);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_set_style_radius(row, 12, 0);

    uint32_t farve = sev_farve(it->sev);

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, sev_ikon(it->sev));
    lv_obj_set_style_text_font(ic, &zs_icons_20, 0);
    lv_obj_set_style_text_color(ic, lv_color_hex(farve), 0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 1);

    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, it->tekst);
    zs_style_text(t, &zs_font_20, ZS_C_TEXT);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, ZS_CONTENT_WIDTH - 24 - 30);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 30, 0);

    if (it->detalje[0] != '\0') {
        lv_obj_t *d = lv_label_create(row);
        lv_label_set_text(d, it->detalje);
        zs_style_text(d, &zs_font_16, ZS_C_LABEL);
        lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(d, ZS_CONTENT_WIDTH - 24 - 30);
        lv_obj_align_to(d, t, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    }
}

void zs_status_page_update(zs_status_page_t *p, const zs_home_data_t *d)
{
    if (p == NULL || d == NULL || p->page == NULL) {
        return;
    }

    if (!d->have_data) {
        lv_label_set_text(p->hero_icon, ZS_ICON_SPINNER);
        lv_obj_set_style_text_color(p->hero_icon, lv_color_hex(ZS_C_LABEL), 0);
        lv_label_set_text(p->hero_title, "Henter");
        lv_obj_set_style_text_color(p->hero_title, lv_color_hex(ZS_C_STALE), 0);
        lv_label_set_text(p->hero_sub, "Venter på svar fra inverteren");
        lv_obj_clean(p->list);
        return;
    }

    zs_status_list_t liste;
    zs_status_build(&liste, &d->live);

    /* ── feltet foroven ── */
    zs_sev_t vis = liste.har_svar ? liste.vaerst : ZS_SEV_INFO;
    if (liste.har_svar && liste.vaerst <= ZS_SEV_INFO) {
        vis = ZS_SEV_OK;
    }
    lv_label_set_text(p->hero_icon, sev_ikon(vis));
    lv_obj_set_style_text_color(p->hero_icon, lv_color_hex(sev_farve(vis)), 0);

    lv_label_set_text(p->hero_title, zs_status_summary(&liste, &d->live));
    lv_obj_set_style_text_color(p->hero_title,
        lv_color_hex(d->stale ? ZS_C_STALE : ZS_C_TEXT), 0);

    char sub[96];
    if (!liste.har_svar) {
        snprintf(sub, sizeof(sub),
                 "Inverteren udfylder ikke felterne til tilstand og fejl");
    } else if (liste.vaerst >= ZS_SEV_WARN) {
        snprintf(sub, sizeof(sub), "%s  ·  %u %s",
                 zs_status_state_text(d->live.inverter_state),
                 (unsigned)liste.antal,
                 liste.antal == 1 ? "melding" : "meldinger");
    } else {
        snprintf(sub, sizeof(sub), "Inverteren: %s",
                 zs_status_state_text(d->live.inverter_state));
    }
    lv_label_set_text(p->hero_sub, sub);

    /* ── listen ── */
    lv_obj_clean(p->list);

    if (liste.antal == 0) {
        lv_obj_t *tom = lv_label_create(p->list);
        lv_label_set_text(tom, "Ingen meldinger fra inverteren.");
        zs_style_text(tom, &zs_font_16, ZS_C_LABEL);
        lv_obj_set_style_text_align(tom, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(tom, ZS_CONTENT_WIDTH);
        lv_obj_set_style_pad_top(tom, 20, 0);
        return;
    }

    for (uint8_t i = 0; i < liste.antal; i++) {
        tilfoej_raekke(p->list, &liste.poster[i]);
    }
    if (liste.afkortet) {
        lv_obj_t *mere = lv_label_create(p->list);
        lv_label_set_text(mere, "Der er flere meldinger end der er plads til.");
        zs_style_text(mere, &zs_font_16, ZS_C_WARN);
        lv_label_set_long_mode(mere, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(mere, ZS_CONTENT_WIDTH);
    }
}
