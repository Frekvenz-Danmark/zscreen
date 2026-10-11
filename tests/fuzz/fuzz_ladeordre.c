/*
 * zScreen - hamrer ladeordren og efterser INVARIANTER.
 *
 * HVORFOR DEN IKKE ER EN ALMINDELIG ENHEDSTEST.
 *
 * test_ladeordre.c proever de tilfaelde VI har taenkt paa: en ny
 * tilstand, et livstegn, en timer der loeber ud. Den her proever de
 * foelger ingen har taenkt paa - felterne i tilfaeldig orden, med
 * tilfaeldige tal, og et ur der hopper frem i tilfaeldige spring.
 *
 * Og den proever ikke VAERDIER, den proever REGLER. Otte ting der skal
 * holde uanset hvad der kommer ind, og den vigtigste er den femte: en
 * ordre kan kun vaere klar hvis baade tilstanden og timeren er sat
 * SIDEN sidste tilstandsskift. Det er hele grunden til at modulet
 * findes, og en regel er det eneste der kan fange at den holder i et
 * foelge ingen har skrevet ned.
 *
 * ET FORBEHOLD, SOM I DE ANDRE FUZZERE: en groen fuzzer der ikke naar
 * koden er vaerre end ingen. Derfor taelles hvor mange gange ordren
 * faktisk NAAEDE at blive klar, og den siger FRA hvis den aldrig gjorde.
 *
 *   ./fuzz_ladeordre <runder> <froe>
 */
#include "zs_ladeordre.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Baade gyldige navne og det der ligner dem. */
static const char *NAVNE[] = {
    "oplad", "aflad", "normal", "zeroExport", "laas",
    "OPLAD", "oplad ", "", "noedstop", "ladeplan", "normal\n", "zeroexport",
};
static const int32_t SEK[] = { -1, 0, 1, 2, 599, 600, 899, 900, 901,
                               65535, 2147483647, -2147483647 };
static const float KW[] = { 0.0f, 1.0f, -1.0f, 499.9f, 500.0f, 500.1f,
                            -500.0f, -500.1f, 99999.0f, -99999.0f };

static uint64_t froe;
static uint32_t naeste(void);
static uint32_t naeste(void)
{
    froe = froe * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(froe >> 33);
}

#define ANTAL(a)  (sizeof(a) / sizeof((a)[0]))

int main(int argc, char **argv)
{
    long runder = (argc > 1) ? atol(argv[1]) : 20000;
    froe = (argc > 2) ? (uint64_t)atoll(argv[2]) : 1u;

    static const char *const REGEL[] = {
        "standen var KLAR, men plan() gav ingen plan",
        "plan() gav en plan, men standen var ikke KLAR",
        "planens timer var ikke ordrens",
        "en udloebet ordre gav en plan",
        "KLAR uden baade tilstand og timer",
        "KLAR for en tilstand der bruger raten",
        "standen var uden for enummen",
        "standens navn var NULL",
    };
    long brudt[ANTAL(REGEL)];
    memset(brudt, 0, sizeof(brudt));

    long maalinger = 0, naaede_klar = 0, naaede_udloebet = 0;

    for (long r = 0; r < runder; r++) {
        zs_ladeordre_t o;
        zs_ladeordre_nulstil(&o);
        int64_t ur = (int64_t)(naeste() % 1000000u);

        for (int trin = 0; trin < 6; trin++) {
            switch (naeste() % 4u) {
            case 0:
                (void)zs_ladeordre_tilstand(&o, NAVNE[naeste() % ANTAL(NAVNE)],
                                            ur);
                break;
            case 1:
                (void)zs_ladeordre_sekunder(&o, SEK[naeste() % ANTAL(SEK)], ur);
                break;
            case 2:
                (void)zs_ladeordre_setpunkt(&o, KW[naeste() % ANTAL(KW)], ur);
                break;
            default:
                /* Uret frem, nogle gange langt nok til at noget udloeber. */
                ur += (int64_t)(naeste() % 1200000u);
                break;
            }

            zs_ordre_stand_t s = zs_ladeordre_stand(&o, ur);
            zs_ladeplan_t p;
            /* Fyldt med skrald foerst: en plan der ikke blev skrevet,
             * maa ikke kunne forveksles med en der blev. */
            memset(&p, 0xA5, sizeof(p));
            bool fik = zs_ladeordre_plan(&o, ur, &p);
            maalinger++;
            if (s == ZS_ORDRE_KLAR)     { naaede_klar++; }
            if (s == ZS_ORDRE_UDLOEBET) { naaede_udloebet++; }

            if (s == ZS_ORDRE_KLAR && !fik)                    { brudt[0]++; }
            if (fik && s != ZS_ORDRE_KLAR)                     { brudt[1]++; }
            if (fik && p.doedmand_s != o.sekunder)             { brudt[2]++; }
            if (s == ZS_ORDRE_UDLOEBET && fik)                 { brudt[3]++; }
            if (s == ZS_ORDRE_KLAR
                && !(o.har_tilstand && o.har_sekunder))        { brudt[4]++; }
            if (s == ZS_ORDRE_KLAR
                && (o.tilstand == ZS_LADE_OPLAD
                    || o.tilstand == ZS_LADE_AFLAD))           { brudt[5]++; }
            if ((int)s < 0 || (int)s > (int)ZS_ORDRE_KLAR)     { brudt[6]++; }
            if (zs_ladeordre_stand_navn(s) == NULL)            { brudt[7]++; }
        }
    }

    int fejl = 0;
    for (size_t i = 0; i < ANTAL(REGEL); i++) {
        if (brudt[i] != 0) {
            printf("  BRUDT %ld gange: %s\n", brudt[i], REGEL[i]);
            fejl = 1;
        }
    }

    /* NAAEDE DEN IND? En fuzzer der aldrig fik en ordre klar, proever
     * ingenting - og den skal sige fra i stedet for at se groen ud. */
    if (naaede_klar == 0) {
        printf("  INGEN ordre blev nogensinde KLAR. Fuzzeren naar ikke "
               "koden, og den beviser ingenting.\n");
        fejl = 1;
    }
    if (naaede_udloebet == 0) {
        printf("  INGEN ordre udloeb nogensinde. Doedmandsknappen blev "
               "ikke proevet.\n");
        fejl = 1;
    }

    printf("  %ld maalinger: %ld gange klar, %ld gange udloebet\n",
           maalinger, naaede_klar, naaede_udloebet);
    if (!fejl) {
        printf("  alle %zu regler holdt\n", ANTAL(REGEL));
    }
    return fejl;
}
