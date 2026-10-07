/*
 * zScreen - de rene tal bag layoutet.
 *
 * Samme vaerdier som i zs_theme.h, men uden LVGL, saa udregninger der
 * skal kunne testes paa en almindelig maskine kan naa dem. Der er en
 * test der tjekker at de to filer er enige, saa de ikke kan komme ud af
 * trit.
 */

#ifndef ZS_LAYOUT_H
#define ZS_LAYOUT_H

/*
 * SKAERMENS MAAL. Kan saettes udefra.
 *
 * HVORFOR #ifndef og ikke bare et tal.
 *
 * To grunde, og de er lige vigtige:
 *
 * 1. ET BOARD SKAL KUNNE SAETTE DEM. Vi har en 480 x 480 i dag. Kommer
 *    der en anden, fx reTerminal D1001 paa 800 x 1280, skal den kunne
 *    give sine egne maal uden at nogen retter i den her fil.
 *
 * 2. SAA KAN LAYOUTET PROEVES VED ANDRE STOERRELSER. Uden det her kan en
 *    test kun regne efter paa 480, og saa beviser den kun at regnestykket
 *    passer der. Hele paastanden er at layoutet gaar op paa ENHVER
 *    skaerm, og den paastand kan nu faktisk proeves. Se test_layout_800.c.
 */
#ifndef ZS_G_SCR_WIDTH
#define ZS_G_SCR_WIDTH      480
#endif
#ifndef ZS_G_SCR_HEIGHT
#define ZS_G_SCR_HEIGHT     480
#endif
#define ZS_G_BAR_HEIGHT     44
#define ZS_G_DOTS_HEIGHT    28
#define ZS_G_PAGE_HEIGHT    (ZS_G_SCR_HEIGHT - ZS_G_BAR_HEIGHT - ZS_G_DOTS_HEIGHT)
#define ZS_G_EDGE           12
#define ZS_G_GRID_GAP       12
#define ZS_G_CONTENT_WIDTH  (ZS_G_SCR_WIDTH - 2 * ZS_G_EDGE)
/*
 * Kortets hoejde, UDREGNET og ikke skrevet af.
 *
 * Stod som 186 med regnestykket i en kommentar ved siden af. Det er den
 * slags der goer ondt den dag skaermen skifter stoerrelse: alt det andet
 * her retter sig selv, og saa staar det her tal tilbage og giver et
 * layout der er lidt forkert i stedet for tydeligt forkert.
 *
 * To kort over hinanden med luft over, imellem og under:
 *   (408 - 3 * 12) / 2 = 186, altsaa praecis det der stod foer.
 */
#define ZS_G_CARD_HEIGHT    ((ZS_G_PAGE_HEIGHT - 3 * ZS_G_GRID_GAP) / 2)

/*
 * Og kortets bredde samme sted, saa de to foelges ad. Stod foer som 222
 * nede i zs_theme.h, hvor alt andet er alias herfra.
 *
 *   (456 - 12) / 2 = 222
 */
#define ZS_G_CARD_WIDTH     ((ZS_G_CONTENT_WIDTH - ZS_G_GRID_GAP) / 2)

/* ------------------------------------------------------------------ */
/* Inde i kortet                                                       */
/* ------------------------------------------------------------------ */

/*
 * De her staar HER og ikke i zs_theme.h, selv om de er layout.
 *
 * zs_theme.h henter lvgl.h, og saa kan en test paa en almindelig maskine
 * ikke naa dem. Og det er netop de her tal der skal kunne proeves af: de
 * afgoer om teksten i et kort fylder det ud eller loeber over kanten, og
 * det er det foerste der gaar galt den dag skaermen skifter stoerrelse.
 */
#define ZS_G_CARD_PAD       14    /* luft inde i kortet               */
#define ZS_G_CARD_IN_WIDTH  (ZS_G_CARD_WIDTH - 2 * ZS_G_CARD_PAD)
#define ZS_G_CARD_IN_HEIGHT (ZS_G_CARD_HEIGHT - 2 * ZS_G_CARD_PAD)

/*
 * HOEJDERNE staar fast. De er skriftstoerrelser, og en skrift bliver ikke
 * stoerre af at skaermen goer. Et ikon paa 20 px og et tal paa 54 px er
 * valgt efter hvad man kan laese paa to meters afstand, ikke efter hvor
 * mange pixels der tilfaeldigvis er.
 */
#define ZS_G_CARD_HEAD_HEIGHT  20
#define ZS_G_CARD_VALUE_HEIGHT 54
#define ZS_G_CARD_SUB_HEIGHT   18

