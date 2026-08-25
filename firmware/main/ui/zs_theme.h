/*
 * zScreen - designsystem.
 *
 * Alle farver, skrifttyper og maal staar her. Ingen anden fil maa
 * skrive en farvekode eller en pixelvaerdi direkte. Det er ikke
 * pedanteri: naar der om et halvt aar skal justeres en nuance, skal
 * det kunne goeres ét sted og slaa igennem alle steder, i stedet for
 * at man skal lede efter fire forskellige steder hvor der tilfaeldigvis
 * staar 0x16403E.
 *
 * Grundlaget er Frekvenz' brandfarver fra brand-mappen:
 *     #174A48  moerkegroen, primaerfarven
 *     #FBAC18  orange, accentfarven
 *     #FFFFFF  hvid
 *
 * Der er to paletter. Moerk er standard: skaermen haenger paa en vaeg i
 * en stue og skal kunne ses om dagen uden at lyse rummet op om aftenen.
 * Lys kan vaelges under Indstillinger. Begge er maalt mod WCAG, og
 * tests/host tjekker kontrasten hver gang der bygges, saa en nuance
 * ikke kan skubbes til noget ulaeseligt uden at det opdages.
 *
 * Reglerne fra Zbox' DESIGN.txt gaelder ogsaa her:
 *   ingen forloeb, ingen matteret glas, ingen indgangsanimationer,
 *   ingen unoedige emoji, og dansk uden fagsprog i alt brugeren laeser.
 */

#ifndef ZS_THEME_H
#define ZS_THEME_H

#include "lvgl.h"

/* Ikonerne hoerer med til designsystemet, saa den der bruger temaet
 * faar dem uden at skulle huske en include mere. */
#include "zs_icons.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Farver ───────────────────────────────────────────────────────── */

/* Brandets egne, som de staar i logofilen. De to skifter aldrig. */
#define ZS_BRAND_GREEN      0x174A48
#define ZS_BRAND_ORANGE     0xFBAC18

/*
 * Farverne slaas op i den palet der er valgt lige nu.
 *
 * Navnene er de samme som da de var faste tal, saa hvert eneste sted i
 * koden der bruger en farve laeser nu den aktive palet. Der findes ikke
 * et sted der blev glemt ved skiftet, for der findes ikke et navn der
 * gaar uden om opslaget.
 *
 * Prisen er at de ikke kan staa i en initialisering paa filniveau.
 * tools/check-colors.sh haandhaever baade det, og at ingen skriver en
 * raa farvekode ind i brugerfladen udenom paletten.
 */
typedef enum {
    ZS_ID_BG = 0,
    ZS_ID_CARD,
    ZS_ID_CARD_PRESSED,
    ZS_ID_BORDER,
    ZS_ID_TEXT,
    ZS_ID_TEXT_DIM,
    ZS_ID_LABEL,
    ZS_ID_STALE,
    ZS_ID_VALUE,
    ZS_ID_ACCENT,
    ZS_ID_GOOD,
    ZS_ID_BAD,
    ZS_ID_WARN,
    ZS_ID_COUNT
} zs_col_id_t;

/* Slaar en farve op i den aktive palet. */
uint32_t zs_col(zs_col_id_t id);

/* Fladerne. I moerkt tema er baggrunden brandgroen trukket ned i
 * lysstyrke, saa skaermen ser ud som Frekvenz og ikke som en tilfaeldig
 * moerk app. Kortene ligger et lille trin over, saa de skiller sig ud
 * uden skygger. I lyst tema er det vendt om. */
#define ZS_C_BG             zs_col(ZS_ID_BG)
#define ZS_C_CARD           zs_col(ZS_ID_CARD)
#define ZS_C_CARD_PRESSED   zs_col(ZS_ID_CARD_PRESSED)
#define ZS_C_BORDER         zs_col(ZS_ID_BORDER)

/* Teksten. Tre niveauer er nok: overskrift, brOEdtekst og etiket. */
#define ZS_C_TEXT           zs_col(ZS_ID_TEXT)
#define ZS_C_TEXT_DIM       zs_col(ZS_ID_TEXT_DIM)
#define ZS_C_LABEL          zs_col(ZS_ID_LABEL)

/* Naar en maaling er gammel eller mangler. Tallet bliver staaende, men
 * daempet, saa man kan se hvad det sidst var uden at tro det er nu. */
#define ZS_C_STALE          zs_col(ZS_ID_STALE)

/*
 * De store tal, og accenten, er to forskellige farver.
 *
 * I moerkt tema er begge Frekvenz-orange, som de altid har vaeret. I
 * lyst tema kan de ikke vaere det: orangen har kun 1,8:1 mod hvid, og
 * tekst skal have 4,5:1 for at kunne laeses. Derfor bliver tallene
 * moerk brandgroen, og orangen bliver toneret ned til 4,5:1 og brugt
 * hvor den fylder nok til at ses: prikker, streger og aktive knapper.
 */
