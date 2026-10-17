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
/*
 * ── SKALERINGEN ────────────────────────────────────────────────────────
 *
 * HVORFOR DEN FINDES.
 *
 * Fladen blev tegnet paa 480 x 480. Da den samme flade blev bygget til
 * 800 x 1280, fulgte HALVDELEN af tallene med og halvdelen stod fast.
 * Det blev maalt, og det ser saadan ud:
 *
 *                        480 x 480    800 x 1280
 *   siden                  408          1208     foelger
 *   ét kort            222 x 186     382 x 586    foelger
 *   kortets tekstblok       92            92      STAAR FAST
 *   luften i kortet         33           233      foelger
 *   soejlediagrammet       104           904      foelger
 *   de to priskort         100           100      STAAR FAST
 *   ventesidens spinder      64            64      STAAR FAST
 *
 * Resultatet er praecis det man ville forvente: kasser der er alt for
 * store, med indhold der er alt for lille midt i dem. Og
 * stroemdiagrammet, som havde sine egne faste tal, fyldte 345 px af en
 * side paa 1208.
 *
 * HVORDAN DE TRE MACROER SKAL BRUGES.
 *
 *   ZS_G_SK_WIDTH(v)   bredder og x-positioner. Foelger skaermens BREDDE.
 *   ZS_G_SK_HEIGHT(v)   hoejder og y-positioner. Foelger skaermens HOEJDE.
 *   ZS_G_SK(v)     FORMER - en cirkel, et ikon, en skriftstoerrelse.
 *                  Den mindste af de to, saa en cirkel bliver ved at
 *                  vaere rund og en tekst bliver ved at kunne vaere der
 *                  i bredden.
 *
 * PAA REFERENCESKAERMEN ER DE ALLE TRE PRAECIS 1:1. 480/480 = 1, saa
 * hver eneste vaerdi i den her fil er uaendret paa zScreen. Det er ikke
 * en hensigt, det er aritmetik, og der er en proeve paa det i
 * tests/host/test_layout.c.
 *
 * HELTALSMATEMATIK, ingen flydende tal. En skaerm skal ikke regne i
 * kommatal for at finde ud af hvor en kasse skal staa.
 *
 * DET DER IKKE MAA SKALERES: en finger. Se ZS_G_TOUCH_MIN.
 */
#define ZS_G_REF_WIDTH      480
#define ZS_G_REF_HEIGHT     480

#define ZS_G_SK_WIDTH(v)        ((v) * ZS_G_SCR_WIDTH  / ZS_G_REF_WIDTH)
#define ZS_G_SK_HEIGHT(v)        ((v) * ZS_G_SCR_HEIGHT / ZS_G_REF_HEIGHT)
#define ZS_G_SK(v)          (ZS_G_SK_WIDTH(v) < ZS_G_SK_HEIGHT(v) \
                             ? ZS_G_SK_WIDTH(v) : ZS_G_SK_HEIGHT(v))

#define ZS_G_BAR_HEIGHT     ZS_G_SK(44)
#define ZS_G_EDGE           ZS_G_SK(12)
#define ZS_G_GRID_GAP       ZS_G_SK(12)
#define ZS_G_CONTENT_WIDTH  (ZS_G_SCR_WIDTH - 2 * ZS_G_EDGE)

/*
 * SIDEN SKAL KUNNE DELES I TO HELE KORT, og den ene pixel der kan blive
 * tilbage, laegges i prikstriben.
 *
 * Her stod bare "skaermen minus linjen minus prikkerne". Det gik op paa
 * 480: 408 - 3 x 12 = 372, og 372 / 2 = 186 praecis. Paa 800 x 1280
 * blev det 1161 - 60 = 1101, og 1101 er ULIGE - saa to kort plus tre
 * mellemrum gav 1160 og ikke 1161. Vagten nederst i filen fangede det,
 * og det er praecis hvad den er til for.
 *
 * En pixel ser ingen. Men "layoutet gaar op" er en paastand man kan
 * regne efter, og den slags skal holde paa ENHVER skaerm - ellers kan
 * man ikke bruge den til noget.
 *
 * Derfor: prikstriben har et MINDSTEMAAL, siden tager det der kan deles
 * i to, og resten falder ned i striben. Paa 480 giver det 28 praecis
 * som foer.
 */
#define ZS_G_DOTS_MIN       ZS_G_SK(28)
#define ZS_G_PAGE_RAA       (ZS_G_SCR_HEIGHT - ZS_G_BAR_HEIGHT - ZS_G_DOTS_MIN)
#define ZS_G_PAGE_HEIGHT    (((ZS_G_PAGE_RAA - 3 * ZS_G_GRID_GAP) / 2) * 2 \
                             + 3 * ZS_G_GRID_GAP)
#define ZS_G_DOTS_HEIGHT    (ZS_G_SCR_HEIGHT - ZS_G_BAR_HEIGHT \
                             - ZS_G_PAGE_HEIGHT)
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
#define ZS_G_CARD_PAD       ZS_G_SK(14)    /* luft inde i kortet               */
/*
 * ── KORTETS INDMAD ────────────────────────────────────────────────────
 *
 * TO TING VAR GALT, OG BEGGE BLEV MAALT PAA ENHEDEN af maalebaandet i
 * zs_ui_maal.c. Paa alle fire kort:
 *
 *   324 x 90 stikker ud (hoejre 2)
 *   324 x 33 stikker ud (hoejre 2, ned 5)
 *
 * 1. KANTEN BLEV IKKE REGNET MED. Kortets stil har border_width 1 (se
 *    base_surface i zs_theme.c), og LVGL traekker den fra
 *    indholdsfladen. Indholdet er altsaa 2 px smallere og 2 px lavere
 *    end bredden minus polstringen.
 *
 * 2. DER BLEV RESERVERET MINDRE END SKRIFTEN BRUGER. Linjehoejderne var
 *    skaleringer af 480-tallene, og lv_font_conv regner noget andet:
 *
 *                 reserveret   skriften bruger
 *      broed 480       18            20        2 px for lidt
 *      broed 800       30            33        3 px for lidt
 *
 *    Derfor stak underteksten ned under kortets inderkant.
 *
 * zSCREEN STAAR STILLE, OG DET ER EN BESLUTNING.
 *
 * Begge fejl har vaeret der paa zScreen hele tiden - 2 px, som ingen har
 * bemaerket. At rette dem ville flytte zScreens korttekst 1 px, og det
 * er valgt FRA: produktet virker, og en pixel er ikke vaerd at roere et
 * produkt for. Den store skaerm gjorde fejlen synlig (5 px), den skabte
 * den ikke.
 *
 * Saa paa referenceskaermen staar de gamle tal, ord for ord. Paa alt
 * andet regnes der rigtigt, og vagten nederst i filen haandhaever at der
 * aldrig reserveres mindre end skriften bruger.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_CARD_KANT      0    /* regnes ikke med, se noten ovenfor */
#else
#include "zs_skrift_maal.h"
#define ZS_G_CARD_KANT      1    /* border_width i kortets stil        */
#endif

/*
 * Inde i et kort af en GIVEN stoerrelse.
 *
 * De to herunder er for et kort i gitteret. Prisfladen og detaljerne
 * laver kort i andre stoerrelser og regnede selv:
 *     inde = w - 2 * ZS_CARD_PAD
 * og det glemte kanten. Paa zScreen er kanten 0 og regnestykket
 * rigtigt; paa ZDisplay er den 1 i hver side, og saa blev tallet og
 * underteksten 2 px for brede og stak ud over kortets inderkant. Det
 * er praecis det maalebaandet meldte som "hoejre 2" paa fire kort paa
 * forsiden.
 *
 * Derfor staar regnestykket ÉT sted nu, og de faste to er lavet af
 * det samme.
 */
#define ZS_G_CARD_INDE_WIDTH(w) ((w) - 2 * ZS_G_CARD_PAD - 2 * ZS_G_CARD_KANT)
#define ZS_G_CARD_INDE_HEIGHT(h) ((h) - 2 * ZS_G_CARD_PAD - 2 * ZS_G_CARD_KANT)

#define ZS_G_CARD_IN_WIDTH  ZS_G_CARD_INDE_WIDTH(ZS_G_CARD_WIDTH)
#define ZS_G_CARD_IN_HEIGHT ZS_G_CARD_INDE_HEIGHT(ZS_G_CARD_HEIGHT)

/*
 * HOEJDERNE staar fast. De er skriftstoerrelser, og en skrift bliver ikke
 * stoerre af at skaermen goer. Et ikon paa 20 px og et tal paa 54 px er
 * valgt efter hvad man kan laese paa to meters afstand, ikke efter hvor
 * mange pixels der tilfaeldigvis er.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
/* De gamle tal, ord for ord. Se noten ved ZS_G_CARD_KANT. */
#define ZS_G_CARD_HEAD_HEIGHT  20
#define ZS_G_CARD_VALUE_HEIGHT 54
#define ZS_G_CARD_SUB_HEIGHT   18
#else
/* Det skriften faktisk bruger, skrevet af lv_font_conv selv. */
#define ZS_G_CARD_HEAD_HEIGHT  ZS_LH_ETIKET
#define ZS_G_CARD_VALUE_HEIGHT ZS_LH_TAL
#define ZS_G_CARD_SUB_HEIGHT   ZS_LH_BROED
#endif

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
#define ZS_G_CARD_TEXT_SUM  (ZS_G_CARD_HEAD_HEIGHT + ZS_G_CARD_VALUE_HEIGHT \
                             + ZS_G_CARD_SUB_HEIGHT)
