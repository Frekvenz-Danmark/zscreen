#include "zs_screen_setup.h"
#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_keyboard.h"
#include "zs_step.h"
#include "zs_app.h"
#include "zs_config.h"
#include "zs_modbus_tcp.h"  /* zs_mb_maal_ok: samme vagt som forbindelsen */

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tilstand                                                            */
/* ------------------------------------------------------------------ */

static zs_page_t s_welcome, s_wifi, s_pass, s_connect, s_scan, s_inv;

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

/* Ventetrinnet: titel, forklaring, bjaelke og noten under den. Maalene
 * staar i zs_layout.h og deles med tilmeldingssiden - se zs_step.h. */
static zs_step_t      s_scan_step;

static lv_obj_t      *s_inv_list;
static lv_obj_t      *s_inv_hint;
/* "Find manuelt": siden, feltet, tastaturet og linjen der siger fra. */
static zs_page_t      s_man;
static lv_obj_t      *s_man_field;
static lv_obj_t      *s_man_fejl;
static zs_keyboard_t *s_man_kb;
static lv_obj_t      *s_inv_rescan;
static lv_obj_t      *s_inv_manual;
/*
 * HVOR FLOWET KOM FRA. Se zs_screen_setup.h og zs_inv_fra_t.
 *
 * Standard er NUL, altsaa opsaetningen. Det er den eneste vej der
 * findes paa en ny skaerm, og en variabel der staar rigtigt naar den er
 * nul, kan ikke glemmes ved den foerste opstart. Det var grunden til at
 * bool'en var vendt som den var, og den gaelder stadig for opregningen:
 * ZS_INV_FRA_OPSAETNING er med vilje nummer nul.
 */
static zs_inv_fra_t   s_inv_fra;
/* "Opret anlaegget": trinnet FOER netvaerket. */
static zs_page_t      s_guide_side;



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

void zs_setup_soeg_inverter(zs_inv_fra_t fra)
{
    s_inv_fra = fra;
    /*
     * Siden skiftes HER og kommandoen sendes bagefter: en scanning tager
     * omkring tyve sekunder, og i det tidsrum laeser appen ikke
     * kommandoer. Ventede vi paa den, ville der gaa tyve sekunder hvor
     * et tryk ikke gjorde noget synligt.
     */
    zs_ui_show(ZS_SCREEN_INVERTER_SCAN);
    send_cmd(ZS_CMD_INVERTER_SCAN);
}

void zs_setup_skift_netvaerk(zs_inv_fra_t fra)
{
    s_inv_fra = fra;
    /*
     * ET TRYK SKAL KUNNE SES MED DET SAMME.
     *
     * Vi vaelger ikke den ENDELIGE side her - det gOEr appen, som er den
     * eneste der ved om der er en opsaetning at gaa tilbage til. Men vi
     * kvitterer.
     *
     * Uden kvitteringen kunne der gaa tyve sekunder hvor der ikke skete
     * noget synligt: er wifi'et vaek, staar loekken inde i
     * zs_wifi_connect med ZS_WIFI_CONNECT_TIMEOUT_MS og laeser ikke
     * koeen. Kunden trykker paa den ENESTE knap paa en pulserende roed
     * side, der sker ingenting, og saa trykker hun igen. Det var praecis
     * den fejl der stod forklaret i zs_setup_soeg_inverter ovenfor - og
     * den gjaldt den her vej lige saa meget.
     *
     * Teksten siger hvad der kommer til at ske, ikke hvad der sker nu.
     * Appen rydder sider frem og tilbage; det her er kun et tegn paa at
     * trykket blev hoert.
     */
    zs_setup_set_connect_status("Søger efter netværk ...", false, false);
    zs_ui_show(ZS_SCREEN_CONNECTING);
    send_cmd(ZS_CMD_SETUP_RESTART);
}

/*
 * Tilbage fra de tre inverter-sider. ÉT sted: tre kopier af det her
 * valg kunne komme til at pege tre forskellige steder.
 *
 * Den SKIFTER SELV siden i stedet for at svare hvor man skal hen, fordi
 * den ene af de tre veje ikke er et sideskift.
 */
static void gaa_tilbage_fra_inverter(void)
{
    switch (s_inv_fra) {
    case ZS_INV_FRA_INDSTILLINGER:
        zs_ui_show(ZS_SCREEN_SETTINGS);
        return;

    /*
     * EN REPARATION GAAR TILBAGE GENNEM APPEN, og det er ikke en omvej.
     *
     * Her stod zs_ui_show(ZS_SCREEN_HOME), og det kunne give en DOED
     * skaerm. Kunden naar naemlig de her sider ad TRE veje, og de
     * efterlader appen i to forskellige tilstande:
     *
     *   "Søg efter anlægget"   fejlsiden    appen bliver i ST_RUNNING
     *   "Vælg netværk"         fejlsiden    ST_SETUP
     *   "Netværk"              Indstillinger ST_SETUP
     *
     * I de to sidste staar der continue i loekken, saa appen hverken
     * forbinder eller aflaeser. Forsiden kom frem med gamle tal og blev
     * staaende saadan til naeste genstart - uden en fejl nogen steder,
     * for fejl_tik bliver heller ikke kaldt i ST_SETUP.
     *
     * ZS_CMD_SETUP_CANCEL er lavet til praecis det: kun appen kan slippe
     * tilstanden, og kun den ved om der er en opsaetning at gaa tilbage
     * til. Den virker ogsaa fra den foerste vej, hvor der ikke var
     * nogen tilstand at slippe - saa alle tre veje kan dele den ene
     * linje.
     */
    case ZS_INV_FRA_REPARATION:
        send_cmd(ZS_CMD_SETUP_CANCEL);
        return;

    case ZS_INV_FRA_OPSAETNING:
        break;
    }
    zs_ui_show(ZS_SCREEN_WIFI_LIST);
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
     * "Kom i gang" foerer nu til GUIDEN og ikke direkte videre.
     *
     * Her stod send_cmd(ZS_CMD_SETUP_CONTINUE), altsaa lige ind i
     * netvaerket. Kunden skal oprettes i appen foerst, og det skal staa
     * foer der er noget der tikker - se noten ved build_guide.
     */
    zs_ui_show(ZS_SCREEN_GUIDE);
}