#define ZS_C_VALUE          zs_col(ZS_ID_VALUE)
#define ZS_C_ACCENT         zs_col(ZS_ID_ACCENT)

/* Retning og tilstand. Groen naar der kommer noget ind i huset uden at
 * koste noget, roed naar der koebes. */
#define ZS_C_GOOD           zs_col(ZS_ID_GOOD)
#define ZS_C_BAD            zs_col(ZS_ID_BAD)
#define ZS_C_WARN           zs_col(ZS_ID_WARN)

/* ── Tema ─────────────────────────────────────────────────────────── */
typedef enum {
    ZS_THEME_DARK  = 0,     /* standard, og det skaermen starter i */
    ZS_THEME_LIGHT = 1,
    ZS_THEME_COUNT
} zs_theme_mode_t;

/*
 * Skifter palet.
 *
 * De delte stilarter faar de nye farver med det samme. De farver der er
 * sat direkte paa et enkelt objekt sidder fast, saa siderne skal bygges
 * om bagefter. Det goer zs_ui_set_theme(), som er den man skal kalde.
 * Denne her er kun til laget under.
 */
void zs_theme_set_mode(zs_theme_mode_t m);

zs_theme_mode_t zs_theme_mode(void);

/* Navnet til brugerfladen: "Mørkt" eller "Lyst". */
const char *zs_theme_name(zs_theme_mode_t m);

/* ── Skrifttyper ──────────────────────────────────────────────────── */
/* Funnel Sans, den samme som frekvenz.nu og Zbox-webfladen. */
LV_FONT_DECLARE(zs_font_num_64)   /* store tal, kun cifre og komma  */
LV_FONT_DECLARE(zs_font_28)       /* enheder og overskrifter        */
LV_FONT_DECLARE(zs_font_20)       /* listerader og knapper          */
LV_FONT_DECLARE(zs_font_16)       /* undertekst og brOEdtekst       */
LV_FONT_DECLARE(zs_font_13)       /* etiketter med versaler         */

/* ── Logoer ───────────────────────────────────────────────────────── */
/*
 * Logoerne findes i to udgaver, og de har praecis samme maal, saa der
 * ikke flytter sig noget paa skaermen naar temaet skifter.
 *
 *   neg  hvidt logo, til moerkt tema
 *   pos  moerkegroent logo, til lyst tema
 *
 * Brug zs_logo_zmark() og zs_logo_wordmark() i stedet for at pege paa
 * en af dem direkte. Saa er der ét sted der ved hvilken der hoerer til
 * hvilket tema.
 */
LV_IMG_DECLARE(zs_img_zmark)          /* Z-maerket, 26 px, negativ  */
LV_IMG_DECLARE(zs_img_zmark_pos)      /* Z-maerket, 26 px, positiv  */
LV_IMG_DECLARE(zs_img_wordmark)       /* hele logoet, 260 px, neg.  */
LV_IMG_DECLARE(zs_img_wordmark_pos)   /* hele logoet, 260 px, pos.  */

/* Det Z-maerke der passer til det tema der er valgt nu. */
const lv_img_dsc_t *zs_logo_zmark(void);

/* Hele logoet med payoff, i den udgave der passer til temaet. */
const lv_img_dsc_t *zs_logo_wordmark(void);

/* ── Maal ─────────────────────────────────────────────────────────── */
/*
 * Hele layoutet er regnet ud, ikke skudt efter. Skaermen er 480 x 480.
 *
 *   STATUSLINJE      0 ..  44     44 px, staar fast
 *   SIDER           44 .. 452    408 px, kan trykkes til side
 *   PRIKKER        452 .. 480     28 px, staar fast
 *
 * Vandret paa side 1:
 *     kant 12 + kort 222 + mellemrum 12 + kort 222 + kant 12 = 480
 * Lodret paa side 1:
 *     kant 12 + kort 186 + mellemrum 12 + kort 186 + kant 12 = 408
 *
 * Alle vaerdier er lige tal, saa intet kan ende paa en halv pixel naar
 * noget bliver centreret.
 */
#define ZS_SCR_WIDTH            480
#define ZS_SCR_HEIGHT            480

#define ZS_BAR_HEIGHT       44    /* statuslinjen foroven             */
#define ZS_DOTS_HEIGHT      28    /* prikkerne der viser hvilken side */
/* Hoejden af én side. Statuslinjen og prikkerne staar fast, kun det
 * imellem kan trykkes til side. */
