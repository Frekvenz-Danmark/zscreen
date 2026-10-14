#include "zs_ui_maal.h"
#include "zs_ui.h"
#include "zs_layout.h"
#include "../zs_log.h"

#include "lvgl.h"

#include <stdbool.h>

static const char *TAG = "maal";

/*
 * Hvor dybt der gaas ned. Et traek i LVGL kan vaere dybt, og en
 * uendelig loekke i en opstart er vaerre end en umaalt flade.
 */
#define MAKS_DYBDE  8

static int s_fund;

/* Navnet paa en side, saa fundet kan findes igen. */
static const char *side_navn(zs_screen_id_t id)
{
    switch (id) {
    case ZS_SCREEN_WELCOME:         return "velkomst";
    case ZS_SCREEN_GUIDE:           return "kom i gang";
    case ZS_SCREEN_WIFI_LIST:       return "vaelg wifi";
    case ZS_SCREEN_PASSWORD:        return "kodeord";
    case ZS_SCREEN_CONNECTING:      return "forbinder";
    case ZS_SCREEN_INVERTER_SCAN:   return "soeger inverter";
    case ZS_SCREEN_INVERTER_LIST:   return "vaelg inverter";
    case ZS_SCREEN_INVERTER_MANUAL: return "inverter i haanden";
    case ZS_SCREEN_HOME:            return "forsiden";
    case ZS_SCREEN_SETTINGS:        return "indstillinger";
    case ZS_SCREEN_DETAILS:         return "detaljer";
    case ZS_SCREEN_THEME:           return "tema";
    case ZS_SCREEN_TILMELD:         return "tilmelding";
    case ZS_SCREEN_FEJL:            return "fejlsiden";
    }
    return "ukendt";
}

/*
 * Ét objekt, maalt mod sin foraelders INDHOLDSFLADE.
 *
 * Det er indholdsfladen og ikke hele foraelderen der er graensen: en
 * kasse med polstring har mindre plads indeni end udenpaa, og det er
 * indeni barnet skal vaere.
 */
static void maal_et(const char *side, lv_obj_t *o, int dybde)
{
    /*
     * SKJULTE OBJEKTER MAALES OGSAA, og det er hele pointen.
     *
     * Her stod "|| lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)", og det
     * virkede fornuftigt: hvorfor maale noget man ikke kan se? Fordi
     * TRETTEN AF FJORTEN SIDER er skjulte ved opstart - kun den der
     * vises lige nu, er synlig. Maaleren gik derfor kun velkomstsiden
     * igennem og skrev "intet stikker ud" om de andre tretten.
     *
     * Det blev fanget ved at FLYTTE tandhjulet 200 px ud over kanten og
     * se at maaleren stadig sagde at alt var i orden. En maaler der
     * ikke kan fejle, maaler ingenting.
     *
     * En skjult side bliver vist et oejeblik senere, og saa er fejlen
     * der. Den skal findes nu.
     */
    if (dybde > MAKS_DYBDE) {
        return;
    }

    lv_obj_t *far = lv_obj_get_parent(o);
    if (far != NULL) {
        lv_area_t b, f;
        lv_obj_get_coords(o, &b);
        lv_obj_get_content_coords(far, &f);

        lv_coord_t bw = lv_area_get_width(&b);
        lv_coord_t bh = lv_area_get_height(&b);

        /* Uden stoerrelse er den ikke der. */
        if (bw <= 0 || bh <= 0) {
            ZS_LOGW(TAG, "%s: et objekt har stoerrelsen %d x %d",
                    side, (int)bw, (int)bh);
            s_fund++;
        }

        /*
         * Uden for foraelderen. Tallene er hvor MANGE pixels der stikker
         * ud, og hvilken vej - det er det man skal vide for at rette
         * det, og det er praecis hvad der manglede da tandhjulet raakte
         * ud over kanten.
         */
        int ud_v = (int)(f.x1 - b.x1);
        int ud_h = (int)(b.x2 - f.x2);
        int ud_o = (int)(f.y1 - b.y1);
        int ud_n = (int)(b.y2 - f.y2);
        if (ud_v > 0 || ud_h > 0 || ud_o > 0 || ud_n > 0) {
            ZS_LOGW(TAG, "%s: %d x %d stikker ud "
                         "(venstre %d, hoejre %d, op %d, ned %d)",
                    side, (int)bw, (int)bh,
                    ud_v > 0 ? ud_v : 0, ud_h > 0 ? ud_h : 0,
                    ud_o > 0 ? ud_o : 0, ud_n > 0 ? ud_n : 0);
            s_fund++;
        }

        /*
         * Noget man kan trykke paa, skal kunne rammes. En finger bliver
         * ikke mindre af at skaermen bliver stoerre, saa graensen er den
         * samme paa begge skaerme - se ZS_G_TOUCH_MIN.
         */
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)
            && (bw < ZS_G_TOUCH_MIN || bh < ZS_G_TOUCH_MIN)) {
            ZS_LOGW(TAG, "%s: noget man kan trykke paa er kun %d x %d, "
                         "og en finger kraever %d",
                    side, (int)bw, (int)bh, (int)ZS_G_TOUCH_MIN);
            s_fund++;
        }
    }

    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) {
        maal_et(side, lv_obj_get_child(o, i), dybde + 1);
    }
}

int zs_ui_maal_graenser(void)
{
    s_fund = 0;
    int sider = 0;

    for (int id = 0; id <= (int)ZS_SCREEN_FEJL; id++) {
        lv_obj_t *rod = zs_ui_root_of((zs_screen_id_t)id);
        if (rod == NULL) {
            continue;
        }
        /* Uden den her er koordinaterne ikke regnet ud endnu, og saa
         * maaler vi paa nuller. */
        lv_obj_update_layout(rod);
        maal_et(side_navn((zs_screen_id_t)id), rod, 0);
        sider++;
    }

    if (s_fund == 0) {
        ZS_LOGI(TAG, "%d sider maalt, intet stikker ud og alt kan rammes",
                sider);
    } else {
        ZS_LOGE(TAG, "%d sider maalt, %d fund. Se linjerne ovenfor",
                sider, s_fund);
    }
    return s_fund;
}
