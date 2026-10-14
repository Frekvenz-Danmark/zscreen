#include "zs_screen_fejl.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "zs_app.h"
#include "zs_config.h"
#include "zs_display.h"
#include "zs_screen_setup.h"
#include "zs_theme.h"
#include "zs_ui.h"

static const char *TAG = "fejl";

static zs_page_t    s_side;
static lv_obj_t    *s_over;      /* den store linje                    */
static lv_obj_t    *s_under;     /* forklaringen                       */
static lv_obj_t    *s_knap;
static lv_obj_t    *s_knap_tekst;
static lv_obj_t    *s_ikon;

static zs_fejl_type_t s_type = ZS_FEJL_WIFI;
/*
 * Klokkeslaettet for den sidste maaling, fx "14:05". Tom hvis vi ikke
 * har et ur.
 *
 * Den GEMMES, og det er ikke pynt: et temaskift river siden ned og
 * bygger den forfra, og uden den her ville den nye side staa med sin
 * standardtekst - altsaa wifi-beskeden - indtil appen fik fyldt den
 * igen et par sekunder senere. Skiftede kunden tema mens anlaegget var
 * nede, ville siden altsaa sige det forkerte i to sekunder.
 */
static char           s_tid[8];
static bool           s_vist;    /* staar siden paa skaermen lige nu    */
static bool           s_blink;   /* skal den pulsere                    */
static bool           s_puls;    /* koerer animationen                  */

/*
 * HVOR HURTIGT DEN PULSERER.
 *
 * En hel omgang er ZS_ALARM_PERIOD_MS, det SAMME tal som baglyset
 * bruger. Det er med vilje: lyset og teksten aander sammen, og to
 * takter der ligger taet uden at vaere ens, ser ud som en fejl i
 * skaermen og ikke som en besked.
 *
 * Det er ikke et alarmblink der skal haste nogen - der er ingenting
 * kunden kan goere hurtigt - det er et der skal faa nogen til at gaa
 * derhen i loebet af dagen.
 */

/*
 * HELE FLADEN FLASHER ROEDT, mellem en lys og en moerk roed.
 *
 * Fire udgaver, og de tre foerste var forkerte:
 *
 * 1. FOERST skjulte den linjen hvert andet halve sekund. Det ser ud som
 *    en skaerm der er i stykker, og vaerre: teksten kan ikke laeses af
 *    en der kigger paa det forkerte tidspunkt.
 *
 * 2. SAA skiftede teksten FARVE, mellem ZS_C_BAD og ZS_C_TEXT_DIM. Det
 *    virkede i moerkt og lyst tema - og var HELT USYNLIGT i det roede,
 *    hvor de to farver er 0xFFFFFF og 0xFEF8F8. En puls man ikke kan
 *    se, er lige saa god som ingen puls, og det var der ingen der
 *    opdagede, fordi det roede tema ikke er det man udvikler i.
 *
 * 3. SAA pulserede teksten og en ramme i gennemsigtighed. Det kunne ses
 *    i alle tre temaer - men kun hvis man kiggede paa skaermen. En
 *    seks pixels kant kan ikke ses fra den anden ende af et koekken.
 *
 * 4. NU er det HELE FLADEN. Den gaar fra den lyse roed til den moerke og
 *    tilbage, og det kan ses i oejenkrogen fra et andet rum.
 *
 * DEN ER ROED HELE TIDEN, og det er ikke et detalje. Gik den fra roed
 * til temaets egen bund, ville halvdelen af blinket se ud som en helt
 * almindelig side - og teksten skulle skifte farve midt i pulsen for at
 * kunne laeses. Nu er den hvid hele vejen, og det er MAALT: hvid paa den
 * lyse roed er 4,75:1 og paa den moerke 10,02:1. Begge over de 4,50 som
 * tekst kraever.
 *
 * Og den LYSE ende er den der binder. En lysere roed - fx #E04B4B -
 * giver kun 3,98:1 for hvid tekst, og saa kan en kunde ikke laese hvad
 * der er galt. Der staar en vagt paa det i tools/check-colors.py.
 *
 * SAMME VAERDI SAETTER BAGLYSET. Se zs_display_alarm_niveau: lys roed
 * paa fuldt lys, moerk roed paa daempet, én fase. To ure ville glide fra
 * hinanden.
 */

