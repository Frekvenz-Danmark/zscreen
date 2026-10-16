#include "zs_ui_maal.h"
#include "zs_ui.h"
#include "zs_layout.h"
#include "zs_theme.h"
#include "../zs_log.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stdio.h>

static const char *TAG = "maal";

/*
 * Hvor dybt der gaas ned. Et traek i LVGL kan vaere dybt, og en
 * uendelig loekke i en opstart er vaerre end en umaalt flade.
 */
#define MAKS_DYBDE  8

static int s_fund;
static int s_undtaget;

/*
 * HER LAA TABELLEN OVER SIDENAVNE, og den ligger nu i zs_ui.c som
 * zs_ui_sidenavn.
 *
 * Grunden: loggen over sideskift skal bruge de SAMME navne som
 * maalebaandet. To tabeller ville sige "tilmelding" det ene sted og
 * noget andet det andet, og saa kan to linjer i den samme log ikke
 * sammenlignes.
 */
#define side_navn zs_ui_sidenavn

/*
 * HVAD er det vi har fundet.
 *
 * Her stod der kun hvilken SIDE fundet var paa, og saa stod der fx
 * "forsiden: 324 x 90 stikker ud". Paa en side med fire kort og
 * tretten labels kan man ikke se hvilken af dem det er, og saa
 * begynder man at gaette. Et maalebaand der ikke siger hvad det har
 * maalt, flytter bare arbejdet.
 *
 * Nu siger den teksten, hvis der er en - enten objektets egen eller
 * det foerste label inde i det, saa en knap kendes paa sin tekst - og
 * stien ned gennem boernene, saa to kort med samme tekst kan skilles.
 */
static const char *teksten_i(lv_obj_t *o, int dybde)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        return (t != NULL && t[0] != '\0') ? t : NULL;
    }
    if (dybde <= 0) {
        return NULL;
    }
    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) {
        const char *t = teksten_i(lv_obj_get_child(o, i), dybde - 1);
        if (t != NULL) {
            return t;
        }
    }
    return NULL;
}

/*
 * Hvad for en slags objekt. LVGL gemmer ikke et navn paa sine
 * klasser, saa de skal slaas op - men uden den kan et fund ikke
 * findes: "720 x 13" siger intet om det er en knap, et felt eller en
 * kasse der kun er der for at holde noget andet, og de tre rettes
 * ikke samme sted.
 */
static const char *arten_af(lv_obj_t *o)
{
    static const struct { const lv_obj_class_t *k; const char *navn; } T[] = {
        { &lv_label_class,     "label"     },
        { &lv_btn_class,       "knap"      },
        { &lv_img_class,       "billede"   },
        { &lv_canvas_class,    "laerred"   },
        { &lv_textarea_class,  "skrivefelt"},
        { &lv_keyboard_class,  "tastatur"  },
        { &lv_btnmatrix_class, "knapgitter"},
        { &lv_switch_class,    "vippe"     },
        { &lv_slider_class,    "skyder"    },
        { &lv_bar_class,       "bjaelke"   },
        { &lv_arc_class,       "bue"       },
        { &lv_line_class,      "linje"     },
        { &lv_obj_class,       "kasse"     },
    };
    for (unsigned i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        if (lv_obj_check_type(o, T[i].k)) {
            return T[i].navn;
        }
    }
    return "ukendt";
}

/* Kort navn paa ét objekt. Bufferen er static, saa den maa bruges én
 * gang per linje - og det er praecis saadan den bruges nedenfor. */
static const char *hvem(lv_obj_t *o, const char *sti)
{
    static char buf[128];
    /* Kun to niveauer ned efter en tekst: en knap har sit label som
     * barn, men et helt kort har tre, og saa ville det foerste af dem
     * komme til at staa som navn paa kortet. */
    const char *t = teksten_i(o, 2);
    if (t != NULL) {
        snprintf(buf, sizeof(buf), "%s \"%s\" [%s]", arten_af(o), t, sti);
    } else {
        snprintf(buf, sizeof(buf), "%s [%s]", arten_af(o), sti);
    }
    return buf;
}

