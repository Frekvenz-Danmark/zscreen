#include "zs_flow.h"
#include "zs_theme.h"
#include "zs_config.h"
#include "zs_format.h"

#include <stdio.h>
#include <string.h>

/*
 * Maalene. Alle y er regnet fra sidens overkant, ikke skaermens.
 * Siden er 480 x 408. Udregningen staar i zs_flow.h og er kontrolleret
 * saa nederste kant lander paa 363 af 408.
 */
/*
 * ALLE MAAL HERUNDER ER SKALEREDE, og de var det ikke foer.
 *
 * Tallene er tegnet paa 480 x 480, og de stod som raa pixels: HUB_Y
 * 182, Y_BAT_SUB 345, SIDE_DX 172. Paa 800 x 1280 betoed det at hele
 * diagrammet - sol, hus, net, batteri og stregerne imellem - fyldte
 * 345 px af en side paa 1160 og laa klemt op i oeverste venstre
 * hjoerne. Det var det foerste man kunne SE var galt paa den store
 * skaerm.
 *
 * Reglen er den samme som i zs_layout.h:
 *   ZS_G_SK_HEIGHThoejder og y-positioner
 *   ZS_G_SK_WIDTH bredder og afstande ud til siden
 *   ZS_G_SK     FORMER - knudepunktets radius, stregtykkelsen, ikoner.
 *               Den mindste af de to, saa en cirkel bliver rund.
 *
 * Paa 480 x 480 er alle tre praecis 1:1, saa hvert tal her er uaendret
 * paa zScreen.
 */
#define CX            ZS_G_CX  /* midterlinjen, se zs_layout.h        */
#define HUB_Y         ZS_G_SK_HEIGHT(182)      /* knudepunktets midte                 */
#define HUB_R         ZS_G_SK(20)

/* De to sideknuder sidder SYMMETRISK om midten.
 *
 * Her stod 68 og 412. De er rigtige paa en 480 px skaerm, for 412 er
 * 480 - 68. Men de to tal saa ud som to uafhaengige tal, og paa en
 * bredere skaerm ville den venstre blive siddende og den hoejre ryge ud
 * over kanten. Nu staar afstanden fra midten ÉT sted, og saa kan de ikke
 * komme fra hinanden. */
#define SIDE_DX       ZS_G_SK_WIDTH(172)      /* fra midten ud til en sideknude       */
#define SIDE_L        (CX - SIDE_DX)
#define SIDE_R        (CX + SIDE_DX)

#define LINE_W        ZS_G_SK(4)        /* stregtykkelse                       */
#define NODE_W        ZS_G_SK_WIDTH(150)      /* bredden af en knudes tekst          */

#define Y_SUN_ICON    ZS_G_SK_HEIGHT(20)
#define Y_SUN_VALUE   ZS_G_SK_HEIGHT(52)
#define Y_VLINE_TOP   ZS_G_SK_HEIGHT(90)       /* fra sol ned til knudepunktet        */
#define VLINE_TOP_H   ZS_G_SK_HEIGHT(72)
#define Y_SIDE_ICON   ZS_G_SK_HEIGHT(168)
#define Y_SIDE_VALUE  ZS_G_SK_HEIGHT(204)
#define Y_SIDE_LABEL  ZS_G_SK_HEIGHT(239)
#define Y_VLINE_BOT   ZS_G_SK_HEIGHT(202)      /* fra knudepunktet ned til batteri    */
#define VLINE_BOT_H   ZS_G_SK_HEIGHT(72)
#define Y_BAT_ICON    ZS_G_SK_HEIGHT(278)
#define Y_BAT_VALUE   ZS_G_SK_HEIGHT(310)
#define Y_BAT_SUB     ZS_G_SK_HEIGHT(345)

/* De vandrette streger stopper foer knudepunktet og foer ikonet.
 *
 * Her stod 92 og 388, og 388 er 480 - 92. Samme sag som sideknuderne:
 * to tal der hoerer sammen, skrevet som om de ikke gjorde. */
