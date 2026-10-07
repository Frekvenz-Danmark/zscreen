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

#define ZS_G_SCR_WIDTH      480
#define ZS_G_SCR_HEIGHT     480
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

#endif /* ZS_LAYOUT_H */
