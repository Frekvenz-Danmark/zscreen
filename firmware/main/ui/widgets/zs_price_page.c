#include "zs_price_page.h"
#include "zs_theme.h"
#include "zs_format.h"

#include <stdio.h>
#include <string.h>

/*
 * Maal. Siden er 480 x 408, og hvert tal er regnet ud af
 * skrifttypernes faktiske linjehoejder:
 *
 *   13 px skrift ->  16 px linje
 *   16 px skrift ->  18 px linje
 *   28 px skrift ->  31 px linje
 *   64 px tal    ->  54 px linje  (kun cifre, ingen underlaengder)
 *
 *   etiket      y  14 ..  30    16
 *   stort tal   y  36 ..  90    54
 *   ord         y  94 .. 118    24
 *   soejler     y 132 .. 236   104
 *   timeakse    y 240 .. 256    16
 *   to kort     y 268 .. 368   100
 *   note        y 380 .. 398    18   (10 px luft til sidens kant)
 *
 * Kortene var 80 px hoeje. Indvendigt gav det 56, men indholdet
 * kraever 73, saa klokkeslaettet stak 14 px ud under kassen. Hoejden
 * regnes nu ud af skrifttyperne i stedet for at blive skrevet ind, og
 * der er et tjek ved opstart der siger fra hvis det alligevel ikke
 * passer.
 */
#define Y_LABEL      14
#define Y_VALUE      36
#define Y_WORD       94
#define Y_CHART      132
#define CHART_H      104
#define Y_AXIS       240
#define Y_CARDS      268
#define CARDS_H      100
#define Y_NOTE       380

/* Luft mellem de tre linjer inde i et kort. */
#define CARD_LINE_GAP 4

#define BAR_GAP      4
#define BAR_MIN_H    3      /* saa en time til nul kroner stadig kan ses */

#define CARD_W       ((ZS_CONTENT_WIDTH - 12) / 2)
#define CARD_PAD     12

/* Hvad de tre linjer kraever i alt, regnet ud af skrifttyperne selv.
 * Aendrer nogen en skriftstoerrelse, foelger hoejden med. */
static lv_coord_t card_content_height(void)
{
    return zs_font_13.line_height + CARD_LINE_GAP
         + zs_font_28.line_height + CARD_LINE_GAP
         + zs_font_16.line_height;
}

