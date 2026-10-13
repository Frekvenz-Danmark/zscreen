#include "zs_screen_tilmeld.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "zs_app.h"
#include "zs_icons.h"
#include "zs_qr.h"
#include "zs_step.h"  /* ventetrinnet, delt med inverter-soegningen */
#include "zs_fleet.h"   /* zs_fleet_state, til bjaelkens fremdrift */
#include "zs_theme.h"
#include "zs_ui.h"

static const char *TAG = "tilmeld";

static zs_page_t  s_side;
static lv_obj_t  *s_over;        /* hvad man skal goere, oeverst       */
static lv_obj_t  *s_plade;       /* hvid bund under QR'en              */
static lv_obj_t  *s_qr;          /* selve QR'en, tegnet modul for modul */
static lv_obj_t  *s_kode;        /* de seks tegn, eller en besked      */
static lv_obj_t  *s_hjaelp;      /* hvad man goer hvis man ikke kan scanne */
static lv_obj_t  *s_guide;       /* hvor vejledningen staar            */
static lv_obj_t  *s_knap;        /* "Lav en kode", noedudgang          */
/*
 * VENTETILSTANDEN ER ET HELT ANDET LAYOUT, OG DET ER DEN SAMME SOM
 * INVERTER-SOEGNINGENS.
 *
 * Her stod en bjaelke alene, som blev vist midt i sidens egen stak.
 * Resultatet var en side der LIGNEDE inverter-soegningen uden at vaere
 * den: titlen i 20 px mod 28, forklaringen i en anden daempet farve, og
 * bjaelken 456 px bred mod 432. Se noten i zs_step.h.
 *
 * Nu vises trinnet i stedet for stakken naar vi venter, og det er det
 * SAMME element som soegningen bruger. De kan ikke skride fra hinanden
 * mere, for der er kun ét sted at rette.
 */
static zs_step_t  s_trin;

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

/*
 * Pladens luft rundt om QR'en, i PIXELS.
 *
 * Fire og ikke otte. Den hvide luft der betyder noget for laeseren,
 * ligger allerede INDE i laerredet - fire moduler, altsaa 28 pixels ved
 * skalaen her - saa det her er kun kanten paa den hvide plade. Otte
 * pixels mere gjorde bare pladen stoerre uden at gavne nogen.
 *
 * Mindre end det kan ikke anbefales: pladen skal kunne ses som en plade,
 * ellers forsvinder den hvide kant mod en lys baggrund.
 */
#define PLADE_LUFT_PX     4

/*
 * Laerredet er ét pixel per modul. Det vokser IKKE naar QR'en tegnes
 * stoerre: opskaleringen sker i LVGL med lv_img_set_zoom, saa 145 px og
 * 203 px koster de samme 1,7 kB. Se noten oeverst.
 */
static lv_color_t s_qr_lae[QR_MAKS_MODULER * QR_MAKS_MODULER];
static uint8_t    s_qr_data[ZS_QR_BUF];

/*
 * DET LODRETTE BUDGET.
 *
 * Siden har 424 pixels at give af (480 minus hovedet paa 56, ingen fod).
 * Her stod der foer faste y-vaerdier for hver linje, og saa laa det hos
 * den der aendrede en tekst at regne efter om den stadig kunne vaere
 * der. En tekst der bliver én linje laengere, laa oven i den naeste.
 *
 * Nu stables alt af stak() ud fra de hoejder LVGL faktisk har regnet, og
 * tallene her er kun LUFTEN imellem. Summen maales ved opstart og
 * skrives i loggen - se maal_stakken().
 */
#define Y_TOP          8
#define LUFT_FOER_QR   6
#define LUFT_FOER_KODE 8
#define LUFT_FOER_HJ   4
#define LUFT_FOER_GU   4
#define Y_BUND         8   /* luft der skal vaere tilbage nederst */

/* Hvor tilbage foerer hen. Se headeren: siden naas fra to steder. */
static zs_screen_id_t s_tilbage_til = ZS_SCREEN_SETTINGS;

