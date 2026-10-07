/*
 * Layoutet, og at det GAAR OP.
 *
 * HVORFOR DEN FINDES.
 *
 * Skaermen er 480 x 480 i dag. Kommer der en anden, fx reTerminal D1001
 * paa 800 x 1280, skal layoutet foelge med af sig selv. Det goer det kun
 * hvis alt er regnet ud af skaermens maal og ikke skrevet af.
 *
 * To tal var skrevet af: kortets bredde og hoejde stod som 222 og 186
 * med regnestykket i en kommentar. De er nu udregnede, og de her proever
 * holder dem fast.
 *
 * Den vigtigste proeve er ikke tallene, men at siden GAAR OP: kant plus
 * kort plus mellemrum plus kort plus kant skal give praecis skaermens
 * maal. Holder den regel, er layoutet rigtigt paa enhver skaerm. Holder
 * den ikke, er der en pixel der mangler eller en for meget, og det ses
 * som en skaev kant.
 */

#include "zs_test.h"
#include "../../firmware/main/ui/zs_layout.h"

void test_layout(void)
{
    ZS_SUITE("Layout: tallene for skærmen vi har i dag");

    CHECK("skaermen er 480 bred", ZS_G_SCR_WIDTH == 480);
    CHECK("og 480 hoej", ZS_G_SCR_HEIGHT == 480);
    CHECK("siden er 408 hoej", ZS_G_PAGE_HEIGHT == 408);
    CHECK("indholdet er 456 bredt", ZS_G_CONTENT_WIDTH == 456);
    CHECK("kortet er 222 bredt, som foer det blev udregnet",
          ZS_G_CARD_WIDTH == 222);
    CHECK("kortet er 186 hoejt, som foer det blev udregnet",
          ZS_G_CARD_HEIGHT == 186);

    ZS_SUITE("Layout: siden går op, uanset skærmstørrelse");

    /*
     * Den vandrette regel: kant + kort + mellemrum + kort + kant skal
     * vaere hele bredden. Ikke "omtrent", praecis.
     */
    CHECK("to kort og luften omkring fylder praecis bredden",
          ZS_G_EDGE + ZS_G_CARD_WIDTH + ZS_G_GRID_GAP
          + ZS_G_CARD_WIDTH + ZS_G_EDGE == ZS_G_SCR_WIDTH);

    /*
     * Den lodrette: luft + kort + luft + kort + luft skal vaere hele
     * sidens hoejde.
     */
    CHECK("to kort og luften over, imellem og under fylder sidens hoejde",
          ZS_G_GRID_GAP + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP
          + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP == ZS_G_PAGE_HEIGHT);

    /* Og at siden, linjen foroven og prikkerne forneden deler skaermen
     * mellem sig uden at overlappe eller efterlade en stribe. */
    CHECK("linjen, siden og prikkerne fylder skaermen",
          ZS_G_BAR_HEIGHT + ZS_G_PAGE_HEIGHT + ZS_G_DOTS_HEIGHT
          == ZS_G_SCR_HEIGHT);

    CHECK("indholdet plus de to kanter er hele bredden",
          ZS_G_CONTENT_WIDTH + 2 * ZS_G_EDGE == ZS_G_SCR_WIDTH);

    ZS_SUITE("Layout: intet mål er skrevet af");

    /*
     * Det her er proeven paa at kaeden er hel. Regner vi tallene ud fra
     * bunden med skaermens maal, skal de ramme praecis det headeren
     * siger. Er bare ét af dem skrevet af i stedet for udregnet, falder
     * den her den dag nogen aendrer skaermen.
     */
    const int page    = ZS_G_SCR_HEIGHT - ZS_G_BAR_HEIGHT - ZS_G_DOTS_HEIGHT;
    const int content = ZS_G_SCR_WIDTH - 2 * ZS_G_EDGE;
    const int kort_b  = (content - ZS_G_GRID_GAP) / 2;
    const int kort_h  = (page - 3 * ZS_G_GRID_GAP) / 2;

    CHECK("sidehoejden er udregnet", ZS_G_PAGE_HEIGHT == page);
    CHECK("indholdsbredden er udregnet", ZS_G_CONTENT_WIDTH == content);
    CHECK("kortbredden er udregnet", ZS_G_CARD_WIDTH == kort_b);
    CHECK("korthoejden er udregnet", ZS_G_CARD_HEIGHT == kort_h);

    /*
     * Og at der ikke er noget tilbage: med heltalsdivision kan to kort
     * komme til at mangle en pixel. Paa 480 gaar det op, men en ny
     * skaerm kan ramme et ulige tal, og saa skal vi vide det.
     */
    CHECK("bredden gaar op uden en pixel til overs",
          (content - ZS_G_GRID_GAP) % 2 == 0);
    CHECK("hoejden gaar op uden en pixel til overs",
          (page - 3 * ZS_G_GRID_GAP) % 2 == 0);
}
