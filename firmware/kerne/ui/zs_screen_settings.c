#include "zs_screen_settings.h"
#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_stroem.h"
#include "zs_app.h"
#include "zs_config.h"
#include "zs_screen_setup.h"
#include "zs_format.h"
#include "../net/zs_fleet.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

static zs_page_t s_set, s_det;

static zs_row_t  s_row_wifi;
static zs_row_t  s_row_inv;
static lv_obj_t *s_slider;
static lv_obj_t *s_slider_val;
static lv_obj_t *s_sw_night;
static lv_obj_t *s_sw_alarm;
static zs_page_t s_avan;
static lv_obj_t *s_sw_meter;
static lv_obj_t *s_det_col;
static zs_row_t  s_row_demo;
static zs_row_t  s_row_zone;
static zs_row_t  s_row_tema;
static zs_row_t  s_row_tilmeld;
static zs_page_t s_tema;
static zs_row_t  s_row_soft;

/* ------------------------------------------------------------------ */
/* Smaa byggeklodser                                                   */
/* ------------------------------------------------------------------ */

static void send_simple(zs_cmd_type_t t)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = t;
    zs_app_send(&c);
}

/* En raekke med en kontakt til hoejre. */
static void make_switch_row(lv_obj_t *parent, const char *icon,
                            const char *title, const char *hint,
                            lv_event_cb_t cb, lv_obj_t **out_sw)
{
    zs_row_t r;
    zs_row_create(&r, parent, icon, title, NULL, false);
    lv_obj_t *row = r.row;
    lv_obj_set_width(row, ZS_CONTENT_WIDTH);
    lv_obj_set_height(row, hint != NULL ? ZS_G_SWROW_HEIGHT : ZS_ROW_HEIGHT);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, ZS_G_SWITCH_WIDTH, ZS_G_SWITCH_HEIGHT);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_ACCENT),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ZS_C_BG), LV_PART_KNOB);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (out_sw != NULL) {
        *out_sw = sw;
    }

    /*
     * TITLEN SKAL OGSAA VIGE FOR KONTAKTEN.
     *
     * zs_row_create regner titlens bredde af hvad der staar til
     * HOEJRE - en vaerdi eller en pil. Her er der ingen af delene,
     * saa den fik hele resten af raekken. Men der ER noget til
     * hoejre: kontakten, som laegges paa bagefter.
     *
     * Hjaelpeteksten nedenfor trak den plads fra, titlen gjorde ikke.
     * Paa raekken "Batteri-alarm" - den foerste uden hjaelpetekst - er
     * titlens kasse derfor 652 px paa ZDisplay og gaar ind under
     * kontaktens 86. Ordet er kort nok i dag, men en laengere
     * overskrift eller en oversaettelse ville blive tegnet UNDER
     * kontakten i stedet for at faa sine tre prikker.
     */
    if (r.title != NULL) {
        lv_obj_set_width(r.title, ZS_CONTENT_WIDTH - 2 * ZS_G_ROW_PAD
                                  - ZS_ROW_ICON_W - ZS_G_SWITCH_PLADS);
    }

    if (hint != NULL) {
        if (r.title != NULL) {
            lv_obj_align(r.title, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W,
                         ZS_G_SWROW_TITEL_DY);
        }
        lv_obj_t *h = lv_label_create(row);
        lv_label_set_text(h, hint);
        zs_style_text(h, &zs_font_broed, ZS_C_LABEL);
        lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
        /* Bredden er raden minus luft, ikonet og kontakten til hoejre. */
        lv_obj_set_width(h, ZS_CONTENT_WIDTH - 2 * ZS_G_ROW_PAD
                            - ZS_ROW_ICON_W - ZS_G_SWITCH_PLADS);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W,
                     ZS_G_SWROW_HJAELP_DY);
    }
}

/* En linje paa Detaljer-siden: etiket til venstre, vaerdi til hoejre. */
static void detail_line(lv_obj_t *parent, const char *label, const char *value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, ZS_CONTENT_WIDTH);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    zs_style_text(l, &zs_font_broed, ZS_C_LABEL);
    lv_obj_set_width(l, ZS_G_SK_WIDTH(170));
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, (value != NULL && value[0] != '\0') ? value : "-");
    zs_style_text(v, &zs_font_broed, ZS_C_TEXT);
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(v, ZS_CONTENT_WIDTH - ZS_G_SK_WIDTH(178));
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, ZS_G_SK_WIDTH(178), 0);
}

static void detail_heading(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = zs_label_create(parent, text);
    lv_obj_set_style_pad_top(l, ZS_G_SK_HEIGHT(14), 0);
}

/* ------------------------------------------------------------------ */
/* Indstillinger                                                       */
/* ------------------------------------------------------------------ */

static void on_set_back(lv_event_t *e)  { (void)e; zs_ui_show(ZS_SCREEN_HOME); }
static void on_det_back(lv_event_t *e)  { (void)e; zs_ui_show(ZS_SCREEN_SETTINGS); }

static void on_open_details(lv_event_t *e) { (void)e; zs_ui_show(ZS_SCREEN_DETAILS); }
static void on_open_avanceret(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_AVANCERET);
}

static void on_change_wifi(lv_event_t *e)
{
    (void)e;
    /*
     * DET HER ER EN REPARATION, ikke en ny opsaetning.
     *
     * Raekken fOErer gennem HELE opsaetningen - netvaerk, kodeord,
     * soegning, valg af inverter - paa en skaerm der allerede koerer.
     * Oprindelsen skal derfor saettes, og den blev det ikke: flowet
     * brugte hvad der tilfaeldigvis stod fra sidste gang nogen havde
     * vaeret inde i det. Havde ingen vaeret det siden opstart, stod der
     * OPSAETNING, og saa sluttede et netvaerksskift med
     * "Anlægget er tilmeldt" og en ny kode.
     *
     * Samme vaerdi som fejlsidens "Vælg netværk" bruger. De to gOEr
     * det samme, saa de skal ende det samme sted. Se zs_inv_fra_t.
     */
    zs_setup_skift_netvaerk(ZS_INV_FRA_REPARATION);
}