void zs_tilmeld_page_set_return(zs_screen_id_t hvorhen)
{
    s_tilbage_til = hvorhen;
}

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
    zs_ui_show(s_tilbage_til);
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
    int skala = zs_qr_skala(moduler, ZS_QR_SKAERM_MAAL_PX);
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
    /*
     * Luft rundt om pladen, i PIXELS.
     *
     * Her stod "px + 2 * ZS_QR_KANT_MODULER", og det lagde et antal
     * MODULER til et antal PIXELS. Det gav 8 pixels og saa rigtigt ud,
     * men kun fordi kanten tilfaeldigvis er 4. Blev kanten sat til 6
     * moduler for at foelge en strengere laeser, ville pladen vokse med
     * 4 pixels i stedet for de 12 man bad om. QR'ens egen lyse kant
     * ligger allerede INDE i laerredet, saa det her er bare luft til
     * kanten af pladen.
     */
    lv_obj_set_size(s_plade, px + 2 * PLADE_LUFT_PX, px + 2 * PLADE_LUFT_PX);
    lv_obj_clear_flag(s_plade, LV_OBJ_FLAG_HIDDEN);
}

/* Er objektet med i stakken lige nu? */
static bool synlig(lv_obj_t *o)
{
    return o != NULL && !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
}

/*
 * STABLER DET DER ER SYNLIGT, OPPEFRA OG NED.
 *
 * Hoejderne tages fra LVGL efter en lv_obj_update_layout, altsaa de
 * hoejder teksten FAKTISK fik, ogsaa naar den braekker i to eller tre
 * linjer. Det er forskellen fra faste y-vaerdier: en tekst der vokser,
 * skubber det naeste ned i stedet for at lande oven i det.
 *
 * Giver hvor mange pixels stakken endte med at bruge.
 */
static int stak(void)
{
    /* Hoejderne er ikke friske foer layoutet er regnet. Uden det her
     * stabler vi efter de hoejder teksten havde FOER den blev sat. */
    lv_obj_update_layout(s_side.content);

    struct { lv_obj_t *o; int luft; } raekke[] = {
        { s_over,    0              },
        { s_plade,   LUFT_FOER_QR   },
        { s_kode,    LUFT_FOER_KODE },
        { s_hjaelp,  LUFT_FOER_HJ   },
        /* Bjaelken er IKKE i stakken mere. Den hoerer til ventetrinnet,
         * som har faste positioner - se noten ved s_trin. */
        { s_guide,   LUFT_FOER_GU   },
        { s_knap,    LUFT_FOER_HJ   },
    };

    int y = Y_TOP;
    for (size_t i = 0; i < sizeof(raekke) / sizeof(raekke[0]); i++) {
        if (!synlig(raekke[i].o)) {
            continue;
        }
        y += raekke[i].luft;
        if (raekke[i].o == s_plade) {
            /* Pladen midtstilles. Alt andet fylder hele bredden og er
             * midtstillet i sin egen tekst. */
            lv_obj_align(s_plade, LV_ALIGN_TOP_MID, 0, y);
        } else {
            lv_obj_set_pos(raekke[i].o, ZS_EDGE, y);
        }
        y += lv_obj_get_height(raekke[i].o);
    }
    return y;
}

/*
 * MAALER STAKKEN ÉN GANG VED OPSTART OG SKRIVER DET I LOGGEN.
 *
 * Den maaler den TUNGESTE tilstand - koden klar, med QR, med alle fire
 * tekster - for det er den der kan blive for hoej. Og den maaler paa
 * rigtige skrifttyper paa rigtigt jern, saa tallet er ikke et overslag.
 *
 * Bliver den for hoej, staar der en advarsel i loggen ved hver opstart.
 * Siden kan rulles, saa der forsvinder ingenting - men en QR man skal
 * rulle til, er en QR montoeren ikke finder, og det skal man kunne se
 * uden at staa med skaermen i haanden.
 */