#if ZS_DEMO_ENABLED
static void on_welcome_demo(lv_event_t *e)
{
    (void)e;
    send_cmd(ZS_CMD_DEMO_START);
}
#endif

/* ------------------------------------------------------------------ */
/* 1b. Hent guiden i appen                                             */
/* ------------------------------------------------------------------ */
/*
 * HVORFOR TRINNET FINDES.
 *
 * Skaermen kan ikke selv fortaelle hele historien. Kunden skal oprettes
 * i Frekvenz-appen, og tilmeldingen senere i flowet kraever at nogen
 * staar klar DÉR med appen aaben. Stod det foerst naar QR'en kom frem,
 * ville montoeren vaere naaet gennem wifi og inverter foer han fik at
 * vide at han manglede noget - og saa skal han forlade skaermen midt i
 * en kode der kun lever 10 minutter.
 *
 * Derfor her, FOER netvaerket: appen hentes og aabnes mens der ikke er
 * noget der tikker.
 *
 * FIRE TRIN, NUMMEREREDE, OG IKKE EN PROSATEKST.
 *
 * Det er en opskrift man foelger med en telefon i den anden haand.
 * Numrene gOEr at man kan se hvor man er naar man ser op igen - og det
 * er netop det man goer, fire gange.
 */

/*
 * ÉT TRIN SOM ET KORT.
 *
 * Kortet har FAST hoejde, og den er udregnet i zs_layout.h af det der er
 * tilbage naar titlen og luften har faaet deres. Saa fylder de fire kort
 * siden praecis, og der staar en vagt paa at det gaar op.
 *
 * Tallets spalte har ogsaa fast bredde. Det er den der goer at de fire
 * tekster begynder paa samme x - uden den ville "1" og "4" vaere lige
 * brede, men et tocifret trin en dag ville skubbe sin egen linje, og saa
 * flugter kortene ikke laengere.
 */