/*
 * Ét objekt, maalt mod sin foraelders INDHOLDSFLADE.
 *
 * Det er indholdsfladen og ikke hele foraelderen der er graensen: en
 * kasse med polstring har mindre plads indeni end udenpaa, og det er
 * indeni barnet skal vaere.
 */
static void maal_et(const char *side, lv_obj_t *o, int dybde,
                    const char *sti)
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
            /*
             * Et TOMT label uden bredde er ikke en fejl. Tallene paa
             * forsiden saettes foerst naar der kommer data, saa ved
             * opstart staar der "" i dem - og et label uden tegn er 0
             * px bredt. Det er rigtigt af dem.
             *
             * Alt ANDET uden stoerrelse er en fejl, og det er derfor
             * linjen ikke bare er slaaet fra: en kasse eller en knap
             * paa 0 px er der ikke, og det skal stadig siges.
             */
            bool tomt_label = lv_obj_check_type(o, &lv_label_class)
                              && teksten_i(o, 0) == NULL;
            if (!tomt_label) {
                ZS_LOGW(TAG, "%s: %s har stoerrelsen %d x %d",
                        side, hvem(o, sti), (int)bw, (int)bh);
                s_fund++;
            }
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

        /*
         * NOGET STIKKER UD MED VILJE, OG DET SKAL VAERE MAERKET.
         *
         * Forsidens tre sider ligger side om side i en beholder man
         * traekker i, og soejlen i indstillingerne er hoejere end
         * siden. Det er ikke fejl.
         *
         * HER STOD: "kan foraelderen rulle den vej, saa se forbi".
         * Det var et hul. zs_page_create giver HVER side en beholder
         * der kan rulle lodret, saa alt hvad der stak ud nedad i hele
         * fladen blev tiet ihjel - og tekst der loeber ud over bunden
         * paa den hoeje skaerm er lige praecis det baandet findes for.
         * En beholder uden et valgt rul har desuden LVGLs standard
         * LV_DIR_ALL, saa dér faldt alle fire retninger vaek.
         *
         * Nu skal undtagelsen staa PAA objektet, som
         * ZS_FLAG_UDENFOR_MED_VILJE. Og den gaelder kun den vej
         * foraelderen faktisk kan rulle: en side der stikker ud til
         * hoejre i en beholder der kun ruller lodret, er stadig en
         * fejl - dér kan man ikke naa den.
         */
        lv_dir_t rul = LV_DIR_NONE;
        if (lv_obj_has_flag(o, ZS_FLAG_UDENFOR_MED_VILJE)
            && lv_obj_has_flag(far, LV_OBJ_FLAG_SCROLLABLE)) {
            rul = lv_obj_get_scroll_dir(far);
        }
        /*
         * TAELLEREN TAELLER BRUGTE UNDTAGELSER, IKKE MAERKEDE OBJEKTER.
         *
         * Her stod s_undtaget++ sammen med opslaget ovenfor, altsaa for
         * hvert maerket objekt uanset om det stak ud. zs_column_create
         * saetter maerket paa HVER soejle i appen, saa linjen til sidst
         * sagde "intet stikker ud (12 begrundede undtagelser)" hvor 12
         * var antallet af soejler - ikke antallet af undtagelser der
         * blev brugt til noget.
         *
         * Og det er stik imod hvorfor den taelles: en undtagelse man
         * ikke kan se, er et hul. Et tal der altid er tolv, kan man
         * ikke se noget i.
         */
        if ((rul & LV_DIR_HOR) && (ud_v > 0 || ud_h > 0)) {
            ud_v = 0;
            ud_h = 0;
            s_undtaget++;
        }
        if ((rul & LV_DIR_VER) && (ud_o > 0 || ud_n > 0)) {
            ud_o = 0;
            ud_n = 0;
            s_undtaget++;
        }
        if (ud_v > 0 || ud_h > 0 || ud_o > 0 || ud_n > 0) {
            ZS_LOGW(TAG, "%s: %s, %d x %d, stikker ud "
                         "(venstre %d, hoejre %d, op %d, ned %d)",
                    side, hvem(o, sti), (int)bw, (int)bh,
                    ud_v > 0 ? ud_v : 0, ud_h > 0 ? ud_h : 0,
                    ud_o > 0 ? ud_o : 0, ud_n > 0 ? ud_n : 0);
            s_fund++;
        }

        /*
         * Noget man kan trykke paa, skal kunne rammes. En finger bliver
         * ikke mindre af at skaermen bliver stoerre, saa graensen er den
         * samme paa begge skaerme - se ZS_G_TOUCH_MIN.
         */
        /*
         * TRYKREGLEN MAALES PAA KLIKFLADEN OG IKKE PAA DEN MAN SER.
         *
         * Nogle ting SKAL vaere tynde. En skyder paa 26 px er pen at
         * se paa, og en skyder paa 44 px ser ud som en fejl. LVGL kan
         * give et objekt en klikflade der er stoerre end det selv -
         * lv_obj_set_ext_click_area - og saa kan man ramme den uden
         * at den bliver tyk.
         *
         * Her stod der bw og bh, altsaa den flade man SER, og saa
         * meldte den en skyder med en udvidet flade som en fejl. Nu
         * spoerger den LVGL hvor man kan trykke.
         */
        lv_area_t k;
        lv_obj_get_click_area(o, &k);
        lv_coord_t kw = lv_area_get_width(&k);
        lv_coord_t kh = lv_area_get_height(&k);

        if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)
            && (kw < ZS_G_TOUCH_MIN || kh < ZS_G_TOUCH_MIN)
            && lv_obj_has_flag(o, ZS_FLAG_LILLE_MED_VILJE)) {
            /* Begrundet undtagelse. Den taelles, saa den staar i
             * udskriften og ikke bare forsvinder. */
            s_undtaget++;
        } else if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)
            && (kw < ZS_G_TOUCH_MIN || kh < ZS_G_TOUCH_MIN)) {
            ZS_LOGW(TAG, "%s: %s kan trykkes paa, men flade er kun "
                         "%d x %d, og en finger kraever %d",
                    side, hvem(o, sti), (int)kw, (int)kh,
                    (int)ZS_G_TOUCH_MIN);
            s_fund++;
        }
    }

    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) {
        char ned[48];
        snprintf(ned, sizeof(ned), "%s.%u", sti, (unsigned)i);
        maal_et(side, lv_obj_get_child(o, i), dybde + 1, ned);
    }
}

int zs_ui_maal_graenser(void)
{
    s_fund = 0;
    s_undtaget = 0;
    int sider = 0;

    for (int id = 0; id <= (int)ZS_SCREEN_FEJL; id++) {
        lv_obj_t *rod = zs_ui_root_of((zs_screen_id_t)id);
        if (rod == NULL) {
            continue;
        }
        /* Uden den her er koordinaterne ikke regnet ud endnu, og saa
         * maaler vi paa nuller. */
        lv_obj_update_layout(rod);
        maal_et(side_navn((zs_screen_id_t)id), rod, 0, "rod");
        sider++;
    }

    if (s_fund == 0) {
        ZS_LOGI(TAG, "%d sider maalt, intet stikker ud og alt kan "
                     "rammes (%d begrundede undtagelser)",
                sider, s_undtaget);
    } else {
        ZS_LOGE(TAG, "%d sider maalt, %d fund og %d begrundede "
                     "undtagelser. Se linjerne ovenfor",
                sider, s_fund, s_undtaget);
    }
    return s_fund;
}
