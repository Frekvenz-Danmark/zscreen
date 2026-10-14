/*
 * zScreen - ordren fra Platformen: samlet, sat paa tid, og ikke blandet.
 *
 * Det her er den del af Fase E der kan proeves uden en inverter: at en
 * ordre der kommer i tre stykker, ikke bliver til en plan nogen har
 * bedt om.
 */

#include "zs_test.h"
#include "../../firmware/kerne/net/zs_ladeordre.h"

#include <math.h>
#include <string.h>

/* Et vilkaarligt udgangspunkt paa det monotone ur. */
#define T0   1000000

void test_ladeordre(void)
{
    zs_ladeordre_t o;
    zs_ladeplan_t p;
    ZS_SUITE("Ordren fra Platformen");

    zs_ladeordre_nulstil(&o);
    CHECK_INT("en tom ordre er ingen ordre",
              zs_ladeordre_stand(&o, T0), ZS_ORDRE_INGEN);
    CHECK("og den giver ingen plan", !zs_ladeordre_plan(&o, T0, &p));

    /* ---- de tre tilstande der kan blive registre i dag ---- */
    CHECK("tilstand normal tages imod",
          zs_ladeordre_tilstand(&o, "normal", T0));
    CHECK_INT("men timeren mangler endnu",
              zs_ladeordre_stand(&o, T0), ZS_ORDRE_UFULD);
    CHECK("timeren tages imod", zs_ladeordre_sekunder(&o, 600, T0));
    CHECK_INT("og saa er ordren klar",
              zs_ladeordre_stand(&o, T0), ZS_ORDRE_KLAR);
    CHECK("planen kan dannes", zs_ladeordre_plan(&o, T0, &p));
    CHECK_INT("og det er normal: StorCtl_Mod 0", p.stor_ctl_mod, 0);
    CHECK_INT("timeren kom med i planen", p.doedmand_s, 600);

    /*
     * SETPUNKTET KRAEVES IKKE af en tilstand der ikke ser paa raten.
     * Gjorde det, ville "normal" aldrig kunne saettes uden at Platformen
     * ogsaa sendte en effekt den ikke mener noget med.
     */
    CHECK("normal blev klar UDEN et setpunkt", !o.har_setpunkt);

    for (int i = 0; i < 2; i++) {
        const char *navn = i ? "laas" : "zeroExport";
        char hvad[72];
        zs_ladeordre_nulstil(&o);
        CHECK(navn, zs_ladeordre_tilstand(&o, navn, T0));
        CHECK("timeren", zs_ladeordre_sekunder(&o, 900, T0));
        snprintf(hvad, sizeof(hvad), "%s bliver en hel plan", navn);
        CHECK(hvad, zs_ladeordre_plan(&o, T0, &p));
    }

    /* ---- de to der IKKE kan, og hvorfor ---- */
    zs_ladeordre_nulstil(&o);
    CHECK("tilstand oplad tages imod",
          zs_ladeordre_tilstand(&o, "oplad", T0));
    CHECK("timeren", zs_ladeordre_sekunder(&o, 600, T0));
    CHECK_INT("oplad mangler stadig setpunktet",
              zs_ladeordre_stand(&o, T0), ZS_ORDRE_UFULD);
    CHECK("setpunktet tages imod", zs_ladeordre_setpunkt(&o, -3.7f, T0));
    /*
     * Og saa staar den paa MANGLER_SKALA, ikke KLAR. Registret vil have
     * noternes raa tal, og omregningen fra kilowatt kraever
     * skalafaktoren - punkt 2 paa maalelisten, og den er ikke maalt.
     */
    CHECK_INT("oplad er gyldig, men raten kan ikke omregnes",
              zs_ladeordre_stand(&o, T0), ZS_ORDRE_MANGLER_SKALA);
    CHECK("og der kommer derfor INGEN plan",
          !zs_ladeordre_plan(&o, T0, &p));
    CHECK_STR("standen har et navn man kan logge",
              zs_ladeordre_stand_navn(ZS_ORDRE_MANGLER_SKALA),
              "mangler skala");
}

