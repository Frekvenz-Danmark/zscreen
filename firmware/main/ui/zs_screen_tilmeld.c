#include "zs_screen_tilmeld.h"

#include <stdio.h>
#include <string.h>

#include "zs_app.h"
#include "zs_icons.h"
#include "zs_qr.h"
#include "zs_theme.h"
#include "zs_ui.h"

static zs_page_t  s_side;
static lv_obj_t  *s_over;        /* linjen over koden              */
static lv_obj_t  *s_kode;        /* de seks tegn, eller en besked  */
static lv_obj_t  *s_under;       /* linjen under                   */
static lv_obj_t  *s_knap;        /* "Lav en kode", kun naar der ingen er */
static lv_obj_t  *s_plade;       /* hvid bund under QR'en              */
static lv_obj_t  *s_qr;          /* selve QR'en, tegnet modul for modul */

/*
 * QR'EN TEGNES SMAA OG SKALERES OP MED HELE TAL.
 *
 * Laerredet er praecis ét pixel per modul, altsaa 29 x 29 for den
 * mindste kode med luft. Saa saettes zoom til et HELT antal gange, og
 * udjaevningen slaas FRA. Resultatet er skarpe kanter.
 *
 * Tegnede vi i fuld stoerrelse i stedet, skulle lærredet vaere 145 x 145
 * i to bytes per pixel, altsaa 42 kB i stedet for 1,7 kB. Og skalerede vi
 * med udjaevning, ville modulernes kanter blive udtvaeret, og en udtvaeret
 * QR er den slags der virker paa tre telefoner ud af fire.
 */
#define QR_MAKS_MODULER  (17 + ZS_QR_MAKS_UDGAVE * 4 + 2 * ZS_QR_KANT_MODULER)
#define QR_MAAL_PX       145

static lv_color_t s_qr_lae[QR_MAKS_MODULER * QR_MAKS_MODULER];
static uint8_t    s_qr_data[ZS_QR_BUF];

/* Luft mellem de tre linjer. Ikke et skaermmaal, bare tekstens rytme. */
#define LUFT_OVER    40
#define LUFT_KODE    36
#define LUFT_UNDER   40

static void on_tilbage(lv_event_t *e)
{
    (void)e;
    /*
     * Koden holder op med at virke naar siden forlades. Det er regel 2,
     * og den skal siges til app'en HER og ikke i zs_ui_show: en kode der
     * blev ved med at virke fordi nogen gik tilbage ad en anden vej, er
     * en kode man kan fotografere og bruge bagefter.
     */
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_TILMELD_VIS;
    c.flag = false;
    zs_app_send(&c);
    zs_ui_show(ZS_SCREEN_SETTINGS);
}

static void on_lav_kode(lv_event_t *e)
{
    (void)e;
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_TILMELD_VIS;
    c.flag = true;
    zs_app_send(&c);
}

/*
 * Tegner koden som QR, eller skjuler den hvis den ikke kan laves.
 *
 * Den hvide plade bliver ved med at vaere hvid i moerkt tema. En omvendt
 * QR - lyse moduler paa moerk bund - bliver afvist af mange laesere, og
 * vi kan ikke vide hvilken telefon montoeren har.
 */
