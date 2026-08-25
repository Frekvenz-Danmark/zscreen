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
 * Skaermen bruger en moerk udgave. Den haenger paa en vaeg i en stue og
 * skal kunne ses om dagen uden at lyse rummet op om aftenen.
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

/* Brandets egne, som de staar i logofilen. */
#define ZS_BRAND_GREEN      0x174A48
#define ZS_BRAND_ORANGE     0xFBAC18

/* Fladerne. Baggrunden er brandgroen trukket ned i lysstyrke, saa
 * skaermen stadig ser ud som Frekvenz og ikke som en tilfaeldig moerk
 * app. Kortene ligger et lille trin over baggrunden, saa de skiller
 * sig ud uden skygger. */
#define ZS_C_BG             0x0E2A29
#define ZS_C_CARD           0x16403E
#define ZS_C_CARD_PRESSED   0x1D504D
#define ZS_C_BORDER         0x205A57

/* Teksten. Tre niveauer er nok: overskrift, brOEdtekst og etiket. */
#define ZS_C_TEXT           0xFFFFFF
#define ZS_C_TEXT_DIM       0xB6D0CE
#define ZS_C_LABEL          0x8FB3B1

/* Naar en maaling er gammel eller mangler. Tallet bliver staaende, men
 * daempet, saa man kan se hvad det sidst var uden at tro det er nu. */
#define ZS_C_STALE          0x5C8280

#define ZS_C_ACCENT         ZS_BRAND_ORANGE

/* Retning og tilstand. Groen naar der kommer noget ind i huset uden at
 * koste noget, roed naar der koebes. */
#define ZS_C_GOOD           0x4ADE80
#define ZS_C_BAD            0xF87171
#define ZS_C_WARN           0xFBBF24

/* ── Skrifttyper ──────────────────────────────────────────────────── */
/* Funnel Sans, den samme som frekvenz.nu og Zbox-webfladen. */
LV_FONT_DECLARE(zs_font_num_64)   /* store tal, kun cifre og komma  */
LV_FONT_DECLARE(zs_font_28)       /* enheder og overskrifter        */
LV_FONT_DECLARE(zs_font_20)       /* listerader og knapper          */
LV_FONT_DECLARE(zs_font_16)       /* undertekst og brOEdtekst       */
LV_FONT_DECLARE(zs_font_13)       /* etiketter med versaler         */

/* ── Logoer ───────────────────────────────────────────────────────── */
LV_IMG_DECLARE(zs_img_zmark)      /* Z-maerket, 26 px, til statuslinjen */
LV_IMG_DECLARE(zs_img_wordmark)   /* hele logoet, 260 px, til velkomst  */

/* ── Maal ─────────────────────────────────────────────────────────── */
/*
 * Hele layoutet er regnet ud, ikke skudt efter. Skaermen er 480 x 480,
 * og hvert tal nedenfor kan foelges tilbage til den regning.
 *
 *   STATUSLINJE      0 .. 44          44 px
 *   KORTOMRAADE     44 .. 480        436 px
 *
 * Vandret:
 *     kant 12 + kort 222 + mellemrum 12 + kort 222 + kant 12 = 480
 * Lodret:
 *     44 + kant 12 + kort 200 + mellemrum 12 + kort 200 + kant 12 = 480
 *
 * Alle vaerdier er lige tal, saa intet kan ende paa en halv pixel naar
 * noget bliver centreret.
 */
#define ZS_SCR_WIDTH            480
#define ZS_SCR_HEIGHT            480

#define ZS_BAR_HEIGHT            44    /* statuslinjen foroven             */
#define ZS_EDGE             12    /* luft ud til skaermkanten         */
#define ZS_GRID_GAP         12    /* mellemrum mellem to kort         */

#define ZS_CARD_WIDTH           222   /* (480 - 12 - 12 - 12) / 2         */
#define ZS_CARD_HEIGHT           200   /* (480 - 44 - 12 - 12 - 12) / 2    */
#define ZS_CARD_RADIUS      18
#define ZS_CARD_PAD         14    /* luft inde i kortet               */

/* Kortets indvendige maal, altsaa det indholdet har at goere godt med.
 *     222 - 2*14 = 194        200 - 2*14 = 172                       */
#define ZS_CARD_IN_WIDTH        (ZS_CARD_WIDTH - 2 * ZS_CARD_PAD)
#define ZS_CARD_IN_HEIGHT        (ZS_CARD_HEIGHT - 2 * ZS_CARD_PAD)

/*
 * Kortets tre baand, maalt fra kortets indvendige overkant.
 *
 *     overskrift    y =   0, hoejde 20   (ikon 20 px og etiket 13 px)
 *     stort tal     y =  60, hoejde 54   (skrifttypens linjehoejde)
 *     undertekst    y = 154, hoejde 18
 *
 * Tallet staar optisk midt imellem de to andre:
 *     ledig plads mellem 20 og 154 er 134 px
 *     134 - 54 = 80, halvdelen er 40, saa y = 20 + 40 = 60
 */
#define ZS_CARD_HEAD_Y      0
#define ZS_CARD_HEAD_HEIGHT      20
#define ZS_CARD_VALUE_Y     60
#define ZS_CARD_VALUE_HEIGHT     54
#define ZS_CARD_SUB_Y       154
#define ZS_CARD_SUB_HEIGHT       18

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

/* Et kort: afrundet, med kant, uden skygge og uden forloeb. */
lv_obj_t *zs_card_create(lv_obj_t *parent);

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

#ifdef __cplusplus
}
#endif

#endif /* ZS_THEME_H */

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