#define HLINE_DX      ZS_G_SK_WIDTH(148)      /* fra midten ud til hvor stregen stopper */
#define HLINE_L_X     (CX - HLINE_DX)
#define HLINE_L_W     (CX - HUB_R - HLINE_L_X)
#define HLINE_R_X     (CX + HUB_R)
#define HLINE_R_W     (CX + HLINE_DX - HLINE_R_X)

/* Under saa mange watt kalder vi det hvile. Det samme tal som paa
 * side 1, saa de to sider ikke kan sige hver sit om samme maaling. */
/* Samme graense som tallet rundes med. Se ZS_DEADBAND_W:
 * staar der 0 W, maa der ikke ogsaa staa en pil. */
#define IDLE_W        ZS_DEADBAND_W

/* ------------------------------------------------------------------ */
/* Byggeklodser                                                        */
/* ------------------------------------------------------------------ */

static lv_obj_t *make_icon(lv_obj_t *p, const char *sym, lv_coord_t cx, lv_coord_t y)
{
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, sym);
    lv_obj_set_style_text_font(o, &zs_ikon_stor, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    /* ZS: ikonets felt er 40 bredt, altsaa 20 til hver side af midten.
     * Begge tal skal foelge skaermen, ellers staar et stort ikon i et
     * lille felt - eller omvendt. */
    lv_obj_set_width(o, ZS_G_SK(40));
    lv_obj_set_pos(o, cx - ZS_G_SK(40) / 2, y);
    return o;
}

static lv_obj_t *make_text(lv_obj_t *p, const lv_font_t *font, uint32_t color,
                           lv_coord_t cx, lv_coord_t y)
{
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, "");
    zs_style_text(o, font, color);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    lv_obj_set_width(o, NODE_W);
    lv_obj_set_pos(o, cx - NODE_W / 2, y);
    return o;
}

/* En streg er bare et afrundet felt. Det er baade billigere at tegne
 * end lv_line og nemmere at give en farve og en tykkelse. */