#define ZS_G_CARD_TEXT_LUFT (ZS_G_CARD_IN_HEIGHT - ZS_G_CARD_TEXT_SUM)
#define ZS_G_CARD_TEXT_GAP  (ZS_G_CARD_TEXT_LUFT / 2)
/*
 * DEN ENE PIXEL DER KAN BLIVE TILBAGE.
 *
 * Luften i kortet deles i to lige store mellemrum. Er den ULIGE, bliver
 * der én pixel til rest, og saa gaar regnestykket ikke op - vagten
 * nederst i filen fangede det paa 800 x 1280, hvor luften er 351.
 *
 * Resten laegges OVER det store tal. Ikke under: tallet er det oejet
 * gaar til, og staar det en pixel for hoejt eller lavt, ser ingen det -
 * men staar overskriften klemt op mod kanten, kan man se det.
 *
 * Paa 480 x 480 er luften 66, altsaa lige, og resten er NUL. Hvert tal
 * i kortet er uaendret der.
 */
#define ZS_G_CARD_TEXT_REST (ZS_G_CARD_TEXT_LUFT % 2)

#define ZS_G_CARD_HEAD_Y    0
#define ZS_G_CARD_VALUE_Y   (ZS_G_CARD_HEAD_Y + ZS_G_CARD_HEAD_HEIGHT \
                             + ZS_G_CARD_TEXT_GAP + ZS_G_CARD_TEXT_REST)
#define ZS_G_CARD_SUB_Y     (ZS_G_CARD_VALUE_Y + ZS_G_CARD_VALUE_HEIGHT \
                             + ZS_G_CARD_TEXT_GAP)

/* Fysiske maal: en finger bliver ikke stoerre af at skaermen goer. De
 * skal BLIVE staaende naar vi skifter skaerm. */
/*
 * EN FINGER VOKSER IKKE MED SKAERMEN.
 *
 * ZS_G_TOUCH_MIN er ikke et designtal, det er en tomme. Og de to
 * skaerme har naesten samme taethed - 4 tommer paa 480 x 480 giver 170
 * punkter pr. tomme, 8 tommer paa 800 x 1280 giver 189 - saa 44 px er
 * naesten lige stort paa fingeren begge steder. Derfor staar det fast.
 *
 * Raekker og knapper skaleres til gengaeld: de er visuelle, og paa en 8
 * tommer skaerm paa en vaeg skal de kunne ses. Men de maa ALDRIG blive
 * mindre end en finger, og det er derfor der staar et gulv.
 */
#define ZS_G_TOUCH_MIN      44

#define ZS_G_GULV(v)        ((v) < ZS_G_TOUCH_MIN ? ZS_G_TOUCH_MIN : (v))
#define ZS_G_ROW_HEIGHT     ZS_G_GULV(ZS_G_SK(56))

/*
 * EN LISTERAD PAA TVAERS.
 *
 * Her stod der ingenting, og tallene stod i zs_theme.c og zs_theme.h
 * som faste pixels: ikonpladsen 34, pilen 30 og vaerdien 100. Det er
 * tal regnet i haanden paa en 480 px skaerm.
 *
 * Ikonpladsen var den vaerste. Ikonskriften bygges i ZS_G_SK(20), saa
 * ikonet er 20 px bredt paa zScreen og 33 px paa ZDisplay - men
 * pladsen stod paa 34 begge steder. Altsaa var der 14 px luft efter
 * ikonet paa den lille skaerm og 1 px paa den store, og
 * netvaerksnavnet klistrede op ad wifi-ikonet paa ZDisplay.
 *
 * Derfor regnes pladsen nu som to ting lagt sammen: ikonets EGEN
 * bredde, og luften efter det. Saa foelger luften ikonet naar
 * skriften vokser, i stedet for at blive aedt af det.
 *
 * ZS_G_SK_WIDTH(20) er samme regnestykke som build-assets.sh bruger
 * naar den bygger zs_ikon_lille, saa de to kan ikke komme i utakt.
 */
/*
 * EN RAEKKE MED EN KONTAKT: TITEL, FORKLARING OG EN VIPPE.
 *
 * "Daemp om natten" og "Byt koeb og salg" bruger den, og alle tre maal
 * i den var skaleret med skaermens HOEJDE i stedet for med formen:
 *
 *                     zScreen        ZDisplay
 *   raden             76 px          202 px   om 75 px tekst
 *   mellem linjerne   26 px           69 px   om linjer paa 42 og 33
 *   vippen            52 x 30         86 x 80
 *
 * 86 x 80 er naesten kvadratisk, og en vippe med rund ende bliver
 * derfor en cirkel. Det er den "store runde grimme cirkel".
 *
 * Reglen er den samme som overalt ellers i filen: en kasse og en
 * knap holder deres FORM, saa de skaleres med ZS_G_SK. Og afstanden
 * mellem to tekstlinjer regnes af LINJERNE, ikke af skaermen.
 *
 * PAA REFERENCESKAERMEN staar de gamle tal, saa zScreen ikke flytter
 * sig - se noten ved ZS_G_CARD_KANT.
 */
#define ZS_G_SWITCH_WIDTH   ZS_G_SK(52)
#define ZS_G_SWITCH_HEIGHT  ZS_G_SK(30)
/* Pladsen der skal holdes fri til hoejre for teksten: vippen plus luft. */
#define ZS_G_SWITCH_PLADS   (ZS_G_SWITCH_WIDTH + ZS_G_SK_WIDTH(10))

#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_SWROW_HEIGHT   76
#define ZS_G_SWROW_TITEL_DY (-12)
#define ZS_G_SWROW_HJAELP_DY 14
#else
/* Luften mellem de to linjer. 3 px paa 480, og den foelger formen. */
#define ZS_G_SWROW_LUFT     ZS_G_SK(3)
/* Hele tekstblokken: titlens linje, luft, forklaringens linje. */
#define ZS_G_SWROW_BLOK     (ZS_LH_RAD + ZS_G_SWROW_LUFT + ZS_LH_BROED)
/* Raden er blokken plus lige meget luft over og under. */
#define ZS_G_SWROW_HEIGHT   (ZS_G_SWROW_BLOK + 2 * ZS_G_SK(15))
/* De to linjer stilles om radens midte, hver med sin halve linje. */
#define ZS_G_SWROW_TITEL_DY (-(ZS_G_SWROW_BLOK / 2) + ZS_LH_RAD / 2)
#define ZS_G_SWROW_HJAELP_DY ((ZS_G_SWROW_BLOK / 2) - ZS_LH_BROED / 2)
#endif

/*
 * ET INDTASTNINGSFELT: EN RAMME MED TEKSTEN I.
 *
 * Kodeordssiden og "find manuelt" bygger det samme felt, og de
 * regnede begge tekstens bredde om fra bunden:
 *     ZS_CONTENT_WIDTH - ... - ZS_G_SK_WIDTH(32)
 * Det var ment som "to gange polstringen paa 16", men
 * ZS_G_SK_WIDTH(32) er 53 og 2 * ZS_G_SK_WIDTH(16) er 52 - og
 * rammens 1 px i hver side var helt glemt. Teksten blev derfor 1 px
 * bredere end rammen indeni, paa BEGGE sider. Paa zScreen er den 2
 * px for bred og har vaeret det hele tiden.
 *
 * Det kunne ikke ses, fordi rammen klipper teksten alligevel. Men to
 * steder der regner det samme forskelligt, bliver ikke rettet samme
 * dag - saa maalene staar her, og zs_field_create i zs_theme.c
 * bygger feltet ét sted for begge sider.
 */
/*
 * HOEJDEN FOELGER FORMEN OG IKKE SKAERMENS HOEJDE.
 *
 * Her stod ZS_G_SK_HEIGHT(56). Paa ZDisplay er skaermen 2,67 gange
 * hoejere end referencen, saa feltet blev 149 px hoejt om en
 * tekstlinje paa 42. Det ser ud som en fejl, og det ER en fejl: en
 * ramme om én linje tekst skal foelge TEKSTEN, ikke skaermen.
 * ZS_G_SK giver 93, altsaa 42 px tekst og 25 px luft over og under -
 * samme forhold som de 56 paa zScreen.
 */
#define ZS_G_FIELD_HEIGHT   ZS_G_SK(56)
#define ZS_G_FIELD_PAD      ZS_G_SK_WIDTH(16)
#define ZS_G_FIELD_KANT     1
#define ZS_G_FIELD_RAD      ZS_G_SK(14)
/* Hvad der er plads til INDE i en ramme der er w bred. */
#define ZS_G_FIELD_INDE(w)  ((w) - 2 * ZS_G_FIELD_PAD - 2 * ZS_G_FIELD_KANT)

