/*
 * Layoutet paa en ANDEN skaerm end den vi har.
 *
 * HVORFOR DEN FINDES.
 *
 * test_layout.c regner efter paa 480 x 480. Den beviser at tallene er
 * rigtige DER, men det var aldrig paastanden. Paastanden er at layoutet
 * gaar op paa enhver skaerm, saa vi kan flytte til reTerminal D1001 paa
 * 800 x 1280 uden at alt skal tegnes om.
 *
 * Den paastand kan kun proeves ved faktisk at saette en anden stoerrelse.
 * Her er den egen fil, fordi en header kun kan hentes én gang per
 * oversaettelse: saetter vi maalene FOER vi henter zs_layout.h, regner den
 * alt om, og saa kan vi se om reglerne stadig holder.
 *
 * Tallene herunder er den rigtige skaerm fra Seeeds datablad.
 */

#define ZS_G_SCR_WIDTH   800
#define ZS_G_SCR_HEIGHT  1280

#include "zs_test.h"
#include "../../firmware/main/ui/zs_layout.h"

void test_layout_800(void)
{
    ZS_SUITE("Layout på 800 x 1280: reglerne skal stadig holde");

    CHECK("skaermen er den nye", ZS_G_SCR_WIDTH == 800 && ZS_G_SCR_HEIGHT == 1280);

    /* Alt det der regnes ud, regnes nu om. */
    CHECK("sidehoejden foelger med", ZS_G_PAGE_HEIGHT == 1280 - 44 - 28);
    CHECK("indholdsbredden foelger med", ZS_G_CONTENT_WIDTH == 800 - 2 * 12);

    /* De samme regler som paa 480. Holder de her, holder de overalt. */
    CHECK("to kort og luften omkring fylder praecis bredden",
          ZS_G_EDGE + ZS_G_CARD_WIDTH + ZS_G_GRID_GAP
          + ZS_G_CARD_WIDTH + ZS_G_EDGE == ZS_G_SCR_WIDTH);
    CHECK("to kort og luften fylder sidens hoejde",
          ZS_G_GRID_GAP + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP
          + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP == ZS_G_PAGE_HEIGHT);
    CHECK("linjen, siden og prikkerne fylder skaermen",
          ZS_G_BAR_HEIGHT + ZS_G_PAGE_HEIGHT + ZS_G_DOTS_HEIGHT
          == ZS_G_SCR_HEIGHT);
    CHECK("underteksten slutter praecis i bunden af kortet",
          ZS_G_CARD_SUB_Y + ZS_G_CARD_SUB_HEIGHT == ZS_G_CARD_IN_HEIGHT);
    CHECK("de to mellemrum er stadig ens",
          ZS_G_CARD_VALUE_Y - (ZS_G_CARD_HEAD_Y + ZS_G_CARD_HEAD_HEIGHT)
          == ZS_G_CARD_SUB_Y - (ZS_G_CARD_VALUE_Y + ZS_G_CARD_VALUE_HEIGHT));

    CHECK("intet gaar tabt i heltalsdivision paa bredden",
          (ZS_G_CONTENT_WIDTH - ZS_G_GRID_GAP) % 2 == 0);
    CHECK("eller paa hoejden",
          (ZS_G_PAGE_HEIGHT - 3 * ZS_G_GRID_GAP) % 2 == 0);

    /* De fysiske maal maa IKKE have aendret sig. En finger er den samme. */
    CHECK("trykfeltet er uaendret", ZS_G_TOUCH_MIN == 44);
    CHECK("raden er uaendret", ZS_G_ROW_HEIGHT == 56);
    CHECK("skriftstoerrelserne i kortet er uaendrede",
          ZS_G_CARD_HEAD_HEIGHT == 20 && ZS_G_CARD_VALUE_HEIGHT == 54
          && ZS_G_CARD_SUB_HEIGHT == 18);

    ZS_SUITE("Layout på 800 x 1280: hvad der ser galt ud selv om det går op");

    /*
     * REGNESTYKKET HOLDER, MEN DESIGNET GOER IKKE, og det skal staa her
     * saa ingen tror at proeverne ovenfor betyder "klar til den nye
     * skaerm".
     *
     * Paa 480 er mellemrummet mellem overskrift og tal 33 px. Paa 800 x
     * 1280 bliver kortet mere end tre gange saa hoejt, mens teksten i det
     * bliver praecis lige saa stor. Saa vokser mellemrummet til noget der
     * ser ud som en fejl: et kort der er naesten tomt med tre smaa linjer
     * spredt ud i det.
     *
     * Det rigtige paa en hoej skaerm er FLERE kort, ikke stoerre kort. Den
     * her proeve siger hvornaar graensen er naaet, saa valget bliver taget
     * med aabne oejne og ikke opdaget paa en vaeg hos en kunde.
     */
    CHECK("kortet bliver mere end dobbelt saa hoejt som paa 480",
          ZS_G_CARD_HEIGHT > 2 * 186);
    CHECK("og mellemrummet i kortet bliver mere end seks gange saa stort",
          ZS_G_CARD_TEXT_GAP > 6 * 33);

    /*
     * Tommelfingerreglen: et mellemrum mellem to tekstlinjer boer ikke
     * vaere stoerre end den stoerste af dem. Er det det, hoerer linjerne
     * ikke laengere sammen for oejet. Paa 480 er 33 under 54 og det ser
     * rigtigt ud. Paa 800 x 1280 er det ikke.
     */
    CHECK("paa den nye skaerm er mellemrummet stoerre end tallet selv, "
          "altsaa skal der taenkes design og ikke bare skaleres",
          ZS_G_CARD_TEXT_GAP > ZS_G_CARD_VALUE_HEIGHT);
}