#define ZS_PAGE_HEIGHT      (ZS_SCR_HEIGHT - ZS_BAR_HEIGHT - ZS_DOTS_HEIGHT)
#define ZS_EDGE             12    /* luft ud til skaermkanten         */
/*
 * Bredden af alt der fylder en side ud: 480 - 12 - 12 = 456.
 *
 * ALT paa en side placeres i skaermens egne koordinater, ogsaa inde i
 * en sides indholdsomraade. Det omraade har derfor INGEN luft i
 * siderne: havde det det, ville x = 0 betyde 12 ét sted og 0 et andet,
 * og saa flugter tingene ikke. Det skete: knapperne til prisomraade
 * laa 12 px laengere til hoejre end overskriften over dem, og stak
 * samtidig 12 px ud over kanten.
 */
#define ZS_CONTENT_WIDTH    (ZS_SCR_WIDTH - 2 * ZS_EDGE)
#define ZS_GRID_GAP         12    /* mellemrum mellem to kort         */

#define ZS_CARD_WIDTH           222   /* (480 - 12 - 12 - 12) / 2         */
#define ZS_CARD_HEIGHT      186   /* (408 - 12 - 12 - 12) / 2         */
#define ZS_CARD_RADIUS      18
#define ZS_CARD_PAD         14    /* luft inde i kortet               */

/* Kortets indvendige maal, altsaa det indholdet har at goere godt med.
 *     222 - 2*14 = 194        200 - 2*14 = 172                       */
#define ZS_CARD_IN_WIDTH        (ZS_CARD_WIDTH - 2 * ZS_CARD_PAD)
#define ZS_CARD_IN_HEIGHT        (ZS_CARD_HEIGHT - 2 * ZS_CARD_PAD)

/*
 * Kortets tre baand, maalt fra kortets indvendige overkant.
 * Indvendigt er kortet 194 x 158 (222 og 186 minus 14 luft i hver side).
 *
 *     overskrift    y =   0, hoejde 20   (ikon 20 px og etiket 13 px)
 *     stort tal     y =  53, hoejde 54   (skrifttypens linjehoejde)
 *     undertekst    y = 140, hoejde 18   slutter praecis paa 158
 *
 * Tallet staar optisk midt imellem de to andre:
 *     ledig plads mellem 20 og 140 er 120 px
 *     120 - 54 = 66, halvdelen er 33, saa y = 20 + 33 = 53
 */
#define ZS_CARD_HEAD_Y      0
#define ZS_CARD_HEAD_HEIGHT 20
#define ZS_CARD_VALUE_Y     53
#define ZS_CARD_VALUE_HEIGHT 54
#define ZS_CARD_SUB_Y       140
#define ZS_CARD_SUB_HEIGHT  18

/*
 * Enheden skal staa paa SAMME GRUNDLINJE som tallet.
 *
 * De to har hver sin skriftstoerrelse, saa man kan ikke bare saette dem
 * ved siden af hinanden og stille dem op efter underkanten: en 64 px
 * skrifttype har langt mere luft under bogstaverne end en 28 px.
 * Goer man det alligevel, kommer "kW" til at hoppe et par pixels op og
 * ned alt efter om tallet har et komma i sig.
 *
 * LVGL fortaeller hvor grundlinjen ligger:
 *     grundlinje fra overkant = line_height - base_line
 *
 *     zs_font_num_64   54 - 9 = 45
 *     zs_font_28       31 - 5 = 26
 *
 * Enheden skal derfor saenkes 45 - 26 = 19 px i forhold til tallet.
 * Vi regner det ud paa stedet ud fra skrifttyperne i stedet for at
 * skrive 19 ind, saa det stadig passer hvis en stoerrelse aendres.
 */
#define ZS_UNIT_GAP         8     /* mellem tallet og enheden         */

/* ── Andet ────────────────────────────────────────────────────────── */
#define ZS_ROW_HEIGHT       56    /* hoejde paa en listerad           */
/* Ikonet plus luften efter det. Bruges baade naar raden bygges og naar
 * en kalder skal saette noget ind paa samme lodrette linje som titlen. */
#define ZS_ROW_ICON_W       34
#define ZS_BTN_HEIGHT            52    /* hoejde paa en knap               */
#define ZS_PAD_SCREEN       ZS_EDGE

/*
 * Mindste flade man kan ramme med en finger.
 *
 * 44 pixels er den graense baade Apple og Google har staaet paa i
 * mange aar. Paa en 4 tommer skaerm med 480 pixels svarer det til
 * omkring 8 millimeter. Alt hvad brugeren skal kunne trykke paa, skal
 * vaere mindst saa stort, ogsaa naar selve ikonet er mindre. Det er
 * derfor tandhjulet foroven er en 44 x 44 knap med et 20 px ikon i
 * midten og ikke bare et ikon.
 */
#define ZS_TOUCH_MIN        44