/*
 * ETIKET, FELT OG HJAELPETEKST - DEN SAMME BLOK PAA TO SIDER.
 *
 * Kodeordssiden og "find manuelt" har begge en etiket, et felt og en
 * linje hjaelpetekst under det. Y-vaerdierne stod som tre faste tal -
 * 16, 42 og 110 - og de tre tal siger ikke hvad de BETYDER. Det kom
 * frem paa ZDisplay:
 *
 *     luft over feltet   41 px
 *     luft under feltet  88 px
 *
 * Teksten laa altsaa mere end dobbelt saa langt fra feltet som
 * etiketten gjorde, og det ses med det samme. Grunden er at de tre
 * tal foelger skaermens HOEJDE (2,67 gange) mens feltet foelger sin
 * form (1,67 gange) - saa hullet voksede hurtigere end kassen.
 *
 * Nu er luften ÉT tal, og den bruges to gange: over feltet og under
 * det. Saa kan de to afstande ikke komme i utakt, uanset skaerm.
 *
 * PAA REFERENCESKAERMEN staar de tre gamle tal. Der er luften 9 px
 * over og 12 under, altsaa ogsaa skaev - men zScreen skal ikke flytte
 * sig, og 3 px er ikke vaerd at flytte en flade der staar i marken
 * for. Tallene staar skrevet ned her, saa beslutningen kan tages om.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_FIELD_LABEL_Y  16
#define ZS_G_FIELD_BOX_Y    42
#define ZS_G_FIELD_HJAELP_Y 110
#else
#define ZS_G_FIELD_LABEL_Y  ZS_G_SK_HEIGHT(16)
#define ZS_G_FIELD_BOX_Y    ZS_G_SK_HEIGHT(42)
/* Luften over feltet, maalt fra etikettens egen linje. DEN er maalet
 * for luften under feltet ogsaa. */
#define ZS_G_FIELD_LUFT     (ZS_G_FIELD_BOX_Y \
                             - (ZS_G_FIELD_LABEL_Y + ZS_LH_ETIKET))
#define ZS_G_FIELD_HJAELP_Y (ZS_G_FIELD_BOX_Y + ZS_G_FIELD_HEIGHT \
                             + ZS_G_FIELD_LUFT)
#endif
/*
 * Oejet ved siden af feltet staar LODRET MIDT i feltet.
 *
 * Det stod som ZS_G_SK_HEIGHT(48), og det var praecis midten paa
 * zScreen: 48 + 22 = 70, og feltets midte er 42 + 28 = 70. Paa
 * ZDisplay blev oejets midte 150 og feltets 158, altsaa 8 px for
 * hoejt. Nu regnes det ud af feltet, og paa 480 giver det samme 48.
 */
#define ZS_G_FIELD_EYE_Y    (ZS_G_FIELD_BOX_Y \
                             + (ZS_G_FIELD_HEIGHT - ZS_G_TOUCH_MIN) / 2)

/*
 * TASTATURET.
 *
 * Hoejden stod som ZS_KB_HEIGHT 224 i zs_keyboard.h - et raat tal i
 * en widget-header, hvor ingen vagt kiggede. Resultatet var at
 * tastaturet var 224 px paa BEGGE skaerme: paa ZDisplays 1280 px
 * hoeje skaerm blev det en stribe i bunden med taster paa 56 px i en
 * flade der er 80 px bred. Det er det brugeren kalder skrumpet.
 *
 * Hoejden skalerer nu med ZS_G_SK og ikke med skaermens hoejde, saa
 * en tast beholder sin FORM: 48 x 56 paa zScreen og 80 x 93 paa
 * ZDisplay er det samme forhold.
 */
#define ZS_G_KB_RAEKKER     4            /* se kortene i zs_keyboard.c */
#define ZS_G_KB_HOEJDE      ZS_G_SK(224)

/*
 * LYSSTYRKE-KASSEN.
 *
 * Hoejden stod som ZS_G_SK_HEIGHT(90) og blev 240 px paa ZDisplay om
 * en etiket paa 42 og en skyder paa 16. Resten var luft.
 *
 * Nu regnes den af det der ER i den: polstring, etikettens linje,
 * luft, skyderen, og den smule skyderen staar over bunden.
 *
 * REGNESTYKKET GIVER PRAECIS 90 PAA 480, og det er beviset for at det
 * er det rigtige regnestykke:
 *
 *     2*16 + 26 + 16 + 10 + 6 = 90
 *
 * Referenceskaermen bruger alligevel tallet 90 skrevet af, fordi den
 * ikke henter skriftens maal - se noten ved ZS_G_CARD_KANT.
 */
#define ZS_G_BRIGHT_PAD     ZS_G_SK(16)
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_BRIGHT_HEIGHT  90
#else
#define ZS_G_BRIGHT_HEIGHT  (2 * ZS_G_BRIGHT_PAD + ZS_LH_RAD \
                             + ZS_G_SK(16) + ZS_G_SLIDER_HEIGHT \
                             + ZS_G_SLIDER_BUND)
#endif

/*
 * ET VALG: ET KORT MED EN TITEL OG EN FORKLARING.
 *
 * Her stod afstandene inde i kortet som ZS_G_SK_HEIGHT(14) og
 * ZS_G_SK_HEIGHT(50), altsaa skaleret med skaermens HOEJDE. Men
 * kortets egen hoejde er ZS_G_CHOICE_HEIGHT, som skalerer med
 * ZS_G_SK. Paa en skaerm hvor de to forhold er ens, gaar det godt. Paa
 * ZDisplay er bredden 1,67 gange og hoejden 2,67 gange, og saa laa
 * forklaringen 133 px nede i et kort der er 153 px hoejt. Den stak 15
 * px ud i bunden paa alle tre temakort - maalt paa glasset.
 *
 * REGLEN er den samme som alle andre steder i filen: en afstand INDE
 * i en kasse foelger KASSEN, ikke skaermen. Derfor ZS_G_SK og ikke
 * ZS_G_SK_HEIGHT.
 *
 * Og forklaringen laegges under titlens EGEN linjehoejde i stedet for
 * under et tal der er gaettet til at passe. Paa referenceskaermen
 * staar de to gamle tal uberoerte - 14 og 50 - saa zScreen ikke
 * flytter sig. Samme beslutning som ved ZS_G_CARD_KANT.
 */
#define ZS_G_CHOICE_PAD     ZS_G_SK_WIDTH(16)
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_CHOICE_TITLE_Y 14
#define ZS_G_CHOICE_SUB_Y   50
#else
#define ZS_G_CHOICE_TITLE_Y ZS_G_SK(14)
/* titlens linje plus to pixels luft, maalt og ikke gaettet */
#define ZS_G_CHOICE_SUB_Y   (ZS_G_CHOICE_TITLE_Y + ZS_LH_STOR + ZS_G_SK(2))
#endif
/* Den tykkeste kant: det valgte kort har 2 px, de andre 1. Pladsen
 * indeni regnes efter den TYKKESTE, ellers stikker teksten ud paa
 * praecis det kort man har valgt - og det var ogsaa det der skete:
 * 17 px paa det valgte mod 15 paa de to andre. */
#define ZS_G_CHOICE_KANT    2

/*
 * LYSSTYRKESKYDEREN.
 *
 * Bredden stod som ZS_G_CONTENT_WIDTH - ZS_G_SK_WIDTH(32), og det
 * rammer ikke kassens inderkant: kassen har ogsaa 1 px ramme i hver
 * side, og 2 * ZS_G_SK_WIDTH(16) er 52 hvor ZS_G_SK_WIDTH(32) er 53.
 * Skyderen stak derfor 1 px ud paa ZDisplay og 2 px paa zScreen -
 * maalt - og knoppen ved 100 % laa delvist under kassens kant.
 *
 * PAA REFERENCESKAERMEN STAAR DET GAMLE TAL. At rette de 2 px flytter
 * knoppen paa zScreen, og zScreen skal ikke flytte sig. Fejlen er
 * altsaa stadig der paa den lille skaerm, med vilje og skrevet ned.
 * Samme beslutning som ved ZS_G_CARD_KANT.
 *
 * HOEJDEN er 26 px og skal vaere tynd - en skyder paa 44 px ser ud
 * som en fejl. Fladen man kan RAMME udvides i stedet, se
 * ZS_G_SLIDER_EKSTRA.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_SLIDER_WIDTH   (ZS_G_CONTENT_WIDTH - ZS_G_SK_WIDTH(32))
#else
#define ZS_G_SLIDER_WIDTH   ZS_G_FIELD_INDE(ZS_G_CONTENT_WIDTH)
#endif
/*
 * Hoejden foelger FORMEN. Stod som ZS_G_SK_HEIGHT(10) og blev 26 px
 * paa ZDisplay - en tyk stribe paa en skaerm hvor alt andet er
 * skaleret 1,67. ZS_G_SK giver 16, altsaa samme forhold til bredden
 * som de 10 har paa zScreen. Klikfladen er stadig 44, se
 * ZS_G_SLIDER_EKSTRA.
 */
#define ZS_G_SLIDER_HEIGHT  ZS_G_SK(10)
/* Hvor hoejt over kassens inderkant skyderen staar. */
#define ZS_G_SLIDER_BUND    ZS_G_SK(6)
/* Hvor meget klikfladen skal vokse i hver side for at en finger kan
 * ramme den. Bliver 0 hvis skyderen allerede er tyk nok. */
#define ZS_G_SLIDER_EKSTRA  (ZS_G_SLIDER_HEIGHT >= ZS_G_TOUCH_MIN ? 0 \
                             : (ZS_G_TOUCH_MIN - ZS_G_SLIDER_HEIGHT + 1) / 2)