static void guide_trin(lv_obj_t *forael, const char *nr, const char *tekst)
{
    lv_obj_t *kort = lv_obj_create(forael);
    lv_obj_remove_style_all(kort);
    lv_obj_set_width(kort, LV_PCT(100));
    lv_obj_set_height(kort, ZS_G_GUIDE_CARD_HEIGHT);
    lv_obj_set_style_bg_color(kort, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(kort, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(kort, ZS_G_SK(14), 0);
    lv_obj_set_style_pad_hor(kort, ZS_G_GUIDE_CARD_PAD, 0);
    lv_obj_set_style_pad_column(kort, ZS_G_GUIDE_NUM_GAP, 0);
    /* Indholdet midtstilles LODRET i kortet, saa en tekst der en dag
     * bliver to linjer, vokser lige meget op og ned. */
    lv_obj_set_flex_flow(kort, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(kort, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(kort, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(kort);
    lv_label_set_text(t, nr);
    zs_style_text(t, &zs_font_rad, ZS_C_ACCENT);
    lv_obj_set_width(t, ZS_G_GUIDE_NUM_WIDTH);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *l = lv_label_create(kort);
    lv_label_set_text(l, tekst);
    zs_style_text(l, &zs_font_broed, ZS_C_TEXT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(l, 1);
}

static void on_guide_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_WELCOME);
}

static void on_guide_fortsaet(lv_event_t *e)
{
    (void)e;
    /*
     * HER begynder opsaetningen for alvor, saa inverter-flowet hoerer til
     * DEN og ikke til Indstillinger. Saettes her og ikke naar soegningen
     * starter: appen starter den selv i tre forskellige tilfaelde, og ét
     * af dem ville blive glemt.
     */
    s_inv_fra = ZS_INV_FRA_OPSAETNING;
    /*
     * Vi beder appen fortsaette i stedet for at gaa direkte til listen
     * over netvaerk: har vi et netvaerk og et kodeord fra sidst, proever
     * den det foerst. Se ZS_CMD_SETUP_CONTINUE.
     */
    send_cmd(ZS_CMD_SETUP_CONTINUE);
}

static void build_guide(void)
{
    zs_page_create(&s_guide_side, "Opret anlægget", on_guide_back, NULL, true);

    /*
     * TITLEN STAAR MIDT I BAANDET mellem linjen foroven og det foerste
     * kort - baade vandret og lodret.
     *
     * Den laa foer med 16 px luft over og en afsat hoejde paa 52. Men
     * teksten er ÉN linje paa 26 px, saa den sad i toppen af sin egen
     * kasse: 16 px over og 46 under. Den saa skubbet op ud, og det var
     * den.
     *
     * Midtstillingen laves af en FLEXBEHOLDER og ikke af et udregnet y,
     * og det er med vilje: saa skal vi ikke vide om teksten bliver én
     * eller to linjer. Paa et laengere sprog bryder den i to og bliver
     * stadig staaende i midten. Baandets hoejde staar i zs_layout.h med
     * en vagt paa at der er plads til to linjer.
     *
     * Beholderen er ikke en kasse man kan se: remove_style_all tager
     * baade bund, kant og luft, saa de 86 px er rene.
     */
    lv_obj_t *baand = lv_obj_create(s_guide_side.content);
    lv_obj_remove_style_all(baand);
    lv_obj_set_size(baand, ZS_CONTENT_WIDTH, ZS_G_GUIDE_BAAND);
    lv_obj_set_pos(baand, ZS_EDGE, 0);
    lv_obj_clear_flag(baand, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(baand, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(baand, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *over = lv_label_create(baand);
    lv_label_set_text(over, "Gør dette i Frekvenz-appen først");
    zs_style_text(over, &zs_font_rad, ZS_C_TEXT);
    lv_label_set_long_mode(over, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(over, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(over, ZS_CONTENT_WIDTH);

    lv_obj_t *col = zs_column_create(s_guide_side.content,
                                     ZS_G_GUIDE_CARD_GAP);
    /* Kolonnen har ZS_EDGE luft foroven og forneden indbygget, og den
     * skal IKKE have den her: kortenes placering er regnet ud til
     * pixlen i zs_layout.h, og en ekstra luft ville skubbe den sidste
     * ned under foden. */
    lv_obj_set_style_pad_ver(col, 0, 0);
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_GUIDE_CARDS_Y);

    guide_trin(col, "1", "Åbn Frekvenz-appen og log ind.");
    guide_trin(col, "2", "Tryk på plus-ikonet øverst.");
    guide_trin(col, "3", "Vælg zScreen på listen.");
    guide_trin(col, "4", "Følg guiden i appen.");

    lv_obj_t *btn = zs_btn_primary_create(s_guide_side.footer, "Fortsæt");
    lv_obj_set_width(btn, ZS_CONTENT_WIDTH);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, on_guide_fortsaet, LV_EVENT_CLICKED, NULL);
}

static void build_welcome(void)
{
    /* Ingen overskrift og ingen vej tilbage: det er den foerste side. */
    zs_page_create(&s_welcome, "", NULL, NULL, true);
    lv_obj_add_flag(s_welcome.head, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *c = s_welcome.content;

    lv_obj_t *logo = lv_img_create(c);
    lv_img_set_src(logo, zs_logo_wordmark());
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, ZS_G_SK_HEIGHT(90));

    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, "Se hvad dit anlæg laver lige nu");
    zs_style_text(t, &zs_font_rad, ZS_C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, ZS_G_SK_HEIGHT(175));

    lv_obj_t *s = lv_label_create(c);
    lv_label_set_text(s,
        "Skærmen skal på samme netværk som din inverter.\n"
        "Det tager et par minutter.");
    zs_style_text(s, &zs_font_broed, ZS_C_LABEL);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, ZS_G_SK_HEIGHT(210));

#if ZS_DEMO_ENABLED
    /*
     * To knapper: den primaere fylder mest, demoen staar under som en
     * rolig kant-knap. Foden er 76 px hoej, saa der er plads til en
     * 52 px knap. Demo-knappen laegges derfor i indholdet lige over
     * foden, hvor der er luft.
     */
    lv_obj_t *demo = zs_btn_secondary_create(c, "Se demo");
    lv_obj_set_width(demo, ZS_CONTENT_WIDTH);
    lv_obj_align(demo, LV_ALIGN_TOP_MID, 0, ZS_G_SK_HEIGHT(300));
    lv_obj_add_event_cb(demo, on_welcome_demo, LV_EVENT_CLICKED, NULL);

    lv_obj_t *dh = lv_label_create(c);
    lv_label_set_text(dh, "Se hvordan skærmen ser ud, uden et anlæg");
    zs_style_text(dh, &zs_font_broed, ZS_C_LABEL);
    lv_obj_set_style_text_align(dh, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(dh, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    lv_obj_align(dh, LV_ALIGN_TOP_MID, 0, ZS_G_SK_HEIGHT(358));
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
    /*
     * Vi vaelger IKKE siden selv. Appen gOEr.
     *
     * Trykker en kunde paa Netvaerk i indstillingerne og fortryder, skal
     * de hjem til skaermen igen. Foer foerte den her knap til
     * velkomstsiden, hvor den eneste knap er "Kom i gang", som foerer
     * tilbage hertil. Saa var der ingen vej hjem uden at gaa hele
     * opsaetningen igennem, inklusive en ny soegning efter inverteren.
     *
     * Og vi kan ikke bare vise hovedskaermen herfra: appen staar i
     * ST_SETUP, hvor den hverken forbinder eller aflaeser, saa skaermen
     * ville staa doed med gamle tal. Kun appen kan slippe tilstanden, og
     * kun den ved om der er en opsaetning at gaa tilbage til.
     */
    send_cmd(ZS_CMD_SETUP_CANCEL);
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
    zs_style_text(s_wifi_hint, &zs_font_broed, ZS_C_LABEL);
    lv_obj_align(s_wifi_hint, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_EDGE);

    s_wifi_list = zs_column_create(s_wifi.content, 8);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_SK_HEIGHT(34));

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
    /*
     * INGEN LAENGDE KRAEVES. OGSAA IKKE ET TOMT FELT.
     *
     * Her stod der at kodeordet skulle vaere paa mindst 8 tegn, fordi
     * det er WPA2's regel, og tanken var at sige det med det samme i
     * stedet for at lade routeren afvise os om ti sekunder.
     *
     * Men den spaerrede for det AABNE net. Listen springer
     * kodeordssiden over naar scanningen melder "Åbent" - men melder
     * den forkert, eller vil man videre alligevel, kunne man ikke komme
     * forbi et tomt felt: fluebenet svarede "mindst 8 tegn" paa et net
     * der ikke vil have nogen.
     *
     * Nu bliver et tomt felt et tomt kodeord, og zs_wifi_connect saetter
     * selv WIFI_AUTH_OPEN naar der ikke er noget. Se noten dér.
     *
     * OG ROUTEREN AFGOER RESTEN. Er kodeordet for kort til det net man
     * har valgt, siger routeren nej, og siden siger det videre med en
     * "Prøv igen" - i stedet for at vi afviser paa forhaand ud fra en
     * antagelse om hvilken slags net det er.
     *
     * DET GOER IKKE WEP MULIGT. Er der et kodeord, saetter
     * zs_wifi_connect WIFI_AUTH_WPA2_PSK som mindstekrav, og WEP bliver
     * afvist dér - med vilje, se noten i zs_wifi.c. Den beslutning
     * roerer vi ikke; den her aendring handler kun om at vi ikke selv
     * spaerrer inden routeren er spurgt.
     */
    if (pw == NULL) {
        pw = "";
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
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_LABEL_Y);

    /* Indtastningsfeltet. En ramme med teksten i, og et oeje ved siden
     * af til at vise hvad der staar. Samme felt som "find manuelt" -
     * se zs_field_create. Her stod de fjorten linjer der byggede
     * rammen i haanden, og de regnede tekstens bredde 1 px for bred. */
    zs_field_t felt;
    zs_field_create(&felt, s_pass.content,
                    ZS_CONTENT_WIDTH - ZS_TOUCH_MIN - ZS_G_SK_WIDTH(8));
    lv_obj_align(felt.box, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_BOX_Y);
    s_pass_field = felt.field;

    s_pass_eye = lv_btn_create(s_pass.content);
    lv_obj_remove_style_all(s_pass_eye);
    lv_obj_set_size(s_pass_eye, ZS_TOUCH_MIN, ZS_TOUCH_MIN);
    lv_obj_align(s_pass_eye, LV_ALIGN_TOP_RIGHT, -ZS_EDGE, ZS_G_FIELD_EYE_Y);
    lv_obj_add_event_cb(s_pass_eye, on_pass_eye, LV_EVENT_CLICKED, NULL);

    s_pass_eye_icon = lv_label_create(s_pass_eye);
    lv_label_set_text(s_pass_eye_icon, ZS_ICON_EYE);
    lv_obj_set_style_text_font(s_pass_eye_icon, &zs_ikon_lille, 0);
    lv_obj_set_style_text_color(s_pass_eye_icon, lv_color_hex(ZS_C_LABEL), 0);
    lv_obj_center(s_pass_eye_icon);

    /* At et TOMT felt er lovligt, skal staa paa siden.
     *
     * Ellers er det en mulighed man kun kender hvis man har laest koden:
     * et felt med en markoer i ser ud som et felt der SKAL udfyldes, og
     * saa staar montoeren og leder efter et kodeord til et aabent net. */
    lv_obj_t *hj = lv_label_create(s_pass.content);
    /* Tasten SKRIVES IKKE som et tegn her.
     *
     * Der stod "tryk bare ✓", og check-text.py faeldede det: U+2713
     * findes ikke i nogen af tekst-skrifttyperne, saa der ville staa en
     * firkant paa glasset. Tasten selv tegnes med ZS_ICON_CHECK fra
     * ikon-skrifttypen, og den kan ikke bruges midt i en saetning. Saa
     * beskrives den med ord i stedet. */
    lv_label_set_text(hj, "Er netværket åbent, så tryk bare på tasten "
                          "med fluebenet uden at skrive noget.");
    zs_style_text(hj, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(hj, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hj, ZS_CONTENT_WIDTH);
    lv_obj_align(hj, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_HJAELP_Y);

    /* Tastaturet ligger paa siden selv, ikke i indholdet, saa det
     * bliver staaende naar indholdet rulles. */
    s_kb = zs_keyboard_create(s_pass.root, s_pass_field, on_pass_done, NULL);
    /* Et kodeord maskeres. Oejet ved siden af slaar det fra, se
     * on_pass_eye. Det staar HER og ikke som tastaturets standard: se
     * noten i zs_keyboard_create. */
    zs_keyboard_set_password_hidden(s_kb, true);
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
    lv_obj_set_size(s_connect_icon, ZS_G_WAIT_SPIN_SIZE,
                    ZS_G_WAIT_SPIN_SIZE);
    lv_obj_align(s_connect_icon, LV_ALIGN_TOP_MID, 0, ZS_G_WAIT_SPIN_Y);
    lv_obj_set_style_arc_color(s_connect_icon, lv_color_hex(ZS_C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_connect_icon, lv_color_hex(ZS_C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_connect_icon, ZS_G_SK(5), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_connect_icon, ZS_G_SK(5), LV_PART_INDICATOR);

    s_connect_text = lv_label_create(s_connect.content);
    lv_label_set_text(s_connect_text, "Forbinder til netværket ...");
    zs_style_text(s_connect_text, &zs_font_rad, ZS_C_TEXT);
    lv_obj_set_style_text_align(s_connect_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_connect_text, ZS_SCR_WIDTH - 4 * ZS_EDGE);
    /* Ikke 160 skrevet ind: se ZS_G_WAIT_TEXT_Y i zs_layout.h. */
    lv_obj_align(s_connect_text, LV_ALIGN_TOP_MID, 0, ZS_G_WAIT_TEXT_Y);

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
    gaa_tilbage_fra_inverter();
}

static void build_scan(void)
{
    zs_page_create(&s_scan, "Søger", on_scan_back, NULL, false);

    /*
     * HELE TRINNET KOMMER FRA ÉT STED NU.
     *
     * Her stod skrift, farve, bredde og fire y-positioner skrevet ind.
     * Tilmeldingssiden havde sit eget saet, og de to var ikke de samme -
     * se noten i zs_step.h. Tallene er uaendrede paa 480, og der staar
     * en vagt paa det i zs_layout.h.
     */
    zs_step_create(&s_scan_step, s_scan.content);
    zs_step_saet(&s_scan_step, "Leder efter din inverter",
                 "Enheden gennemgår netværket. Vent venligst.");
}

/* ------------------------------------------------------------------ */
/* 6. Vælg inverter                                                    */
/* ------------------------------------------------------------------ */

static void on_inv_back(lv_event_t *e)
{
    (void)e;
    gaa_tilbage_fra_inverter();
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
    c.oprindelse = s_inv_fra;
    snprintf(c.ip, sizeof(c.ip), "%s", s_found[idx].ip);
    zs_app_send(&c);
    /*
     * Vi skifter IKKE side her.
     *
     * Appen bestemmer hvor brugeren skal hen, i slut_opsaetningen. Gjorde
     * vi begge dele, ville de to kappes om det, og LVGL holder laasen
     * gennem hele sin runde, saa appens valg altid vandt: den side vi
     * satte her, blev erstattet foer nogen saa den.
     *
     * Dengang var det trinnet med prisomraade der aldrig blev vist. Det
     * trin findes ikke mere - prisomraadet kommer ned fra serveren - men
     * reglen staar, for den handler om laasen og ikke om det trin.
     */
}

/*
 * SIDENS LODRETTE REGNESTYKKE, ÉT STED.
 *
 * Siden har ingen fod - de to knapper staar nederst i indholdet - saa
 * indholdet er hele hoejden under overskriften. Listen faar det der er
 * tilbage naar hjaelpelinjen foeroven og de to knapper nederst har faaet
 * deres.
 *
 * Tallene staar HER og ikke inde i build_inverter, fordi de skal gaa op:
 * aendrer nogen knaphoejden, skal listen blive kortere af sig selv og
 * ikke loebe ind under dem.
 */
/* Luft omkring tekst foelger TEKSTEN, altsaa ZS_G_SK. Tallene stod
 * raa her, og saa var luften den samme paa en 480 og en 1280 px
 * hoej skaerm. ZS_G_SK(v) giver praecis v paa 480, saa zScreen
 * staar stille. */
#define ZS_INV_LIST_Y   ZS_G_SK(56)  /* under hjaelpelinjen           */
#define ZS_INV_BTN_GAP  ZS_G_SK(8)   /* mellem de to knapper          */
/* Plads til: luft, knap, mellemrum, knap, luft. */
#define ZS_INV_BTNS_HEIGHT   (ZS_EDGE + ZS_BTN_HEIGHT + ZS_INV_BTN_GAP \
                         + ZS_BTN_HEIGHT + ZS_EDGE)
#define ZS_INV_CONTENT_HEIGHT (ZS_SCR_HEIGHT - ZS_PAGE_HEAD_HEIGHT)
#define ZS_INV_LIST_HEIGHT   (ZS_INV_CONTENT_HEIGHT - ZS_INV_LIST_Y - ZS_INV_BTNS_HEIGHT)

_Static_assert(ZS_INV_LIST_HEIGHT >= ZS_ROW_HEIGHT,
               "listen over invertere har ikke plads til én raekke: "
               "knapperne nederst fylder for meget");

/* ------------------------------------------------------------------ */
/* 6b. Find manuelt                                                    */
/* ------------------------------------------------------------------ */
/*
 * HVORFOR SIDEN FINDES.
 *
 * Soegningen gennemgaar undernettet og giver hver adresse 250 ms. Paa et
 * net hvor svartiderne svinger - maalt paa en mobil router 13. oktober
 * 2026: 3 til 180 ms til samme enhed - rammer den ikke altid. Og en
 * inverter kan staa paa et andet undernet end skaermen, hvor der ikke er
 * nogen soegning der kan naa den.
 *
 * Saa er der to muligheder: at montoeren proever igen og haaber, eller at
 * han skriver den adresse han kan laese paa inverterens egen webside. Den
 * sidste virker ogsaa naar soegningen aldrig vil kunne.
 *
 * SERIENUMMERET KOMMER AF SIG SELV.
 *
 * Vi sender den samme kommando som et valg fra listen,
 * ZS_CMD_INVERTER_SELECT. Appen taemmer serienummeret naar adressen ikke
 * stod i soegningens liste og tager det ved foerste forbindelse - se
 * noten dér. Der skal altsaa ingen ny vej gennem appen til, og de to
 * maader at vaelge en inverter paa kan ikke komme ud af trit.
 */

static void on_man_back(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_INVERTER_LIST);
}

static void on_inv_manual(lv_event_t *e)
{
    (void)e;
    /* Feltet starter TOMT og ikke med den sidste adresse.
     *
     * Den der staar her, er kommet fordi soegningen ikke fandt noget. Stod
     * der en adresse i forvejen, skulle han foerst slette den - og han
     * ville ikke kunne se om den var hans egen eller noget vi havde
     * gaettet. */
    if (s_man_kb != NULL) {
        zs_keyboard_clear(s_man_kb);
    }
    if (s_man_field != NULL) {
        lv_label_set_text(s_man_field, "");
    }
    if (s_man_fejl != NULL) {
        lv_label_set_text(s_man_fejl, "");
    }
    zs_ui_show(ZS_SCREEN_INVERTER_MANUAL);
}

/* Fluebenet paa tastaturet, og knappen i foden, gaar samme vej. */
static void on_man_brug(lv_event_t *e)
{
    (void)e;
    if (s_man_kb == NULL) {
        return;
    }
    const char *ip = zs_keyboard_get_text(s_man_kb);

    /*
     * ADRESSEN PROEVES HER, OG MED MODBUS' EGEN REGEL.
     *
     * zs_mb_maal_ok er den samme vagt forbindelsen selv bruger. En
     * adresse der godtages her og afvises dér, ville give en skaerm der
     * siger ja og saa staar og ikke kan forbinde - uden at nogen kan se
     * hvorfor. Én regel, ét sted.
     */
    if (!zs_mb_maal_ok(ip, ZS_MB_DEFAULT_PORT)) {
        lv_label_set_text(s_man_fejl,
            "Det ser ikke ud som en adresse. Den skal være fire tal med "
            "punktum imellem, fx 192.168.1.50");
        return;
    }
    lv_label_set_text(s_man_fejl, "");

    zs_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.type = ZS_CMD_INVERTER_SELECT;
    c.oprindelse = s_inv_fra;
    snprintf(c.ip, sizeof(c.ip), "%s", ip);
    zs_app_send(&c);
    /* Appen bestemmer hvor brugeren skal hen, som ved et valg fra
     * listen. Se noten i on_inv_pick om hvorfor vi ikke skifter side. */
}

static void build_manual(void)
{
    zs_page_create(&s_man, "Find manuelt", on_man_back, NULL, false);

    lv_obj_t *lbl = zs_label_create(s_man.content, "INVERTERENS IP-ADRESSE");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_LABEL_Y);

    /* Samme felt som kodeordssiden: en ramme med teksten i. To felter der
     * ser ens ud, er to felter montoeren ikke skal laere hver for sig -
     * og nu er det ogsaa den SAMME kode, saa de ikke kan komme i utakt. */
    zs_field_t felt;
    zs_field_create(&felt, s_man.content, ZS_CONTENT_WIDTH);
    lv_obj_align(felt.box, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_BOX_Y);
    s_man_field = felt.field;

    /* Hvor man finder den. Det er det foerste spoergsmaal der kommer. */
    lv_obj_t *hj = lv_label_create(s_man.content);
    lv_label_set_text(hj,
        "Står på inverterens webside under Kommunikation, hvor Modbus TCP "
        "også slås til.");
    zs_style_text(hj, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(hj, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hj, ZS_CONTENT_WIDTH);
    lv_obj_align(hj, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_FIELD_HJAELP_Y);

    /*
     * FEJLLINJEN HAR SIN EGEN PLADS, OGSAA NAAR DEN ER TOM.
     *
     * Stod den ikke der, ville alt nedenfor hoppe en linje ned foerste
     * gang en adresse blev afvist - og saa flytter knappen sig lige
     * inden man trykker paa den igen.
     */
    s_man_fejl = lv_label_create(s_man.content);
    lv_label_set_text(s_man_fejl, "");
    zs_style_text(s_man_fejl, &zs_font_broed, ZS_C_BAD);
    lv_label_set_long_mode(s_man_fejl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_man_fejl, ZS_CONTENT_WIDTH);
    lv_obj_align(s_man_fejl, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_G_SK_HEIGHT(168));

    /* Tastaturet ligger paa siden selv og ikke i indholdet, saa det
     * bliver staaende naar indholdet rulles. Som paa kodeordssiden. */
    s_man_kb = zs_keyboard_create(s_man.root, s_man_field, on_man_brug, NULL);
    /* Cifre fra foerste tastetryk: der skal staa en adresse. */
    zs_keyboard_set_numeric(s_man_kb, true);
    /* Og tegnene SKAL kunne ses. Det er standarden nu, men det staar her
     * udtrykkeligt: en adresse hvor man ikke kan se hvad man har tastet,
     * er en adresse man taster forkert. */
    zs_keyboard_set_password_hidden(s_man_kb, false);
}

static void build_inverter(void)
{
    zs_page_create(&s_inv, "Vælg inverter", on_inv_back, NULL, false);

    s_inv_hint = lv_label_create(s_inv.content);
    lv_label_set_text(s_inv_hint, "");
    zs_style_text(s_inv_hint, &zs_font_broed, ZS_C_LABEL);
    lv_label_set_long_mode(s_inv_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_inv_hint, ZS_CONTENT_WIDTH);
    lv_obj_align(s_inv_hint, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_EDGE);

    s_inv_list = zs_column_create(s_inv.content, 8);
    lv_obj_align(s_inv_list, LV_ALIGN_TOP_LEFT, ZS_EDGE, ZS_INV_LIST_Y);
    /*
     * LISTEN FAAR SIN EGEN HOEJDE OG RULLER INDE I DEN.
     *
     * Kolonnen er LV_SIZE_CONTENT, altsaa saa hoej som sit indhold.
     * Knapperne nedenfor er bundet til indholdets NEDERSTE kant, saa med
     * otte fundne invertere - ZS_DISCOVERY_MAX - ville raekkerne loebe
     * ind under dem. To ting oven i hinanden, hvor den oeverste kan
     * trykkes, er den slags der vaelger en anden inverter end den
     * montoeren sigtede efter.
     *
     * Nu stopper listen dér hvor knapperne begynder, og ruller indeni.
     * Regnestykket staar i ZS_INV_LIST_HEIGHT, saa de to tal ikke kan komme
     * ud af trit.
     */
    lv_obj_set_height(s_inv_list, ZS_INV_LIST_HEIGHT);
    lv_obj_add_flag(s_inv_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_inv_list, LV_DIR_VER);

    /*
     * TO KNAPPER, OG DE KAN IKKE BEGGE VAERE I FODEN.
     *
     * Foden er ZS_PAGE_FOOT_HEIGHT hoej med ZS_EDGE luft hele vejen
     * rundt, altsaa praecis plads til ÉN knap paa ZS_BTN_HEIGHT. Lagde
     * vi to derned, ville den nederste rage ud over skaermkanten, og det
     * ses ikke paa en oversigt - kun paa glasset.
     *
     * De staar derfor nederst i INDHOLDET, stablet, og siden har ingen
     * fod. Raekkefoelgen er med vilje: "Søg igen" er det man vil i ni af
     * ti tilfaelde, saa den staar OVERST og er den primaere. At skrive en
     * adresse i haanden er undtagelsen, og den skal se ud som en
     * undtagelse.
     */
    s_inv_rescan = zs_btn_primary_create(s_inv.content, "Søg igen");
    lv_obj_set_width(s_inv_rescan, ZS_CONTENT_WIDTH);
    lv_obj_align(s_inv_rescan, LV_ALIGN_BOTTOM_MID, 0,
                 -(ZS_EDGE + ZS_BTN_HEIGHT + ZS_INV_BTN_GAP));
    lv_obj_add_event_cb(s_inv_rescan, on_inv_rescan, LV_EVENT_CLICKED, NULL);

    s_inv_manual = zs_btn_secondary_create(s_inv.content, "Find manuelt");
    lv_obj_set_width(s_inv_manual, ZS_CONTENT_WIDTH);
    lv_obj_align(s_inv_manual, LV_ALIGN_BOTTOM_MID, 0, -ZS_EDGE);
    lv_obj_add_event_cb(s_inv_manual, on_inv_manual, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* Udadtil                                                             */
/* ------------------------------------------------------------------ */

void zs_setup_create(void)
{
    build_welcome();
    build_guide();
    build_wifi();
    build_password();
    build_connecting();
    build_scan();
    build_inverter();
    build_manual();
}

lv_obj_t *zs_setup_root_welcome(void)    { return s_welcome.root; }
lv_obj_t *zs_setup_root_guide(void)      { return s_guide_side.root; }
lv_obj_t *zs_setup_root_inverter_manual(void) { return s_man.root; }
lv_obj_t *zs_setup_root_wifi(void)       { return s_wifi.root; }
lv_obj_t *zs_setup_root_password(void)   { return s_pass.root; }
lv_obj_t *zs_setup_root_connecting(void) { return s_connect.root; }
lv_obj_t *zs_setup_root_scan(void)       { return s_scan.root; }
lv_obj_t *zs_setup_root_inverter(void)   { return s_inv.root; }

/* Netvaerkene skal overleve at listen tegnes om, fordi hver rad peger
 * paa sin egen post. Derfor en kopi her og ikke en pegepind udefra. */
static zs_ap_t s_aps[ZS_WIFI_MAX_APS];
static int     s_aps_n;

/*
 * "NETKORTET SVAREDE IKKE" ER IKKE "DER ER INGEN NETVAERK".
 *
 * zs_wifi_scan skelner: -1 naar der ikke kunne soeges, 0 naar der ER
 * soegt og intet fundet. Men zs_app gjorde "n < 0 ? 0 : n" paa alle
 * tre kaldssteder, saa fejlen blev lavet om til et tal - og det tal
 * havde en besked der peger det forkerte sted hen.
 *
 * Paa ZDisplay er wifi foerst klar efter 26 sekunder, se
 * ZS_WIFI_SCAN_FORSOEG. Saa stod der "der blev ikke fundet nogen
 * netvaerk" paa et sted hvor der var fem.
 */
void zs_setup_set_wifi_intet_svar(void)
{
    clear_list(s_wifi_list);
    s_aps_n = 0;
    if (s_wifi_hint == NULL) {
        return;
    }
    lv_label_set_text(s_wifi_hint,
        "Det trådløse er ikke klart endnu.\n\n"
        "Skærmen søgte flere gange, men netkortet svarede ikke. Efter "
        "en opstart kan det tage op mod et halvt minut.\n\n"
        "Tryk \"Søg igen\" om et øjeblik.");
}

void zs_setup_set_wifi_list(const zs_ap_t *aps, int n)
{
    clear_list(s_wifi_list);
    s_aps_n = 0;

    if (n <= 0) {
        lv_label_set_text(s_wifi_hint,
            "Der blev ikke fundet nogen netværk.\nTryk \"Søg igen\" nedenfor.");
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
    if (s_scan_step.bjaelke == NULL) {
        return;
    }
    /* Klipningen ligger i zs_step_fremdrift, altsaa dér hvor bjaelken
     * bor. Her regnes der kun om til procent. */
    zs_step_fremdrift(&s_scan_step,
                      (total > 0) ? (done * 100 / total) : 0);

    char t[64];
    if (found <= 0) {
        t[0] = '\0';
    } else if (found == 1) {
        snprintf(t, sizeof(t), "1 inverter fundet");
    } else {
        snprintf(t, sizeof(t), "%d invertere fundet", found);
    }
    zs_step_note(&s_scan_step, t);
}

/*
 * "SKAERMEN ER IKKE PAA NETTET" ER IKKE "DER ER INGEN INVERTER".
 *
 * Foer stod de to ting samme sted: naar skaermen ikke havde nogen
 * adresse, blev den tomme liste vist med beskeden nedenfor - altsaa
 * "slaa Modbus TCP til i inverterens webside".
 *
 * Maalt paa et rigtigt anlaeg 17. oktober 2026: inverteren svarede paa
 * 192.168.0.25 og .43, Modbus var slaaet til, og SunS-markoeren stod
 * paa 40000. Der var INTET at rette paa inverteren. Skaermen var bare
 * ikke paa nettet endnu. Beskeden sendte montoeren ud i inverterens
 * menuer i stedet for tilbage til wi-fi-trinnet, og det er den dyreste
 * slags fejl vi kan lave: en der er sikker paa sig selv og peger det
 * forkerte sted hen.
 *
 * Derfor sin egen tilstand, og med vilje UDEN et ord om Modbus.
 */
void zs_setup_set_inverter_intet_net(void)
{
    clear_list(s_inv_list);
    s_found_n = 0;

    if (s_inv_hint == NULL) {
        return;
    }
    lv_label_set_text(s_inv_hint,
        "Skærmen er ikke på et netværk.\n\n"
        "Søgningen efter inverteren går gennem netværket, så den kan "
        "ikke begynde endnu. Gå tilbage og vælg dit wi-fi.\n\n"
        "Står netværket allerede som forbundet, har routeren ikke givet "
        "skærmen en adresse endnu. Vent et øjeblik og søg igen.");
}

void zs_setup_set_inverter_list(const zs_found_t *list, int n)
{
    clear_list(s_inv_list);
    s_found_n = 0;

    if (n <= 0) {
        lv_label_set_text(s_inv_hint,
            /*
             * DEN HER TEKST ER DEN VIGTIGSTE PAA SIDEN, for den laeses
             * af en der staar og er gaaet i staa.
             *
             * Den skal svare paa tre ting i den raekkefoelge et menneske
             * spoerger dem:
             *
             *   1. hvad skete der        "der blev ikke fundet nogen"
             *   2. hvad GOER jeg         slaa Modbus TCP til, og hvor
             *                            man kan se hvordan
             *   3. hvad saa              de to knapper nedenfor, nævnt
             *                            saa man ved hvad de er til
             *
             * Og den peger paa guiden i appen i stedet for at forklare
             * inverterens webside her. Den ser forskellig ud fra model
             * til model, og en forklaring der passer paa én Fronius og
             * ikke paa den naeste, sender montoeren det forkerte sted
             * hen. Guiden kan rettes uden en firmwareudgivelse.
             *
             * Der staar IKKE "fejl" eller "kunne ikke". Det er normalt
             * at Modbus TCP er slaaet fra fra fabrikken, saa det her er
             * et trin der mangler - ikke noget der gik i stykker.
             */
            "Der blev ikke fundet nogen inverter.\n\n"
            "Modbus TCP skal slås til i inverterens webside, under "
            "Kommunikation. Følg guiden i Frekvenz-appen. Den viser "
            "hvordan på netop din inverter.\n\n"
            "Tjek også at skærmen er på samme netværk som inverteren.\n\n"
            "Søg igen når det er gjort. Kender du inverterens "
            "IP-adresse, kan du skrive den selv.");
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
        lv_obj_set_height(r.row, ZS_G_SK_HEIGHT(72));   /* to linjer: navn og detaljer */
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
        zs_style_text(sl, &zs_font_broed, ZS_C_LABEL);
        lv_obj_align(sl, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, ZS_G_SK_HEIGHT(15));

        /* Ryk navnet op saa de to linjer staar paent under hinanden.
         * 72 px rad, to linjer paa 24 og 18: 12 op og 15 ned lader dem
         * staa symmetrisk om midten. */
        if (r.title != NULL) {
            lv_obj_align(r.title, LV_ALIGN_LEFT_MID, ZS_ROW_ICON_W, -ZS_G_SK_HEIGHT(12));
        }
    }
}

/*
 * River opsaetningens syv sider ned.
 *
 * Hver side har sin egen rod paa skaermen, saa de skal slettes hver for
 * sig. Tastaturet rydder op efter sig selv naar dets matrix slettes,
 * saa her nulstiller vi kun pegepinden.
 *
 * De fundne netvaerk og den valgte inverter bliver staaende. Det er
 * data, ikke objekter, og de skal ikke findes forfra bare fordi
 * skaermen blev tegnet om.
 */
void zs_setup_destroy(void)
{
    zs_page_t *sider[] = { &s_welcome, &s_wifi, &s_pass, &s_connect,
                           &s_scan, &s_inv };
    for (size_t i = 0; i < sizeof(sider) / sizeof(sider[0]); i++) {
        if (sider[i]->root != NULL) {
            lv_obj_del(sider[i]->root);
        }
        memset(sider[i], 0, sizeof(*sider[i]));
    }

    s_wifi_list     = NULL;
    s_wifi_hint     = NULL;
    s_wifi_rescan   = NULL;
    s_kb            = NULL;
    s_pass_field    = NULL;
    s_pass_eye      = NULL;
    s_pass_eye_icon = NULL;
    s_connect_icon  = NULL;
    s_connect_text  = NULL;
    s_connect_retry = NULL;
    s_scan_step     = (zs_step_t){ 0 };
    s_inv_list      = NULL;
    s_inv_hint      = NULL;
    s_inv_rescan    = NULL;
    s_inv_manual    = NULL;
    s_man_field     = NULL;
    s_man_fejl      = NULL;
    /* Tastaturet ryddes af LVGL med siden, se kb_on_delete. Her nulstilles
     * kun pegeren, saa den ikke peger paa noget der er vaek. */
    s_man_kb        = NULL;
}
