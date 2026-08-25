#include "zs_screen_setup.h"
#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_keyboard.h"
#include "zs_app.h"
#include "zs_config.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tilstand                                                            */
/* ------------------------------------------------------------------ */

static zs_page_t s_welcome, s_wifi, s_pass, s_connect, s_scan, s_inv, s_zone;

static lv_obj_t      *s_wifi_list;
static lv_obj_t      *s_wifi_hint;
static lv_obj_t      *s_wifi_rescan;

static zs_keyboard_t *s_kb;
static lv_obj_t      *s_pass_field;
static lv_obj_t      *s_pass_eye;
static lv_obj_t      *s_pass_eye_icon;

static lv_obj_t      *s_connect_icon;
static lv_obj_t      *s_connect_text;
static lv_obj_t      *s_connect_retry;

static lv_obj_t      *s_scan_bar;
static lv_obj_t      *s_scan_text;
static lv_obj_t      *s_scan_found;

static lv_obj_t      *s_inv_list;
static lv_obj_t      *s_inv_hint;

/* Det netvaerk brugeren valgte, mens kodeordet skrives. */
static char s_chosen_ssid[ZS_SSID_MAX];
static bool s_chosen_secured;

/* De fundne invertere. Vi gemmer adresserne, saa et tryk paa en rad
 * kan sende den rigtige med videre uden at gaette ud fra placeringen. */
static zs_found_t s_found[ZS_DISCOVERY_MAX];
static int        s_found_n;

/* ------------------------------------------------------------------ */
/* Smaa hjaelpere                                                      */
/* ------------------------------------------------------------------ */

static void send_cmd(zs_cmd_type_t type)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = type;
    zs_app_send(&c);
}

/* Rydder alle boern i en beholder. lv_obj_clean sletter dem rigtigt,
 * inklusive de hukommelsesblokke LVGL selv har lagt ved siden af. */
static void clear_list(lv_obj_t *list)
{
    if (list != NULL) {
        lv_obj_clean(list);
    }
}

/* ------------------------------------------------------------------ */
/* 1. Velkomst                                                         */
/* ------------------------------------------------------------------ */

static void on_welcome_start(lv_event_t *e)
{
    (void)e;
    /*
     * Vi beder appen fortsaette, i stedet for at gaa direkte til
     * listen over netvaerk.
     *
     * Er der gemt et netvaerk fra sidst, skal brugeren ikke taste
     * kodeordet igen. Det sker naar man er kommet gennem wifi-trinnet
     * men ikke fandt en inverter, og saa slukker for stroemmen.
     */
    send_cmd(ZS_CMD_SETUP_CONTINUE);
}

#if ZS_DEMO_ENABLED
static void on_welcome_demo(lv_event_t *e)
{
    (void)e;
    send_cmd(ZS_CMD_DEMO_START);
}
#endif

static void build_welcome(void)
{
    /* Ingen overskrift og ingen vej tilbage: det er den foerste side. */
    zs_page_create(&s_welcome, "", NULL, NULL, true);
    lv_obj_add_flag(s_welcome.head, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *c = s_welcome.content;

    lv_obj_t *logo = lv_img_create(c);
    lv_img_set_src(logo, &zs_img_wordmark);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 90);

    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, "Se hvad dit anlæg laver lige nu");
    zs_style_text(t, &zs_font_20, ZS_C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 175);

    lv_obj_t *s = lv_label_create(c);
    lv_label_set_text(s,
        "Skærmen skal på samme netværk som din inverter.\n"
        "Det tager et par minutter.");
    zs_style_text(s, &zs_font_16, ZS_C_LABEL);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 210);