/*
 * PILEN I ET KORTS UNDERTEKST.
 *
 * Kortene NETTET og BATTERI saetter et ikon foran underteksten -
 * "saelger til nettet", "lader", "aflader" - og teksten skulle rykke
 * til side for det. Afstanden stod som (20 + 6) i zs_tile.c: ikonets
 * bredde paa zScreen plus luften efter det.
 *
 * Paa ZDisplay er ikonet 33 px bredt, og teksten startede stadig 26 px
 * inde. Altsaa laa den oven i pilen, uden luft imellem - paa BEGGE
 * kort. Samme fejl som ZS_G_ROW_ICON_W, og rettet paa samme maade:
 * ikonets EGEN bredde plus luften, saa luften foelger ikonet naar
 * skriften vokser i stedet for at blive aedt af det.
 */
#define ZS_G_CARD_SUB_IKON  (ZS_G_ROW_ICON_SIZE + ZS_G_SK_WIDTH(6))

/*
 * PRIKKERNE NEDERST PAA FORSIDEN.
 *
 * Her stod de tre tal i zs_screen_home.c som raa pixels - 10, 30 og
 * 38 - og de blev ikke fanget af vagten paa fladen, fordi de HAVDE et
 * navn. Et navn er ikke det samme som et regnestykke.
 *
 * Resultatet var at fingerfladen blev 38 px bred paa en 800 px
 * skaerm, altsaa under de 44 en finger kraever, mens hoejden var
 * vokset til 47. Maalebaandet meldte "38 x 47" tre gange.
 *
 * Afstanden er ikke pynt: fingerfladerne maa ikke overlappe, for saa
 * rammer et tryk til hoejre for den foerste prik den anden. Vagten
 * nederst i filen holder det fast, saa regnestykket ikke skal
 * efterregnes i hovedet naar tallene aendres.
 */
#define ZS_G_DOT_SIZE       ZS_G_SK(10)        /* selve prikken      */
#define ZS_G_DOT_GAP        ZS_G_SK_WIDTH(30)  /* luft imellem       */
#define ZS_G_DOT_HIT_W      ZS_G_SK_WIDTH(38)  /* fingerfladen       */

#define ZS_G_ROW_ICON_SIZE  ZS_G_SK_WIDTH(20)  /* = zs_ikon_lille   */
#define ZS_G_ROW_ICON_GAP   ZS_G_SK_WIDTH(14)  /* luft efter ikonet */
#define ZS_G_ROW_ICON_W     (ZS_G_ROW_ICON_SIZE + ZS_G_ROW_ICON_GAP)
/* Pilen til hoejre: samme ikonskrift, og luften foer den. */
#define ZS_G_ROW_CHEV_GAP   ZS_G_SK_WIDTH(10)
#define ZS_G_ROW_CHEV_W     (ZS_G_ROW_ICON_SIZE + ZS_G_ROW_CHEV_GAP)
/* Pladsen til vaerdien yderst til hoejre. */
#define ZS_G_ROW_VALUE_W    ZS_G_SK_WIDTH(100)
/* Radens egen vandrette luft, den samme i begge sider. */
#define ZS_G_ROW_PAD        ZS_G_SK_WIDTH(16)
#define ZS_G_BTN_HEIGHT     ZS_G_GULV(ZS_G_SK(52))
#define ZS_G_CHOICE_HEIGHT  ZS_G_GULV(ZS_G_SK(92))

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
#define ZS_G_BAR_EDGE       ZS_G_SK(6)     /* luft ud til kanten               */
#define ZS_G_BAR_ICON_W     ZS_G_SK(20)    /* statusikonet                     */
#define ZS_G_BAR_ICON_GAP   ZS_G_SK(10)    /* mellem ikonet og naboen          */

/*
 * ── LODRET MIDTE, ÉT STED ─────────────────────────────────────────────
 *
 * HVORFOR DEN HER MACRO FINDES.
 *
 * I zs_statusbar.c stod der tre gange "(ZS_BAR_HEIGHT - N) / 2", hvor N
 * var elementets hoejde paa 480 skrevet af i haanden: 26 for logoet, 22
 * for maerket, 20 for ikonet. Det virkede paa den skaerm tallene blev
 * skrevet til.
 *
 * Paa 800 x 1280 er linjen 73 px og elementerne 1,67 gange stoerre - og
 * de tre N'er stod stille. Maalt:
 *
 *   logoet       y = (73-26)/2 = 23, men det er 43 hoejt -> slutter paa
 *                66 af 73. Midten ville vaere 15
 *   statusikonet y = (73-20)/2 = 26, men det er 33 hoejt -> 6 px FOR
 *                LAVT
 *   tandhjulet   y = 0 med hoejde 44 i en linje paa 73 -> 29 px tom
 *                under, altsaa FOR HOEJT
 *
 * Derfor: ÉN macro, og den tager elementets EGEN hoejde. Saa kan man
 * ikke skrive et tal der ikke er elementets.
 */
#define ZS_G_BAR_MIDT(h)    ((ZS_G_BAR_HEIGHT - (h)) / 2)

/*
 * Elementernes maal. De SKAL foelge samme broek som skrifterne og
 * logoerne, fordi det er de samme tal tools/build-assets.sh regner med
 * - se "sk()" der. Et logo paa ZS_G_SK(26) px er praecis det logo
 * build-assets lagde i saettet.
 */
#define ZS_G_BAR_LOGO_SIZE  ZS_G_SK(26)    /* z-maerket, kvadratisk    */
#define ZS_G_BAR_LOGO_X     ZS_G_SK(14)
#define ZS_G_BAR_BADGE_HOEJDE    ZS_G_SK(22)    /* 16 px linje + 3 px luft  */
#define ZS_G_BAR_BADGE_GAP  ZS_G_SK(10)
#define ZS_G_BAR_BADGE_RAD  ZS_G_SK(6)
#define ZS_G_BAR_BADGE_PADH ZS_G_SK(8)
#define ZS_G_BAR_BADGE_PADV ZS_G_SK(3)
#define ZS_G_BAR_TIME_LINE  ZS_G_SK(26)    /* 20 px skrift -> 26 linje */

/*
 * MAERKET STARTER EFTER LOGOET, og det er et regnestykke og ikke et tal.
 *
 * Her stod "14 + 26 + 10" skrevet ud i zs_statusbar.c. De tre tal er
 * logoets x, logoets bredde og luften imellem - men kun paa 480. Paa 800
 * fylder logoet 14..57, mens maerket startede paa 50: det OVERLAPPEDE
 * logoet med 7 px. Det var det foerste man kunne se var galt.
 */
#define ZS_G_BAR_BADGE_X    (ZS_G_BAR_LOGO_X + ZS_G_BAR_LOGO_SIZE \
                             + ZS_G_BAR_BADGE_GAP)

/*
 * Tandhjulet er en hel fingerflade, helt ude i hoejre hjoerne.
 *
 * STOERRELSEN ER LINJENS HOEJDE. Den var ZS_G_TOUCH_MIN, altsaa 44, og
 * det er praecis linjens hoejde paa 480 - men paa 800 er linjen 73, og
 * saa sad knappen i toppen med 29 px tom under. Dertil: x blev regnet
 * med 44 og knappen tegnet med 73, saa den raakte 19 px ud over hoejre
 * kant (746 + 73 = 819 af 800). Vagten nederst i filen saa det ikke,
 * fordi den ogsaa regnede med 44.
 */
#define ZS_G_BAR_GEAR_SIZE  ZS_G_BAR_HEIGHT
#define ZS_G_BAR_GEAR_X     (ZS_G_SCR_WIDTH - ZS_G_BAR_EDGE \
                             - ZS_G_BAR_GEAR_SIZE)
/* Statusikonet lige til venstre for det. */
#define ZS_G_BAR_ICON_X     (ZS_G_BAR_GEAR_X - ZS_G_BAR_ICON_GAP \
                             - ZS_G_BAR_ICON_W)
/* Og klokkeslaettet slutter lige foer ikonet. */
#define ZS_G_BAR_TIME_RIGHT (ZS_G_BAR_ICON_X - ZS_G_BAR_ICON_GAP - ZS_G_SK(2))

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
#define ZS_G_PRICE_TOP        ZS_G_SK_HEIGHT(14)   /* luft foeroven                    */
#define ZS_G_PRICE_LABEL_HEIGHT    ZS_G_SK(16)   /* 13 px skrift -> 16 px linje      */
#define ZS_G_PRICE_GAP_LABEL   ZS_G_SK_HEIGHT(6)
#define ZS_G_PRICE_VALUE_HEIGHT    ZS_G_SK(54)   /* 64 px tal, kun cifre             */
#define ZS_G_PRICE_GAP_VALUE   ZS_G_SK_HEIGHT(4)
#define ZS_G_PRICE_WORD_HEIGHT     ZS_G_SK(24)
#define ZS_G_PRICE_GAP_WORD   ZS_G_SK_HEIGHT(14)
#define ZS_G_PRICE_GAP_CHART   ZS_G_SK_HEIGHT(4)
#define ZS_G_PRICE_AXIS_HEIGHT     ZS_G_SK(16)   /* timeaksen                        */
#define ZS_G_PRICE_GAP_AXIS   ZS_G_SK_HEIGHT(12)
#define ZS_G_PRICE_CARDS_HEIGHT   ZS_G_SK_HEIGHT(100)
#define ZS_G_PRICE_GAP_CARDS  ZS_G_SK_HEIGHT(12)
#define ZS_G_PRICE_NOTE_HEIGHT     ZS_G_SK(18)
#define ZS_G_PRICE_BOTTOM     ZS_G_SK_HEIGHT(10)   /* luft ned til sidens kant         */

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
#define ZS_G_WAIT_SPIN_Y    ZS_G_SK_HEIGHT(70)
#define ZS_G_WAIT_SPIN_SIZE ZS_G_SK(64)
#define ZS_G_WAIT_GAP       ZS_G_SK_HEIGHT(26)
#define ZS_G_WAIT_TEXT_Y    (ZS_G_WAIT_SPIN_Y + ZS_G_WAIT_SPIN_SIZE \
                             + ZS_G_WAIT_GAP)

