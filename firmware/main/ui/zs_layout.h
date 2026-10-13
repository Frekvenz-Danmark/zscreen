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
 *
 * HVORDAN et board saetter dem. Fra byggeriet og ikke fra en header:
 *
 *     idf_build_set_property(COMPILE_DEFINITIONS
 *                            "ZS_G_SCR_WIDTH=800" APPEND)
 *     idf_build_set_property(COMPILE_DEFINITIONS
 *                            "ZS_G_SCR_HEIGHT=1280" APPEND)
 *
 * Det SKAL vaere derfra. Satte et board dem i en header i stedet, ville
 * den header skulle hentes foer zs_layout.h i hver eneste fil, og den
 * foerste der glemte det ville stille og roligt faa 480 igen. Fra
 * byggeriet gaelder de for alt, og saa kan det ikke ske.
 *
 * Reglerne nederst i filen tjekkes for den stoerrelse der FAKTISK bygges
 * med, saa en skaerm hvor layoutet ikke gaar op stopper byggeriet.
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
/* Statuslinjen foroven                                                */
/* ------------------------------------------------------------------ */
/*
 * Pladserne stod som faste tal i zs_statusbar.c: tandhjulet paa 430 og
 * statusikonet paa 400, med en kommentar om at det var "regnet fra hoejre
 * kant paa en 480 px skaerm". Det er praecis den slags der gaar i stykker
 * naar skaermen bliver stoerre: tandhjulet ville blive siddende midt paa
 * en 800 px skaerm i stedet for i hoejre hjoerne.
 *
 * Nu regnes de FRA HOEJRE KANT, saa de flytter med. Tallene er de samme
 * paa 480, og det staar der en vagt paa nedenfor.
 */
#define ZS_G_BAR_EDGE       6     /* luft ud til kanten               */
#define ZS_G_BAR_ICON_W     20    /* statusikonet                     */
#define ZS_G_BAR_ICON_GAP   10    /* mellem ikonet og naboen          */

/* Tandhjulet er en hel fingerflade, helt ude i hoejre hjoerne. */
#define ZS_G_BAR_GEAR_X     (ZS_G_SCR_WIDTH - ZS_G_BAR_EDGE - ZS_G_TOUCH_MIN)
/* Statusikonet lige til venstre for det. */
#define ZS_G_BAR_ICON_X     (ZS_G_BAR_GEAR_X - ZS_G_BAR_ICON_GAP \
                             - ZS_G_BAR_ICON_W)
/* Og klokkeslaettet slutter lige foer ikonet. */
#define ZS_G_BAR_TIME_RIGHT (ZS_G_BAR_ICON_X - ZS_G_BAR_ICON_GAP - 2)

/* ------------------------------------------------------------------ */
/* Midterlinjen                                                        */
/* ------------------------------------------------------------------ */
/*
 * Stod som "#define CX 240" i zs_flow.c med kommentaren "midterlinjen".
 * 240 ER midten af 480, men kun af 480.
 */
#define ZS_G_CX             (ZS_G_SCR_WIDTH / 2)
#define ZS_G_CY_PAGE        (ZS_G_PAGE_HEIGHT / 2)

/* ------------------------------------------------------------------ */
/* Prissiden, lodret                                                   */
/* ------------------------------------------------------------------ */
/*
 * Tallene stod som faste y-positioner i zs_price_page.c, regnet i haanden
 * ud af skrifttypernes linjehoejder paa en side der er 408 px hoej. De er
 * rigtige, og de er regnet rigtigt. Men de staar ét sted nu, og de regnes
 * oven paa hinanden i stedet for at vaere skrevet ind hver for sig.
 *
 * HVOR KOMMER DEN EKSTRA PLADS HEN paa en hoejere skaerm?
 *
 * Linjerne med tekst skal IKKE vokse: en skrifttype bliver ikke stoerre af
 * at skaermen goer, og et tal paa 64 px fylder 54 px uanset hvad. Kortene
 * og noten er ogsaa tekst. Det eneste der kan bruge mere plads
 * fornuftigt, er SOEJLERNE: en hoejere graf viser den samme doegn bedre.
 *
 * Derfor er soejlernes hoejde den der bliver regnet til sidst, af det der
 * er tilbage. Det er et valg, og det staar her saa det er til at se og
 * aendre ét sted.
 */