static void maal_stakken(void)
{
    /*
     * Tegn FOERST, maal saa. Her blev pladsen laest foerst, og saa gav
     * lv_obj_get_height nul: LVGL havde ikke regnet sidens layout endnu,
     * for intet var blevet vist. Saa stod der "der er 0" i loggen og en
     * advarsel ved hver opstart, om en side der passede fint.
     *
     * En kode der ligner en rigtig: seks tegn af det samme alfabet.
     */
    zs_tilmeld_page_update(ZS_TILMELD_KLAR, "ABCDEF");
    int brugt = stak();
    int plads = lv_obj_get_height(s_side.content);
    if (brugt + Y_BUND > plads) {
        ESP_LOGW(TAG, "tilmeldingssiden bruger %d px, der er %d. "
                      "QR'en skal rulles frem", brugt, plads);
    } else {
        ESP_LOGI(TAG, "tilmeldingssiden bruger %d af %d px, QR %d px",
                 brugt, plads, ZS_QR_SKAERM_MAAL_PX);
    }
}

/* En midtstillet tekstlinje der fylder sidens bredde. Alle fem er ens
 * paa alt andet end skrift og farve, saa de laves ét sted. */
static lv_obj_t *linje(const lv_font_t *skrift, uint32_t farve)
{
    lv_obj_t *l = lv_label_create(s_side.content);
    zs_style_text(l, skrift, farve);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, ZS_CONTENT_WIDTH);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

void zs_tilmeld_page_create(void)
{
    zs_page_create(&s_side, "Tilmeld anlægget", on_tilbage, NULL, false);

    /*
     * Handlingen staar OEVERST og i den stoerste tekst paa siden.
     *
     * Det er det foerste et menneske laeser, og det eneste de skal goere.
     * Alt det andet - koden, hvad man goer hvis man ikke kan scanne,
     * vejledningen, hvor laenge den gaelder - er svar paa spoergsmaal
     * man kun stiller hvis det foerste ikke lykkedes.
     */
    s_over = linje(&zs_font_20, ZS_C_TEXT);

    /* Hvid plade med luft, saa QR'en staar paa lyst uanset tema. */
    s_plade = lv_obj_create(s_side.content);
    lv_obj_set_style_bg_color(s_plade, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_plade, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_plade, 0, 0);
    lv_obj_set_style_pad_all(s_plade, 0, 0);
    lv_obj_set_style_radius(s_plade, 0, 0);
    lv_obj_clear_flag(s_plade, LV_OBJ_FLAG_SCROLLABLE);
    s_qr = lv_canvas_create(s_plade);
    lv_obj_center(s_qr);
    lv_obj_add_flag(s_plade, LV_OBJ_FLAG_HIDDEN);

    /*
     * Koden staar i zs_font_28 og ikke i zs_font_num_64.
     *
     * Den store skrift har KUN cifre og komma - den blev lavet til
     * wattene paa hovedskaermen - og koden har bogstaver. Skulle den
     * staa stoerre, skal de 32 tegn i alfabetet med i den store
     * skrifttype, og det koster flash vi foerst vil maale.
     */
    s_kode   = linje(&zs_font_28, ZS_C_VALUE);
    s_hjaelp = linje(&zs_font_16, ZS_C_TEXT_DIM);

    /*
     * VENTETRINNET.
     *
     * Her stod otte linjer der byggede en bjaelke med "de samme maal og
     * farver som inverter-soegningens" - og en kommentar der sagde at
     * tallene var de samme med vilje. De var ikke. En kommentar kan ikke
     * holde to filer enige.
     *
     * Hele trinnet kommer nu fra zs_step.c, og maalene fra zs_layout.h.
     */
    zs_step_create(&s_trin, s_side.content);
    zs_step_vis(&s_trin, false);

    s_guide  = linje(&zs_font_16, ZS_C_TEXT_DIM);

    s_knap = zs_btn_primary_create(s_side.content, "Lav en kode");
    lv_obj_set_width(s_knap, ZS_CONTENT_WIDTH);
    lv_obj_add_event_cb(s_knap, on_lav_kode, LV_EVENT_CLICKED, NULL);

    /* Maal den tungeste tilstand én gang, mens skrifttyperne er de
     * rigtige og skaermen er den rigtige. Saa staar tallet i loggen. */
    maal_stakken();

    zs_tilmeld_page_update(ZS_TILMELD_MANGLER_INVERTER, "");
}

