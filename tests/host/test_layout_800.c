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
#include "../../firmware/kerne/ui/zs_layout.h"

void test_layout_800(void)
{
    ZS_SUITE("Layout på 800 x 1280: reglerne skal stadig holde");

    CHECK("skaermen er den nye", ZS_G_SCR_WIDTH == 800 && ZS_G_SCR_HEIGHT == 1280);

    /*
     * HER STOD DER FOER at sidehoejden var 1280 - 44 - 28 og bredden
     * 800 - 2 x 12. Begge tal var FASTE, og det var netop fejlen: en
     * statuslinje paa 44 px og en kant paa 12 px er tegnet til en 4
     * tommer skaerm. Nu skalerer de, og proeven spoerger om
     * REGLERNE holder i stedet for om tallene er de samme.
     */
    CHECK("statuslinjen er blevet hoejere", ZS_G_BAR_HEIGHT > 44);
    CHECK("kanten er blevet bredere", ZS_G_EDGE > 12);
    CHECK("siden fylder stadig det der er tilbage",
          ZS_G_BAR_HEIGHT + ZS_G_PAGE_HEIGHT + ZS_G_DOTS_HEIGHT
          == ZS_G_SCR_HEIGHT);
    CHECK("og indholdet fylder bredden mellem kanterne",
          ZS_G_CONTENT_WIDTH + 2 * ZS_G_EDGE == ZS_G_SCR_WIDTH);

    /* De samme regler som paa 480. Holder de her, holder de overalt. */
    CHECK("to kort og luften omkring fylder praecis bredden",
          ZS_G_EDGE + ZS_G_CARD_WIDTH + ZS_G_GRID_GAP
          + ZS_G_CARD_WIDTH + ZS_G_EDGE == ZS_G_SCR_WIDTH);
    CHECK("to kort og luften fylder sidens hoejde",
          ZS_G_GRID_GAP + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP
          + ZS_G_CARD_HEIGHT + ZS_G_GRID_GAP == ZS_G_PAGE_HEIGHT);
    CHECK("underteksten slutter praecis i bunden af kortet",
          ZS_G_CARD_SUB_Y + ZS_G_CARD_SUB_HEIGHT == ZS_G_CARD_IN_HEIGHT);

    /*
     * DEN ENE PIXEL. Luften i kortet er 351 her, altsaa ulige, og
     * resten laegges over det store tal. De to mellemrum er derfor ikke
     * HELT ens - de er ens plus en pixel, og det er med vilje. Se
     * ZS_G_CARD_TEXT_REST.
     */
    CHECK("de to mellemrum er ens, paa den ene pixel naer",
          (ZS_G_CARD_VALUE_Y - (ZS_G_CARD_HEAD_Y + ZS_G_CARD_HEAD_HEIGHT))
          - (ZS_G_CARD_SUB_Y - (ZS_G_CARD_VALUE_Y + ZS_G_CARD_VALUE_HEIGHT))
          == ZS_G_CARD_TEXT_REST);
    CHECK("og resten er hoejst én pixel", ZS_G_CARD_TEXT_REST <= 1);

    CHECK("intet gaar tabt i heltalsdivision paa bredden",
          (ZS_G_CONTENT_WIDTH - ZS_G_GRID_GAP) % 2 == 0);
    CHECK("eller paa hoejden",
          (ZS_G_PAGE_HEIGHT - 3 * ZS_G_GRID_GAP) % 2 == 0);

    /*
     * EN FINGER VOKSER IKKE. Trykfeltet skal staa fast, og alt man kan
     * trykke paa skal vaere mindst saa stort.
     */
    CHECK("trykfeltet er uaendret", ZS_G_TOUCH_MIN == 44);
    CHECK("raden er mindst en finger", ZS_G_ROW_HEIGHT >= ZS_G_TOUCH_MIN);
    CHECK("knappen er mindst en finger", ZS_G_BTN_HEIGHT >= ZS_G_TOUCH_MIN);
    CHECK("valget er mindst en finger", ZS_G_CHOICE_HEIGHT >= ZS_G_TOUCH_MIN);

    /*
     * TEKSTEN I KORTET SKAL VOKSE MED KORTET.
     *
     * Her stod der foer at skriftstoerrelserne var UAENDREDE, og det
     * var sandt - det var bare ikke i orden. Et kort der er 3 gange
     * hoejere med praecis lige saa smaa linjer i er det der ser forkert
     * ud paa en vaeg.
     *
     * Nu foelger de skaermens BREDDE, fordi en tekst skal kunne vaere
     * der i bredden.
     *
     * HER STOD DER FOER at tallet skulle vaere 54 * 800 / 480 = 90.
     * Det var et overslag, og overslaget var forkert: skriften bliver
     * bygget af lv_font_conv, og DEN bestemmer hvor hoej en linje er.
     * Den blev 88 og ikke 90. Reserverede vi 90, stod der 2 px luft
     * for meget i bunden af kortet; reserverede vi 86, blev linjen
     * klippet. Derfor kommer tallene nu fra zs_skrift_maal.h, som
     * build-assets.sh skriver ud fra den skrift der faktisk ligger i
     * flashen - og proeven spoerger om DET er det der er reserveret.
     */
    CHECK("overskriften er vokset", ZS_G_CARD_HEAD_HEIGHT > 20);
    CHECK("det store tal er vokset", ZS_G_CARD_VALUE_HEIGHT > 54);
    CHECK("underteksten er vokset", ZS_G_CARD_SUB_HEIGHT > 18);
    CHECK_INT("der er reserveret praecis det skriften maaler",
              ZS_G_CARD_VALUE_HEIGHT, ZS_LH_TAL);
    CHECK_INT("og underteksten ogsaa", ZS_G_CARD_SUB_HEIGHT, ZS_LH_BROED);
    /*
     * OG SAA EN VAGT PAA SELVE SKRIFTEN. Tallene ovenfor passer ogsaa
     * hvis nogen henter saettet til 480 ind paa en 800 px skaerm - saa
     * ville der staa 54 begge steder, og proeven ville sige ok til en
     * skaerm med knappenaalsskrift. Overslaget er stadig det rigtige
     * sted at vaere taet paa, bare ikke praecis: 4 px er et afrundet
     * skridt i en skriftstoerrelse, 36 px er et forkert saet.
     */
    CHECK("og skriften hoerer til DEN HER skaerm, ikke en anden",
          ZS_G_CARD_VALUE_HEIGHT > 54 * 800 / 480 - 4
          && ZS_G_CARD_VALUE_HEIGHT < 54 * 800 / 480 + 4);

    ZS_SUITE("Layout på 800 x 1280: det der stadig er en designbeslutning");

    /*
     * REGNESTYKKET HOLDER NU, OG KORTET ER STADIG FOR TOMT.
     *
     * Tommelfingerreglen er at et mellemrum mellem to tekstlinjer ikke
     * boer vaere stoerre end den stoerste af dem. Paa 480 er 33 under
     * 54, og det ser rigtigt ud.
     *
     * Efter skaleringen er tallet 88 og mellemrummet 176. Altsaa stadig
     * for tomt - og det kan skaleringen ikke loese, for kortet er 550
     * px hoejt og tre tekstlinjer fylder 153.
     *
     * Der er to veje, og de er begge DESIGN og ikke aritmetik:
     *
     *   1. FLERE KORT. Tre raekker i stedet for to giver kort paa 360
     *      px, og saa holder reglen. Men vi har fire tal og seks
     *      pladser, saa der skal findes to mere.
     *   2. STOERRE TAL. Et tal paa 160 px i stedet for 90 lukker hullet
     *      med to kort. Det kraever en skrifttype paa omkring 190 px,
     *      og saa skal det maales om "100" kan vaere der i bredden.
     *
     * Proeven herunder siger hvor langt der er, saa valget bliver
     * taget med aabne oejne og ikke opdaget paa en vaeg hos en kunde.
     */
    CHECK("kortet er mere end dobbelt saa hoejt som paa 480",
          ZS_G_CARD_HEIGHT > 2 * 186);
    CHECK("mellemrummet er stadig stoerre end tallet selv - altsaa "
          "mangler der en designbeslutning, ikke en skalering",
          ZS_G_CARD_TEXT_GAP > ZS_G_CARD_VALUE_HEIGHT);
    CHECK_INT("og hvor meget tallet skal vokse for at reglen holder",
              (ZS_G_CARD_IN_HEIGHT - ZS_G_CARD_HEAD_HEIGHT
               - ZS_G_CARD_SUB_HEIGHT) / 3, 146);
}