#define ZS_G_PRICE_TOP        14   /* luft foeroven                    */
#define ZS_G_PRICE_LABEL_HEIGHT    16   /* 13 px skrift -> 16 px linje      */
#define ZS_G_PRICE_GAP_LABEL   6
#define ZS_G_PRICE_VALUE_HEIGHT    54   /* 64 px tal, kun cifre             */
#define ZS_G_PRICE_GAP_VALUE   4
#define ZS_G_PRICE_WORD_HEIGHT     24
#define ZS_G_PRICE_GAP_WORD   14
#define ZS_G_PRICE_GAP_CHART   4
#define ZS_G_PRICE_AXIS_HEIGHT     16   /* timeaksen                        */
#define ZS_G_PRICE_GAP_AXIS   12
#define ZS_G_PRICE_CARDS_HEIGHT   100
#define ZS_G_PRICE_GAP_CARDS  12
#define ZS_G_PRICE_NOTE_HEIGHT     18
#define ZS_G_PRICE_BOTTOM     10   /* luft ned til sidens kant         */

/* Alt det der IKKE er soejler. */
#define ZS_G_PRICE_FAST \
    (ZS_G_PRICE_TOP + ZS_G_PRICE_LABEL_HEIGHT + ZS_G_PRICE_GAP_LABEL \
     + ZS_G_PRICE_VALUE_HEIGHT + ZS_G_PRICE_GAP_VALUE + ZS_G_PRICE_WORD_HEIGHT \
     + ZS_G_PRICE_GAP_WORD + ZS_G_PRICE_GAP_CHART + ZS_G_PRICE_AXIS_HEIGHT \
     + ZS_G_PRICE_GAP_AXIS + ZS_G_PRICE_CARDS_HEIGHT + ZS_G_PRICE_GAP_CARDS \
     + ZS_G_PRICE_NOTE_HEIGHT + ZS_G_PRICE_BOTTOM)

/* Og soejlerne faar resten. */
#define ZS_G_PRICE_CHART_HEIGHT    (ZS_G_PAGE_HEIGHT - ZS_G_PRICE_FAST)

/* y-positionerne, lagt oven paa hinanden. */
#define ZS_G_PRICE_Y_LABEL    ZS_G_PRICE_TOP
#define ZS_G_PRICE_Y_VALUE    (ZS_G_PRICE_Y_LABEL + ZS_G_PRICE_LABEL_HEIGHT \
                               + ZS_G_PRICE_GAP_LABEL)
#define ZS_G_PRICE_Y_WORD     (ZS_G_PRICE_Y_VALUE + ZS_G_PRICE_VALUE_HEIGHT \
                               + ZS_G_PRICE_GAP_VALUE)
#define ZS_G_PRICE_Y_CHART    (ZS_G_PRICE_Y_WORD + ZS_G_PRICE_WORD_HEIGHT \
                               + ZS_G_PRICE_GAP_WORD)
#define ZS_G_PRICE_Y_AXIS     (ZS_G_PRICE_Y_CHART + ZS_G_PRICE_CHART_HEIGHT \
                               + ZS_G_PRICE_GAP_CHART)
#define ZS_G_PRICE_Y_CARDS    (ZS_G_PRICE_Y_AXIS + ZS_G_PRICE_AXIS_HEIGHT \
                               + ZS_G_PRICE_GAP_AXIS)
#define ZS_G_PRICE_Y_NOTE     (ZS_G_PRICE_Y_CARDS + ZS_G_PRICE_CARDS_HEIGHT \
                               + ZS_G_PRICE_GAP_CARDS)

/* ------------------------------------------------------------------ */
/* Siden der venter paa netvaerket                                     */
/* ------------------------------------------------------------------ */
/*
 * Teksten "Forbinder til netvaerket ..." stod paa y = 160, skrevet ind som
 * et naegent tal. De 160 er ikke tilfaeldige: spinderen staar paa 70, er
 * 64 px hoej, og saa er der 26 px luft. Men det forhold var ikke til at
 * se, og retter man spinderens hoejde, bliver teksten siddende.
 *
 * Om hele blokken skal staa foeroven eller midt paa en hoejere skaerm er
 * et valg om hvordan den skal se ud, ikke et regnestykke. Det valg ligger
 * nu ÉT sted, her, i stedet for at vaere gemt i et tal midt i koden.
 */
#define ZS_G_WAIT_SPIN_Y    70
#define ZS_G_WAIT_SPIN_SIZE 64
#define ZS_G_WAIT_GAP       26
#define ZS_G_WAIT_TEXT_Y    (ZS_G_WAIT_SPIN_Y + ZS_G_WAIT_SPIN_SIZE \
                             + ZS_G_WAIT_GAP)