void test_ladeordre_blandet(void)
{
    zs_ladeordre_t o;
    ZS_SUITE("Ordren kan ikke blandes");

    /*
     * DET HER ER HELE GRUNDEN TIL AT MODULET FINDES.
     *
     * Felterne kommer hver for sig og uden lovet raekkefoelge. Kom en ny
     * tilstand ind mens timeren stadig stod paa den forriges tal, havde
     * vi en ordre ingen har bedt om - og alle tre felter ser gyldige ud
     * hver for sig, saa den kan ikke ses udefra.
     */
    zs_ladeordre_nulstil(&o);
    CHECK("foerst en hel plan: laas i 900 s",
          zs_ladeordre_tilstand(&o, "laas", T0));
    CHECK("timeren 900", zs_ladeordre_sekunder(&o, 900, T0));
    CHECK_INT("den er klar", zs_ladeordre_stand(&o, T0), ZS_ORDRE_KLAR);

    CHECK("saa kommer en NY tilstand alene",
          zs_ladeordre_tilstand(&o, "normal", T0 + 100));
    CHECK_INT("og ordren er UFULD, ikke klar med den gamle timer",
              zs_ladeordre_stand(&o, T0 + 100), ZS_ORDRE_UFULD);
    CHECK("timeren blev nulstillet", !o.har_sekunder);

    /* Den SAMME tilstand igen er et livstegn, ikke en ny plan. */
    zs_ladeordre_nulstil(&o);
    (void)zs_ladeordre_tilstand(&o, "laas", T0);
    (void)zs_ladeordre_sekunder(&o, 600, T0);
    CHECK("samme tilstand igen tages imod",
          zs_ladeordre_tilstand(&o, "laas", T0 + 5000));
    CHECK("og timeren staar der stadig", o.har_sekunder);
    CHECK_INT("saa den er stadig klar",
              zs_ladeordre_stand(&o, T0 + 5000), ZS_ORDRE_KLAR);
    CHECK("og uret blev stillet, saa den ikke udloeber paa det gamle",
          zs_ladeordre_stand(&o, T0 + 5000 + 599000) == ZS_ORDRE_KLAR);

    /* Et ukendt navn maa ikke kunne slaa en gyldig ordre i stykker. */
    zs_ladeordre_nulstil(&o);
    (void)zs_ladeordre_tilstand(&o, "normal", T0);
    (void)zs_ladeordre_sekunder(&o, 600, T0);
    CHECK("et ukendt navn afvises",
          !zs_ladeordre_tilstand(&o, "noedstop", T0 + 10));
    CHECK_INT("og den gamle ordre staar uroert",
              zs_ladeordre_stand(&o, T0 + 10), ZS_ORDRE_KLAR);
    CHECK("ogsaa med stort", !zs_ladeordre_tilstand(&o, "NORMAL", T0 + 20));
    CHECK("og med et mellemrum", !zs_ladeordre_tilstand(&o, "normal ", T0 + 30));
    CHECK_INT("stadig uroert", zs_ladeordre_stand(&o, T0 + 30), ZS_ORDRE_KLAR);
}

void test_ladeordre_doedmand(void)
{
    zs_ladeordre_t o;
    zs_ladeplan_t p;
    ZS_SUITE("Doedmandsknappen, paa det monotone ur");

    zs_ladeordre_nulstil(&o);
    (void)zs_ladeordre_tilstand(&o, "laas", T0);
    (void)zs_ladeordre_sekunder(&o, 600, T0);

    CHECK_INT("klar med det samme", zs_ladeordre_stand(&o, T0), ZS_ORDRE_KLAR);
    CHECK_INT("klar efter ét sekund",
              zs_ladeordre_stand(&o, T0 + 1000), ZS_ORDRE_KLAR);
    CHECK_INT("klar ét millisekund foer tiden",
              zs_ladeordre_stand(&o, T0 + 599999), ZS_ORDRE_KLAR);
    /* Ved den graense vaelges den strenge ende: doed NAAR tiden er gaaet. */
    CHECK_INT("doed PRAECIS naar de 600 sekunder er gaaet",
              zs_ladeordre_stand(&o, T0 + 600000), ZS_ORDRE_UDLOEBET);
    CHECK_INT("og doed laenge efter",
              zs_ladeordre_stand(&o, T0 + 99999999), ZS_ORDRE_UDLOEBET);
    CHECK("en udloebet ordre giver INGEN plan",
          !zs_ladeordre_plan(&o, T0 + 600000, &p));

    /* Et livstegn flytter graensen. */
    CHECK("et livstegn tages imod",
          zs_ladeordre_sekunder(&o, 600, T0 + 500000));
    CHECK_INT("og saa lever den igen",
              zs_ladeordre_stand(&o, T0 + 600000), ZS_ORDRE_KLAR);

    /*
     * EN KORT TIMER UDLOEBER HURTIGT. Ét sekund er lovligt paa serveren,
     * og det skal vaere ét sekund her ogsaa - ikke afrundet til noget
     * paenere.
     */
    zs_ladeordre_nulstil(&o);
    (void)zs_ladeordre_tilstand(&o, "normal", T0);
    (void)zs_ladeordre_sekunder(&o, 1, T0);
    CHECK_INT("timer paa 1 s: klar efter 999 ms",
              zs_ladeordre_stand(&o, T0 + 999), ZS_ORDRE_KLAR);
    CHECK_INT("og doed efter 1000 ms",
              zs_ladeordre_stand(&o, T0 + 1000), ZS_ORDRE_UDLOEBET);

    /*
     * ET UR DER GAAR BAGLAENS. Det monotone ur skal ikke kunne, men en
     * kalder kan give et forkert tal, og saa maa ordren ikke blive
     * UDOEDELIG: en negativ forskel skal ikke se ud som "ingen tid gaaet
     * i al evighed". Her er den stadig klar, hvilket er det sikre: en
     * ordre der lever for laenge opdages, en der doer for tidligt gor
     * ingen skade.
     */
    CHECK_INT("et ur der er gaaet baglaens giver ikke et nedbrud",
              zs_ladeordre_stand(&o, T0 - 50000), ZS_ORDRE_KLAR);
}

