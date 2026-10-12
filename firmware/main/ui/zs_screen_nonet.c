#include "zs_screen_nonet.h"

#include <string.h>

#include "esp_log.h"
#include "zs_app.h"
#include "zs_theme.h"

static const char *TAG = "nonet";

static zs_page_t    s_side;
static lv_obj_t    *s_over;      /* den store linje, den der blinker  */
static lv_obj_t    *s_under;     /* forklaringen                      */
static lv_obj_t    *s_knap;
static lv_timer_t  *s_blink;
static bool         s_taendt;    /* hvor blinket staar lige nu        */

/*
 * HVOR HURTIGT DEN BLINKER.
 *
 * 700 ms taendt, 700 ms daempet. Hurtigere blev det uroligt at staa i
 * et koekken med, og langsommere blev det noget man kunne gaa forbi
 * uden at se det. Det er ikke et alarmblink der skal haste nogen - der
 * er ingenting kunden kan goere hurtigt - det er et der skal faa nogen
 * til at gaa derhen i loebet af dagen.
 */
#define BLINK_MS  700

static void send_cmd(zs_cmd_type_t type)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = type;
    zs_app_send(&c);
}

/*
 * Blinket er en FARVE, ikke en skjulning.
 *
 * Foerste udgave skjulte linjen hvert andet halve sekund. Det ser ud
 * som en skaerm der er i stykker, og vaerre: teksten kan ikke laeses af
 * en der kigger paa det forkerte tidspunkt. Nu staar bogstaverne stille
 * og skifter mellem den fulde roede og en daempet, saa beskeden kan
 * laeses hele tiden.
 */
static void blink_tik(lv_timer_t *t)
{
    (void)t;
    if (s_over == NULL) {
        return;
    }
    s_taendt = !s_taendt;
    lv_obj_set_style_text_color(
        s_over, lv_color_hex(s_taendt ? ZS_C_BAD : ZS_C_TEXT_DIM),
        LV_PART_MAIN);
}

static void on_fix(lv_event_t *e)
{
    (void)e;
    /*
     * Samme kommando som "Netvaerk" i Indstillinger bruger. App'en
     * kobler af inverteren, gaar i opsaetningstilstand, viser
     * netvaerkslisten og soeger. Vi vaelger IKKE siden her: app'en er
     * den eneste der ved om der er en opsaetning at gaa tilbage til,
     * og den slipper selv tilstanden igen.
     */
    ESP_LOGI(TAG, "kunden trykkede Fix fejl");
    send_cmd(ZS_CMD_SETUP_RESTART);
}

void zs_nonet_page_create(void)
{
    /*
     * Ingen tilbageknap. Der er ikke noget at gaa tilbage TIL: siden
     * vises netop fordi hovedsiden ikke har noget at vise. Vejen ud er
     * knappen, og app'en viser hovedsiden af sig selv i samme sekund
     * som netvaerket er tilbage.
     */
    zs_page_create(&s_side, "Ingen forbindelse", NULL, NULL, false);

    /*
     * Alt staar midt paa siden, baade vandret og lodret.
     *
     * Siden har ingen tal og ingen liste, saa der er ikke noget at
     * stille op efter. En blok oppe i hjoernet paa en ellers tom side
     * ser ud som om der mangler noget under den.
     */
    s_over = lv_label_create(s_side.content);
    lv_label_set_text(s_over, "Skærmen er ikke på netværket");
    zs_style_text(s_over, &zs_font_28, ZS_C_BAD);
    lv_label_set_long_mode(s_over, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_over, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(s_over, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_over, LV_ALIGN_CENTER, 0, -56);

    /*
     * Der staar HVAD det betyder, og ikke bare at der er en fejl.
     *
     * "Ingen forbindelse" alene faar en kunde til at tro at skaermen er
     * i stykker. Det den skal vide, er at tallene paa hovedsiden ikke
     * er friske - det er det eneste der har en foelge for hende.
     */
    s_under = lv_label_create(s_side.content);
    lv_label_set_text(s_under,
        "Tallene fra anlægget kan ikke hentes, før skærmen er "
        "på netværket igen.");
    zs_style_text(s_under, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(s_under, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_under, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(s_under, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_under, LV_ALIGN_CENTER, 0, 8);

    s_knap = zs_btn_primary_create(s_side.content, "Fix fejl");
    lv_obj_set_width(s_knap, ZS_CONTENT_WIDTH);
    lv_obj_align(s_knap, LV_ALIGN_CENTER, 0, 92);
    lv_obj_add_event_cb(s_knap, on_fix, LV_EVENT_CLICKED, NULL);
}

lv_obj_t *zs_nonet_page_root(void)
{
    return s_side.root;
}

void zs_nonet_page_set_active(bool aktiv)
{
    if (s_over == NULL) {
        return;             /* ikke bygget endnu */
    }
    if (aktiv) {
        if (s_blink == NULL) {
            s_blink = lv_timer_create(blink_tik, BLINK_MS, NULL);
        }
        return;
    }
    if (s_blink != NULL) {
        lv_timer_del(s_blink);
        s_blink = NULL;
    }
    /*
     * Og farven saettes tilbage til den fulde roede naar siden
     * forlades. Ellers stod den i den daempede naeste gang siden blev
     * vist, hvis blinket tilfaeldigvis blev stoppet paa den halvdel -
     * og saa saa den foerste halve sekund ud som noget andet.
     */
    s_taendt = true;
    lv_obj_set_style_text_color(s_over, lv_color_hex(ZS_C_BAD),
                                LV_PART_MAIN);
}