static void on_change_tema(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_THEME);
}

static void on_tilmeld(lv_event_t *e)
{
    (void)e;
    /* Siden bliver aabnet. Koden laves nu og doer naar den forlades. */
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_TILMELD_VIS;
    c.flag = true;
    zs_app_send(&c);
    zs_ui_show(ZS_SCREEN_TILMELD);
}

static void on_tema_tilbage(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_SETTINGS);
}

static void on_tema_valgt(lv_event_t *e)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_THEME;
    c.u8   = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    zs_app_send(&c);
    /*
     * Vi gaar tilbage med det samme.
     *
     * Selve skiftet river alle sider ned og bygger dem op igen,
     * inklusive den her side og knappen der lige blev trykket paa. Vi
     * beder om siden FOER det sker, saa det er zs_ui_set_theme der
     * lander os paa Indstillinger i det nye tema. Blev vi staaende,
     * ville kunden se tema-siden tegnet om og skulle trykke tilbage
     * selv.
     */
    zs_ui_show(ZS_SCREEN_SETTINGS);
}

static void build_tema(void)
{
    zs_page_create(&s_tema, "Tema", on_tema_tilbage, NULL, false);

    lv_obj_t *h = lv_label_create(s_tema.content);
    lv_label_set_text(h, "Vælg hvordan skærmen skal se ud.");
    zs_style_text(h, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, ZS_CONTENT_WIDTH);
    lv_obj_set_pos(h, ZS_EDGE, ZS_G_SK_HEIGHT(16));

    /*
     * Én knap pr. tema, i den raekkefoelge de staar i opregningen.
     * Tilfoejes et tema, kommer det med her af sig selv: der er ingen
     * liste at huske at rette.
     */
    zs_theme_mode_t nu = zs_theme_mode();
    for (int i = 0; i < ZS_THEME_COUNT; i++) {
        zs_theme_mode_t m = (zs_theme_mode_t)i;
        lv_coord_t y = ZS_G_SK(56) + i * (ZS_CHOICE_HEIGHT + ZS_CHOICE_GAP);
        zs_choice_create(s_tema.content, zs_theme_name(m), zs_theme_hint(m),
                         y, m == nu, on_tema_valgt, (void *)(uintptr_t)i);
    }
}

lv_obj_t *zs_theme_page_root(void) { return s_tema.root; }

/* Skriver hvilket tema der er valgt, i raekken under Indstillinger. */
void zs_settings_set_theme(zs_theme_mode_t m)
{
    if (s_row_tema.value != NULL) {
        lv_label_set_text(s_row_tema.value, zs_theme_name(m));
    }
}

static void on_change_inverter(lv_event_t *e)
{
    (void)e;
    /*
     * SIG HVOR VI KOMMER FRA, FOER SIDEN VISES.
     *
     * De tre inverter-sider er de samme som opsaetningens. Uden det her
     * endte et skift herfra med "Anlægget er tilmeldt" og en tur til
     * forsiden, fordi appen altid koerte opsaetningens afslutning - og
     * tilbage-knappen foerte ned i opsaetningens liste over netvaerk.
     *
     * Her skal den bare skiftes, og saa tilbage hertil.
     */
    zs_setup_soeg_inverter(ZS_INV_FRA_INDSTILLINGER);
}

static void on_brightness(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int v = (int)lv_slider_get_value(sl);

    char t[8];
    snprintf(t, sizeof(t), "%d %%", v);
    lv_label_set_text(s_slider_val, t);

    /*
     * IKKE gennem koeen.
     *
     * LVGL sender en haendelse for hvert trin under et traek, saa et
     * traek fra 5 til 100 giver op mod halvfems vaerdier paa under et
     * sekund. Koeen holder otte og smider resten vaek uden at nogen ser
     * det. Var den sidste vaerdi blandt dem, stod skyderen paa ét og
     * skaermen lyste som noget andet. Se zs_app_set_brightness.
     */
    zs_app_set_brightness((uint8_t)v);
}

static void on_night(lv_event_t *e)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_NIGHT_DIM;
    c.flag = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    zs_app_send(&c);
}

static void on_meter_sign(lv_event_t *e)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_METER_SIGN;
    /* Kontakten hedder "Byt køb og salg". Er den slaaet TIL, skal
     * fortegnet vendes, altsaa er positiv IKKE laengere køb. */
    c.flag = !lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    zs_app_send(&c);
}

/*
 * FABRIKSNULSTILLINGENS SPOERGSMAAL.
 *
 * HVORFOR DEN IKKE ER EN lv_msgbox LAENGERE.
 *
 * Den var det, og de to knapper saa forkerte ud. Grunden er ikke
 * stilarbejde der mangler: en lv_msgbox laver sine knapper som ÉT
 * knapgitter, og et knapgitter kan ikke farve én knap anderledes end de
 * andre. Saa stod "Nulstil" og "Fortryd" ens - samme baggrund, samme
 * tekstfarve - og den handling der sletter kundens wifi og inverter var
 * ikke til at skelne fra den der ikke goer noget.
 *
 * Oven i: gitteret satte dem side om side i en raekke paa 52 px med
 * LVGL's egen luft indeni, saa hver knap blev smallere og lavere end
 * ZS_TOUCH_MIN. To smaa knapper taet sammen, hvor den ene er farlig.
 *
 * Nu er den bygget af husets egne dele: et kort som paa alle andre
 * sider, og to knapper i fuld bredde oven paa hinanden - samme form som
 * "Søg igen" og "Find manuelt". Saa er der rød traad, og den farlige kan
 * faa sin egen farve.
 *
 * RAEKKEFOELGEN. Den farlige staar OEVERST, for den er svaret paa det
 * spoergsmaal man selv aabnede. Fortryd staar nederst, hvor en tommel
 * hviler, og den er den rolige af de to. Og de kan ikke forveksles:
 * den ene er roed.
 *
 * SLOERET er ikke pynt. Uden det kan man trykke paa raekkerne bagved
 * mens spoergsmaalet staar aabent, og saa kan man havne et andet sted
 * med en halv nulstilling i gang.
 */
