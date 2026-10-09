#include "zs_screen_tilmeld.h"

#include <stdio.h>
#include <string.h>

#include "zs_app.h"
#include "zs_icons.h"
#include "zs_theme.h"
#include "zs_ui.h"

static zs_page_t  s_side;
static lv_obj_t  *s_over;        /* linjen over koden              */
static lv_obj_t  *s_kode;        /* de seks tegn, eller en besked  */
static lv_obj_t  *s_under;       /* linjen under                   */
static lv_obj_t  *s_knap;        /* "Lav en kode", kun naar der ingen er */

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

    if (s_knap != NULL) {
        if (vis_knap) {
            lv_obj_clear_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