#if ZS_DEMO_ENABLED
    /*
     * To knapper: den primaere fylder mest, demoen staar under som en
     * rolig kant-knap. Foden er 76 px hoej, saa der er plads til en
     * 52 px knap. Demo-knappen laegges derfor i indholdet lige over
     * foden, hvor der er luft.
     */
    lv_obj_t *demo = zs_btn_secondary_create(c, "Se demo");
    lv_obj_set_width(demo, ZS_CONTENT_WIDTH);
    lv_obj_align(demo, LV_ALIGN_TOP_MID, 0, 300);
    lv_obj_add_event_cb(demo, on_welcome_demo, LV_EVENT_CLICKED, NULL);

    lv_obj_t *dh = lv_label_create(c);
    lv_label_set_text(dh, "Se hvordan skærmen ser ud, uden et anlæg");
    zs_style_text(dh, &zs_font_16, ZS_C_LABEL);
    lv_obj_set_style_text_align(dh, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(dh, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(dh, LV_ALIGN_TOP_MID, 0, 358);
#endif

    lv_obj_t *btn = zs_btn_primary_create(s_welcome.footer, "Kom i gang");
    lv_obj_set_width(btn, ZS_CONTENT_WIDTH);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, on_welcome_start, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* 2. Vælg netværk                                                     */
/* ------------------------------------------------------------------ */

static void on_wifi_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_WELCOME);
}

static void on_wifi_rescan(lv_event_t *e)
{
    (void)e;
    send_cmd(ZS_CMD_WIFI_SCAN);
}

static void on_wifi_pick(lv_event_t *e)
{
    const zs_ap_t *ap = lv_event_get_user_data(e);
    if (ap == NULL) {
        return;
    }
    snprintf(s_chosen_ssid, sizeof(s_chosen_ssid), "%s", ap->ssid);
    s_chosen_secured = ap->secured;

    lv_label_set_text(s_pass.title, s_chosen_ssid);
    zs_keyboard_clear(s_kb);

    if (!s_chosen_secured) {
        /* Aabent netvaerk. Vi springer kodeordet over i stedet for at
         * vise et tomt felt brugeren skal gaette sig til at forlade. */
        zs_cmd_t c;
        memset(&c, 0, sizeof(c));
        c.type = ZS_CMD_WIFI_CONNECT;
        snprintf(c.ssid, sizeof(c.ssid), "%s", s_chosen_ssid);
        zs_app_send(&c);
        zs_ui_show(ZS_SCREEN_CONNECTING);
        return;
    }
    zs_ui_show(ZS_SCREEN_PASSWORD);
}