static lv_obj_t *s_reset_dlg;        /* NULL naar der ikke spoerges */

static void luk_reset_dlg(void)
{
    if (s_reset_dlg != NULL) {
        lv_obj_del(s_reset_dlg);
        s_reset_dlg = NULL;
    }
}

static void on_reset_fortryd(lv_event_t *e)
{
    (void)e;
    luk_reset_dlg();
}

static void on_reset_udfoer(lv_event_t *e)
{
    (void)e;
    /* Lukkes FOERST. Nulstillingen genstarter skaermen efter 300 ms, og
     * et spoergsmaal der staar aabent mens den gaar ned, er det sidste
     * man ser - saa ser det ud som om den haengte. */
    luk_reset_dlg();
    send_simple(ZS_CMD_FACTORY_RESET);
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    if (s_reset_dlg != NULL) {
        return;                      /* staar der allerede */
    }

    /* Paa det oeverste lag, saa den ligger over hele skaermen - ogsaa
     * over statuslinjen. */
    s_reset_dlg = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_reset_dlg);
    lv_obj_set_size(s_reset_dlg, ZS_SCR_WIDTH, ZS_SCR_HEIGHT);
    lv_obj_set_pos(s_reset_dlg, 0, 0);
    lv_obj_set_style_bg_color(s_reset_dlg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_reset_dlg, LV_OPA_60, 0);
    lv_obj_clear_flag(s_reset_dlg, LV_OBJ_FLAG_SCROLLABLE);
    /* Sluger tryk, saa der ikke kan rammes noget bagved. */
    lv_obj_add_flag(s_reset_dlg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *kort = lv_obj_create(s_reset_dlg);
    lv_obj_remove_style_all(kort);
    lv_obj_set_width(kort, ZS_CONTENT_WIDTH);
    lv_obj_set_height(kort, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(kort, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(kort, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(kort, ZS_G_SK(16), 0);
    lv_obj_set_style_border_color(kort, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_border_width(kort, 1, 0);
    lv_obj_set_style_pad_all(kort, ZS_EDGE + ZS_G_SK(4), 0);
    lv_obj_set_flex_flow(kort, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(kort, ZS_G_SK_HEIGHT(12), 0);
    lv_obj_clear_flag(kort, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(kort);

    lv_obj_t *titel = lv_label_create(kort);
    lv_label_set_text(titel, "Nulstil skærmen");
    zs_style_text(titel, &zs_font_rad, ZS_C_TEXT);
    lv_obj_set_width(titel, LV_PCT(100));

    lv_obj_t *tekst = lv_label_create(kort);
    lv_label_set_text(tekst,
        "Alt bliver slettet, også wifi og den valgte inverter. "
        "Skærmen skal sættes op forfra bagefter.");
    zs_style_text(tekst, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(tekst, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tekst, LV_PCT(100));

    /*
     * DEN FARLIGE FAAR SIN EGEN FARVE.
     *
     * Formen er husets sekundaere knap, saa den sidder som alle andre
     * knapper. Det er KUN farven der er anderledes, og det er med vilje:
     * en knap der ogsaa har en anden form, ser ud som noget andet end en
     * knap.
     */
    lv_obj_t *nul = zs_btn_secondary_create(kort, "Nulstil skærmen");
    lv_obj_set_width(nul, LV_PCT(100));
    lv_obj_set_style_text_color(nul, lv_color_hex(ZS_C_BAD), 0);
    lv_obj_set_style_border_color(nul, lv_color_hex(ZS_C_BAD), 0);
    lv_obj_add_event_cb(nul, on_reset_udfoer, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fortryd = zs_btn_secondary_create(kort, "Fortryd");
    lv_obj_set_width(fortryd, LV_PCT(100));
    lv_obj_add_event_cb(fortryd, on_reset_fortryd, LV_EVENT_CLICKED, NULL);
}

#if ZS_DEMO_ENABLED
static void on_demo_stop(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_DEMO_STOP);
}
#endif

static void on_check_update(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_CHECK_UPDATE);
}

static void on_reboot(lv_event_t *e)
{
    (void)e;
    send_simple(ZS_CMD_REBOOT);
}

static void build_settings(void)
{
    zs_page_create(&s_set, "Indstillinger", on_set_back, NULL, false);
    lv_obj_t *col = zs_column_create(s_set.content, 8);
    lv_obj_set_x(col, ZS_EDGE);

    detail_heading(col, "ANLÆG");

    zs_row_create(&s_row_wifi, col, ZS_ICON_WIFI, "Netværk", "", true);
    lv_obj_set_width(s_row_wifi.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(s_row_wifi.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_wifi.row, on_change_wifi, LV_EVENT_CLICKED, NULL);

    zs_row_create(&s_row_inv, col, ZS_ICON_PLUG, "Inverter", "", true);
    lv_obj_set_width(s_row_inv.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(s_row_inv.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_inv.row, on_change_inverter, LV_EVENT_CLICKED, NULL);

    /*
     * Prisomraadet VISES, det vaelges ikke.
     *
     * Raekken var en knap der aabnede et valg mellem DK1 og DK2. Vaek
     * 12. oktober 2026: omraadet kommer ned fra serveren, se prisZone og
     * brug_prisomraade. Derfor ingen pil og intet tryk - en raekke der
     * ser ud som en knap og ikke goer noget, er vaerre end en raekke der
     * bare staar der.
     */
    zs_row_create(&s_row_zone, col, ZS_ICON_ZAP, "Prisområde", "", false);
    lv_obj_set_width(s_row_zone.row, ZS_CONTENT_WIDTH);

    zs_row_t det;
    zs_row_create(&det, col, ZS_ICON_INFO, "Detaljer om anlægget", NULL, true);
    lv_obj_set_width(det.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(det.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(det.row, on_open_details, LV_EVENT_CLICKED, NULL);

    /*
     * Avancerede indstillinger, lige under Detaljer og med samme slags
     * raekke. Den staar her og ikke i SKAERM-afsnittet, fordi den ikke
     * er et valg om skaermens udseende: den samler det man kun skal
     * finde hvis man LEDER efter det.
     */
    zs_row_t avan;
    zs_row_create(&avan, col, ZS_ICON_SETTINGS, "Avancerede indstillinger",
                  NULL, true);
    lv_obj_set_width(avan.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(avan.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(avan.row, on_open_avanceret, LV_EVENT_CLICKED, NULL);

    detail_heading(col, "SKÆRM");

    /* Tema. Ligger foerst i SKAERM, fordi det aendrer hele skaermens
     * udseende og de andre valg herunder ses i det tema man vaelger.
     * Samme slags raekke som Prisområde: den viser hvad der er valgt og
     * aabner en side hvor man vaelger. */
    zs_row_create(&s_row_tema, col, ZS_ICON_SUN, "Tema", "", true);
    lv_obj_set_width(s_row_tema.row, ZS_CONTENT_WIDTH);

    /*
     * Tilmeldingen staar under Indstillinger og ikke i opsaetningen.
     *
     * Opsaetningen er kundens: wifi, inverter, prisomraade. Tilmeldingen
     * er montoerens eller Frekvenz', og den sker ofte EFTER kunden har
     * sat skaermen op. Stod den i opsaetningen, skulle den hele vejen
     * igennem igen for at naa den.
     */
    zs_row_create(&s_row_tilmeld, col, ZS_ICON_PLUG, "Tilmeld anlægget",
                  "", true);
    lv_obj_set_width(s_row_tilmeld.row, ZS_CONTENT_WIDTH);
    lv_obj_add_event_cb(s_row_tilmeld.row, on_tilmeld, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_row_tema.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_tema.row, on_change_tema, LV_EVENT_CLICKED, NULL);

    /* Lysstyrke. Skyderen er 40 px hoej og hele raden 76, saa den kan
     * rammes uden at sigte. */
    lv_obj_t *brow = lv_obj_create(col);
    lv_obj_remove_style_all(brow);
    lv_obj_set_size(brow, ZS_CONTENT_WIDTH, ZS_G_BRIGHT_HEIGHT);
    lv_obj_clear_flag(brow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(brow, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(brow, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(brow, ZS_G_SK(14), 0);
    lv_obj_set_style_border_width(brow, 1, 0);
    lv_obj_set_style_border_color(brow, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_pad_all(brow, ZS_G_BRIGHT_PAD, 0);

    lv_obj_t *bl = lv_label_create(brow);
    lv_label_set_text(bl, "Lysstyrke");
    zs_style_text(bl, &zs_font_rad, ZS_C_TEXT);
    lv_obj_align(bl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_slider_val = lv_label_create(brow);
    lv_label_set_text(s_slider_val, "80 %");
    zs_style_text(s_slider_val, &zs_font_broed, ZS_C_LABEL);
    lv_obj_align(s_slider_val, LV_ALIGN_TOP_RIGHT, 0, 2);

    s_slider = lv_slider_create(brow);
    lv_obj_set_size(s_slider, ZS_G_SLIDER_WIDTH, ZS_G_SLIDER_HEIGHT);
    /* Tynd at se paa, men til at ramme: klikfladen vokser, selve
     * skyderen goer ikke. Se noten ved ZS_G_SLIDER_EKSTRA. */
    lv_obj_set_ext_click_area(s_slider, ZS_G_SLIDER_EKSTRA);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_LEFT, 0, -ZS_G_SLIDER_BUND);
    lv_slider_set_range(s_slider, 5, 100);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(ZS_C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, ZS_G_SK(10), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    make_switch_row(col, ZS_ICON_POWER, "Dæmp om natten",
                    "Skærmen lyser mindre mellem 22 og 6", on_night, &s_sw_night);

    detail_heading(col, "ELMÅLER");

    make_switch_row(col, ZS_ICON_ZAP, "Byt køb og salg",
                    "Slå til hvis NETTET viser køb når du sælger",
                    on_meter_sign, &s_sw_meter);

    detail_heading(col, "SKÆRMEN");

#if ZS_DEMO_ENABLED
    /* Kun synlig mens demoen koerer. Se zs_settings_set_demo. */
    zs_row_create(&s_row_demo, col, ZS_ICON_CLOSE, "Afslut demo", NULL, true);
    lv_obj_set_width(s_row_demo.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_demo.row, on_demo_stop, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    if (s_row_demo.title != NULL) {
        lv_obj_set_style_text_color(s_row_demo.title, lv_color_hex(ZS_C_ACCENT), 0);
    }
#endif

    zs_row_create(&s_row_soft, col, ZS_ICON_INFO, "Software", "", true);
    lv_obj_set_width(s_row_soft.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(s_row_soft.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_row_soft.row, on_check_update, LV_EVENT_CLICKED, NULL);

    zs_row_t rb;
    zs_row_create(&rb, col, ZS_ICON_REFRESH, "Genstart", NULL, true);
    lv_obj_set_width(rb.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(rb.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rb.row, on_reboot, LV_EVENT_CLICKED, NULL);

    zs_row_t fr;
    zs_row_create(&fr, col, ZS_ICON_ROTATE,
                  "Nulstil til fabriksindstillinger", NULL, true);
    lv_obj_set_width(fr.row, ZS_CONTENT_WIDTH);
    lv_obj_add_flag(fr.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fr.row, on_reset, LV_EVENT_CLICKED, NULL);
    /* Den eneste røde tekst i hele fladen. Den skal skille sig ud. */
    if (fr.title != NULL) {
        lv_obj_set_style_text_color(fr.title, lv_color_hex(ZS_C_BAD), 0);
    }

    lv_obj_t *ver = lv_label_create(col);
    char vtxt[32];
    snprintf(vtxt, sizeof(vtxt), "%s %s", ZS_PRODUCT_NAME, zs_version());
    lv_label_set_text(ver, vtxt);
    zs_style_text(ver, &zs_font_broed, ZS_C_STALE);
    lv_obj_set_style_pad_top(ver, ZS_G_SK_HEIGHT(16), 0);
    lv_obj_set_style_pad_bottom(ver, ZS_G_SK_HEIGHT(16), 0);
}

/* ------------------------------------------------------------------ */
/* Detaljer                                                            */
/* ------------------------------------------------------------------ */

static void build_details(void)
{
    zs_page_create(&s_det, "Detaljer", on_det_back, NULL, false);
    s_det_col = zs_column_create(s_det.content, 6);
    lv_obj_set_x(s_det_col, ZS_EDGE);
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Avancerede indstillinger                                            */
/* ------------------------------------------------------------------ */

static void on_avan_tilbage(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_SETTINGS);
}

static void on_alarm(lv_event_t *e)
{
    bool til = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_ALARM;
    c.flag = til;
    zs_app_send(&c);
}

/*
 * SIDEN TIL DET MAN SKAL LEDE EFTER.
 *
 * Alarmen staar foerst, fordi den er den eneste her der gaelder noget
 * en kunde kan opleve som generende: en skaerm i en stue der blinker
 * roedt og bipper om aftenen. Hun skal kunne slaa DEN fra uden at
 * skulle ringe nogen op.
 *
 * Og hun kan ikke slaa BESKEDEN fra. Fejlsiden og statuslinjen bliver,
 * saa en skaerm med alarmen slaaet fra stadig siger hvad der er galt -
 * ellers havde vi lavet en knap der skjuler fejl, og den ville blive
 * brugt den dag nogen blev traet af en rigtig fejl.
 */
static void build_avanceret(void)
{
    zs_page_create(&s_avan, "Avanceret", on_avan_tilbage, NULL, false);

    lv_obj_t *h = lv_label_create(s_avan.content);
    /*
     * Underteksten siger hvad knappen GOER og hvad den ikke gOEr.
     *
     * Her stod "Indstillinger man sjaeldent skal aendre", og det
     * fortaeller ingenting om hvad man kan komme til. Forklaringen stod
     * i stedet nede i kortet, hvor den gjorde raden dobbelt saa hoej -
     * og en forklaring der staar under den kontakt den handler om,
     * laeses foerst efter man har trykket.
     *
     * Maalt med tools/maal-tekst.py: 697 og 269 px paa ZDisplays 760,
     * 429 og 165 paa zScreens 456. To linjer paa begge.
     */
    lv_label_set_text(h, "Skærmen blinker og bipper, når strømmen "
                         "ryger. Beskeden bliver vist uanset hvad.");
    zs_style_text(h, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, ZS_CONTENT_WIDTH);
    lv_obj_set_pos(h, ZS_EDGE, ZS_G_SK_HEIGHT(16));

    /*
     * SOEJLEN LIGGER UNDER UNDERTEKSTENS BLAEK, IKKE PAA ET TAL.
     *
     * Her stod ZS_G_SK(56). Underteksten ovenfor staar paa
     * ZS_G_SK_HEIGHT(16). De to skalerer forskelligt - den ene efter
     * formen, den anden efter hoejden - og paa ZDisplay betyder det at
     * underteksten slutter paa 108 mens soejlen begynder paa 93.
     * Altsaa 15 px OVEN I hinanden, hvor zScreen har de to lige efter
     * hinanden.
     *
     * Maalt med tools/maal-tekst.py: underteksten er to linjer paa
     * begge skaerme, 40 px paa 480 og 66 paa 800.
     *
     * Nu spoerges LVGL hvor blaekket slutter. Paa zScreen giver det
     * 16 + 40 = 56, altsaa praecis det tal der stod der - fladen
     * flytter sig ikke. Og bliver teksten tre linjer paa et andet
     * sprog, flytter soejlen med i stedet for at blive skrevet hen
     * over.
     */
    lv_obj_update_layout(s_avan.content);
    lv_coord_t under_y = lv_obj_get_y(h) + lv_obj_get_height(h);

    lv_obj_t *col = zs_column_create(s_avan.content, ZS_CHOICE_GAP);
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, ZS_EDGE, under_y);

    /*
     * "Batteri-alarm" og ikke "Alarm", OG kontakten styrer kun
     * stroemalarmen.
     *
     * De to ting hoerer sammen. Hed den "Alarm" mens den slog ALT fra,
     * var navnet for bredt; hed den "Batteri-alarm" mens den slog alt
     * fra, var navnet en loegn - man slaar den fra for batteriet og
     * hoerer saa aldrig om en doed inverter. Nu passer navnet med hvad
     * den gOEr: se ind.alarm_fra i zs_app.c, som kun daemper
     * ZS_FEJL_INGEN_STROEM.
     *
     * Ingen undertekst. Forklaringen staar i sidens undertekst
     * ovenfor, saa raden er ÉN linje hoej som alle andre raekker.
     */
    make_switch_row(col, ZS_ICON_BATTERY, "Batteri-alarm", NULL,
                    on_alarm, &s_sw_alarm);
}

lv_obj_t *zs_avanceret_root(void) { return s_avan.root; }

void zs_avanceret_set_alarm(bool til)
{
    if (s_sw_alarm == NULL) {
        return;
    }
    /*
     * Uden LV_EVENT_VALUE_CHANGED. add_state sender ikke et event i
     * LVGL 8, saa der kommer ingen kommando tilbage - men den linje
     * staar her som en note, for det er netop den slags der giver en
     * uendelig runde mellem flade og motor den dag nogen skifter kald.
     */
    if (til) {
        lv_obj_add_state(s_sw_alarm, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_alarm, LV_STATE_CHECKED);
    }
}

void zs_settings_create(void)
{
    build_settings();
    build_details();
    build_tema();
    build_avanceret();
}

lv_obj_t *zs_settings_root(void) { return s_set.root; }
lv_obj_t *zs_details_root(void)  { return s_det.root; }

void zs_settings_update(const zs_settings_t *s, const char *ip)
{
    if (s == NULL) {
        return;
    }
    if (s_row_wifi.value != NULL) {
        lv_label_set_text(s_row_wifi.value,
                          s->wifi_ssid[0] ? s->wifi_ssid : "Ikke valgt");
    }
    if (s_row_inv.value != NULL) {
        lv_label_set_text(s_row_inv.value,
                          s->inverter_ip[0] ? s->inverter_ip : "Ikke valgt");
    }
    zs_settings_set_theme((zs_theme_mode_t)
        (s->theme < ZS_THEME_COUNT ? s->theme : ZS_THEME_DARK));

    if (s_row_zone.value != NULL) {
        /* Vi skriver hvor det er, ikke kun koden. "DK2" siger ingen
         * kunde noget. */
        /* Ikke "Ikke valgt": kunden vaelger den ikke. */
        const char *tekst = "Ikke sat endnu";
        if (strcmp(s->price_zone, "DK1") == 0) {
            tekst = "Vest for Storebælt";
        } else if (strcmp(s->price_zone, "DK2") == 0) {
            tekst = "Øst for Storebælt";
        }
        lv_label_set_text(s_row_zone.value, tekst);
    }

    lv_slider_set_value(s_slider, s->brightness, LV_ANIM_OFF);
    char t[8];
    snprintf(t, sizeof(t), "%u %%", (unsigned)s->brightness);
    lv_label_set_text(s_slider_val, t);

    if (s->night_dimming) {
        lv_obj_add_state(s_sw_night, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_night, LV_STATE_CHECKED);
    }
    /* Alarmen staar paa den avancerede side, men den fyldes HERFRA:
     * motoren sender hele indstillingen over i hver runde, og saa er
     * der ét sted der ved hvad der staar i vippen. */
    zs_avanceret_set_alarm(s->alarm_til);
    /* Kontakten hedder "Byt køb og salg", saa den er slaaet TIL naar
     * fortegnet IKKE er standard. */
    if (!s->meter_import_positive) {
        lv_obj_add_state(s_sw_meter, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_meter, LV_STATE_CHECKED);
    }
    (void)ip;
}

void zs_settings_set_ota(const zs_ota_status_t *o)
{
    if (s_row_soft.value == NULL) {
        return;
    }
    char t[64];
    uint32_t farve = ZS_C_LABEL;

    if (o == NULL) {
        snprintf(t, sizeof(t), "%s", zs_version());
    } else {
        switch (o->state) {
        case ZS_OTA_CHECKING:
            snprintf(t, sizeof(t), "Søger ...");
            break;
        case ZS_OTA_DOWNLOADING:
            snprintf(t, sizeof(t), "Henter %u %%", (unsigned)o->procent);
            farve = ZS_C_ACCENT;
            break;
        case ZS_OTA_READY:
            snprintf(t, sizeof(t), "Genstarter ...");
            farve = ZS_C_ACCENT;
            break;
        case ZS_OTA_FAILED:
            /* Kun teksten, ikke fejlkoden. Den staar under Detaljer
             * for den der skal fejlsoege. */
            snprintf(t, sizeof(t), "Kunne ikke søge");
            farve = ZS_C_WARN;
            break;
        case ZS_OTA_UP_TO_DATE:
            snprintf(t, sizeof(t), "%.20s, nyeste", o->koerende);
            farve = ZS_C_GOOD;
            break;
        case ZS_OTA_IDLE:
        default:
            snprintf(t, sizeof(t), "%s", zs_version());
            break;
        }
    }
    lv_label_set_text(s_row_soft.value, t);
    lv_obj_set_style_text_color(s_row_soft.value, lv_color_hex(farve), 0);
}

void zs_settings_set_demo(bool demo)
{
#if ZS_DEMO_ENABLED
    if (s_row_demo.row == NULL) {
        return;
    }
    if (demo) {
        lv_obj_clear_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_row_demo.row, LV_OBJ_FLAG_HIDDEN);
    }
#else
    (void)demo;
#endif
}

void zs_details_update(const zs_fr_t *fr, const char *own_ip, int rssi,
                       const zs_stroem_t *stroem)
{
    if (s_det_col == NULL) {
        return;
    }
    lv_obj_clean(s_det_col);

    char buf[96];

    detail_heading(s_det_col, "INVERTER");
    if (fr != NULL && fr->info.manufacturer[0] != '\0') {
        detail_line(s_det_col, "Fabrikat", fr->info.manufacturer);
        detail_line(s_det_col, "Model", fr->info.model);
        detail_line(s_det_col, "Firmware", fr->info.version);
        detail_line(s_det_col, "Serienummer", fr->info.serial);

        snprintf(buf, sizeof(buf), "%s:%u", fr->host, fr->port);
        detail_line(s_det_col, "Adresse", buf);

        snprintf(buf, sizeof(buf), "%u", fr->inverter_unit);
        detail_line(s_det_col, "Modbus-enhed", buf);

        snprintf(buf, sizeof(buf), "%u%s", fr->info.inverter_model_id,
                 fr->info.inverter_model_id >= 111 ? " (flydende tal)"
                                                   : " (heltal)");
        detail_line(s_det_col, "SunSpec-model", buf);

        if (fr->info.inverter_rated_kw > 0.0f) {
            snprintf(buf, sizeof(buf), "%.1f kW",
                     (double)fr->info.inverter_rated_kw);
            zs_fmt_da_decimal(buf);
            detail_line(s_det_col, "Mærkeeffekt", buf);
        }
    } else {
        detail_line(s_det_col, "Status", "Ikke forbundet");
    }

    if (fr != NULL) {
        detail_heading(s_det_col, "ELMÅLER");
        if (fr->info.has_meter) {
            snprintf(buf, sizeof(buf), "Model %u på enhed %u",
                     fr->info.meter_model_id, fr->info.meter_unit);
            detail_line(s_det_col, "Fundet", buf);
            detail_line(s_det_col, "Placering",
                        fr->meter_in_inverter_chain ? "I inverterens egen liste"
                                                    : "Egen Modbus-enhed");
            detail_line(s_det_col, "Fortegn",
                        fr->meter_import_positive ? "Positiv betyder køb"
                                                  : "Positiv betyder salg");
            if (fr->negative_house_count > 0) {
                snprintf(buf, sizeof(buf), "%u gange", (unsigned)fr->negative_house_count);
                detail_line(s_det_col, "Negativt forbrug", buf);
                lv_obj_t *w = lv_label_create(s_det_col);
                lv_label_set_text(w,
                    "Forbruget er blevet regnet negativt. Det kan ikke lade sig "
                    "gøre, så elmålerens fortegn er sandsynligvis vendt. "
                    "Prøv at slå \"Byt køb og salg\" til under Indstillinger.");
                zs_style_text(w, &zs_font_broed, ZS_C_WARN);
                lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(w, ZS_CONTENT_WIDTH);
            }
        } else {
            detail_line(s_det_col, "Fundet", "Ingen");
            lv_obj_t *w = lv_label_create(s_det_col);
            lv_label_set_text(w,
                "Uden elmåler kan skærmen ikke vise forbrug eller køb og salg. "
                "Solceller og batteri virker som normalt.");
            zs_style_text(w, &zs_font_broed, ZS_C_LABEL);
            lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(w, ZS_CONTENT_WIDTH);
        }

        detail_heading(s_det_col, "BATTERI");
        if (fr->info.has_battery) {
            if (fr->info.battery_capacity_kwh > 0.0f) {
                snprintf(buf, sizeof(buf), "%.2f kWh",
                         (double)fr->info.battery_capacity_kwh);
                zs_fmt_da_decimal(buf);
                detail_line(s_det_col, "Kapacitet", buf);
            }
            detail_line(s_det_col, "Kanalnavne",
                        fr->info.labels_usable
                            ? "Inverteren navngiver sine kanaler"
                            : "Uden navne, opdelt efter Fronius' mønster");
        } else {
            detail_line(s_det_col, "Fundet", "Intet batteri");
        }

        detail_heading(s_det_col, "SUNSPEC");
        snprintf(buf, sizeof(buf), "%u", fr->inv_map.base);
        detail_line(s_det_col, "Startadresse", buf);

        size_t p = 0;
        buf[0] = '\0';
        for (uint8_t i = 0; i < fr->inv_map.count && p + 8 < sizeof(buf); i++) {
            p += (size_t)snprintf(buf + p, sizeof(buf) - p, "%s%u",
                                  i ? ", " : "", fr->inv_map.models[i].id);
        }
        detail_line(s_det_col, "Modeller", buf);
        if (fr->inv_map.truncated) {
            detail_line(s_det_col, "Bemærk", "Listen kunne ikke læses færdig");
        }

        detail_heading(s_det_col, "AFLÆSNING");
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->poll_count);
        detail_line(s_det_col, "Aflæsninger", buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->poll_error_count);
        detail_line(s_det_col, "Fejlede", buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)fr->reconnect_count);
        detail_line(s_det_col, "Genforbindelser", buf);
        snprintf(buf, sizeof(buf), "%u sendt, %u fejl",
                 (unsigned)fr->mb.stat_requests, (unsigned)fr->mb.stat_errors);
        detail_line(s_det_col, "Modbus", buf);
    }

    detail_heading(s_det_col, "SKÆRMEN");
    detail_line(s_det_col, "IP-adresse", own_ip);
    if (rssi != 0) {
        snprintf(buf, sizeof(buf), "%d dBm", rssi);
        detail_line(s_det_col, "Wifi-signal", buf);
    }
    snprintf(buf, sizeof(buf), "%s %s", ZS_PRODUCT_NAME, zs_version());
    detail_line(s_det_col, "Version", buf);

    /*
     * Ledig hukommelse.
     *
     * En langsom laek ville ellers vaere helt usynlig indtil skaermen gik
     * ned: tallet blev kun skrevet én gang ved opstart, i en log ingen
     * kan naa paa en vaeg. Her kan en montoer se det, og to besoeg med
     * maaneder imellem kan sammenlignes.
     *
     * Intern hukommelse er den der goer ondt. PSRAM er der rigeligt af.
     */
    snprintf(buf, sizeof(buf), "%u KB intern, %u KB PSRAM",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    detail_line(s_det_col, "Ledig hukommelse", buf);

    /* Hvor laenge den har koert. En skaerm der genstarter af sig selv har
     * et lille tal her, og det er det foerste en montoer skal se. */
    int64_t oppe_s = esp_timer_get_time() / 1000000;
    if (oppe_s >= 86400) {
        snprintf(buf, sizeof(buf), "%u dage %u timer",
                 (unsigned)(oppe_s / 86400), (unsigned)((oppe_s % 86400) / 3600));
    } else if (oppe_s >= 3600) {
        snprintf(buf, sizeof(buf), "%u timer %u min",
                 (unsigned)(oppe_s / 3600), (unsigned)((oppe_s % 3600) / 60));
    } else {
        snprintf(buf, sizeof(buf), "%u min", (unsigned)(oppe_s / 60));
    }
    detail_line(s_det_col, "Oppe i", buf);

    /*
     * BATTERIET, OG KUN HER.
     *
     * Det staar paa Detaljer og ikke paa forsiden, og det er et valg.
     * Forsiden er kundens fire tal om deres ANLAEG - solceller,
     * forbrug, batteri og nettet - og "batteri" dér betyder husets
     * batteri paa flere kilowattimer. Satte vi skaermens eget
     * knapcellebatteri op ved siden af, ville to vidt forskellige ting
     * hedde det samme paa den samme flade. Montoeren skal kunne se
     * det; kunden skal ikke forholde sig til det.
     *
     * zScreen har intet batteri, og saa staar linjerne der slet ikke -
     * se zs_stroem_ingen.c. Det afgoeres af boardet og ikke af en
     * #ifdef her, saa fladen er den samme kode paa begge produkter.
     */
    /*
     * FLADEN LAESER IKKE HARDWAREN. Her stod zs_stroem_laes(&st),
     * altsaa et kald fra LVGL-opgaven ned i HAL'en - og BSP'ens
     * batteriopgave skriver de samme tal samtidig. Alt andet paa siden
     * kommer som argument, og det er det der goer siden proevbar og
     * holder LVGL-opgaven fri af hardwaren.
     *
     * NULL betyder "det board har ikke batteri", og saa staar
     * linjerne der ikke - samme svar som findes=false gav foer.
     */
    zs_stroem_t st;
    if (stroem != NULL) {
        st = *stroem;
    } else {
        memset(&st, 0, sizeof(st));
        st.procent = -1;
    }
    if (st.findes) {
        if (st.procent >= 0) {
            snprintf(buf, sizeof(buf), "%d %%", st.procent);
        } else {
            /* Maalingen er ikke klar endnu. "0 %" ville se ud som et
             * doeende batteri; det her siger hvad der er. */
            snprintf(buf, sizeof(buf), "maaler ...");
        }
        detail_line(s_det_col, "Batteri", buf);

        /*
         * Og hvad den koerer PAA. Det er den linje der betyder noget:
         * en skaerm paa batteri koerer videre og viser tal som om alt
         * var i orden, mens den loeber toer.
         */
        const char *kilde = st.paa_stroem
                          ? (st.lader ? "Strøm, lader" : "Strøm")
                          : "Batteri - ingen strøm";
        detail_line(s_det_col, "Forsyning", kilde);

        /* De raa millivolt. Til den der skal fejlsoege en daarlig
         * forsyning og ikke kan naa loggen paa en vaeg. */
        snprintf(buf, sizeof(buf), "%d mV stik, %d mV batteri",
                 st.stik_mv, st.batteri_mv);
        detail_line(s_det_col, "Spænding", buf);
    }

    /*
     * Floedestyringen.
     *
     * Staar her fordi den hoerer til skaermen og ikke til anlaegget. Uden
     * den kunne en tekniker staa foran skaermen uden at kunne se om den
     * er med i flaaden, og saa er svaret kun at finde i en log han ikke
     * kan naa.
     *
     * Er den slaaet fra, eller er der intet certifikat, staar der
     * "Slaaet fra" og ikke en fejl. Det er to forskellige ting.
     */
    detail_line(s_det_col, "Flåde", zs_fleet_state_text());
    {
        char asset[32];
        zs_fleet_asset_id(asset, sizeof(asset));
        if (asset[0] != '\0') {
            detail_line(s_det_col, "Flåde-id", asset);
        }
    }

    /* Lidt luft i bunden, saa den sidste linje ikke klistrer til kanten
     * naar man har rullet helt ned. */
    lv_obj_t *pad = lv_obj_create(s_det_col);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, ZS_G_SK_WIDTH(10), ZS_G_SK_HEIGHT(24));
}

/*
 * River Indstillinger og Detaljer ned.
 *
 * Vaerdierne i felterne skrives ind igen af zs_app naeste gang den
 * sender indstillingerne over, hvilket den goer i hver runde.
 */
void zs_settings_destroy(void)
{
    zs_page_t *sider[] = { &s_set, &s_det, &s_tema, &s_avan };
    for (size_t i = 0; i < sizeof(sider) / sizeof(sider[0]); i++) {
        if (sider[i]->root != NULL) {
            lv_obj_del(sider[i]->root);
        }
        memset(sider[i], 0, sizeof(*sider[i]));
    }

    memset(&s_row_wifi, 0, sizeof(s_row_wifi));
    memset(&s_row_inv,  0, sizeof(s_row_inv));
    memset(&s_row_demo, 0, sizeof(s_row_demo));
    memset(&s_row_zone, 0, sizeof(s_row_zone));
    memset(&s_row_tema, 0, sizeof(s_row_tema));
    memset(&s_row_soft, 0, sizeof(s_row_soft));
    s_slider     = NULL;
    s_slider_val = NULL;
    s_sw_night   = NULL;
    s_sw_alarm   = NULL;
    s_sw_meter   = NULL;
    s_det_col    = NULL;
}