/* ------------------------------------------------------------------ */
/* SIDERAMMEN                                                           */
/* ------------------------------------------------------------------ */
/*
 * De stod i zs_theme.h, hvor alt andet layout er alias herfra. De
 * flyttede hertil fordi guide-sidens regnestykke nedenfor skal kunne
 * gaa op MOD dem, og en vagt i den her fil kan ikke naa zs_theme.h.
 * zs_theme.h er nu alias, som den er for resten.
 */
#define ZS_G_PAGE_HEAD_HEIGHT  ZS_G_SK_HEIGHT(56)

/*
 * SIDEHOVEDET: tilbage-knappen og hvor titlen begynder.
 *
 * De tre tal stod raa i zs_theme.c, og det gik galt paa den
 * kedelige maade: knappens x blev pakket i ZS_G_SK_WIDTH, og to
 * linjer laengere ned stod "title_x = 4 + ZS_TOUCH_MIN + 8" med de
 * samme tal UPAKKEDE. De to skred fra hinanden paa ZDisplay - knappen
 * til 6, titlen regnet fra 4 - saa luften mellem pil og titel blev
 * mindre i stedet for stoerre.
 *
 * Nu staar de ÉT sted, og titlen regnes af knappens egne tal.
 * ZS_G_TOUCH_MIN skalerer ikke: en finger bliver ikke stoerre af en
 * stoerre skaerm.
 */
#define ZS_G_HEAD_BACK_X        ZS_G_SK_WIDTH(4)
#define ZS_G_HEAD_TITLE_GAP     ZS_G_SK(8)
#define ZS_G_HEAD_TITLE_X       (ZS_G_HEAD_BACK_X + ZS_G_TOUCH_MIN \
                                 + ZS_G_HEAD_TITLE_GAP)

/* Rullebjaelken. Smal med vilje - den skal kunne ses uden at fylde. */
#define ZS_G_SCROLLBAR_W        ZS_G_SK(4)
#define ZS_G_PAGE_FOOT_HEIGHT  ZS_G_SK_HEIGHT(76)

/* ------------------------------------------------------------------ */
/* GUIDE-SIDEN: fire kort der FLUGTER                                   */
/* ------------------------------------------------------------------ */
/*
 * Fire trin som kort, og de skal fylde siden PRAECIS - ikke "cirka".
 * Et kort der er én pixel for hoejt, giver et hul nederst eller et kort
 * der er klippet, og begge ser ud som et uheld.
 *
 * Derfor er kortets hoejde UDREGNET af det der er tilbage, og ikke
 * skrevet af. Rettes luften foeroven eller mellem kortene, bliver
 * kortene selv lavere - og vagten nedenfor standser byggeriet hvis det
 * ikke gaar op.
 *
 * Siden har en fod, altsaa knappen "Fortsæt", saa indholdet er
 * skaermhoejden minus baade linjen foroven og foden.
 */
#define ZS_G_GUIDE_CONTENT_HEIGHT (ZS_G_SCR_HEIGHT - ZS_G_PAGE_HEAD_HEIGHT \
                                   - ZS_G_PAGE_FOOT_HEIGHT)
/*
 * BAANDET MELLEM LINJEN FOROVEN OG DET FOERSTE KORT.
 *
 * Titlen staar MIDT i det, lodret. FOer laa den med 16 px luft over og
 * en afsat hoejde paa 52 - men teksten fylder kun én linje, saa den sad
 * i toppen af sin egen kasse: 16 px over og 46 under. Det ser ud som om
 * den er skubbet op, og det var den.
 *
 * Nu er baandet ÉT tal, og midtstillingen laves af en flexbeholder i
 * zs_screen_setup.c. Derfor passer den baade naar titlen er én linje og
 * naar den bliver to paa et laengere sprog - vi behoever ikke vide
 * hvilket af de to det bliver.
 *
 * 86 px, og teksten er 26: 30 px over og 30 px under. Vagten nedenfor
 * holder fast paa at baandet kan rumme TO linjer, saa en oversaettelse
 * ikke bliver klippet.
 */
#define ZS_G_GUIDE_BAAND        ZS_G_SK_HEIGHT(86)
/* 20 px skrift giver en 26 px linje. Maalt i zs_font_rad.c,
 * .line_height - ikke skudt efter. */
#define ZS_G_GUIDE_TITLE_LINE   ZS_G_SK(26)
#define ZS_G_GUIDE_CARD_GAP     ZS_G_SK_HEIGHT(10)   /* mellem kortene                */
#define ZS_G_GUIDE_BOTTOM       ZS_G_SK_HEIGHT(12)   /* luft ned til foden            */
#define ZS_G_GUIDE_CARDS        4

/* Kortene begynder praecis hvor baandet slutter. */
/*
 * Resten, delt i fire - og det der IKKE kan deles i fire, laegges i
 * baandet foroven.
 *
 * Paa 480 er luften 220, og 220 / 4 = 55 praecis, saa resten er nul og
 * baandet bliver 86 som foer. Paa 800 x 1280 er luften 590, og 590 / 4
 * giver 147 med 2 til rest. To pixels oeverst paa siden ser ingen; to
 * pixels der ikke er gjort noget ved, faar vagten nedenfor til at
 * stoppe byggeriet - og det er rigtigt af den, for "siden gaar op" skal
 * vaere sandt og ikke naesten sandt.
 */
#define ZS_G_GUIDE_LUFT \
    (ZS_G_GUIDE_CONTENT_HEIGHT - ZS_G_GUIDE_BAAND - ZS_G_GUIDE_BOTTOM \
     - (ZS_G_GUIDE_CARDS - 1) * ZS_G_GUIDE_CARD_GAP)
#define ZS_G_GUIDE_CARD_HEIGHT  (ZS_G_GUIDE_LUFT / ZS_G_GUIDE_CARDS)
#define ZS_G_GUIDE_CARDS_Y      (ZS_G_GUIDE_BAAND \
                                 + ZS_G_GUIDE_LUFT % ZS_G_GUIDE_CARDS)

/* Tallets spalte. FAST bredde, saa teksten begynder paa samme x i alle
 * fire kort - det er det der gOEr at de flugter. */
#define ZS_G_GUIDE_NUM_WIDTH    ZS_G_SK(22)
#define ZS_G_GUIDE_NUM_GAP      ZS_G_SK_WIDTH(12)
#define ZS_G_GUIDE_CARD_PAD     ZS_G_SK(14)

/* ------------------------------------------------------------------ */
/* FEJLSIDEN: én besked, midt paa fladen                              */
/* ------------------------------------------------------------------ */
/*
 * Siden der kommer frem naar netvaerket eller anlaegget er vaek.
 *
 * Den har ÉN besked og ÉN knap, og intet andet. Derfor staar blokken
 * MIDT paa fladen, baade vandret og lodret: der er ingen tal og ingen
 * liste at stille op efter, og en blok oppe i hjoernet paa en ellers
 * tom side ser ud som om der mangler noget under den.
 *
 * Knappen staar i sidens FOD, som paa alle de andre sider. FOer laa den
 * midt i indholdet med et haandskrevet 92 px' spring, og saa stod den
 * et andet sted end knappen paa hver anden side i hele fladen.
 *
 * Luften over og under blokken er UDREGNET af det der er tilbage, og
 * ikke skrevet af. Det er det der goer at siden ogsaa er midtstillet
 * paa en skaerm med en anden hoejde - se test_layout_800.c.
 */
#define ZS_G_FEJL_CONTENT_HEIGHT (ZS_G_SCR_HEIGHT - ZS_G_PAGE_HEAD_HEIGHT \
                                  - ZS_G_PAGE_FOOT_HEIGHT)
/* To linjer 28 px skrift. Den laengste overskrift er "Skærmen er ikke
 * på netværket", som bryder i to paa 456 px. */
#define ZS_G_FEJL_HEAD_HEIGHT   ZS_G_SK_HEIGHT(80)
#define ZS_G_FEJL_GAP           ZS_G_SK_HEIGHT(16)
/* Tre linjer 16 px, med luft. Forklaringen er den laengste tekst paa
 * siden, og den maa ikke blive klippet paa et smalt sprog. */
#define ZS_G_FEJL_BODY_HEIGHT   ZS_G_SK_HEIGHT(96)

#define ZS_G_FEJL_BLOK_HEIGHT   (ZS_G_FEJL_HEAD_HEIGHT + ZS_G_FEJL_GAP \
                                 + ZS_G_FEJL_BODY_HEIGHT)
/* Resten, delt i to: lige meget over og under. */
#define ZS_G_FEJL_TOP           ((ZS_G_FEJL_CONTENT_HEIGHT \
                                  - ZS_G_FEJL_BLOK_HEIGHT) / 2)