/* ------------------------------------------------------------------ */
/* VENTETRINNET: titel, forklaring, bjaelke                            */
/* ------------------------------------------------------------------ */
/*
 * To sider venter paa noget og viser en bjaelke: inverter-soegningen i
 * zs_screen_setup.c og tilmeldingens "Henter forbindelse" i
 * zs_screen_tilmeld.c. De skal se PRAECIS ens ud, og de gjorde det ikke.
 *
 * MAALT I KODEN 13. oktober 2026, foer det her blev lavet:
 *
 *     inverter-soegningen        tilmeldingen
 *     titel  zs_font_28          titel  zs_font_20
 *     tekst  ZS_C_LABEL          tekst  ZS_C_TEXT_DIM
 *     bredde 480 - 4*12 = 432    bredde 480 - 2*12 = 456
 *     y      80 / 124 / 190      y      en flex-stak fra 8
 *
 * Fire forskelle paa to sider der skulle vaere den samme. Og der stod i
 * BEGGE filer, i en kommentar, at de havde de samme maal - det var
 * skrevet i god tro og var ikke sandt. En kommentar kan ikke holde to
 * filer enige; kun én kilde kan.
 *
 * HVORFOR TALLENE STAAR SOM POSITIONER OG IKKE UDREGNES.
 *
 * Resten af den her fil regner: kortets hoejde, prissidens stak. Det
 * kan den, fordi der er en REGEL - tre blokke der skal fylde kortet, en
 * stak der skal gaa op i sidens hoejde.
 *
 * Her er der ikke nogen regel. 80, 124 og 190 er valgt af et menneske,
 * der har set paa siden. Regnede jeg dem ud af linjehoejder jeg ikke har
 * maalt, ville tallene flytte sig, og sidens udseende ville aendre sig
 * uden at nogen havde bedt om det. Et gaet pyntet som et regnestykke er
 * vaerre end et tal.
 *
 * De staar derfor som de er, ét sted, med vagter der fanger den dag
 * nogen flytter dem forbi hinanden eller ud over sidens kant.
 */

/* Bredden. Smallere end sidens indhold, saa teksten faar luft i begge
 * sider - det er inverter-soegningens maal, og nu ogsaa tilmeldingens. */
#define ZS_G_STEP_WIDTH     (ZS_G_SCR_WIDTH - 4 * ZS_G_EDGE)

#define ZS_G_STEP_TITLE_Y   80
#define ZS_G_STEP_BODY_Y    124
#define ZS_G_STEP_BAR_Y     190
#define ZS_G_STEP_BAR_HEIGHT     8
#define ZS_G_STEP_NOTE_Y    214

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

/*
 * STATUSLINJEN: tallene skal vaere de samme som foer paa 480.
 *
 * De stod som faste tal, og de blev lavet om til udregninger. Saa skal det
 * bevises at skaermen ser PRAECIS ud som foer paa den skaerm vi har, og
 * ikke bare "cirka". En udregning der giver 428 i stedet for 430 flytter
 * tandhjulet to pixels, og det er ikke til at se paa et billede, men det
 * er en aendring ingen har bedt om.
 *
 * De her tre vagter gaelder kun paa 480. Bygger man til en anden
 * skaermbredde, se tests/host/test_layout_800.c, er de ikke sande og skal
 * heller ikke vaere det: det er netop meningen at tallene flytter med.
 */
#if ZS_G_SCR_WIDTH == 480
_Static_assert(ZS_G_BAR_GEAR_X == 430,
               "tandhjulet er flyttet. Paa en 480 px skaerm skal det staa "
               "paa 430 som foer.");
_Static_assert(ZS_G_BAR_ICON_X == 400,
               "statusikonet er flyttet. Paa en 480 px skaerm skal det "
               "staa paa 400 som foer.");
_Static_assert(ZS_G_BAR_TIME_RIGHT == 388,
               "klokkeslaettet er flyttet. Paa en 480 px skaerm skal det "
               "slutte paa 388 som foer.");
_Static_assert(ZS_G_CX == 240, "midterlinjen er ikke midten af 480");
/*
 * Og ventetrinnet skal se ud PRAECIS som inverter-soegningen gjorde foer
 * den blev flyttet herind. Tallene stod skrevet ind i build_scan() i
 * zs_screen_setup.c, og det er dem der staar her.
 */