static void tegn_qr(const char *kode)
{
    if (s_qr == NULL || s_plade == NULL) {
        return;
    }
    int moduler = 0;
    if (kode == NULL || kode[0] == '\0'
        || !zs_qr_lav(kode, s_qr_data, sizeof(s_qr_data), &moduler)) {
        lv_obj_add_flag(s_plade, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    int skala = zs_qr_skala(moduler, QR_MAAL_PX);
    if (skala <= 0) {
        lv_obj_add_flag(s_plade, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    int i_alt = moduler + 2 * ZS_QR_KANT_MODULER;

    lv_canvas_set_buffer(s_qr, s_qr_lae, i_alt, i_alt, LV_IMG_CF_TRUE_COLOR);
    /* Hele lærredet lyst FOERST, saa luften rundt om bliver lys af sig
     * selv. Den luft er et krav, ikke pynt - se zs_qr.h. */
    lv_canvas_fill_bg(s_qr, lv_color_white(), LV_OPA_COVER);
    for (int y = 0; y < moduler; y++) {
        for (int x = 0; x < moduler; x++) {
            if (zs_qr_modul(s_qr_data, x, y)) {
                lv_canvas_set_px(s_qr, x + ZS_QR_KANT_MODULER,
                                 y + ZS_QR_KANT_MODULER, lv_color_black());
            }
        }
    }
    /* Udjaevning FRA, og zoom et helt antal gange. 256 er 1:1 i LVGL. */
    lv_img_set_antialias(s_qr, false);
    lv_img_set_zoom(s_qr, (uint16_t)(256 * skala));

    int px = zs_qr_px(moduler, skala);
    lv_obj_set_size(s_plade, px + 2 * ZS_QR_KANT_MODULER,
                    px + 2 * ZS_QR_KANT_MODULER);
    lv_obj_clear_flag(s_plade, LV_OBJ_FLAG_HIDDEN);
}

void zs_tilmeld_page_create(void)
{
    zs_page_create(&s_side, "Tilmeld anlægget", on_tilbage, NULL, false);

    s_over = lv_label_create(s_side.content);
    zs_style_text(s_over, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(s_over, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_over, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(s_over, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_over, ZS_EDGE, LUFT_OVER);

    /*
     * Koden staar i zs_font_28 og ikke i zs_font_num_64.
     *
     * Den store skrift har KUN cifre og komma - den blev lavet til
     * wattene paa hovedskaermen - og koden har bogstaver. Skulle den
     * staa stoerre, skal de 32 tegn i alfabetet med i den store
     * skrifttype, og det koster flash vi foerst vil maale.
     */
    s_kode = lv_label_create(s_side.content);
    zs_style_text(s_kode, &zs_font_28, ZS_C_VALUE);
    lv_label_set_long_mode(s_kode, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_kode, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(s_kode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_kode, ZS_EDGE, LUFT_OVER + LUFT_KODE);

    s_under = lv_label_create(s_side.content);
    zs_style_text(s_under, &zs_font_16, ZS_C_TEXT_DIM);
    lv_label_set_long_mode(s_under, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_under, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(s_under, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_under, ZS_EDGE, LUFT_OVER + LUFT_KODE + LUFT_UNDER);

    /* Hvid plade med luft, saa QR'en staar paa lyst uanset tema. */
    s_plade = lv_obj_create(s_side.content);
    lv_obj_set_style_bg_color(s_plade, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_plade, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_plade, 0, 0);
    lv_obj_set_style_pad_all(s_plade, 0, 0);
    lv_obj_clear_flag(s_plade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_plade, LV_ALIGN_TOP_MID, 0,
                 LUFT_OVER + LUFT_KODE + 2 * LUFT_UNDER);
    s_qr = lv_canvas_create(s_plade);
    lv_obj_center(s_qr);
    lv_obj_add_flag(s_plade, LV_OBJ_FLAG_HIDDEN);

    s_knap = zs_btn_primary_create(s_side.content, "Lav en kode");
    lv_obj_set_width(s_knap, ZS_CONTENT_WIDTH);
    lv_obj_set_pos(s_knap, ZS_EDGE,
                   LUFT_OVER + LUFT_KODE + 2 * LUFT_UNDER + LUFT_KODE);
    lv_obj_add_event_cb(s_knap, on_lav_kode, LV_EVENT_CLICKED, NULL);

    zs_tilmeld_page_update(ZS_TILMELD_MANGLER_INVERTER, "", 0);
}

lv_obj_t *zs_tilmeld_page_root(void) { return s_side.root; }

void zs_tilmeld_page_update(zs_tilmeld_stand_t stand, const char *kode,
                            int min_igen)
{
    if (s_kode == NULL) {
        return;
    }
    if (kode == NULL) {
        kode = "";
    }
    bool vis_knap = false;
    char under[96];

    switch (stand) {
    case ZS_TILMELD_KLAR:
        lv_label_set_text(s_over, "Giv den her kode til Frekvenz sammen "
                                  "med kundens oplysninger.");
        lv_label_set_text(s_kode, kode);
        zs_style_text(s_kode, &zs_font_28, ZS_C_VALUE);
        /*
         * Der staar hvor laenge den gaelder, og ikke bare "den udloeber".
         * En montoer der ikke ved om han har ti minutter eller ti
         * sekunder, skriver den af i hast og laeser forkert.
         */
        if (min_igen > 1) {
            snprintf(under, sizeof(under),
                     "Gælder i %d minutter endnu. Så skal der laves en ny.",
                     min_igen);
        } else {
            snprintf(under, sizeof(under),
                     "Gælder under et minut endnu. Så skal der laves en ny.");
        }
        lv_label_set_text(s_under, under);
        tegn_qr(kode);
        break;

    case ZS_TILMELD_MANGLER_INVERTER:
        /* Den dyre halvdel af beslutningen. Der skal staa HVAD man goer. */
        lv_label_set_text(s_over, "Anlægget er ikke fundet endnu.");
        lv_label_set_text(s_kode, "-");
        zs_style_text(s_kode, &zs_font_28, ZS_C_TEXT_DIM);
        lv_label_set_text(s_under,
            "Skærmen skal finde inverteren, før anlægget kan tilmeldes. "
            "Gå til Indstillinger og vælg Inverter.");
        break;

    case ZS_TILMELD_MANGLER_FLAADE:
        lv_label_set_text(s_over, "Skærmen er ikke på nettet endnu.");
        lv_label_set_text(s_kode, "-");
        zs_style_text(s_kode, &zs_font_28, ZS_C_TEXT_DIM);
        lv_label_set_text(s_under,
            "Koden skal også kendes af serveren, ellers virker den ikke. "
            "Tjek netværket under Indstillinger.");
        break;

    case ZS_TILMELD_UDLOEBET:
        lv_label_set_text(s_over, "Der er ingen kode lige nu.");
        lv_label_set_text(s_kode, "-");
        zs_style_text(s_kode, &zs_font_28, ZS_C_TEXT_DIM);
        lv_label_set_text(s_under,
            "En kode gælder i få minutter, så den laves når du skal "
            "bruge den.");
        vis_knap = true;
        break;

    case ZS_TILMELD_TILMELDT:
        lv_label_set_text(s_over, "Anlægget er tilmeldt.");
        lv_label_set_text(s_kode, ZS_ICON_CHECK);
        zs_style_text(s_kode, &zs_font_28, ZS_C_GOOD);
        lv_label_set_text(s_under,
            "Der skal ikke laves flere koder. Skal anlægget flyttes til "
            "en anden kunde, sker det hos Frekvenz.");
        break;
    }

    /* Ingen kode, ingen QR. En QR der blev staaende efter koden doede,
     * ville blive fotograferet og proevet forgaeves. */
    if (stand != ZS_TILMELD_KLAR && s_plade != NULL) {
        lv_obj_add_flag(s_plade, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_knap != NULL) {
        if (vis_knap) {
            lv_obj_clear_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