#define ZS_G_FEJL_BODY_Y        (ZS_G_FEJL_TOP + ZS_G_FEJL_HEAD_HEIGHT \
                                 + ZS_G_FEJL_GAP)

/*
 * ET IKON MELLEM OVERSKRIFTEN OG FORKLARINGEN.
 *
 * Kun nogle fejl har et - stroemsvigt har et batteri, se zs_fejl_ikon.
 * Derfor er der TO blokke her: én med ikon og én uden. Begge er
 * midtstillet lodret i indholdet, saa en side uden ikon staar hvor den
 * altid har staaet, og en side med ikon ogsaa staar i midten.
 *
 * Luften om ikonet foelger FORMEN og ikke skaermens hoejde: det er luft
 * om blaek. ZS_G_FEJL_GAP ovenfor er skaermens hoejde, og det er
 * rigtigt DER - den skiller to tekstblokke paa en ellers tom side.
 *
 * Hoejden er skriftens EGEN, fra zs_skrift_maal.h, og ikke ZS_G_SK(28).
 * De to er ikke det samme: lv_font_conv gav 44 px hvor overslaget
 * siger 46, og to pixels i en lodret stak man har midtstillet, er to
 * pixels alt under den staar forkert.
 */
#define ZS_G_FEJL_IKON_GAP      ZS_G_SK(12)
/*
 * Luft MELLEM de to linjer i overskriften.
 *
 * Overskriften paa stroemsiden er to linjer - hvad der er sket, og hvad
 * man skal goere. Med line_space 0, som er LVGLs standard, staar de
 * klos op ad hinanden og laeses som én saetning der er brudt tilfaeldigt.
 * En fjerdedel af linjehoejden er nok til at de laeses som to ting.
 */
#define ZS_G_FEJL_LINJE_LUFT    ZS_G_SK(8)
/*
 * KUN REFERENCESKAERMEN, og hoejden er SKRIFTENS EGEN.
 *
 * Her stod 28 - zs_ikon_stor's linjehoejde - men labelet tegnes med
 * zs_ikon_kaempe, som er 24 paa 480-saettet. Ikonet fik altsaa 28 px
 * reserveret og fylder 24, saa der blev 12 px over det og 16 under:
 * hele stakken stod 4 px skaevt. Det er praecis den fejl den maalte
 * gren blev skrevet for at fjerne, og vagterne nedenfor lod den passere
 * fordi de maalte paa den samme forkerte skrift.
 *
 * FIKSTUREN AENDRER SIG HER, og det er med vilje. Siden med ikon er
 * ZS_FEJL_INGEN_STROEM, som er helt ny og aldrig har staaet hos en
 * kunde - der er ingen flade at holde stille. Og zScreen har slet
 * ikke batteri (se zs_stroem_ingen.c), saa den viser aldrig siden;
 * tallene skal alligevel vaere rigtige, for de staar i vagterne.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
/*
 * 24 OG IKKE ZS_LH_IKON_KAEMPE. Den generede header inkluderes IKKE
 * paa referenceskaermen - se noten ved ZS_G_CARD_KANT - saa tallet
 * skal staa skrevet. Det er zs_ikon_kaempe's linjehoejde i
 * assets/480/zs_skrift_maal.h, og tjek_480_skriftmaal() i
 * tools/check-flade.py holder de to sammen.
 */
#define ZS_G_FEJL_IKON_HEIGHT   24
#define ZS_G_FEJL_IBLOK_HEIGHT  (ZS_G_FEJL_HEAD_HEIGHT \
                                 + ZS_G_FEJL_IKON_GAP \
                                 + ZS_G_FEJL_IKON_HEIGHT \
                                 + ZS_G_FEJL_IKON_GAP \
                                 + ZS_G_FEJL_BODY_HEIGHT)
#define ZS_G_FEJL_ITOP          ((ZS_G_FEJL_CONTENT_HEIGHT \
                                  - ZS_G_FEJL_IBLOK_HEIGHT) / 2)
#define ZS_G_FEJL_IKON_Y        (ZS_G_FEJL_ITOP + ZS_G_FEJL_HEAD_HEIGHT \
                                 + ZS_G_FEJL_IKON_GAP)
#define ZS_G_FEJL_IBODY_Y       (ZS_G_FEJL_IKON_Y + ZS_G_FEJL_IKON_HEIGHT \
                                 + ZS_G_FEJL_IKON_GAP)
#endif

/*
 * MAALES STAKKEN, ELLER STAAR DEN FAST?
 *
 * Den maalte er rigtigere. Tallene ovenfor reserverer en KASSE til hver
 * tekst - 80 px til overskriften, 96 til forklaringen - og er teksten
 * kortere end kassen, ligger luften inde i den. Saa maales afstanden
 * ned til ikonet fra en kant man ikke kan se, og ikonet ender for langt
 * nede selvom begge mellemrum er det samme tal. Paa ZDisplay var
 * kassen 213 px om en overskrift paa 60.
 *
 * MEN DEN FLYTTER zScreen. Paa 480 staar overskriften i dag paa y=78 og
 * forklaringen paa y=174. Maalt med tools/maal-tekst.py bliver den
 * maalte stak 110 px hoej, og saa rykker overskriften til y=119 og
 * forklaringen til y=169 - 41 px paa hver skaerm der staar ude hos en
 * kunde.
 *
 * Yassin har sagt nej til at flytte zScreens flade, ogsaa naar det
 * gamle tal er maalt forkert. Derfor: referenceskaermen beholder sine
 * faste tal, alt andet maaler.
 *
 * FIKSTUREN KAN IKKE SE DET HER. tests/fixtures/layout-480.h fanger
 * MAKROER, og opstillingen er C-kode i zs_screen_fejl.c:stak_op(). Det
 * er derfor flytningen slap igennem i foerste omgang. Vagten er i
 * stedet tjek_zscreen_staar_stille() i tools/check-flade.py, som
 * tjekker at grenen ER der og at de gamle tal stadig laeses.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
#define ZS_G_FEJL_MAAL_STAK     0
#else
#define ZS_G_FEJL_MAAL_STAK     1
#endif

/*
 * Her stod maalet paa en RAMME langs kanten, som pulserede. Den er vaek:
 * en seks pixels kant kan ikke ses fra den anden ende af et koekken, og
 * det var hele formaalet. Nu flasher HELE fladen roedt, og en flade har
 * ingen maal at regne - den er sidens egen.
 */

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
 *     titel  zs_font_stor          titel  zs_font_rad
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

#define ZS_G_STEP_TITLE_Y   ZS_G_SK_HEIGHT(80)
#define ZS_G_STEP_BODY_Y    ZS_G_SK_HEIGHT(124)
#define ZS_G_STEP_BAR_Y     ZS_G_SK_HEIGHT(190)
#define ZS_G_STEP_BAR_HEIGHT     ZS_G_SK(8)
#define ZS_G_STEP_NOTE_Y    ZS_G_SK_HEIGHT(214)

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
_Static_assert(ZS_G_CARD_TEXT_REST + ZS_G_CARD_HEAD_HEIGHT + ZS_G_CARD_TEXT_GAP
               + ZS_G_CARD_VALUE_HEIGHT + ZS_G_CARD_TEXT_GAP
               + ZS_G_CARD_SUB_HEIGHT == ZS_G_CARD_IN_HEIGHT,
               "teksten i kortet gaar ikke op: de tre linjer og de to "
               "mellemrum skal fylde kortets indvendige hoejde praecis");

/* Og at der er plads til teksten overhovedet. */
_Static_assert(ZS_G_CARD_TEXT_GAP >= 0,
               "kortet er for lavt til de tre tekstlinjer");

/*
 * SKRIFTEN SKAL VAERE BYGGET TIL DEN HER SKAERM.
 *
 * HER STOD DER FOER at der aldrig maa reserveres mindre plads end
 * skriften bruger:
 *     ZS_G_CARD_SUB_HEIGHT >= ZS_LH_BROED
 * Det var et tjek der ikke kunne fejle. Uden for referenceskaermen
 * ER ZS_G_CARD_SUB_HEIGHT jo defineret som ZS_LH_BROED, saa linjen
 * laeste "88 >= 88". Den saa ud som en vagt og paastod ingenting.
 *
 * Det der KAN gaa galt, er at et board peger paa det forkerte saet
 * af skrifttyper. Saettene hedder 480 og 800 efter den skaermbredde
 * de er bygget til, og vaelger ZDisplay saettet til 480, bliver al
 * skrift en tredjedel for lille mens layoutet regner videre paa 800.
 * Det er ikke en byggefejl, og maalebaandet ser det ikke: alt er
 * inden for sin kant, der er bare luft overalt. Det ville se ud som
 * et designvalg.
 *
 * Gaelder ikke referenceskaermen, som ikke henter headeren - der er
 * tallene skrevet i hoejre hand.
 *
 * DEN NOTE HER LOEJ indtil 17. oktober 2026. Der stod at zScreens saet
 * "tjekkes i stedet af tools/check-byg.py, som holder ZS_ASSET_SAET op
 * mod ZS_G_SCR_WIDTH for BEGGE produkter". Maalt: "SCR_WIDTH" fandtes
 * NUL gange i den fil. Der var altsaa ingen backstop, og asserten her
 * er slaaet fra netop paa det produkt der sendes ud.
 *
 * Nu findes den: tjek_asset_saet_mod_bredden() i tools/check-byg.py.
 * Efterproevet ved at saette zscreens ZS_ASSET_SAET til 800 - vagten
 * sagde fra med exit 1.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(ZS_SKRIFT_SAET_WIDTH == ZS_G_SCR_WIDTH,
               "skrifterne er bygget til en anden skaermbredde end den "
               "her skaerm har. Boardet peger paa det forkerte "
               "ZS_ASSET_SAET - se zs_skrift_maal.h");
#endif

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
/*
 * HER STOD DER ZS_G_TOUCH_MIN, og det var derfor vagten ikke saa det:
 * den regnede med 44 mens knappen blev tegnet med linjens hoejde.
 */