_Static_assert(ZS_G_STEP_WIDTH == 432,
               "ventetrinnets bredde er aendret. Paa en 480 px skaerm skal "
               "den vaere 432 som inverter-soegningens var.");
#endif

/* Og uanset skaermbredde: tandhjulet skal kunne vaere der, og ikke rage
 * ud over kanten. */
_Static_assert(ZS_G_BAR_GEAR_X + ZS_G_TOUCH_MIN + ZS_G_BAR_EDGE
               <= ZS_G_SCR_WIDTH,
               "tandhjulet rager ud over hoejre kant");
_Static_assert(ZS_G_BAR_TIME_RIGHT > 0,
               "skaermen er for smal til statuslinjens indhold");

/*
 * PRISSIDEN: stakken skal gaa op, og tallene skal vaere de samme som foer
 * paa en side der er 408 px hoej.
 */
_Static_assert(ZS_G_PRICE_FAST + ZS_G_PRICE_CHART_HEIGHT == ZS_G_PAGE_HEIGHT,
               "prissidens stak gaar ikke op i sidens hoejde");
_Static_assert(ZS_G_PRICE_CHART_HEIGHT > 0,
               "skaermen er for lav til prissidens indhold: der er ingen "
               "plads tilbage til soejlerne");
_Static_assert(ZS_G_PRICE_Y_NOTE + ZS_G_PRICE_NOTE_HEIGHT + ZS_G_PRICE_BOTTOM
               == ZS_G_PAGE_HEIGHT,
               "noten nederst paa prissiden ender ikke hvor siden slutter");

/*
 * VENTETRINNET: raekkefoelgen skal holde, og det skal passe paa siden.
 *
 * Positionerne er valgt i haanden, saa der er ingen udregning der kan
 * fejle. Det der KAN ske, er at nogen retter ét af tallene og kommer til
 * at lade to blokke bytte plads - fx en bjaelke der havner over
 * forklaringen. Det ser man ikke paa et tal, men det ser man her.
 */
_Static_assert(ZS_G_STEP_TITLE_Y < ZS_G_STEP_BODY_Y,
               "ventetrinnet: forklaringen skal staa UNDER titlen");
_Static_assert(ZS_G_STEP_BODY_Y < ZS_G_STEP_BAR_Y,
               "ventetrinnet: bjaelken skal staa UNDER forklaringen");
_Static_assert(ZS_G_STEP_BAR_Y + ZS_G_STEP_BAR_HEIGHT <= ZS_G_STEP_NOTE_Y,
               "ventetrinnet: noten ligger oven i bjaelken");
_Static_assert(ZS_G_STEP_NOTE_Y + ZS_G_CARD_SUB_HEIGHT <= ZS_G_PAGE_HEIGHT,
               "ventetrinnet: noten nederst har ikke plads paa siden");
_Static_assert(ZS_G_STEP_WIDTH > 0,
               "skaermen er for smal til ventetrinnet");
_Static_assert(ZS_G_STEP_WIDTH < ZS_G_CONTENT_WIDTH,
               "ventetrinnet skal vaere SMALLERE end sidens indhold: det "
               "er luften i siderne der goer at teksten kan laeses");

_Static_assert(ZS_G_WAIT_TEXT_Y == 160,
               "teksten paa ventesiden er flyttet. Den skal staa paa 160 "
               "som foer, altsaa 26 px under spinderen.");
_Static_assert(ZS_G_WAIT_TEXT_Y + 2 * ZS_G_ROW_HEIGHT <= ZS_G_PAGE_HEIGHT,
               "ventesidens tekst har ikke plads paa siden");

#if ZS_G_PAGE_HEIGHT == 408
_Static_assert(ZS_G_PRICE_Y_LABEL == 14,  "etiketten er flyttet");
_Static_assert(ZS_G_PRICE_Y_VALUE == 36,  "det store tal er flyttet");
_Static_assert(ZS_G_PRICE_Y_WORD  == 94,  "ordet er flyttet");
_Static_assert(ZS_G_PRICE_Y_CHART == 132, "soejlerne er flyttet");
_Static_assert(ZS_G_PRICE_CHART_HEIGHT == 104, "soejlerne har skiftet hoejde");
_Static_assert(ZS_G_PRICE_Y_AXIS  == 240, "timeaksen er flyttet");
_Static_assert(ZS_G_PRICE_Y_CARDS == 268, "de to kort er flyttet");
_Static_assert(ZS_G_PRICE_Y_NOTE  == 380, "noten er flyttet");
#endif

#endif /* ZS_LAYOUT_H */
