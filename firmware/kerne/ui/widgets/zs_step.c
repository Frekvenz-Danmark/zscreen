/*
 * zScreen - ventetrinnet. Se zs_step.h for hvorfor det findes.
 */

#include "zs_step.h"
#include "zs_theme.h"
#include "zs_layout.h"

/*
 * ALLE FIRE LIGGER PAA TOP_MID OG IKKE PAA EN STAK.
 *
 * Inverter-soegningen gjorde det, tilmeldingen stakkede. En stak flytter
 * sig naar en tekst bliver to linjer i stedet for én, og saa staar
 * bjaelken ét sted paa den ene side og et andet paa den anden. Faste
 * positioner staar stille, og det er dem der er valgt - se noten i
 * zs_layout.h om hvorfor de ikke udregnes.
 */
void zs_step_create(zs_step_t *s, lv_obj_t *forael)
{
    if (s == NULL || forael == NULL) {
        return;
    }

    s->titel = lv_label_create(forael);
    zs_style_text(s->titel, &zs_font_stor, ZS_C_TEXT);
    lv_label_set_long_mode(s->titel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s->titel, ZS_G_STEP_WIDTH);
    lv_obj_set_style_text_align(s->titel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s->titel, LV_ALIGN_TOP_MID, 0, ZS_G_STEP_TITLE_Y);

    s->tekst = lv_label_create(forael);
    zs_style_text(s->tekst, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(s->tekst, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s->tekst, ZS_G_STEP_WIDTH);
    lv_obj_set_style_text_align(s->tekst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s->tekst, LV_ALIGN_TOP_MID, 0, ZS_G_STEP_BODY_Y);

    s->bjaelke = lv_bar_create(forael);
    /* En fremdriftsbjaelke viser noget, den goer ikke noget. LVGL
     * giver den LV_OBJ_FLAG_CLICKABLE alligevel, og saa laa der et
     * trykfelt paa 720 x 13 paa to sider - maalt. */
    lv_obj_clear_flag(s->bjaelke, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s->bjaelke, ZS_G_STEP_WIDTH, ZS_G_STEP_BAR_HEIGHT);
    lv_obj_align(s->bjaelke, LV_ALIGN_TOP_MID, 0, ZS_G_STEP_BAR_Y);
    lv_bar_set_range(s->bjaelke, 0, 100);
    lv_bar_set_value(s->bjaelke, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s->bjaelke, lv_color_hex(ZS_C_CARD),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(s->bjaelke, lv_color_hex(ZS_C_ACCENT),
                              LV_PART_INDICATOR);
    lv_obj_set_style_radius(s->bjaelke, ZS_G_SK(4), LV_PART_MAIN);
    lv_obj_set_style_radius(s->bjaelke, ZS_G_SK(4), LV_PART_INDICATOR);

    s->note = lv_label_create(forael);
    lv_label_set_text(s->note, "");
    zs_style_text(s->note, &zs_font_broed, ZS_C_ACCENT);
    lv_obj_set_width(s->note, ZS_G_STEP_WIDTH);
    lv_obj_set_style_text_align(s->note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s->note, LV_ALIGN_TOP_MID, 0, ZS_G_STEP_NOTE_Y);
}

void zs_step_saet(zs_step_t *s, const char *titel, const char *tekst)
{
    if (s == NULL) {
        return;
    }
    if (titel != NULL && s->titel != NULL) {
        lv_label_set_text(s->titel, titel);
    }
    if (tekst != NULL && s->tekst != NULL) {
        lv_label_set_text(s->tekst, tekst);
    }
}

void zs_step_fremdrift(zs_step_t *s, int procent)
{
    if (s == NULL || s->bjaelke == NULL) {
        return;
    }
    /* KLIPPES HER OG IKKE HOS KALDEREN.
     *
     * Soegningen regner procenten af hvor mange adresser der er proevet,
     * og en aendring i det regnestykke maa ikke kunne give en bjaelke der
     * tegner uden for sig selv. Grænsen staar paa det sted der ejer
     * bjaelken. */
    if (procent < 0) {
        procent = 0;
    } else if (procent > 100) {
        procent = 100;
    }
    lv_bar_set_value(s->bjaelke, procent, LV_ANIM_OFF);
}

void zs_step_note(zs_step_t *s, const char *note)
{
    if (s == NULL || s->note == NULL) {
        return;
    }
    lv_label_set_text(s->note, note != NULL ? note : "");
}

void zs_step_vis(zs_step_t *s, bool paa)
{
    if (s == NULL) {
        return;
    }
    lv_obj_t *alle[] = { s->titel, s->tekst, s->bjaelke, s->note };
    for (size_t i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
        if (alle[i] == NULL) {
            continue;
        }
        if (paa) {
            lv_obj_clear_flag(alle[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(alle[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