/*
 * Skriver alle fire tekster ud fra s_type og s_tid.
 *
 * ÉT sted, som baade create og update bruger. To kopier af det her
 * ville betyde at en side bygget forfra efter et temaskift kunne komme
 * til at staa med noget andet end den samme side gjorde et sekund foer.
 */
/* Fejlens ikon som en streng. Opregningen bor i motoren, strengene
 * her - se noten ved zs_fejl_ikon_t. */
static const char *ikonet(zs_fejl_type_t t)
{
    switch (zs_fejl_ikon(t)) {
    case ZS_FEJL_IKON_BATTERI: return ZS_ICON_BATTERY;
    case ZS_FEJL_IKON_INTET:   break;
    }
    return "";
}

/*
 * STILLER DE TRE TING OP, MIDT PAA SIDEN.
 *
 * HVORFOR DET REGNES HER OG IKKE I zs_layout.h.
 *
 * Overskriften er én linje paa to af fejlene og to linjer paa den
 * tredje, og forklaringen er to linjer paa ZDisplay og kan blive tre
 * paa en anden skaerm. Hoejden af stakken er altsaa ikke et tal man
 * kan skrive ned - den afhaenger af teksten OG af skaermens bredde.
 *
 * Foerst stod der faste hoejder, og saa laa luften INDE i labelet:
 * overskriftens kasse var 213 px om 60 px tekst paa ZDisplay, saa
 * afstanden ned til ikonet maaltes fra en kant man ikke kan se. Ikonet
 * laa langt nede selvom begge mellemrum var det samme tal.
 *
 * Nu er labelerne LV_SIZE_CONTENT, vi SPOERGER LVGL hvor hoeje de blev,
 * og stilles op efter det. Saa er afstanden over og under ikonet den
 * samme paa glasset og ikke kun i koden.
 *
 * Kalderen holder LVGL-laasen, se zs_ui_set_fejl.
 */