_Static_assert(ZS_G_BAR_GEAR_X + ZS_G_BAR_GEAR_SIZE + ZS_G_BAR_EDGE
               <= ZS_G_SCR_WIDTH,
               "tandhjulet rager ud over hoejre kant");
_Static_assert(ZS_G_BAR_GEAR_SIZE >= ZS_G_TOUCH_MIN,
               "tandhjulet er mindre end en finger");
/* Alt i linjen skal vaere INDE i linjen. */
_Static_assert(ZS_G_BAR_MIDT(ZS_G_BAR_LOGO_SIZE) >= 0
               && ZS_G_BAR_MIDT(ZS_G_BAR_BADGE_HOEJDE) >= 0
               && ZS_G_BAR_MIDT(ZS_G_BAR_ICON_W) >= 0
               && ZS_G_BAR_MIDT(ZS_G_BAR_TIME_LINE) >= 0,
               "noget i statuslinjen er hoejere end linjen selv");
/* Og maerket maa ikke ligge oven i logoet. */
_Static_assert(ZS_G_BAR_BADGE_X >= ZS_G_BAR_LOGO_X + ZS_G_BAR_LOGO_SIZE,
               "maerket overlapper logoet");
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
/*
 * GUIDE-SIDEN: stakken skal gaa op i indholdets hoejde, praecis.
 */
_Static_assert(ZS_G_GUIDE_CARDS_Y
               + ZS_G_GUIDE_CARDS * ZS_G_GUIDE_CARD_HEIGHT
               + (ZS_G_GUIDE_CARDS - 1) * ZS_G_GUIDE_CARD_GAP
               + ZS_G_GUIDE_BOTTOM == ZS_G_GUIDE_CONTENT_HEIGHT,
               "guide-siden gaar ikke op: titlen, de fire kort, luften "
               "imellem og luften nederst skal fylde indholdet praecis");
/* Baandet skal kunne rumme to linjer, saa et laengere sprog ikke
 * bliver klippet. 86 >= 52. */
_Static_assert(ZS_G_GUIDE_BAAND >= 2 * ZS_G_GUIDE_TITLE_LINE,
               "guide-siden: baandet over kortene kan ikke rumme to "
               "linjer af titlen");
/* Og der skal vaere luft omkring den ene linje, ikke bare plads. */
_Static_assert((ZS_G_GUIDE_BAAND - ZS_G_GUIDE_TITLE_LINE) / 2 >= ZS_G_EDGE,
               "guide-siden: der er ikke luft nok over og under titlen");
_Static_assert(ZS_G_GUIDE_CARD_HEIGHT >= ZS_G_TOUCH_MIN,
               "guide-kortene er lavere end et trykfelt, og saa ser de "
               "ud som streger og ikke som kort");
/* Der skal vaere plads til en tekstlinje inde i kortet. */
_Static_assert(ZS_G_GUIDE_CARD_HEIGHT - 2 * ZS_G_GUIDE_CARD_PAD
               >= ZS_G_CARD_SUB_HEIGHT,
               "guide-kortet er for lavt til sin egen tekst");
_Static_assert(ZS_G_GUIDE_NUM_WIDTH + ZS_G_GUIDE_NUM_GAP
               < ZS_G_CONTENT_WIDTH - 2 * ZS_G_GUIDE_CARD_PAD,
               "tallets spalte fylder hele kortet");

/*
 * FEJLSIDEN: beskeden skal kunne vaere der, og staa i midten.
 */
_Static_assert(ZS_G_FEJL_BLOK_HEIGHT < ZS_G_FEJL_CONTENT_HEIGHT,
               "fejlsiden: overskriften og forklaringen fylder mere end "
               "indholdet, saa noget bliver klippet");
_Static_assert(ZS_G_FEJL_TOP >= ZS_G_EDGE,
               "fejlsiden: der er ikke luft nok over overskriften");
/* Midtstillingen er HELE pointen: lige meget luft over og under. */
_Static_assert(ZS_G_FEJL_TOP + ZS_G_FEJL_BLOK_HEIGHT + ZS_G_FEJL_TOP
               <= ZS_G_FEJL_CONTENT_HEIGHT,
               "fejlsiden: blokken staar ikke i midten");
_Static_assert(ZS_G_FEJL_BODY_Y > ZS_G_FEJL_TOP + ZS_G_FEJL_HEAD_HEIGHT,
               "fejlsiden: forklaringen skal staa UNDER overskriften");
_Static_assert(ZS_G_FEJL_HEAD_HEIGHT >= 2 * 32,
               "fejlsiden: der er ikke plads til to linjer 28 px i "
               "overskriften");
/*
 * BLOKKEN MED IKON SKAL OGSAA KUNNE VAERE DER, og den er hoejere.
 *
 * VAGTERNE HER STAAR KUN HVOR TALLENE BRUGES. ZS_G_FEJL_ITOP, _IKON_Y
 * og _IBODY_Y laeses af zs_screen_fejl.c paa REFERENCESKAERMEN - de
 * andre skaerme maaler stakken i stedet, se ZS_G_FEJL_MAAL_STAK. Stod
 * vagterne udenfor, ville de passe uden at betyde noget: en vagt der
 * ikke KAN fejle, faar man til at stole paa.
 */
#if !ZS_G_FEJL_MAAL_STAK
_Static_assert(ZS_G_FEJL_IBLOK_HEIGHT < ZS_G_FEJL_CONTENT_HEIGHT,
               "blokken med ikon fylder mere end fejlsiden");
_Static_assert(ZS_G_FEJL_ITOP >= ZS_G_EDGE,
               "blokken med ikon staar for taet paa sidehovedet");
_Static_assert(ZS_G_FEJL_IBODY_Y + ZS_G_FEJL_BODY_HEIGHT
               <= ZS_G_FEJL_CONTENT_HEIGHT,
               "forklaringen under ikonet gaar ud over fejlsiden");
/* Og ikonet skal ligge MELLEM de to, ikke oven i nogen af dem. */
_Static_assert(ZS_G_FEJL_IKON_Y
               >= ZS_G_FEJL_ITOP + ZS_G_FEJL_HEAD_HEIGHT,
               "ikonet ligger oven i overskriften");
_Static_assert(ZS_G_FEJL_IBODY_Y
               >= ZS_G_FEJL_IKON_Y + ZS_G_FEJL_IKON_HEIGHT,
               "forklaringen ligger oven i ikonet");
#else
/*
 * DEN MAALTE STAK KAN IKKE EFTERSES HER - hoejderne kommer fra LVGL
 * naar teksten staar der. Det der KAN efterses, er at den mindst
 * mulige stak er plads til: én linje overskrift, ikonet, én linje
 * forklaring og luften imellem. Er DEN for hoej, er siden for lav til
 * fejlsiden overhovedet, og saa hjaelper ingen maaling.
 *
 * Ikonet maales med zs_ikon_kaempe, som er den skrift labelet FAKTISK
 * tegnes med. Her stod ZS_LH_IKON_STOR, en anden skrift - og de to er
 * ikke engang i samme retning: kaempe har line_height 40 hvor stor har
 * 44, fordi lv_font_conv regner den af de tegn der er med.
 */
_Static_assert(ZS_LH_STOR + ZS_G_FEJL_IKON_GAP + ZS_LH_IKON_KAEMPE
               + ZS_G_FEJL_IKON_GAP + ZS_LH_BROED
               < ZS_G_FEJL_CONTENT_HEIGHT,
               "fejlsiden kan ikke rumme én linje overskrift, ikonet og "
               "én linje forklaring - saa er skaermen for lav til siden");
#endif

_Static_assert(ZS_G_FEJL_BODY_HEIGHT >= 3 * 24,
               "fejlsiden: der er ikke plads til tre linjer 16 px i "
               "forklaringen");

/*
 * Og foden skal kunne rumme en knap. Det gaelder alle sider med en fod,
 * ikke kun fejlsiden, og det var ikke tjekket nogen steder.
 */
_Static_assert(ZS_G_PAGE_FOOT_HEIGHT - 2 * ZS_G_EDGE >= ZS_G_BTN_HEIGHT,
               "sidens fod er for lav til en knap");

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

/*
 * Den her gaelder KUN referenceskaermen. Paa en anden stoerrelse
 * flytter teksten sig med skaleringen, og det er netop meningen.
 * De tre andre 480-vagter staar laengere nede, bag samme betingelse.
 */
#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT
_Static_assert(ZS_G_WAIT_TEXT_Y == 160,
               "teksten paa ventesiden er flyttet. Den skal staa paa 160 "
               "som foer, altsaa 26 px under spinderen.");
#endif
/*
 * EN TAST SKAL KUNNE RAMMES, OG TASTATURET SKAL KUNNE VAERE PAA
 * SKAERMEN.
 */