static lv_obj_t *make_line(lv_obj_t *p, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, LINE_W / 2, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* Pilen sidder midt paa stregen og siger hvilken vej det loeber. */
static lv_obj_t *make_arrow(lv_obj_t *p, lv_coord_t cx, lv_coord_t cy)
{
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, "");
    lv_obj_set_style_text_font(o, &zs_ikon_lille, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(o, ZS_G_SK(24));
    lv_obj_set_pos(o, cx - ZS_G_SK(24) / 2, cy - ZS_G_SK(10));
    /* Baggrund i sidens farve, saa pilen "klipper" stregen og ikke
     * ligger oven paa den som en klat. */
    lv_obj_set_style_bg_color(o, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(o, ZS_G_SK(2), 0);
    return o;
}

void zs_flow_create(zs_flow_t *f, lv_obj_t *parent)
{
    memset(f, 0, sizeof(*f));
    f->page = parent;

    /* Stregerne foerst, saa ikoner og tal tegnes ovenpaa. */
    f->line_sun   = make_line(parent, CX - LINE_W / 2, Y_VLINE_TOP, LINE_W, VLINE_TOP_H);
    f->line_bat   = make_line(parent, CX - LINE_W / 2, Y_VLINE_BOT, LINE_W, VLINE_BOT_H);
    f->line_house = make_line(parent, HLINE_L_X, HUB_Y - LINE_W / 2, HLINE_L_W, LINE_W);
    f->line_grid  = make_line(parent, HLINE_R_X, HUB_Y - LINE_W / 2, HLINE_R_W, LINE_W);

    /* Knudepunktet. Det er husets eltavle, tegnet som en ring. */
    f->hub = lv_obj_create(parent);
    lv_obj_remove_style_all(f->hub);
    lv_obj_set_size(f->hub, 2 * HUB_R, 2 * HUB_R);
    lv_obj_set_pos(f->hub, CX - HUB_R, HUB_Y - HUB_R);
    lv_obj_set_style_radius(f->hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(f->hub, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(f->hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(f->hub, ZS_G_SK(3), 0);
    lv_obj_set_style_border_color(f->hub, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_clear_flag(f->hub, LV_OBJ_FLAG_SCROLLABLE);

    f->arrow_sun   = make_arrow(parent, CX, Y_VLINE_TOP + VLINE_TOP_H / 2);
    f->arrow_bat   = make_arrow(parent, CX, Y_VLINE_BOT + VLINE_BOT_H / 2);
    f->arrow_house = make_arrow(parent, HLINE_L_X + HLINE_L_W / 2, HUB_Y);
    f->arrow_grid  = make_arrow(parent, HLINE_R_X + HLINE_R_W / 2, HUB_Y);

    /* Sol */
    f->sun_icon  = make_icon(parent, ZS_ICON_SUN, CX, Y_SUN_ICON);
    f->sun_value = make_text(parent, &zs_font_stor, ZS_C_TEXT, CX, Y_SUN_VALUE);

    /* Hus */
    f->house_icon  = make_icon(parent, ZS_ICON_HOUSE, SIDE_L, Y_SIDE_ICON);
    f->house_value = make_text(parent, &zs_font_stor, ZS_C_TEXT, SIDE_L, Y_SIDE_VALUE);
    f->house_label = make_text(parent, &zs_font_etiket, ZS_C_LABEL, SIDE_L, Y_SIDE_LABEL);
    lv_obj_set_style_text_letter_space(f->house_label, 1, 0);

    /* Net */
    f->grid_icon  = make_icon(parent, ZS_ICON_ZAP, SIDE_R, Y_SIDE_ICON);
    f->grid_value = make_text(parent, &zs_font_stor, ZS_C_TEXT, SIDE_R, Y_SIDE_VALUE);
    f->grid_label = make_text(parent, &zs_font_etiket, ZS_C_LABEL, SIDE_R, Y_SIDE_LABEL);
    lv_obj_set_style_text_letter_space(f->grid_label, 1, 0);

    /* Batteri */
    f->bat_icon  = make_icon(parent, ZS_ICON_BATTERY, CX, Y_BAT_ICON);
    f->bat_value = make_text(parent, &zs_font_stor, ZS_C_TEXT, CX, Y_BAT_VALUE);
    f->bat_sub   = make_text(parent, &zs_font_broed, ZS_C_LABEL, CX, Y_BAT_SUB);
}

/* ------------------------------------------------------------------ */
/* Opdatering                                                          */
/* ------------------------------------------------------------------ */

/* Skriver en effekt, eller en streg hvis den mangler. */
static void set_power(lv_obj_t *lbl, zs_val_t v, uint32_t color)
{
    zs_num_t n;
    if (!v.ok) {
        zs_fmt_none(&n);
        lv_label_set_text(lbl, n.value);
        lv_obj_set_style_text_color(lbl, lv_color_hex(ZS_C_STALE), 0);
        return;
    }
    zs_fmt_power(v.v, &n);
    char t[24];
    snprintf(t, sizeof(t), "%s %s", n.value, n.unit);
    lv_label_set_text(lbl, t);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
}

/*
 * Saetter en streg og dens pil.
 *
 * flowing er false naar der ikke loeber noget: saa er stregen graa og
 * pilen skjult. Det er vigtigere end det lyder. Uden det ville en
 * skaerm om natten vise fire farvede pile der peger et sted hen, selv
 * om ingenting bevaeger sig.
 */
static void set_link(lv_obj_t *line, lv_obj_t *arrow, bool flowing,
                     const char *dir_icon, uint32_t color, bool stale)
{
    uint32_t c = stale ? ZS_C_STALE : color;

    lv_obj_set_style_bg_color(line, lv_color_hex(flowing ? c : ZS_C_BORDER), 0);

    if (flowing && dir_icon != NULL) {
        lv_label_set_text(arrow, dir_icon);
        lv_obj_set_style_text_color(arrow, lv_color_hex(c), 0);
        lv_obj_clear_flag(arrow, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
    }
}

void zs_flow_update(zs_flow_t *f, const zs_home_data_t *d)
{
    if (f == NULL || d == NULL || f->page == NULL) {
        return;
    }
    bool stale = d->stale || !d->have_data;
    uint32_t text_col = stale ? ZS_C_STALE : ZS_C_TEXT;
    uint32_t lbl_col  = stale ? ZS_C_STALE : ZS_C_LABEL;

    lv_obj_set_style_text_color(f->sun_icon,   lv_color_hex(lbl_col), 0);
    lv_obj_set_style_text_color(f->house_icon, lv_color_hex(lbl_col), 0);
    lv_obj_set_style_text_color(f->grid_icon,  lv_color_hex(lbl_col), 0);
    lv_obj_set_style_text_color(f->bat_icon,   lv_color_hex(lbl_col), 0);
    lv_obj_set_style_text_color(f->house_label, lv_color_hex(lbl_col), 0);
    lv_obj_set_style_text_color(f->grid_label,  lv_color_hex(lbl_col), 0);

    if (!d->have_data) {
        lv_label_set_text(f->sun_value,   "-");
        lv_label_set_text(f->house_value, "-");
        lv_label_set_text(f->grid_value,  "-");
        lv_label_set_text(f->bat_value,   "-");
        lv_label_set_text(f->house_label, "HENTER");
        lv_label_set_text(f->grid_label,  "HENTER");
        lv_label_set_text(f->bat_sub,     "");
        set_link(f->line_sun,   f->arrow_sun,   false, NULL, ZS_C_LABEL, true);
        set_link(f->line_house, f->arrow_house, false, NULL, ZS_C_LABEL, true);
        set_link(f->line_grid,  f->arrow_grid,  false, NULL, ZS_C_LABEL, true);
        set_link(f->line_bat,   f->arrow_bat,   false, NULL, ZS_C_LABEL, true);
        return;
    }

    /* ── sol ── */
    zs_val_t sol = d->live.solar_w;
    set_power(f->sun_value, sol, text_col);
    bool sol_flow = sol.ok && sol.v > IDLE_W;
    /* Solen leverer altid nedad, ind i huset. */
    set_link(f->line_sun, f->arrow_sun, sol_flow,
             ZS_ICON_ARROW_DOWN, ZS_C_ACCENT, stale);

    /* ── forbrug ── */
    if (!d->has_meter) {
        /* Uden elmaaler kan forbruget ikke udledes. Vi siger det med
         * ord i stedet for at vise et nul der ligner en maaling. */
        lv_label_set_text(f->house_value, "-");
        lv_obj_set_style_text_color(f->house_value, lv_color_hex(ZS_C_STALE), 0);
        lv_label_set_text(f->house_label, "INGEN ELMÅLER");
        set_link(f->line_house, f->arrow_house, false, NULL, ZS_C_LABEL, stale);
    } else {
        zs_val_t hus = d->live.house_w;
        set_power(f->house_value, hus, text_col);
        lv_label_set_text(f->house_label, "FORBRUG");
        /* Stroemmen loeber altid FRA knudepunktet OG UD i huset. */
        set_link(f->line_house, f->arrow_house,
                 hus.ok && hus.v > IDLE_W,
                 ZS_ICON_ARROW_LEFT, ZS_C_TEXT, stale);
    }

    /* ── net ── */
    if (!d->has_meter) {
        lv_label_set_text(f->grid_value, "-");
        lv_obj_set_style_text_color(f->grid_value, lv_color_hex(ZS_C_STALE), 0);
        lv_label_set_text(f->grid_label, "INGEN ELMÅLER");
        set_link(f->line_grid, f->arrow_grid, false, NULL, ZS_C_LABEL, stale);
    } else {
        zs_val_t net = d->live.grid_w;
        set_power(f->grid_value, net, text_col);
        bool koeb  = net.ok && net.v >  IDLE_W;
        bool salg  = net.ok && net.v < -IDLE_W;
        if (koeb) {
            lv_label_set_text(f->grid_label, "KØBER");
            /* Ind i huset, altsaa fra hoejre mod midten. */
            set_link(f->line_grid, f->arrow_grid, true,
                     ZS_ICON_ARROW_LEFT, ZS_C_BAD, stale);
        } else if (salg) {
            lv_label_set_text(f->grid_label, "SÆLGER");
            set_link(f->line_grid, f->arrow_grid, true,
                     ZS_ICON_ARROW_RIGHT, ZS_C_GOOD, stale);
        } else {
            lv_label_set_text(f->grid_label, net.ok ? "I BALANCE" : "INGEN MÅLING");
            set_link(f->line_grid, f->arrow_grid, false, NULL, ZS_C_LABEL, stale);
        }
    }

    /* ── batteri ── */
    if (!d->has_battery) {
        lv_label_set_text(f->bat_value, "-");
        lv_obj_set_style_text_color(f->bat_value, lv_color_hex(ZS_C_STALE), 0);
        lv_label_set_text(f->bat_sub, "Intet batteri");
        lv_label_set_text(f->bat_icon, ZS_ICON_BATTERY);
        set_link(f->line_bat, f->arrow_bat, false, NULL, ZS_C_LABEL, stale);
    } else {
        zs_val_t soc = d->live.soc_pct;
        zs_val_t pw  = d->live.battery_w;

        zs_num_t n;
        if (soc.ok) {
            zs_fmt_percent(soc.v, &n);
            char t[16];
            snprintf(t, sizeof(t), "%s %s", n.value, n.unit);
            lv_label_set_text(f->bat_value, t);
            lv_obj_set_style_text_color(f->bat_value, lv_color_hex(text_col), 0);
        } else {
            lv_label_set_text(f->bat_value, "-");
            lv_obj_set_style_text_color(f->bat_value, lv_color_hex(ZS_C_STALE), 0);
        }

        bool lader   = pw.ok && pw.v < -IDLE_W;
        bool aflader = pw.ok && pw.v >  IDLE_W;
        lv_label_set_text(f->bat_icon,
                          (lader || aflader) ? ZS_ICON_BATTERY_CHARGE : ZS_ICON_BATTERY);

        if (!pw.ok) {
            lv_label_set_text(f->bat_sub, "Effekt ukendt");
            set_link(f->line_bat, f->arrow_bat, false, NULL, ZS_C_LABEL, stale);
        } else if (lader) {
            zs_fmt_power(pw.v, &n);
            char t[40];
            snprintf(t, sizeof(t), "Lader %s %s", n.value, n.unit);
            lv_label_set_text(f->bat_sub, t);
            /* Ned i batteriet. */
            set_link(f->line_bat, f->arrow_bat, true,
                     ZS_ICON_ARROW_DOWN, ZS_C_GOOD, stale);
        } else if (aflader) {
            zs_fmt_power(pw.v, &n);
            char t[40];
            snprintf(t, sizeof(t), "Aflader %s %s", n.value, n.unit);
            lv_label_set_text(f->bat_sub, t);
            /* Op fra batteriet og ind i huset. */
            set_link(f->line_bat, f->arrow_bat, true,
                     ZS_ICON_ARROW_UP, ZS_C_ACCENT, stale);
        } else if (soc.ok && soc.v >= 99.0f) {
            lv_label_set_text(f->bat_sub, "Fuldt opladt");
            set_link(f->line_bat, f->arrow_bat, false, NULL, ZS_C_LABEL, stale);
        } else {
            lv_label_set_text(f->bat_sub, "Hviler");
            set_link(f->line_bat, f->arrow_bat, false, NULL, ZS_C_LABEL, stale);
        }
        lv_obj_set_style_text_color(f->bat_sub, lv_color_hex(lbl_col), 0);
    }
}