void zs_price_page_create(zs_price_page_t *p, lv_obj_t *parent)
{
    memset(p, 0, sizeof(*p));
    p->page = parent;

    p->label = zs_label_create(parent, "SPOTPRIS NU");
    lv_obj_set_pos(p->label, ZS_EDGE, Y_LABEL);

    p->zone = zs_label_create(parent, "");
    lv_obj_set_style_text_align(p->zone, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(p->zone, 80);
    lv_obj_set_pos(p->zone, ZS_SCR_WIDTH - ZS_EDGE - 80, Y_LABEL);

    /* Prisen og enheden staar paa samme grundlinje, som paa forsiden. */
    p->value = lv_label_create(parent);
    lv_label_set_text(p->value, "-");
    zs_style_text(p->value, &zs_font_num_64, ZS_C_VALUE);
    lv_obj_set_pos(p->value, ZS_EDGE, Y_VALUE);

    p->unit = lv_label_create(parent);
    lv_label_set_text(p->unit, "");
    zs_style_text(p->unit, &zs_font_28, ZS_C_LABEL);
    /* Samme udregning som i zs_tile.c: forskellen paa de to
     * skrifttypers grundlinjer. */
    lv_obj_set_pos(p->unit, ZS_EDGE, Y_VALUE
        + (zs_font_num_64.line_height - zs_font_num_64.base_line)
        - (zs_font_28.line_height - zs_font_28.base_line));

    p->word = lv_label_create(parent);
    lv_label_set_text(p->word, "");
    zs_style_text(p->word, &zs_font_20, ZS_C_LABEL);
    lv_obj_set_pos(p->word, ZS_EDGE, Y_WORD);

    /* ── soejlerne ── */
    p->chart = lv_obj_create(parent);
    lv_obj_remove_style_all(p->chart);
    lv_obj_set_size(p->chart, ZS_SCR_WIDTH, CHART_H);
    lv_obj_set_pos(p->chart, 0, Y_CHART);
    lv_obj_clear_flag(p->chart, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < ZS_PRICE_BARS; i++) {
        p->bar[i] = lv_obj_create(p->chart);
        lv_obj_remove_style_all(p->bar[i]);
        lv_obj_set_style_radius(p->bar[i], 2, 0);
        lv_obj_set_style_bg_opa(p->bar[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(p->bar[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(p->bar[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Timetallene under. Kun fire, ellers bliver det en grød. */
    for (int i = 0; i < 4; i++) {
        p->axis[i] = lv_label_create(parent);
        lv_label_set_text(p->axis[i], "");
        zs_style_text(p->axis[i], &zs_font_13, ZS_C_STALE);
        lv_obj_set_style_text_align(p->axis[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(p->axis[i], 40);
        lv_obj_set_y(p->axis[i], Y_AXIS);
    }

    /* ── billigst og dyrest ── */
    struct { lv_obj_t **kort, **vaerdi, **tid; const char *tekst; lv_coord_t x; }
    k[2] = {
        { &p->low_card,  &p->low_value,  &p->low_time,  "BILLIGST", ZS_EDGE },
        { &p->high_card, &p->high_value, &p->high_time, "DYREST",
          ZS_EDGE + CARD_W + 12 },
    };
    /* Linjerne stables ud fra skrifttypernes egne hoejder, ikke ud fra
     * tal skrevet i haanden. Saa kan de ikke komme til at overlappe
     * eller staa uden for kassen naar en stoerrelse aendres. */
    lv_coord_t y_label = 0;
    lv_coord_t y_value = y_label + zs_font_13.line_height + CARD_LINE_GAP;
    lv_coord_t y_time  = y_value + zs_font_28.line_height + CARD_LINE_GAP;

    lv_coord_t indvendigt = CARDS_H - 2 * CARD_PAD;
    if (card_content_height() > indvendigt) {
        /* Sker kun hvis nogen aendrer en skriftstoerrelse uden at
         * aendre CARDS_H. Saa staar det i loggen med det samme i
         * stedet for at blive opdaget paa en skaerm hos en kunde. */
        LV_LOG_ERROR("prisikort: indholdet kraever %d px, kassen har %d",
                     (int)card_content_height(), (int)indvendigt);
    }

    for (int i = 0; i < 2; i++) {
        *k[i].kort = zs_card_create(parent, false);
        lv_obj_set_size(*k[i].kort, CARD_W, CARDS_H);
        lv_obj_set_pos(*k[i].kort, k[i].x, Y_CARDS);
        lv_obj_set_style_pad_all(*k[i].kort, CARD_PAD, 0);
        lv_obj_set_style_radius(*k[i].kort, 12, 0);

        lv_obj_t *l = zs_label_create(*k[i].kort, k[i].tekst);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, y_label);

        *k[i].vaerdi = lv_label_create(*k[i].kort);
        lv_label_set_text(*k[i].vaerdi, "-");
        zs_style_text(*k[i].vaerdi, &zs_font_28, ZS_C_TEXT);
        lv_obj_align(*k[i].vaerdi, LV_ALIGN_TOP_LEFT, 0, y_value);

        *k[i].tid = lv_label_create(*k[i].kort);
        lv_label_set_text(*k[i].tid, "");
        zs_style_text(*k[i].tid, &zs_font_16, ZS_C_LABEL);
        lv_obj_align(*k[i].tid, LV_ALIGN_TOP_LEFT, 0, y_time);
    }

    /* Den vigtigste linje paa siden. */
    p->note = lv_label_create(parent);
    lv_label_set_text(p->note, "Spotpris uden afgifter og moms");
    zs_style_text(p->note, &zs_font_16, ZS_C_STALE);
    lv_label_set_long_mode(p->note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p->note, ZS_CONTENT_WIDTH);
    lv_obj_set_pos(p->note, ZS_EDGE, Y_NOTE);

    /* Vises i stedet for det hele naar der ikke er priser. */
    p->error = lv_label_create(parent);
    lv_label_set_text(p->error, "");
    zs_style_text(p->error, &zs_font_20, ZS_C_LABEL);
    lv_obj_set_style_text_align(p->error, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(p->error, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p->error, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_set_pos(p->error, 2 * ZS_EDGE, 150);
    lv_obj_add_flag(p->error, LV_OBJ_FLAG_HIDDEN);
}

static void vis_alt(zs_price_page_t *p, bool vis)
{
    lv_obj_t *ting[] = { p->value, p->unit, p->word, p->chart,
                         p->low_card, p->high_card, p->note,
                         p->axis[0], p->axis[1], p->axis[2], p->axis[3] };
    for (size_t i = 0; i < sizeof(ting) / sizeof(ting[0]); i++) {
        if (vis) {
            lv_obj_clear_flag(ting[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ting[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void zs_price_page_update(zs_price_page_t *p, const zs_price_day_t *d)
{
    if (p == NULL || p->page == NULL) {
        return;
    }

    lv_label_set_text(p->zone, (d != NULL && d->zone[0]) ? d->zone : "");

    if (d == NULL || !d->ok || d->antal == 0) {
        vis_alt(p, false);
        lv_obj_clear_flag(p->error, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(p->error,
            (d != NULL && d->fejl[0]) ? d->fejl : "Henter priser ...");
        return;
    }
    lv_obj_add_flag(p->error, LV_OBJ_FLAG_HIDDEN);
    vis_alt(p, true);

    /* ── prisen lige nu ── */
    char kr[16];
    if (d->nu >= 0 && d->nu < d->antal) {
        zs_fmt_kroner(d->timer[d->nu].dkk, kr, sizeof(kr));
        lv_label_set_text(p->value, kr);
        lv_label_set_text(p->unit, "kr/kWh");

        float v = d->timer[d->nu].dkk;
        const char *ord;
        uint32_t farve;
        /* Sammenlignet med dagens eget gennemsnit, ikke med et fast
         * beloeb. En dag hvor alt er dyrt, er den billigste time
         * stadig den billigste. */
        if (v <= d->gennemsnit * 0.85f)      { ord = "Billigt lige nu";  farve = ZS_C_GOOD; }
        else if (v >= d->gennemsnit * 1.15f) { ord = "Dyrt lige nu";     farve = ZS_C_BAD; }
        else                                 { ord = "Almindelig pris";  farve = ZS_C_LABEL; }
        lv_label_set_text(p->word, ord);
        lv_obj_set_style_text_color(p->word, lv_color_hex(farve), 0);
        lv_obj_set_style_text_color(p->value, lv_color_hex(ZS_C_VALUE), 0);
    } else {
        /* Uret er ikke sat, saa vi ved ikke hvilken time vi er i. Vi
         * viser dagens gennemsnit i stedet for at gaette. */
        zs_fmt_kroner(d->gennemsnit, kr, sizeof(kr));
        lv_label_set_text(p->value, kr);
        lv_label_set_text(p->unit, "kr/kWh");
        lv_label_set_text(p->word, "Gennemsnit i dag");
        lv_obj_set_style_text_color(p->word, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_set_style_text_color(p->value, lv_color_hex(ZS_C_TEXT), 0);
    }

    /* Enheden skal staa lige efter tallet, ikke paa en fast plads:
     * "0,62" og "-0,05" er ikke lige brede. */
    lv_obj_update_layout(p->value);
    lv_obj_set_x(p->unit, ZS_EDGE + lv_obj_get_width(p->value) + ZS_UNIT_GAP);

    /* ── soejlerne ── */
    uint8_t n = d->antal;
    if (n > ZS_PRICE_BARS) {
        n = ZS_PRICE_BARS;
    }
    lv_coord_t avail = ZS_CONTENT_WIDTH;
    lv_coord_t bw = (avail - (lv_coord_t)(n - 1) * BAR_GAP) / (lv_coord_t)n;
    if (bw < 2) {
        bw = 2;
    }
    lv_coord_t total = (lv_coord_t)n * bw + (lv_coord_t)(n - 1) * BAR_GAP;
    lv_coord_t x0 = (ZS_SCR_WIDTH - total) / 2;

    float hoejest = d->timer[d->dyrest].dkk;
    /* Negative priser findes. Uden dette ville soejlerne blive vendt
     * paa hovedet eller forsvinde. */
    float lavest = d->timer[d->billigst].dkk;
    float bund = lavest < 0.0f ? lavest : 0.0f;
    float spaend = hoejest - bund;
    if (spaend < 0.01f) {
        spaend = 0.01f;
    }

    for (int i = 0; i < ZS_PRICE_BARS; i++) {
        if (i >= n) {
            lv_obj_add_flag(p->bar[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(p->bar[i], LV_OBJ_FLAG_HIDDEN);

        float andel = (d->timer[i].dkk - bund) / spaend;
        lv_coord_t h = (lv_coord_t)(andel * (float)(CHART_H - BAR_MIN_H))
                       + BAR_MIN_H;
        if (h > CHART_H) { h = CHART_H; }

        lv_obj_set_size(p->bar[i], bw, h);
        lv_obj_set_pos(p->bar[i], x0 + (lv_coord_t)i * (bw + BAR_GAP),
                       CHART_H - h);

        uint32_t farve;
        lv_opa_t opa;
        if (i == d->nu) {
            farve = ZS_C_ACCENT;  opa = LV_OPA_COVER;
        } else if (d->timer[i].dkk <= d->gennemsnit) {
            farve = ZS_C_GOOD;    opa = LV_OPA_50;
        } else {
            farve = ZS_C_BAD;     opa = LV_OPA_50;
        }
        lv_obj_set_style_bg_color(p->bar[i], lv_color_hex(farve), 0);
        lv_obj_set_style_bg_opa(p->bar[i], opa, 0);
    }

    /* Timetallene under, placeret under den soejle de hoerer til. */
    static const int vis_timer[4] = { 0, 6, 12, 18 };
    for (int a = 0; a < 4; a++) {
        int idx = -1;
        for (uint8_t i = 0; i < n; i++) {
            if (d->timer[i].hour == (uint8_t)vis_timer[a]) { idx = i; break; }
        }
        if (idx < 0) {
            lv_obj_add_flag(p->axis[a], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(p->axis[a], LV_OBJ_FLAG_HIDDEN);
        char t[8];
        snprintf(t, sizeof(t), "%02d", vis_timer[a]);
        lv_label_set_text(p->axis[a], t);
        lv_obj_set_x(p->axis[a],
                     x0 + (lv_coord_t)idx * (bw + BAR_GAP) + bw / 2 - 20);
    }

    /* ── billigst og dyrest ── */
    zs_fmt_kroner(d->timer[d->billigst].dkk, kr, sizeof(kr));
    char t[24];
    snprintf(t, sizeof(t), "%s kr", kr);
    lv_label_set_text(p->low_value, t);
    lv_obj_set_style_text_color(p->low_value, lv_color_hex(ZS_C_GOOD), 0);
    snprintf(t, sizeof(t), "kl. %02u", (unsigned)d->timer[d->billigst].hour);
    lv_label_set_text(p->low_time, t);

    zs_fmt_kroner(d->timer[d->dyrest].dkk, kr, sizeof(kr));
    snprintf(t, sizeof(t), "%s kr", kr);
    lv_label_set_text(p->high_value, t);
    lv_obj_set_style_text_color(p->high_value, lv_color_hex(ZS_C_BAD), 0);
    snprintf(t, sizeof(t), "kl. %02u", (unsigned)d->timer[d->dyrest].hour);
    lv_label_set_text(p->high_time, t);
}