static void build_wifi(void)
{
    zs_page_create(&s_wifi, "Vælg netværk", on_wifi_back, NULL, true);

    s_wifi_hint = lv_label_create(s_wifi.content);
    lv_label_set_text(s_wifi_hint, "Søger efter netværk ...");
    zs_style_text(s_wifi_hint, &zs_font_16, ZS_C_LABEL);
    lv_obj_align(s_wifi_hint, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_EDGE);

    s_wifi_list = zs_column_create(s_wifi.content, 8);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_LEFT, ZS_EDGE, 34);

    s_wifi_rescan = zs_btn_secondary_create(s_wifi.footer, "Søg igen");
    lv_obj_set_width(s_wifi_rescan, ZS_CONTENT_WIDTH);
    lv_obj_center(s_wifi_rescan);
    lv_obj_add_event_cb(s_wifi_rescan, on_wifi_rescan, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* 3. Kodeord                                                          */
/* ------------------------------------------------------------------ */

static void on_pass_back(lv_event_t *e)
{
    (void)e;
    /* Ryd kodeordet naar man gaar tilbage. Det skal ikke staa og vente
     * hvis nogen vaelger et andet netvaerk. */
    zs_keyboard_clear(s_kb);
    zs_ui_show(ZS_SCREEN_WIFI_LIST);
}

static void on_pass_eye(lv_event_t *e)
{
    (void)e;
    bool hidden = !zs_keyboard_get_password_hidden(s_kb);
    zs_keyboard_set_password_hidden(s_kb, hidden);
    lv_label_set_text(s_pass_eye_icon, hidden ? ZS_ICON_EYE : ZS_ICON_EYE_OFF);
}

static void on_pass_done(lv_event_t *e)
{
    (void)e;
    const char *pw = zs_keyboard_get_text(s_kb);
    if (pw == NULL || strlen(pw) < 8) {
        /* WPA2 kraever mindst 8 tegn. Vi siger det her i stedet for at
         * lade routeren afvise os om ti sekunder med en fejl der ikke
         * fortaeller hvad der var galt. */
        zs_ui_set_connect_status(
            "Kodeordet skal være på mindst 8 tegn", true, true);
        zs_ui_show(ZS_SCREEN_CONNECTING);
        return;
    }
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_WIFI_CONNECT;
    snprintf(c.ssid, sizeof(c.ssid), "%s", s_chosen_ssid);
    snprintf(c.pass, sizeof(c.pass), "%s", pw);
    zs_app_send(&c);
    zs_ui_show(ZS_SCREEN_CONNECTING);
}

static void build_password(void)
{
    zs_page_create(&s_pass, "Kodeord", on_pass_back, NULL, false);

    lv_obj_t *lbl = zs_label_create(s_pass.content, "KODEORD TIL NETVÆRKET");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, ZS_EDGE, 16);

    /* Indtastningsfeltet. En ramme med teksten i, og et oeje ved siden
     * af til at vise hvad der staar. */
    lv_obj_t *box = lv_obj_create(s_pass.content);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, ZS_CONTENT_WIDTH - ZS_TOUCH_MIN - 8, 56);
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, ZS_EDGE, 42);
    lv_obj_set_style_bg_color(box, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_pad_hor(box, 16, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    s_pass_field = lv_label_create(box);
    lv_label_set_text(s_pass_field, "");
    zs_style_text(s_pass_field, &zs_font_20, ZS_C_TEXT);
    lv_label_set_long_mode(s_pass_field, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_pass_field, ZS_CONTENT_WIDTH - ZS_TOUCH_MIN - 8 - 32);
    lv_obj_align(s_pass_field, LV_ALIGN_LEFT_MID, 0, 0);

    s_pass_eye = lv_btn_create(s_pass.content);
    lv_obj_remove_style_all(s_pass_eye);
    lv_obj_set_size(s_pass_eye, ZS_TOUCH_MIN, ZS_TOUCH_MIN);
    lv_obj_align(s_pass_eye, LV_ALIGN_TOP_RIGHT, -ZS_EDGE, 48);
    lv_obj_add_event_cb(s_pass_eye, on_pass_eye, LV_EVENT_CLICKED, NULL);

    s_pass_eye_icon = lv_label_create(s_pass_eye);
    lv_label_set_text(s_pass_eye_icon, ZS_ICON_EYE);
    lv_obj_set_style_text_font(s_pass_eye_icon, &zs_icons_20, 0);
    lv_obj_set_style_text_color(s_pass_eye_icon, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_center(s_pass_eye_icon);

    /* Tastaturet ligger paa siden selv, ikke i indholdet, saa det
     * bliver staaende naar indholdet rulles. */
    s_kb = zs_keyboard_create(s_pass.root, s_pass_field, on_pass_done, NULL);
}

/* ------------------------------------------------------------------ */
/* 4. Forbinder                                                        */
/* ------------------------------------------------------------------ */

static void on_connect_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_WIFI_LIST);
}

static void on_connect_retry(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_PASSWORD);
}