_Static_assert(ZS_G_KB_HOEJDE / ZS_G_KB_RAEKKER >= ZS_G_TOUCH_MIN,
               "en tast er lavere end en finger. Se ZS_G_KB_HOEJDE");
_Static_assert(ZS_G_KB_HOEJDE + ZS_G_BAR_HEIGHT < ZS_G_SCR_HEIGHT,
               "tastaturet og statuslinjen fylder hele skaermen, saa "
               "der er ikke plads til det man skriver i");

/*
 * KONTAKT-RAEKKEN SKAL GAA OP.
 *
 * Gaelder ikke referenceskaermen, som ikke har skriftens maal: der er
 * de gamle tal regnet efter i haanden én gang.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
/* Vippen skal vaere en PILLE og ikke en cirkel: bredere end hoej. */
_Static_assert(ZS_G_SWITCH_WIDTH > ZS_G_SWITCH_HEIGHT * 3 / 2,
               "vippen er naesten kvadratisk, og saa ser den ud som en "
               "cirkel. Se ZS_G_SWITCH_WIDTH");
/* Og den skal kunne rammes. */
_Static_assert(ZS_G_SWITCH_WIDTH >= ZS_G_TOUCH_MIN,
               "vippen er smallere end en finger");
/* De to linjer maa ikke ligge oven i hinanden. */
_Static_assert(ZS_G_SWROW_TITEL_DY + ZS_LH_RAD / 2
               <= ZS_G_SWROW_HJAELP_DY - ZS_LH_BROED / 2,
               "titlen og forklaringen i en kontakt-raekke ligger oven "
               "i hinanden. Se ZS_G_SWROW_LUFT");
/* Og begge skal vaere inde i raden. */
_Static_assert(ZS_G_SWROW_BLOK <= ZS_G_SWROW_HEIGHT,
               "teksten i en kontakt-raekke fylder mere end raden");
#endif

/*
 * LUFTEN OVER OG UNDER FELTET SKAL VAERE DEN SAMME.
 *
 * Den er sand af sig selv i regnestykket ovenfor, og det er netop
 * meningen - men den staar her, saa den dag nogen skriver et tal ind
 * i stedet for udtrykket, siger bygningen fra. Gaelder ikke
 * referenceskaermen, hvor de tre gamle tal staar med vilje.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(ZS_G_FIELD_BOX_Y - (ZS_G_FIELD_LABEL_Y + ZS_LH_ETIKET)
               == ZS_G_FIELD_HJAELP_Y
                  - (ZS_G_FIELD_BOX_Y + ZS_G_FIELD_HEIGHT),
               "luften over og under indtastningsfeltet er ikke den "
               "samme. Se ZS_G_FIELD_LUFT");
/* Og hjaelpeteksten skal kunne staa i to linjer uden at gaa ind under
 * tastaturet. */
_Static_assert(ZS_G_FIELD_HJAELP_Y + 2 * ZS_LH_BROED
               <= ZS_G_SCR_HEIGHT - ZS_G_KB_HOEJDE
                  - ZS_G_PAGE_HEAD_HEIGHT,
               "hjaelpeteksten under feltet gaar ind under tastaturet");
#endif
/* Oejet skal vaere midt i feltet, paa den ene pixel naer som en ulige
 * hoejde giver. */
_Static_assert(ZS_G_FIELD_EYE_Y + ZS_G_TOUCH_MIN / 2
               - (ZS_G_FIELD_BOX_Y + ZS_G_FIELD_HEIGHT / 2) <= 1
               && ZS_G_FIELD_BOX_Y + ZS_G_FIELD_HEIGHT / 2
                  - (ZS_G_FIELD_EYE_Y + ZS_G_TOUCH_MIN / 2) <= 1,
               "oejet staar ikke midt i indtastningsfeltet. Se "
               "ZS_G_FIELD_EYE_Y");

/*
 * EN RAMME OM ÉN LINJE TEKST SKAL KUNNE RUMME LINJEN.
 *
 * Gaelder ikke referenceskaermen, som ikke har skriftens maal: der er
 * de 56 px om en linje paa 26 regnet efter én gang.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(ZS_G_FIELD_HEIGHT >= ZS_LH_RAD + 2 * ZS_G_FIELD_KANT,
               "indtastningsfeltet er lavere end den tekstlinje der "
               "skal staa i det. Se ZS_G_FIELD_HEIGHT");
#endif

/*
 * FORKLARINGEN I ET VALG SKAL KUNNE VAERE I KORTET.
 *
 * Gaelder ikke referenceskaermen, som ikke har skriftens maal - der
 * er der 20 px luft tilbage, og det er regnet efter i haanden én
 * gang: 50 + 20 + 2 = 72 i et kort paa 92.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(ZS_G_CHOICE_SUB_Y + ZS_LH_BROED + ZS_G_CHOICE_KANT
               <= ZS_G_CHOICE_HEIGHT,
               "forklaringen i et valg stikker ud under kortet. Se "
               "ZS_G_CHOICE_SUB_Y");
_Static_assert(ZS_G_CHOICE_TITLE_Y + ZS_LH_STOR <= ZS_G_CHOICE_SUB_Y,
               "titlen og forklaringen i et valg ligger oven i "
               "hinanden");
#endif

/*
 * OG ALT I LYSSTYRKE-KASSEN SKAL KUNNE VAERE I DEN.
 *
 * Gaelder ikke referenceskaermen, hvor de 90 er regnet efter i haanden.
 */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(2 * ZS_G_BRIGHT_PAD + ZS_LH_RAD + ZS_G_SLIDER_HEIGHT
               + ZS_G_SLIDER_BUND <= ZS_G_BRIGHT_HEIGHT,
               "etiketten og skyderen fylder mere end lysstyrke-kassen");
#endif

/*
 * SKYDEREN SKAL KUNNE RAMMES, OGSAA NAAR DEN ER TYND.
 */
_Static_assert(ZS_G_SLIDER_HEIGHT + 2 * ZS_G_SLIDER_EKSTRA
               >= ZS_G_TOUCH_MIN,
               "skyderens klikflade er lavere end en finger. Se "
               "ZS_G_SLIDER_EKSTRA");
/* Og den skal kunne vaere inde i kassen den ligger i. Gaelder ikke
 * referenceskaermen, hvor de 2 px staar med vilje - se noten. */
#if !(ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ZS_G_SCR_HEIGHT == ZS_G_REF_HEIGHT)
_Static_assert(ZS_G_SLIDER_WIDTH
               <= ZS_G_FIELD_INDE(ZS_G_CONTENT_WIDTH),
               "skyderen er bredere end kassen den ligger i");
#endif

/*
 * TO PRIKKERS FINGERFLADER MAA IKKE OVERLAPPE.
 *
 * Midterne ligger ZS_G_DOT_SIZE + ZS_G_DOT_GAP fra hinanden, og hver
 * flade er ZS_G_DOT_HIT_W bred. Er fladen bredere end afstanden,
 * ligger de oven i hinanden, og den oeverste tager trykket - altsaa
 * rammer man den forkerte side.
 */
_Static_assert(ZS_G_DOT_HIT_W < ZS_G_DOT_SIZE + ZS_G_DOT_GAP,
               "prikkernes fingerflader overlapper, og saa rammer et "
               "tryk den forkerte side. Se ZS_G_DOT_GAP");
/*
 * HER STOD DER OGSAA at fladen skulle vaere mindst ZS_G_TOUCH_MIN
 * bred. Den linje var forkert, og den var forkert paa den vaerste
 * maade: paa zScreen er fladen 38 px med VILJE. Prikkerne er en
 * genvej, og at traekke siden til side virker altid - det stod
 * skrevet i zs_screen_home.c, og jeg satte en vagt op der sagde at
 * det stod galt. zScreen holdt op med at bygge.
 *
 * Paa ZDisplay bliver de 38 til 63, og saa er den over graensen af
 * sig selv. Undtagelsen gaelder altsaa kun referenceskaermen, og den
 * er maerket med ZS_FLAG_LILLE_MED_VILJE der hvor knappen laves, saa
 * maalebaandet kan se forskel paa en undtagelse og en fejl.
 */

/*
 * DER SKAL VAERE LUFT MELLEM IKONET OG TEKSTEN.
 *
 * Den her kunne have fanget fejlen: med ZS_G_ROW_ICON_W paa faste 34
 * og et ikon paa 33 var der 1 px, og det er det en kunde ser som at
 * teksten sidder fast i ikonet. Graensen er en tredjedel af ikonet,
 * altsaa 6 px paa zScreen og 11 paa ZDisplay - mindre end det er
 * ikke luft, det er et sammenstoed.
 */
_Static_assert(ZS_G_ROW_ICON_W - ZS_G_ROW_ICON_SIZE >= ZS_G_ROW_ICON_SIZE / 3,
               "der er for lidt luft mellem ikonet og teksten i en "
               "listerad. Se ZS_G_ROW_ICON_GAP");
/* Og titlen skal have plads tilbage at staa paa. */
_Static_assert(ZS_G_CONTENT_WIDTH - 2 * ZS_G_ROW_PAD - ZS_G_ROW_ICON_W
               - ZS_G_ROW_CHEV_W - ZS_G_ROW_VALUE_W > 0,
               "en listerad med ikon, vaerdi og pil har ingen plads "
               "tilbage til titlen");
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