/*
 * PLACERINGERNE er derimod udregnet, for de afhaenger af kortets hoejde.
 * Stod de som faste tal, ville teksten loebe ud over kanten eller
 * efterlade et hul den dag kortet skifter stoerrelse.
 *
 * Reglen: tre blokke med to ENS mellemrum, der fylder kortet helt ud.
 *     mellemrum = (158 - 20 - 54 - 18) / 2 = 33
 *     tallet paa 20 + 33 = 53, underteksten paa 53 + 54 + 33 = 140
 * Praecis de tal der stod her foer.
 */
#define ZS_G_CARD_TEXT_GAP  ((ZS_G_CARD_IN_HEIGHT - ZS_G_CARD_HEAD_HEIGHT \
                              - ZS_G_CARD_VALUE_HEIGHT \
                              - ZS_G_CARD_SUB_HEIGHT) / 2)
#define ZS_G_CARD_HEAD_Y    0
#define ZS_G_CARD_VALUE_Y   (ZS_G_CARD_HEAD_Y + ZS_G_CARD_HEAD_HEIGHT \
                             + ZS_G_CARD_TEXT_GAP)
#define ZS_G_CARD_SUB_Y     (ZS_G_CARD_VALUE_Y + ZS_G_CARD_VALUE_HEIGHT \
                             + ZS_G_CARD_TEXT_GAP)

/* Fysiske maal: en finger bliver ikke stoerre af at skaermen goer. De
 * skal BLIVE staaende naar vi skifter skaerm. */
#define ZS_G_TOUCH_MIN      44
#define ZS_G_ROW_HEIGHT     56
#define ZS_G_BTN_HEIGHT     52
#define ZS_G_CHOICE_HEIGHT  92

/* ------------------------------------------------------------------ */
/* Reglerne, tjekket naar der BYGGES                                   */
/* ------------------------------------------------------------------ */

/*
 * HVORFOR HER OG IKKE KUN I EN TEST.
 *
 * Testene regner efter paa 480 x 480 og paa 800 x 1280. Det er to
 * stoerrelser. Kommer der en tredje, fx en skaerm med en ULIGE bredde,
 * ville heltalsdivisionen tabe en pixel, og ingen test ville fejle, for
 * ingen test kender den stoerrelse.
 *
 * Her tjekkes det derimod for den stoerrelse der FAKTISK bygges med,
 * hver eneste gang. Rammer nogen en skaerm hvor layoutet ikke gaar op,
 * stopper byggeriet med en forklaring i stedet for at give en skaev kant
 * paa en vaeg hos en kunde.
 *
 * Beskederne er ren ASCII med vilje: oversaetteren skriver aeoeaa ud som
 * raa bytes, og saa kan de ikke laeses.
 */

/* Et kort kan ikke vaere mindre end et trykfelt. */
_Static_assert(ZS_G_CARD_WIDTH >= ZS_G_TOUCH_MIN &&
               ZS_G_CARD_HEIGHT >= ZS_G_TOUCH_MIN,
               "skaermen er for lille: et kort bliver mindre end et trykfelt");

/* Vandret: kant + kort + mellemrum + kort + kant skal vaere hele bredden. */
_Static_assert(ZS_G_EDGE + ZS_G_CARD_WIDTH + ZS_G_GRID_GAP
               + ZS_G_CARD_WIDTH + ZS_G_EDGE == ZS_G_SCR_WIDTH,
               "layoutet gaar ikke op i bredden: skaermbredden minus kanter "
               "og mellemrum skal kunne deles i to hele kort");

/* Lodret: luft + kort + luft + kort + luft skal vaere sidens hoejde. */
_Static_assert(ZS_G_GRID_GAP + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP
               + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP == ZS_G_PAGE_HEIGHT,
               "layoutet gaar ikke op i hoejden: sidehoejden minus tre "
               "mellemrum skal kunne deles i to hele kort");

/* Linjen foroven, siden og prikkerne skal dele skaermen uden at overlappe. */
_Static_assert(ZS_G_BAR_HEIGHT + ZS_G_PAGE_HEIGHT + ZS_G_DOTS_HEIGHT
               == ZS_G_SCR_HEIGHT,
               "statuslinjen, siden og prikkerne fylder ikke skaermen");

/* Teksten i kortet skal fylde det helt ud, hverken mere eller mindre. */
_Static_assert(ZS_G_CARD_HEAD_HEIGHT + ZS_G_CARD_TEXT_GAP
               + ZS_G_CARD_VALUE_HEIGHT + ZS_G_CARD_TEXT_GAP
               + ZS_G_CARD_SUB_HEIGHT == ZS_G_CARD_IN_HEIGHT,
               "teksten i kortet gaar ikke op: de tre linjer og de to "
               "mellemrum skal fylde kortets indvendige hoejde praecis");

/* Og at der er plads til teksten overhovedet. */
_Static_assert(ZS_G_CARD_TEXT_GAP >= 0,
               "kortet er for lavt til de tre tekstlinjer");

#endif /* ZS_LAYOUT_H */
