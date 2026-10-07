/*
 * Timens energi.
 *
 * De tilfaelde der er svaere at fremkalde paa en skaerm, fordi man
 * skulle vente en time hver gang: et doegnskift, en genstart, en skaerm
 * der har vaeret slukket, og en taeller der loeber rundt.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_energi.h"

#include <string.h>

/* Kort hjaelper, saa proeverne kan laeses. */
static zs_energi_t tik(zs_energi_basis_t *b, float prod, float koebt,
                       float solgt, float ind, float ud_bat,
                       int time, uint16_t dag,
                       float *ud, bool *ud_har, int8_t *ud_time)
{
    float t[ZS_E_ANTAL] = { prod, koebt, solgt, ind, ud_bat };
    bool  h[ZS_E_ANTAL] = { true, true, true, true, true };
    return zs_energi_tik(b, t, h, time, dag, ud, ud_har, ud_time);
}

void test_energi(void)
{
    zs_energi_basis_t b;
    float ud[ZS_E_ANTAL];
    bool  har[ZS_E_ANTAL];
    int8_t t_ud = -1;

    ZS_SUITE("Timeenergi: den almindelige time");

    memset(&b, 0, sizeof(b));
    CHECK("foerste maaling saetter kun udgangspunktet",
          tik(&b, 1000, 500, 200, 100, 50, 12, 20000, ud, har, &t_ud)
          == ZS_ENERGI_FOERSTE);
    CHECK("samme time igen giver intet",
          tik(&b, 1200, 520, 260, 140, 50, 12, 20000, ud, har, &t_ud)
          == ZS_ENERGI_VENTER);

    CHECK("naeste time giver et resultat",
          tik(&b, 4000, 700, 900, 400, 80, 13, 20000, ud, har, &t_ud)
          == ZS_ENERGI_KLAR);
    CHECK("og det er timen der GIK, ikke den vi staar i", t_ud == 12);
    CHECK("produceret: 4000 minus 1000", ud[ZS_E_PRODUCERET] == 3000.0f);
    CHECK("koebt: 700 minus 500",        ud[ZS_E_KOEBT]      == 200.0f);
    CHECK("solgt: 900 minus 200",        ud[ZS_E_SOLGT]      == 700.0f);
    CHECK("batteri ind: 400 minus 100",  ud[ZS_E_BAT_IND]    == 300.0f);
    CHECK("batteri ud: 80 minus 50",     ud[ZS_E_BAT_UD]     == 30.0f);

    ZS_SUITE("Timeenergi: hen over midnat");

    /*
     * Fra klokken 23 til klokken 0 er der gaaet ÉN time, ikke 23
     * baglaens. Regnede vi kun paa timetallet, ville hver eneste nat
     * blive sprunget over, og en kunde ville mangle en soejle hver dag.
     */
    memset(&b, 0, sizeof(b));
    (void)tik(&b, 1000, 0, 0, 0, 0, 23, 20000, ud, har, &t_ud);
    CHECK("23 til 0 er én time frem",
          tik(&b, 1500, 0, 0, 0, 0, 0, 20001, ud, har, &t_ud) == ZS_ENERGI_KLAR);
    CHECK("og det er time 23 der sendes", t_ud == 23);
    CHECK("med det rigtige tal", ud[ZS_E_PRODUCERET] == 500.0f);

    ZS_SUITE("Timeenergi: skærmen har været væk");

    memset(&b, 0, sizeof(b));
    (void)tik(&b, 1000, 0, 0, 0, 0, 10, 20000, ud, har, &t_ud);
    CHECK("tre timer senere springes der over, der laegges ikke sammen",
          tik(&b, 9000, 0, 0, 0, 0, 13, 20000, ud, har, &t_ud) == ZS_ENERGI_HUL);
    CHECK("men udgangspunktet er sat, saa NAESTE time virker",
          tik(&b, 9500, 0, 0, 0, 0, 14, 20000, ud, har, &t_ud) == ZS_ENERGI_KLAR);
    CHECK("og den time er rigtig", ud[ZS_E_PRODUCERET] == 500.0f);

    /* Et helt doegn vaek skal ogsaa springes over. */
    memset(&b, 0, sizeof(b));
    (void)tik(&b, 1000, 0, 0, 0, 0, 12, 20000, ud, har, &t_ud);
    CHECK("samme klokkeslaet dagen efter er ikke én time",
          tik(&b, 2000, 0, 0, 0, 0, 12, 20001, ud, har, &t_ud) == ZS_ENERGI_HUL);

    ZS_SUITE("Timeenergi: uret bliver stillet tilbage");

    memset(&b, 0, sizeof(b));
    (void)tik(&b, 1000, 0, 0, 0, 0, 14, 20000, ud, har, &t_ud);
    CHECK("et skridt baglaens springes over, ikke et negativt tal",
          tik(&b, 1100, 0, 0, 0, 0, 13, 20000, ud, har, &t_ud) == ZS_ENERGI_HUL);

    ZS_SUITE("Timeenergi: en tæller der løber rundt");

    /*
     * En acc32 loeber rundt efter cirka 4,29 milliarder wattimer, og en
     * inverter der bliver skiftet starter forfra. Vi kan ikke se forskel,
     * og fire millioner kilowattimer paa én time ville staa paa skaermen
     * som et aabenlyst forkert tal. Saa springes feltet over.
     */
    memset(&b, 0, sizeof(b));
    (void)tik(&b, 4000000000.0f, 100, 0, 0, 0, 9, 20000, ud, har, &t_ud);
    CHECK("timen kan stadig regnes",
          tik(&b, 500.0f, 300, 0, 0, 0, 10, 20000, ud, har, &t_ud)
          == ZS_ENERGI_KLAR);
    CHECK("men det felt der gik baglaens springes over",
          har[ZS_E_PRODUCERET] == false);
    CHECK("mens de andre stadig er rigtige",
          har[ZS_E_KOEBT] && ud[ZS_E_KOEBT] == 200.0f);

    ZS_SUITE("Timeenergi: inverteren har ikke alle tællere");

    memset(&b, 0, sizeof(b));
    {
        float t1[ZS_E_ANTAL] = { 1000, 0, 0, 0, 0 };
        bool  h1[ZS_E_ANTAL] = { true, false, false, false, false };
        (void)zs_energi_tik(&b, t1, h1, 8, 20000, ud, har, &t_ud);
        float t2[ZS_E_ANTAL] = { 1700, 0, 0, 0, 0 };
        CHECK("en inverter uden maaler giver stadig produktionen",
              zs_energi_tik(&b, t2, h1, 9, 20000, ud, har, &t_ud)
              == ZS_ENERGI_KLAR);
        CHECK("produktionen er der", har[ZS_E_PRODUCERET]
              && ud[ZS_E_PRODUCERET] == 700.0f);
        CHECK("og vi opfinder ikke de andre",
              !har[ZS_E_KOEBT] && !har[ZS_E_BAT_IND]);
    }

    ZS_SUITE("Timeenergi: det der ikke må vælte");

    CHECK("NULL afvises",
          zs_energi_tik(NULL, NULL, NULL, 0, 0, NULL, NULL, NULL)
          == ZS_ENERGI_ARG);
    memset(&b, 0, sizeof(b));
    CHECK("en umulig klokke afvises",
          tik(&b, 0, 0, 0, 0, 0, 24, 20000, ud, har, &t_ud) == ZS_ENERGI_ARG);
    CHECK("og en negativ",
          tik(&b, 0, 0, 0, 0, 0, -1, 20000, ud, har, &t_ud) == ZS_ENERGI_ARG);

    ZS_SUITE("Timeenergi: navnene på serveren");

    for (int i = 0; i < ZS_E_ANTAL; i++) {
        const char *n = zs_energi_felt_navn((zs_energi_felt_t)i);
        CHECK("hvert felt har et navn", n != NULL && n[0] != '\0');
    }
    CHECK("uden for listen giver en tom streng, ikke NULL",
          zs_energi_felt_navn((zs_energi_felt_t)99) != NULL
          && zs_energi_felt_navn((zs_energi_felt_t)99)[0] == '\0');
}