/* ── Faelles opsaetning ───────────────────────────────────────────── */

/* Saetter LVGL's tema op og bygger de genbrugte stilarter.
 * Skal kaldes én gang, efter lv_port_init() og med LVGL-laasen taget. */
void zs_theme_init(void);

/*
 * Et kort: afrundet, med kant, uden skygge og uden forloeb.
 *
 * pressable bestemmer om kortet lyser op naar man roerer det. Kort man
 * IKKE kan trykke paa skal have false: ellers lyser de fire kasser paa
 * forsiden op naar man traekker siden til side, og det ligner at man
 * har trykket paa noget der sker noget ved.
 */
lv_obj_t *zs_card_create(lv_obj_t *parent, bool pressable);

/* En etiket med versaler i 13 px og lidt luft mellem bogstaverne.
 * Bruges til overskriften i et kort og over et indtastningsfelt. */
lv_obj_t *zs_label_create(lv_obj_t *parent, const char *text);

/* En primaer knap: orange flade, moerk tekst. Til det brugeren skal
 * goere. Der maa hoejst vaere én paa en skaerm ad gangen. */
lv_obj_t *zs_btn_primary_create(lv_obj_t *parent, const char *text);

/* En sekundaer knap: gennemsigtig med kant. Til alt det andet. */
lv_obj_t *zs_btn_secondary_create(lv_obj_t *parent, const char *text);

/*
 * En rad i en liste, fx et wifi-netvaerk eller en indstilling.
 *
 * Delene afleveres i en struct i stedet for at kalderen skal finde dem
 * med lv_obj_get_child(row, 1). Det er ikke pynt: raekkefoelgen af
 * boern afhaenger af om der er et ikon og en vaerdi, saa et fast
 * indeks peger paa noget forskelligt fra rad til rad. Tilfoejer man en
 * dag noget nyt til raden, ville alle de indekser stille og roligt
 * begynde at pege forkert, uden en eneste advarsel.
 */
typedef struct {
    lv_obj_t *row;
    lv_obj_t *icon;      /* NULL naar der ikke er et ikon    */
    lv_obj_t *title;
    lv_obj_t *value;     /* NULL naar der ikke er en vaerdi  */
    lv_obj_t *chevron;   /* NULL naar der ikke er en pil     */
} zs_row_t;

/* icon og value maa vaere NULL. out maa ikke. */
void zs_row_create(zs_row_t *out, lv_obj_t *parent, const char *icon,
                   const char *title, const char *value, bool chevron);

/*
 * En beholder der stabler sine boern lodret med lige meget luft
 * imellem. Alle lister bruger den, saa afstanden er den samme paa
 * tvaers af skaermene i stedet for at hver side vaelger sin egen.
 */
lv_obj_t *zs_column_create(lv_obj_t *parent, lv_coord_t gap);

/* Hjaelper: saet skrifttype og farve paa ét kald. */
void zs_style_text(lv_obj_t *obj, const lv_font_t *font, uint32_t color);


/* ── Sider ────────────────────────────────────────────────────────── */
/*
 * Alle sider ud over hovedskaermen er bygget over den samme ramme, saa
 * tilbageknappen og overskriften staar det samme sted hver gang. En
 * bruger skal ikke lede efter vejen tilbage.
 *
 *     ┌────────────────────────────────────────┐
 *     │ ←   Vælg netværk                       │  56 px
 *     ├────────────────────────────────────────┤
 *     │                                        │
 *     │   indhold, kan rulles                  │  424 px, eller 348
 *     │                                        │  naar der er en fod
 *     ├────────────────────────────────────────┤
 *     │        [ knap ]                        │  76 px, valgfri
 *     └────────────────────────────────────────┘
 */
#define ZS_PAGE_HEAD_HEIGHT   56
#define ZS_PAGE_FOOT_HEIGHT   76

typedef struct {
    lv_obj_t *root;
    lv_obj_t *head;
    lv_obj_t *title;
    lv_obj_t *back;      /* NULL naar der ikke er en vej tilbage */
    lv_obj_t *content;   /* her laegges sidens indhold           */
    lv_obj_t *footer;    /* NULL naar with_footer er false       */
} zs_page_t;

/*
 * Bygger rammen. back_cb maa vaere NULL, og saa tegnes tilbageknappen
 * ikke: det er meningen paa den foerste side i en opsaetning, hvor der
 * ikke er noget at gaa tilbage til.
 */
void zs_page_create(zs_page_t *p, const char *title,
                    lv_event_cb_t back_cb, void *user_data,
                    bool with_footer);

/* Skjuler eller viser hele siden. */
void zs_page_set_hidden(zs_page_t *p, bool hidden);

#ifdef __cplusplus
}
#endif

#endif /* ZS_THEME_H */