void test_ladeordre_vagter(void)
{
    zs_ladeordre_t o;
    ZS_SUITE("Ordren: hvad den naegter");

    zs_ladeordre_nulstil(&o);

    /* Timeren: samme graenser som serveren haandhaever. */
    CHECK("timer 0 afvises", !zs_ladeordre_sekunder(&o, 0, T0));
    CHECK("timer -1 afvises", !zs_ladeordre_sekunder(&o, -1, T0));
    CHECK("timer 1 godtages", zs_ladeordre_sekunder(&o, 1, T0));
    CHECK("timer paa loftet godtages",
          zs_ladeordre_sekunder(&o, ZS_LADE_DOEDMAND_MAKS_S, T0));
    CHECK("ét over loftet afvises",
          !zs_ladeordre_sekunder(&o, ZS_LADE_DOEDMAND_MAKS_S + 1, T0));
    CHECK("65535 afvises", !zs_ladeordre_sekunder(&o, 65535, T0));
    CHECK("og et vildt tal", !zs_ladeordre_sekunder(&o, 2147483647, T0));

    /*
     * SETPUNKTET. NaN er den vigtige: paa serversiden slap NaN igennem
     * HVER graensekontrol, fordi enhver sammenligning med NaN er falsk.
     * Den fejl skal ikke kunne komme ind her ad bagdoeren.
     */
    CHECK("setpunkt 0 godtages", zs_ladeordre_setpunkt(&o, 0.0f, T0));
    CHECK("setpunkt -3.7 godtages", zs_ladeordre_setpunkt(&o, -3.7f, T0));
    CHECK("setpunkt paa loftet godtages",
          zs_ladeordre_setpunkt(&o, (float)ZS_ORDRE_SETPUNKT_MAKS_KW, T0));
    CHECK("og i den negative ende",
          zs_ladeordre_setpunkt(&o, -(float)ZS_ORDRE_SETPUNKT_MAKS_KW, T0));
    CHECK("ét over loftet afvises",
          !zs_ladeordre_setpunkt(&o, (float)ZS_ORDRE_SETPUNKT_MAKS_KW + 1.0f, T0));
    CHECK("99999 kW afvises", !zs_ladeordre_setpunkt(&o, 99999.0f, T0));
    CHECK("-99999 kW afvises", !zs_ladeordre_setpunkt(&o, -99999.0f, T0));
    CHECK("NaN afvises", !zs_ladeordre_setpunkt(&o, NAN, T0));
    CHECK("uendelig afvises", !zs_ladeordre_setpunkt(&o, INFINITY, T0));
    CHECK("og minus uendelig", !zs_ladeordre_setpunkt(&o, -INFINITY, T0));

    /* NULL maa ikke kunne naa noget. */
    CHECK("NULL-ordre: tilstand", !zs_ladeordre_tilstand(NULL, "normal", T0));
    CHECK("NULL-ordre: sekunder", !zs_ladeordre_sekunder(NULL, 600, T0));
    CHECK("NULL-ordre: setpunkt", !zs_ladeordre_setpunkt(NULL, 0.0f, T0));
    CHECK("NULL-navn", !zs_ladeordre_tilstand(&o, NULL, T0));
    CHECK_INT("NULL-ordre har ingen stand",
              zs_ladeordre_stand(NULL, T0), ZS_ORDRE_INGEN);
    {
        zs_ladeplan_t p2;
        CHECK("NULL-ordre giver ingen plan", !zs_ladeordre_plan(NULL, T0, &p2));
        zs_ladeordre_nulstil(&o);
        (void)zs_ladeordre_tilstand(&o, "normal", T0);
        (void)zs_ladeordre_sekunder(&o, 600, T0);
        CHECK("NULL-ud giver ingen plan", !zs_ladeordre_plan(&o, T0, NULL));
    }
    zs_ladeordre_nulstil(NULL);     /* maa ikke vaelte */
    CHECK("nulstil(NULL) vaelter ikke", true);

    /* Standenes navne. */
    CHECK_STR("ingen",    zs_ladeordre_stand_navn(ZS_ORDRE_INGEN), "ingen");
    CHECK_STR("ufuld",    zs_ladeordre_stand_navn(ZS_ORDRE_UFULD), "ufuld");
    CHECK_STR("udloebet", zs_ladeordre_stand_navn(ZS_ORDRE_UDLOEBET), "udloebet");
    CHECK_STR("klar",     zs_ladeordre_stand_navn(ZS_ORDRE_KLAR), "klar");
    CHECK_STR("uden for listen",
              zs_ladeordre_stand_navn((zs_ordre_stand_t)99), "ukendt");
}