static void build_connecting(void)
{
    zs_page_create(&s_connect, "Forbinder", on_connect_back, NULL, true);

    s_connect_icon = lv_spinner_create(s_connect.content, 1200, 70);
    lv_obj_set_size(s_connect_icon, 64, 64);
    lv_obj_align(s_connect_icon, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_arc_color(s_connect_icon, lv_color_hex(ZS_C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_connect_icon, lv_color_hex(ZS_C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_connect_icon, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_connect_icon, 5, LV_PART_INDICATOR);

    s_connect_text = lv_label_create(s_connect.content);
    lv_label_set_text(s_connect_text, "Forbinder til netværket ...");
    zs_style_text(s_connect_text, &zs_font_20, ZS_C_TEXT);
    lv_obj_set_style_text_align(s_connect_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_connect_text, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(s_connect_text, LV_ALIGN_TOP_MID, 0, 160);

    s_connect_retry = zs_btn_primary_create(s_connect.footer, "Prøv igen");
    lv_obj_set_width(s_connect_retry, ZS_CONTENT_WIDTH);
    lv_obj_center(s_connect_retry);
    lv_obj_add_event_cb(s_connect_retry, on_connect_retry, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_connect_retry, LV_OBJ_FLAG_HIDDEN);
}

/* ------------------------------------------------------------------ */
/* 5. Søger efter inverteren                                           */
/* ------------------------------------------------------------------ */

static void on_scan_back(lv_event_t *e)
{
    (void)e;
    zs_discovery_abort();
    zs_ui_show(ZS_SCREEN_WIFI_LIST);
}

static void build_scan(void)
{
    zs_page_create(&s_scan, "Søger", on_scan_back, NULL, false);

    lv_obj_t *t = lv_label_create(s_scan.content);
    lv_label_set_text(t, "Leder efter din inverter");
    zs_style_text(t, &zs_font_28, ZS_C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 80);

    s_scan_text = lv_label_create(s_scan.content);
    lv_label_set_text(s_scan_text,
        "Enheden gennemgår netværket. Det tager få sekunder.");
    zs_style_text(s_scan_text, &zs_font_16, ZS_C_LABEL);
    lv_obj_set_style_text_align(s_scan_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_scan_text, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(s_scan_text, LV_ALIGN_TOP_MID, 0, 124);

    s_scan_bar = lv_bar_create(s_scan.content);
    lv_obj_set_size(s_scan_bar, ZS_SCR_WIDTH - 4 * ZS_EDGE, 8);
    lv_obj_align(s_scan_bar, LV_ALIGN_TOP_MID, 0, 190);
    lv_bar_set_range(s_scan_bar, 0, 100);
    lv_bar_set_value(s_scan_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_scan_bar, lv_color_hex(ZS_C_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_scan_bar, lv_color_hex(ZS_C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_scan_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_scan_bar, 4, LV_PART_INDICATOR);

    s_scan_found = lv_label_create(s_scan.content);
    lv_label_set_text(s_scan_found, "");
    zs_style_text(s_scan_found, &zs_font_16, ZS_C_ACCENT);
    lv_obj_align(s_scan_found, LV_ALIGN_TOP_MID, 0, 214);
}

/* ------------------------------------------------------------------ */
/* 6. Vælg inverter                                                    */
/* ------------------------------------------------------------------ */

static void on_inv_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_WIFI_LIST);
}

static void on_inv_rescan(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_INVERTER_SCAN);
    send_cmd(ZS_CMD_INVERTER_SCAN);
}

static void on_inv_pick(lv_event_t *e)
{
    /* Vi sender indekset med, ikke en pegepind ind i listen. Listen
     * bliver bygget om hver gang der soeges, og en gammel pegepind
     * ville pege paa noget der ikke findes laengere. */
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_found_n) {
        return;
    }
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_INVERTER_SELECT;
    snprintf(c.ip, sizeof(c.ip), "%s", s_found[idx].ip);
    zs_app_send(&c);
    /*
     * Vi skifter IKKE side her.
     *
     * Appen bestemmer hvor brugeren skal hen, for den er den eneste
     * der ved om der ogsaa mangler et prisomraade. Gjorde vi begge
     * dele, ville de to kappes om det, og LVGL holder laasen gennem
     * hele sin runde, saa appens valg altid vandt. Resultatet var at
     * trinnet med prisomraade aldrig blev vist.
     */
}

static void build_inverter(void)
{
    zs_page_create(&s_inv, "Vælg inverter", on_inv_back, NULL, true);

    s_inv_hint = lv_label_create(s_inv.content);
    lv_label_set_text(s_inv_hint, "");
    zs_style_text(s_inv_hint, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(s_inv_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_inv_hint, ZS_CONTENT_WIDTH);
    lv_obj_align(s_inv_hint, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_EDGE);

    s_inv_list = zs_column_create(s_inv.content, 8);
    lv_obj_align(s_inv_list, LV_ALIGN_TOP_LEFT, ZS_EDGE, 56);

    lv_obj_t *btn = zs_btn_secondary_create(s_inv.footer, "Søg igen");
    lv_obj_set_width(btn, ZS_CONTENT_WIDTH);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, on_inv_rescan, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* 7. Vælg prisområde                                                  */
/* ------------------------------------------------------------------ */

static void send_zone(const char *zone)
{
    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_SET_PRICE_ZONE;
    /* ssid-feltet genbruges til teksten. Det er den eneste streng i
     * beskeden, og at lave et felt mere kun til tre bogstaver ville
     * bare goere beskeden stoerre for alle andre kommandoer. */
    snprintf(c.ssid, sizeof(c.ssid), "%s", zone);
    zs_app_send(&c);
}

static void on_zone_dk1(lv_event_t *e) { (void)e; send_zone("DK1"); }
static void on_zone_dk2(lv_event_t *e) { (void)e; send_zone("DK2"); }

/*
 * Hvor "tilbage" foerer hen fra prisomraade.
 *
 * Siden naas to steder fra: som sidste trin i opsaetningen, og fra
 * Indstillinger. Hardkodede vi det ene, ville den anden vej ende paa
 * en side der ikke giver mening. Den der viser siden, siger hvor
 * tilbage foerer hen.
 */
static zs_screen_id_t s_zone_tilbage = ZS_SCREEN_INVERTER_LIST;

void zs_setup_zone_set_return(zs_screen_id_t hvorhen)
{
    s_zone_tilbage = hvorhen;
}

static void on_zone_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(s_zone_tilbage);
}

/* Ét stort valg med forklaring under. To knapper der fylder halvdelen
 * hver ville tvinge teksten ned i to smalle spalter. */
static lv_obj_t *zone_button(lv_obj_t *parent, const char *titel,
                             const char *forklaring, lv_coord_t y,
                             lv_event_cb_t cb)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, ZS_CONTENT_WIDTH, 92);
    lv_obj_set_pos(b, ZS_EDGE, y);   /* samme kant som alt andet */
    lv_obj_set_style_bg_color(b, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(ZS_C_BORDER), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(ZS_C_ACCENT), LV_STATE_PRESSED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *t = lv_label_create(b);
    lv_label_set_text(t, titel);
    zs_style_text(t, &zs_font_28, ZS_C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 14);

    lv_obj_t *f = lv_label_create(b);
    lv_label_set_text(f, forklaring);
    zs_style_text(f, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(f, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(f, ZS_CONTENT_WIDTH - 32);
    lv_obj_align(f, LV_ALIGN_TOP_LEFT, 16, 50);
    return b;
}

static void build_zone(void)
{
    zs_page_create(&s_zone, "Hvor bor du", on_zone_back, NULL, false);

    lv_obj_t *h = lv_label_create(s_zone.content);
    lv_label_set_text(h, "Elprisen er ikke den samme i hele landet.");
    zs_style_text(h, &zs_font_16, ZS_C_LABEL);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, ZS_CONTENT_WIDTH);
    lv_obj_set_pos(h, ZS_EDGE, 16);

    zone_button(s_zone.content, "Vest for Storebælt",
                "Jylland og Fyn. Kaldes DK1.", 56, on_zone_dk1);
    zone_button(s_zone.content, "Øst for Storebælt",
                "Sjælland, Lolland, Falster og Bornholm. Kaldes DK2.",
                160, on_zone_dk2);
}

/* ------------------------------------------------------------------ */
/* Udadtil                                                             */
/* ------------------------------------------------------------------ */

void zs_setup_create(void)
{
    build_zone();
    build_welcome();
    build_wifi();
    build_password();
    build_connecting();
    build_scan();
    build_inverter();
}

lv_obj_t *zs_setup_root_welcome(void)    { return s_welcome.root; }
lv_obj_t *zs_setup_root_wifi(void)       { return s_wifi.root; }
lv_obj_t *zs_setup_root_password(void)   { return s_pass.root; }
lv_obj_t *zs_setup_root_connecting(void) { return s_connect.root; }
lv_obj_t *zs_setup_root_scan(void)       { return s_scan.root; }
lv_obj_t *zs_setup_root_inverter(void)   { return s_inv.root; }
lv_obj_t *zs_setup_root_zone(void)       { return s_zone.root; }

/* Netvaerkene skal overleve at listen tegnes om, fordi hver rad peger
 * paa sin egen post. Derfor en kopi her og ikke en pegepind udefra. */
static zs_ap_t s_aps[ZS_WIFI_MAX_APS];
static int     s_aps_n;

void zs_setup_set_wifi_list(const zs_ap_t *aps, int n)
{
    clear_list(s_wifi_list);
    s_aps_n = 0;

    if (n <= 0) {
        lv_label_set_text(s_wifi_hint,
            "Der blev ikke fundet nogen netværk. Prøv igen.");
        return;
    }
    if (n > ZS_WIFI_MAX_APS) {
        n = ZS_WIFI_MAX_APS;
    }
    memcpy(s_aps, aps, (size_t)n * sizeof(zs_ap_t));
    s_aps_n = n;

    lv_label_set_text(s_wifi_hint, "Tryk på dit netværk");

    for (int i = 0; i < n; i++) {
        /* Signalstyrken siges med ikonet, ikke med et tal i dBm. Ingen
         * kunde skal forholde sig til minus syvogtres. */
        const char *icon = (s_aps[i].rssi >= -60) ? ZS_ICON_WIFI
                         : (s_aps[i].rssi >= -75) ? ZS_ICON_WIFI_HIGH
                                                  : ZS_ICON_WIFI_LOW;
        zs_row_t r;
        zs_row_create(&r, s_wifi_list, icon, s_aps[i].ssid,
                      s_aps[i].secured ? NULL : "Åbent", true);
        lv_obj_set_width(r.row, ZS_CONTENT_WIDTH);
        lv_obj_add_flag(r.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r.row, on_wifi_pick, LV_EVENT_CLICKED, &s_aps[i]);
    }
}

void zs_setup_set_wifi_scanning(bool scanning)
{
    if (scanning) {
        lv_label_set_text(s_wifi_hint, "Søger efter netværk ...");
        clear_list(s_wifi_list);
    }
    if (s_wifi_rescan != NULL) {
        if (scanning) {
            lv_obj_add_state(s_wifi_rescan, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_wifi_rescan, LV_STATE_DISABLED);
        }
    }
}

void zs_setup_set_connect_status(const char *text, bool is_error, bool can_retry)
{
    if (s_connect_text == NULL) {
        return;
    }
    lv_label_set_text(s_connect_text, text != NULL ? text : "");
    lv_obj_set_style_text_color(s_connect_text,
        lv_color_hex(is_error ? ZS_C_BAD : ZS_C_TEXT), 0);

    if (is_error) {
        lv_obj_add_flag(s_connect_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_connect_icon, LV_OBJ_FLAG_HIDDEN);
    }
    if (can_retry) {
        lv_obj_clear_flag(s_connect_retry, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_connect_retry, LV_OBJ_FLAG_HIDDEN);
    }
}

void zs_setup_set_scan_progress(int done, int total, int found)
{
    if (s_scan_bar == NULL) {
        return;
    }
    int pct = (total > 0) ? (done * 100 / total) : 0;
    if (pct > 100) { pct = 100; }
    lv_bar_set_value(s_scan_bar, pct, LV_ANIM_OFF);

    char t[64];
    if (found <= 0) {
        t[0] = '\0';
    } else if (found == 1) {
        snprintf(t, sizeof(t), "1 inverter fundet");
    } else {
        snprintf(t, sizeof(t), "%d invertere fundet", found);
    }
    lv_label_set_text(s_scan_found, t);
}

void zs_setup_set_inverter_list(const zs_found_t *list, int n)
{
    clear_list(s_inv_list);
    s_found_n = 0;

    if (n <= 0) {
        lv_label_set_text(s_inv_hint,
            "Der blev ikke fundet nogen inverter.\n\n"
            "Tjek at Modbus TCP er slået til i inverterens webside, "
            "under Kommunikation, og at skærmen er på samme netværk.");
        return;
    }
    if (n > ZS_DISCOVERY_MAX) {
        n = ZS_DISCOVERY_MAX;
    }
    memcpy(s_found, list, (size_t)n * sizeof(zs_found_t));
    s_found_n = n;

    lv_label_set_text(s_inv_hint, n == 1 ? "Tryk for at bruge den"
                                         : "Tryk på den du vil bruge");

    for (int i = 0; i < n; i++) {
        const zs_fr_info_t *inf = &s_found[i].info;

        /*
         * Præcisionen i formatet er ikke pynt.
         *
         * Felterne ligger i en struct inde i et array, og saa kan
         * oversaetteren ikke se hvor de slutter: den regner med at en
         * streng kan loebe helt til enden af HELE arrayet, altsaa over
         * tusind tegn, og standser byggeriet.
         *
         * "%.32s" siger at der hoejst tages 32 tegn. Saa er graensen
         * baade tydelig for den der laeser koden og bevislig for
         * oversaetteren.
         *     32 + 1 + 32 + afslutning = 66, og der er 72.
         */
        char title[72];
        if (inf->model[0] != '\0') {
            snprintf(title, sizeof(title), "%.32s %.32s",
                     inf->manufacturer[0] ? inf->manufacturer : "Inverter",
                     inf->model);
        } else {
            snprintf(title, sizeof(title), "Inverter");
        }

        zs_row_t r;
        zs_row_create(&r, s_inv_list, ZS_ICON_PLUG, title, NULL, true);
        lv_obj_set_width(r.row, ZS_CONTENT_WIDTH);
        lv_obj_set_height(r.row, 72);   /* to linjer: navn og detaljer */
        lv_obj_add_flag(r.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r.row, on_inv_pick, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        /* Anden linje: serienummer og adresse, saa man kan se forskel
         * paa to ens invertere paa samme netvaerk. */
        /* Serienummer, en skillelinje, og adressen.
         *     32 + 6 + 15 + afslutning = 54, og der er 64. */
        char sub[64];
        if (inf->serial[0] != '\0') {
            snprintf(sub, sizeof(sub), "%.32s  ·  %.15s",
                     inf->serial, s_found[i].ip);
        } else {
            snprintf(sub, sizeof(sub), "%.15s", s_found[i].ip);
        }
        lv_obj_t *sl = lv_label_create(r.row);
        lv_label_set_text(sl, sub);
        zs_style_text(sl, &zs_font_16, ZS_C_LABEL);
        lv_obj_align(sl, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, 15);

        /* Ryk navnet op saa de to linjer staar paent under hinanden.
         * 72 px rad, to linjer paa 24 og 18: 12 op og 15 ned lader dem
         * staa symmetrisk om midten. */
        if (r.title != NULL) {
            lv_obj_align(r.title, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, -12);
        }
    }
}