static void stak_op(void)
{
    if (s_over == NULL || s_under == NULL) {
        return;
    }

    const char *ik = ikonet(s_type);
    bool har_ikon = (ik != NULL && ik[0] != '\0');
    if (s_ikon != NULL) {
        lv_label_set_text(s_ikon, har_ikon ? ik : "");
        if (har_ikon) {
            lv_obj_clear_flag(s_ikon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ikon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Uden den her er hoejderne fra FOER teksten blev sat. */
    lv_obj_update_layout(s_side.content);

    lv_coord_t h_over  = lv_obj_get_height(s_over);
    lv_coord_t h_under = lv_obj_get_height(s_under);
    lv_coord_t h_ikon  = (har_ikon && s_ikon != NULL)
                       ? lv_obj_get_height(s_ikon) : 0;

    lv_coord_t luft = har_ikon ? ZS_G_FEJL_IKON_GAP : ZS_G_FEJL_GAP;
    lv_coord_t stak = h_over + luft + h_under
                    + (har_ikon ? (h_ikon + luft) : 0);

    /*
     * Midt paa siden. Bliver stakken hoejere end siden - en meget lang
     * forklaring paa en meget lav skaerm - laegges den oeverst i stedet
     * for at blive skubbet op over sidehovedet.
     */
    lv_coord_t top = (ZS_G_FEJL_CONTENT_HEIGHT - stak) / 2;
    if (top < 0) {
        top = 0;
    }

    lv_obj_align(s_over, LV_ALIGN_TOP_MID, 0, top);
    lv_coord_t y = top + h_over + luft;
    if (har_ikon && s_ikon != NULL) {
        lv_obj_align(s_ikon, LV_ALIGN_TOP_MID, 0, y);
        y += h_ikon + luft;
    }
    lv_obj_align(s_under, LV_ALIGN_TOP_MID, 0, y);
}

static void skriv_teksten(void)
{
    if (s_over == NULL) {
        return;
    }

    lv_label_set_text(s_side.title, zs_fejl_titel(s_type));
    lv_label_set_text(s_over, zs_fejl_overskrift(s_type));
    /*
     * Overskriftens farve vaelges IKKE laengere af alvoren. Siden vises
     * kun for de to fejl der ER alvorlige - soevn har ingen side - og
     * paa en roed flade er der kun én farve der kan laeses: hvid. Se
     * noten ved forklaringen.
     */
    if (s_knap_tekst != NULL) {
        lv_label_set_text(s_knap_tekst, zs_fejl_knap(s_type));
    }
    /*
     * EN KNAP UDEN HANDLING SKAL IKKE STAA DER.
     *
     * "Ingen strøm" har ingen knap: kablet skal i, og det kan kun et
     * menneske. Teksten blev sat uanset hvad, saa en tom streng gav en
     * tom roed bjaelke nederst paa siden - og en knap man kan trykke
     * paa uden at der sker noget, er det der gOEr at man holder op med
     * at tro paa siden.
     */
    if (s_knap != NULL) {
        const char *k = zs_fejl_knap(s_type);
        if (k == NULL || k[0] == '\0') {
            lv_obj_add_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_knap, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /*
     * Forklaringen, og klokkeslaettet hvis vi har et.
     *
     * Der staar IKKE et tidspunkt naar uret ikke er sat. En skaerm der
     * aldrig har haft internet, har et ur der staar i 1970, og "fra kl.
     * 01:00" ville vaere det eneste forkerte paa hele siden.
     */
    /*
     * HALEN HOERER IKKE TIL PAA HVER FEJL.
     *
     * Her stod den paa alle: "Tallene paa forsiden er fra kl. X og
     * staar stille." Paa stroemsvigt er det ikke sandt - skaermen
     * koerer videre paa batteriet, er paa netvaerket og laeser
     * inverteren, saa tallene er friske. Den ene saetning var det
     * eneste forkerte paa hele siden, og den stod der fordi halen var
     * skrevet ind ét sted for alle.
     *
     * Nu spoerger vi fejlen selv, se zs_fejl_tal_staar_stille.
     */
    if (s_tid[0] != '\0' && zs_fejl_tal_staar_stille(s_type)) {
        char t[200];
        snprintf(t, sizeof(t),
                 "%s Tallene på forsiden er fra kl. %s og står stille.",
                 zs_fejl_forklaring(s_type), s_tid);
        lv_label_set_text(s_under, t);
    } else {
        lv_label_set_text(s_under, zs_fejl_forklaring(s_type));
    }

    stak_op();
}

/*
 * Ét trin af pulsen. v er 255 paa den lyse roed og 0 paa den moerke.
 *
 * ÉN funktion saetter baade farven og baglyset, af den samme v. Var det
 * to animationer, ville de ikke starte i samme billede og glide fra
 * hinanden i loebet af minutter.
 */
static void puls_exec(void *var, int32_t v)
{
    (void)var;
    if (s_side.root == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(
        s_side.root,
        lv_color_mix(lv_color_hex(ZS_C_ALARM_LYS),
                     lv_color_hex(ZS_C_ALARM_MOERK), (uint8_t)v),
        LV_PART_MAIN);
    zs_display_alarm_niveau((uint8_t)v);
}

/* Tilbage til den lyse roed, og baglyset afleveres. */
static void puls_stop(void)
{
    if (s_puls) {
        lv_anim_del(s_side.root, puls_exec);
        s_puls = false;
    }
    if (s_side.root != NULL) {
        lv_obj_set_style_bg_color(s_side.root,
                                  lv_color_hex(ZS_C_ALARM_LYS), LV_PART_MAIN);
    }
    /*
     * Baglyset gives tilbage HER, af den samme funktion der tog det.
     * Det var foer app-loekkens opgave, og den havde en gren hvor den
     * glemte det. Se zs_display_set_alarm.
     */
    zs_display_set_alarm(false);
}

static void puls_start(void)
{
    if (s_puls || s_side.root == NULL) {
        return;
    }
    zs_display_set_alarm(true);
    /*
     * LVGL's egen animation, og ikke en lv_timer der skifter en farve.
     *
     * Det er ikke kun kortere kode: den regner sin egen
     * bevaegelseskurve, og den har playback indbygget - saa vejen ned og
     * vejen op er nOEjagtigt lige lange uden at vi selv skal holde styr
     * paa hvilken halvdel vi er i.
     *
     * MEN DEN STOPPER IKKE AF SIG SELV NAAR SIDEN SKJULES. Her stod der
     * foer at LVGL satte den paa pause - det gOEr den ikke. En lv_anim
     * koerer videre paa et skjult objekt, saa skaermen ville blive tegnet
     * om i hvert billede for en bevaegelse ingen kan se, og baglyset
     * ville blive staaende hos zs_display_alarm_niveau. Derfor kalder
     * zs_ui.c zs_fejl_page_set_active(false) naar siden forlades, og
     * derfor staar fejlsiden i zs_selftest.c. Fjerner man det, siver
     * der baade stroem og hukommelse.
     */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_side.root);
    lv_anim_set_exec_cb(&a, puls_exec);
    /*
     * FRA LYS ROED og NED, ikke omvendt. Siden kommer frem i samme
     * oejeblik animationen starter, og det foerste et menneske ser, skal
     * vaere beskeden paa fuldt lys - ikke den moerkeste halvdel.
     */
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, ZS_ALARM_PERIOD_MS / 2);
    lv_anim_set_playback_time(&a, ZS_ALARM_PERIOD_MS / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    /* BlOEd i begge ender. En lineaer puls ser mekanisk ud, som en
     * blinkende lysdiode; den her ser ud som noget der aander. */
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
    s_puls = true;
}

/*
 * Knappen. Hvor den foerer hen, afhaenger af HVAD der er galt - og det
 * er hele grunden til at siden kender sin type.
 */
static void on_fix(lv_event_t *e)
{
    (void)e;

    /*
     * BEGGE VEJE ER EN REPARATION. Skaermen koerer allerede, og kunden
     * bad ikke om en opsaetning - hun bad om at tallene kom tilbage.
     * Oprindelsen er et argument til de to indgange herunder, saa den
     * ikke kan glemmes i den ene gren. Det var den foer.
     */
    if (s_type == ZS_FEJL_WIFI) {
        /*
         * Samme kommando som "Netvaerk" i Indstillinger bruger. Appen
         * kobler af anlaegget, gaar i opsaetningstilstand, viser
         * netvaerkslisten og soeger. Vi vaelger IKKE siden her: appen er
         * den eneste der ved om der er en opsaetning at gaa tilbage til,
         * og den slipper selv tilstanden igen.
         */
        ESP_LOGI(TAG, "kunden trykkede paa knappen: vaelg netvaerk");
        zs_setup_skift_netvaerk(ZS_INV_FRA_REPARATION);
        return;
    }

    /*
     * Det er anlaegget der er vaek, og saa er der ingen grund til at
     * bede kunden om et wifi-kodeord hun ikke har brug for. Vi leder
     * efter anlaegget i stedet.
     *
     * OPRINDELSEN SKAL MED. De tre inverter-sider bruges tre steder nu,
     * og de skal ende tre forskellige steder: en opsaetning slutter med
     * tilmeldingen, et skift fra Indstillinger gaar tilbage dertil, og
     * en reparation herfra skal tilbage til HOVEDSIDEN - hun stod paa
     * forsiden da det gik galt. Uden det ville en kunde der retter en
     * forbindelse, faa "Anlægget er tilmeldt" og en ceremoni for noget
     * der skete for et halvt aar siden.
     *
     * Siden skiftes HER og kommandoen sendes bagefter: scanningen tager
     * omkring tyve sekunder, og i det tidsrum laeser appen ikke
     * kommandoer. Ventede vi paa den, ville der gaa tyve sekunder hvor
     * et tryk ikke gjorde noget synligt.
     */
    ESP_LOGI(TAG, "kunden trykkede paa knappen: soeg efter anlaegget");
    zs_setup_soeg_inverter(ZS_INV_FRA_REPARATION);
}

void zs_fejl_page_create(void)
{
    /*
     * Ingen tilbageknap. Der er ikke noget at gaa tilbage TIL: siden
     * vises netop fordi hovedsiden ikke har noget at vise. Vejen ud er
     * knappen, og appen viser hovedsiden af sig selv i samme sekund som
     * forbindelsen er tilbage.
     *
     * MED en fod. Knappen staar dér, som paa alle de andre sider i
     * fladen. FOer laa den midt i indholdet med et haandskrevet spring
     * paa 92 px, og saa stod den ét sted paa den her side og et andet
     * sted alle andre steder.
     */
    zs_page_create(&s_side, zs_fejl_titel(s_type), NULL, NULL, true);

    /*
     * HELE FLADEN ER ROED, ogsaa naar den staar stille.
     *
     * zs_page_create gav den temaets bund. Her males den om, og
     * animationen flytter den mellem de to roede. Titlen foroven bliver
     * hvid af samme grund som resten: temaets tekstfarve er moerkegroen
     * i lyst tema, og den kan ikke laeses paa roedt.
     */
    lv_obj_set_style_bg_color(s_side.root, lv_color_hex(ZS_C_ALARM_LYS),
                              LV_PART_MAIN);
    lv_obj_set_style_text_color(s_side.title,
                                lv_color_hex(ZS_C_ALARM_TEKST),
                                LV_PART_MAIN);

    /*
     * Alt staar midt paa siden, baade vandret og lodret. Tallene er
     * regnet i zs_layout.h, hvor der ogsaa staar en vagt paa at blokken
     * kan vaere der og staar i midten.
     *
     * Siden har ingen tal og ingen liste, saa der er ikke noget at
     * stille op efter. En blok oppe i hjoernet paa en ellers tom side
     * ser ud som om der mangler noget under den.
     */
    s_over = lv_label_create(s_side.content);
    lv_label_set_text(s_over, zs_fejl_overskrift(s_type));
    zs_style_text(s_over, &zs_font_stor, ZS_C_ALARM_TEKST);
    lv_label_set_long_mode(s_over, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_over, ZS_CONTENT_WIDTH);
    /*
     * HOEJDEN ER BLAEKKETS, ikke en reserveret kasse.
     *
     * Her stod ZS_G_FEJL_HEAD_HEIGHT, altsaa 213 px paa ZDisplay - og
     * overskriften er 60 px hoej med én linje. De 153 px luft laa
     * INDE i labelet, saa afstanden ned til ikonet maaltes fra en kant
     * der ikke var der at se. Resultatet var et ikon der laa alt for
     * langt nede, selvom begge mellemrum var sat til det samme tal.
     *
     * Med LV_SIZE_CONTENT er labelets kant der hvor teksten slutter,
     * og saa kan stakken stilles op efter det man kan SE. Den regnes i
     * stak_op() nedenfor.
     */
    lv_obj_set_height(s_over, LV_SIZE_CONTENT);
    /* Luft mellem de to linjer i overskriften. Uden den klistrer
     * "Tilslut den igen." op under linjen over. */
    lv_obj_set_style_text_line_space(s_over, ZS_G_FEJL_LINJE_LUFT, 0);
    lv_obj_set_style_text_align(s_over, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    /* Placeringen saettes af stak_op() nedenfor, naar teksten er
     * paa plads og hoejderne kan maales. */

    /*
     * Der staar HVAD det betyder, og ikke bare at der er en fejl.
     *
     * "Ingen forbindelse" alene faar en kunde til at tro at skaermen er
     * i stykker. Det hun skal vide, er at tallene paa hovedsiden ikke er
     * friske - det er det eneste der har en foelge for hende. Og staar
     * der et klokkeslaet, ved hun ogsaa HVOR gammelt det hun lige
     * kiggede paa, var.
     */
    /*
     * Ikonet mellem overskriften og forklaringen. Kun nogle fejl har
     * et - se zs_fejl_ikon - og har den ikke, skjules labelet og
     * blokken staar hvor den altid har staaet. Tallene er regnet i
     * zs_layout.h, med vagter paa at ikonet ligger MELLEM de to og at
     * hele blokken kan vaere paa siden.
     */
    s_ikon = lv_label_create(s_side.content);
    lv_label_set_text(s_ikon, ikonet(s_type));
    lv_obj_set_style_text_font(s_ikon, &zs_ikon_kaempe, 0);
    lv_obj_set_style_text_color(s_ikon, lv_color_hex(ZS_C_ALARM_TEKST), 0);
    lv_obj_set_style_text_align(s_ikon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(s_ikon, ZS_CONTENT_WIDTH);


    s_under = lv_label_create(s_side.content);
    lv_label_set_text(s_under, zs_fejl_forklaring(s_type));
    /*
     * OGSAA HVID, og ikke en daempet farve.
     *
     * Forklaringen var ZS_C_LABEL, altsaa den daempede. Paa den lyse
     * roed holder ingen daempet hvid: #FEF8F8 er 4,52:1 og alt under den
     * falder igennem - #FFF1F1 giver 4,32. Maalt 14. oktober 2026.
     *
     * Hierarkiet kommer derfor fra STOERRELSEN, 28 px mod 16 px, og ikke
     * fra farven. Det er en foelge af fladen, ikke en forglemmelse.
     */
    zs_style_text(s_under, &zs_font_broed, ZS_C_ALARM_TEKST);
    lv_label_set_long_mode(s_under, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_under, ZS_CONTENT_WIDTH);
    lv_obj_set_height(s_under, LV_SIZE_CONTENT);   /* se noten ovenfor */
    lv_obj_set_style_text_align(s_under, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);


    /* Hvid flade, moerkeroed skrift. Se zs_btn_alarm_create. */
    s_knap = zs_btn_alarm_create(s_side.footer, zs_fejl_knap(s_type));
    lv_obj_set_width(s_knap, ZS_CONTENT_WIDTH);
    lv_obj_center(s_knap);
    lv_obj_add_event_cb(s_knap, on_fix, LV_EVENT_CLICKED, NULL);
    /* Knappens tekst skal kunne skiftes. zs_btn_primary_create saetter
     * en label som eneste barn. */
    s_knap_tekst = lv_obj_get_child(s_knap, 0);

    /* Og saa stilles de tre ting op efter deres MAALTE hoejder. Den
     * samme funktion koerer igen hver gang teksten skifter. */
    stak_op();


    /*
     * Og saa den besked der stod paa siden foer den blev revet ned.
     * Objekterne ovenfor er bygget med s_type, men klokkeslaettet skal
     * ogsaa med, og det staar kun ét sted: i skriv_teksten.
     */
    skriv_teksten();
}

lv_obj_t *zs_fejl_page_root(void)
{
    return s_side.root;
}

void zs_fejl_page_update(zs_fejl_type_t type, const char *tid, bool blink)
{
    if (s_over == NULL) {
        return;                 /* ikke bygget endnu */
    }

    s_type  = type;
    s_blink = blink;
    snprintf(s_tid, sizeof(s_tid), "%s", tid != NULL ? tid : "");

    skriv_teksten();

    /* Pulsen foelger baade om siden vises og om der SKAL blinkes. */
    if (s_vist && s_blink) {
        puls_start();
    } else {
        puls_stop();
    }
}

void zs_fejl_page_set_active(bool aktiv)
{
    if (s_over == NULL) {
        return;
    }
    s_vist = aktiv;
    if (aktiv && s_blink) {
        puls_start();
    } else {
        puls_stop();
    }
}

void zs_fejl_page_destroy(void)
{
    /*
     * Animationen SKAL vaek foer objekterne.
     *
     * Et temaskift river alle sider ned og bygger dem op igen. Blev
     * animationen staaende, ville den blive ved med at kalde puls_exec
     * med pegepinde ind i objekter der er slettet - og det er ikke en
     * fejl man ser med det samme, det er en der vaelter skaermen et
     * tilfaeldigt sted senere.
     */
    puls_stop();
    if (s_side.root != NULL) {
        lv_obj_del(s_side.root);
    }
    memset(&s_side, 0, sizeof(s_side));
    s_over = NULL;
    s_under = NULL;
    s_knap = NULL;
    s_ikon = NULL;
    s_knap_tekst = NULL;
    s_vist = false;
    /*
     * s_blink ryddes IKKE. Det er den samme fejl der staar, ogsaa efter
     * et temaskift - og nulstillede vi den, ville siden blive bygget op
     * igen uden puls og give baglyset tilbage, indtil appen tilfaeldigvis
     * sendte noget nyt. s_type og s_tid overlever af samme grund, og
     * create kalder selv skriv_teksten.
     */
    s_puls = false;
}