lv_obj_t *zs_tilmeld_page_root(void) { return s_side.root; }

/* Vis eller skjul. Samme sted for alle fem, saa en tilstand ikke kan
 * komme til at glemme at skjule noget den anden viste. */
static void vis(lv_obj_t *o, bool paa)
{
    if (o == NULL) {
        return;
    }
    if (paa) {
        lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

void zs_tilmeld_page_update(zs_tilmeld_stand_t stand, const char *kode)
{
    if (s_kode == NULL) {
        return;
    }
    if (kode == NULL) {
        kode = "";
    }
    bool klar = (stand == ZS_TILMELD_KLAR);
    bool tilmeldt = (stand == ZS_TILMELD_TILMELDT);
    /* Beregnes HER og ikke nede ved bjaelken. Den afgoer nu hele sidens
     * udseende og ikke bare om der er en bjaelke, saa den skal vaere
     * kendt foer der bliver vist eller skjult noget. */
    bool venter = (stand == ZS_TILMELD_VENTER_PAA_SERVER);

    switch (stand) {
    case ZS_TILMELD_KLAR:
        /*
         * TEKSTEN ER BYGGET SOM TRE TRIN, OG DET ER MED VILJE.
         *
         *   1. Hvad du skal goere          -> oeverst, stoerst
         *   2. Koden, hvis du ikke kan     -> de seks tegn, saa linjen
         *                                     der siger hvad de er til
         *   3. Hvor du laeser mere         -> nederst, mindst
         *
         * Der staar "koden" og ikke "QR-koden" i det foerste trin, fordi
         * det der staar paa skaermen ER baade en QR og seks tegn - det er
         * den samme kode. Stod der "Scan QR-koden ... kan du ikke scanne
         * QR-koden, saa indtast denne kode", skulle laeseren selv finde
         * ud af om de tre ting var det samme.
         *
         * Og der staar ikke "evt". Et raad man har skrevet "evt" foran,
         * er et raad man ikke mener.
         */
        lv_label_set_text(s_over, "Scan koden i Frekvenz-appen");
        lv_label_set_text(s_kode, kode);
        lv_label_set_text(s_hjaelp,
            "Kan du ikke scanne? Indtast koden i appen eller på portalen.");
        lv_label_set_text(s_guide, "Vejledning: www.frekvenz.nu/qr-guide");
        /*
         * Der staar IKKE hvor laenge koden gaelder.
         *
         * Den stod her foer, med minutter og det hele. Men koden fornyes
         * af sig selv: tilmeld_tik i zs_app.c beder om en ny fem gange i
         * sekundet mens siden er aaben, og zs_tilmeld_bed_om laver kun en
         * ny naar den gamle er doed. Kunden kan altsaa ikke komme til at
         * staa med en kode der ikke virker, og saa er en nedtaelling
         * noget man skal forholde sig til uden at kunne bruge den til
         * noget. Udloebet selv bliver, se zs_tilmeld_min_igen og
         * tests/host/test_tilmeld.c.
         */
        tegn_qr(kode);
        break;

    case ZS_TILMELD_MANGLER_INVERTER:
        /* Den dyre halvdel af beslutningen. Der skal staa HVAD man goer. */
        lv_label_set_text(s_over, "Anlægget er ikke fundet endnu");
        lv_label_set_text(s_kode, "-");
        lv_label_set_text(s_hjaelp,
            "Skærmen skal finde inverteren, før anlægget kan tilmeldes. "
            "Gå til Indstillinger og vælg Inverter.");
        break;

    /*
     * DE TRE HER STOD FOER SOM ÉN, OG DEN ENE LOEJ.
     *
     * Der stod "Skærmen er ikke på nettet endnu. Tjek netværket under
     * Indstillinger" i hele det halve minut hvor skærmen var på nettet
     * med IP og bare ikke indmeldt endnu. Montøren blev sendt ud at
     * lede efter en fejl i et netværk der virkede. Se noten ved
     * zs_tilmeld_vej_t.
     *
     * De tre kræver tre forskellige ting: sæt nettet op, vent, eller
     * ring til Frekvenz. Derfor tre beskeder.
     */
    case ZS_TILMELD_MANGLER_NET:
        lv_label_set_text(s_over, "Skærmen er ikke på et netværk");
        lv_label_set_text(s_kode, "-");
        lv_label_set_text(s_hjaelp,
            "Koden skal også kendes af serveren, ellers virker den ikke. "
            "Vælg netværk under Indstillinger.");
        break;

    case ZS_TILMELD_VENTER_PAA_SERVER:
        /*
         * Der staar IKKE "fejl" nogen steder, for det er der ikke. Og
         * der staar hvad man skal gøre: ingenting. En montør der tror
         * han skal gribe ind, går ud af siden - og så var koden væk.
         *
         * Der står heller ikke "om et øjeblik". Målt på jern 11.
         * oktober 2026 kl. 23:12: serveren lukkede forbindelsen fire
         * gange i træk, og indmeldelsen var først i hus efter 64
         * sekunder, fordi den forrige session levede 90 sekunder efter
         * at enheden var væk - se dublet.py i zScreen-server. Syv
         * minutter senere gik den igennem i første forsøg efter 34
         * sekunder. Når ventetiden svinger sådan, er "et øjeblik" en ny
         * lille løgn oven i den vi lige fjernede.
         */
        /*
         * OG DEN ER KORT, som de andre ventesider.
         *
         * Her stod tre linjer der forklarede hvad der skete og hvad man
         * ikke skulle gøre. Det er rigtigt indhold, men det er for meget
         * paa en side man staar og venter paa: en montoer laeser ikke tre
         * linjer, han ser om der sker noget.
         *
         * Husets egne ventesider siger "Søger" og "Leder efter din
         * inverter". Den her siger nu det samme slags.
         */
        /*
         * TEKSTEN STAAR I TRINNET OG IKKE I SIDENS EGEN STAK.
         *
         * Der stod "-" i kodefeltet, altsaa i den store skrift hvor de
         * seks tegn ellers staar. Paa en side man venter paa, ser det ud
         * som et felt der skulle have haft indhold og ikke fik det.
         * Hele stakken skjules nu i stedet, og trinnet vises - det samme
         * element inverter-soegningen bruger.
         */
        zs_step_saet(&s_trin, "Henter forbindelse",
                     "Enheden melder sig ind hos Frekvenz. Det tager "
                     "få sekunder.");
        break;

    case ZS_TILMELD_TILMELDT:
        /*
         * Navnet kommer i "kode"-parameteren.
         *
         * Det ser mærkeligt ud, og alternativet var værre: en parameter
         * mere på en funktion der kaldes fra fire steder, og som i tre
         * af dem altid ville være NULL. Strengen er "det der skal stå i
         * den store linje", og i den her tilstand er det navnet.
         *
         * Og der står HVEM. Det er ikke pynt: det er den eneste måde
         * montøren kan se at koden endte hos den rigtige kunde. Står der
         * et navn han ikke kender, er anlægget tilmeldt den forkerte,
         * og det skal opdages nu og ikke om en måned.
         */
        lv_label_set_text(s_over, "Anlægget er tilmeldt");
        lv_label_set_text(s_kode, (kode != NULL && kode[0] != '\0')
                                  ? kode : "-");
        lv_label_set_text(s_hjaelp,
            "Står der et navn du ikke kender, er anlægget tilmeldt den "
            "forkerte kunde. Ring til Frekvenz.");
        break;

    case ZS_TILMELD_SERVER_NEJ:
        lv_label_set_text(s_over, "Frekvenz kender ikke skærmen");
        lv_label_set_text(s_kode, "-");
        lv_label_set_text(s_hjaelp,
            "Det går ikke væk af sig selv. Ring til Frekvenz og oplys "
            "skærmens nummer bag på kassen.");
        break;

    case ZS_TILMELD_UDLOEBET:
        /*
         * DEN HER SKAL NORMALT IKKE KUNNE SES.
         *
         * Koden laves af sig selv ved hvert tik så længe siden står
         * åben - se zs_tilmeld.h. Så det her er sekundet før det første
         * tik, og ellers kun hvis noget gik galt. Knappen bliver
         * stående som nødudgang, og teksten lover ikke at den er
         * nødvendig.
         */
        lv_label_set_text(s_over, "Koden laves nu");
        lv_label_set_text(s_kode, "-");
        lv_label_set_text(s_hjaelp,
            "Sker der ikke noget om et øjeblik, så tryk her.");
        break;

    }

    /*
     * Hvad der er med i stakken.
     *
     * Ingen kode, ingen QR: en QR der blev staaende efter koden doede,
     * ville blive fotograferet og proevet forgaeves. Og vejledningen og
     * levetiden hoerer til koden - staar de under en besked om at
     * nettet mangler, ser de ud som en del af fejlen.
     */
    /*
     * QR'en vises KUN naar der er en kode. I tilmeldt-tilstanden staar
     * der et navn i den store linje, og en QR ved siden af et navn
     * ville blive scannet - og den kode er brugt.
     */
    /*
     * NAAR VI VENTER, ER DET TRINNET DER ER SIDEN.
     *
     * Alt sidens eget indhold skjules, og trinnet vises. Det er derfor
     * de to ventesider nu er 1:1: det er ikke to layouts der er lavet
     * ens, det er det samme layout.
     */
    zs_step_vis(&s_trin, venter);
    vis(s_over, !venter);
    vis(s_hjaelp, !venter);

    vis(s_plade, klar && !venter);
    vis(s_guide, klar && !venter);
    (void)tilmeldt;
    vis(s_knap, stand == ZS_TILMELD_UDLOEBET);

    /*
     * BJAELKEN VISER RIGTIG FREMDRIFT, IKKE EN ANIMATION.
     *
     * Trinnene findes allerede som en enum i flaaden, og de er dem
     * montoeren venter paa. En bjaelke der bare loeber, ville sige
     * "der sker noget" ogsaa naar der ikke gjorde - og saa staar han og
     * ser paa den i et minut uden at vide at certifikatet blev afvist.
     *
     *   paa vej op        en tredjedel
     *   sendt certifikat  to tredjedele
     *   indmeldt          fuld, og saa er vi alligevel ude af den her
     *                     tilstand ved naeste tik
     *
     * Afvist og fejl faar ingen bjaelke: dér er ventetilstanden slet
     * ikke den rigtige side, se zs_tilmeld_vejen.
     */
    if (venter) {
        int pct;
        switch (zs_fleet_state()) {
        case ZS_FLEET_CONNECTING: pct = 33;  break;
        case ZS_FLEET_ENROLLING:  pct = 66;  break;
        case ZS_FLEET_READY:      pct = 100; break;
        default:                  pct = 10;  break;
        }
        /* UDEN ANIMATION, som paa inverter-soegningen.
         *
         * Her stod LV_ANIM_ON. Det er en forskel man ser: den ene bjaelke
         * gled, den anden sprang. Og 1:1 betyder ogsaa det. Trinnene er i
         * forvejen rigtige tal og ikke pynt, saa de behoever ikke en
         * glidning for at vise at der sker noget. */
        zs_step_fremdrift(&s_trin, pct);
    }

    /* Kodefeltet skjules naar der ikke staar noget i det. En tom linje i
     * 28 px skubber alt nedenfor ned uden at vise noget. */
    vis(s_kode, !venter);
    if (!klar) {
        /* Tankestregen staar i daempet farve naar der ikke er en kode,
         * saa den ikke ser ud som noget man kan bruge. */
        zs_style_text(s_kode, &zs_font_28, ZS_C_TEXT_DIM);
    } else {
        zs_style_text(s_kode, &zs_font_28, ZS_C_VALUE);
    }

    (void)stak();
}
