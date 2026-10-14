/*
 * Timens energi.
 *
 * De tilfaelde der er svaere at fremkalde paa en skaerm, fordi man
 * skulle vente en time hver gang: et doegnskift, en genstart, en skaerm
 * der har vaeret slukket, og en taeller der loeber rundt.
 */

#include "zs_test.h"
#include "../../firmware/kerne/net/zs_energi.h"
#include "../../firmware/kerne/zs_config.h"

#include <string.h>

/* Simulatorens serienummer. Samme inverter hele vejen i de proever der
 * ikke handler om et skift, saa de betyder praecis det samme som foer
 * serienummeret kom ind i regningen. */
#define SN "31234567"

/* Kort hjaelper, saa proeverne kan laeses. */
static zs_energi_t tik(zs_energi_basis_t *b, float prod, float koebt,
                       float solgt, float ind, float ud_bat,
                       int time, uint16_t dag,
                       float *ud, bool *ud_har, int8_t *ud_time)
{
    float t[ZS_E_ANTAL] = { prod, koebt, solgt, ind, ud_bat };
    bool  h[ZS_E_ANTAL] = { true, true, true, true, true };
    return zs_energi_tik(b, t, h, SN, time, dag, ud, ud_har, ud_time);
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
        (void)zs_energi_tik(&b, t1, h1, SN, 8, 20000, ud, har, &t_ud);
        float t2[ZS_E_ANTAL] = { 1700, 0, 0, 0, 0 };
        CHECK("en inverter uden maaler giver stadig produktionen",
              zs_energi_tik(&b, t2, h1, SN, 9, 20000, ud, har, &t_ud)
              == ZS_ENERGI_KLAR);
        CHECK("produktionen er der", har[ZS_E_PRODUCERET]
              && ud[ZS_E_PRODUCERET] == 700.0f);
        CHECK("og vi opfinder ikke de andre",
              !har[ZS_E_KOEBT] && !har[ZS_E_BAT_IND]);
    }

    ZS_SUITE("Timeenergi: det der ikke må vælte");

    CHECK("NULL afvises",
          zs_energi_tik(NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL)
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

/*
 * Feltnavnene skaermen skriver til serveren.
 *
 * Rammer et navn ved siden af, sker der ikke noget synligt: serveren
 * tager imod beskeden og smider den vaek, og feltet staar tomt for
 * evigt. Der kommer ingen fejl nogen steder, og derfor skal den slags
 * fanges her.
 */
void test_feltnavne(void)
{
    ZS_SUITE("Feltnavne: alle er der, og ingen går igen");

    const char *navne[] = {
        ZS_FLEET_TARGET_FELT,  ZS_FLEET_VERSION_FELT,
        ZS_FLEET_SOL_FELT,     ZS_FLEET_FORBRUG_FELT,
        ZS_FLEET_BATTERI_FELT, ZS_FLEET_NET_FELT,
        ZS_FLEET_SOC_FELT,     ZS_FLEET_MODEL_FELT,
        ZS_FLEET_SERIENR_FELT, ZS_FLEET_EFFEKT_FELT,
        ZS_FLEET_KAPACITET_FELT,
        ZS_ENERGI_TIME_FELT,
        zs_energi_felt_navn(ZS_E_PRODUCERET),
        zs_energi_felt_navn(ZS_E_KOEBT),
        zs_energi_felt_navn(ZS_E_SOLGT),
        zs_energi_felt_navn(ZS_E_BAT_IND),
        zs_energi_felt_navn(ZS_E_BAT_UD),
    };
    const size_t n = sizeof(navne) / sizeof(navne[0]);

    CHECK("der er sytten felter i alt", n == 17);

    for (size_t i = 0; i < n; i++) {
        CHECK("hvert navn er udfyldt", navne[i] != NULL && navne[i][0] != '\0');
        /*
         * OpenRemotes attributnavne skal begynde med et lille bogstav og
         * kun bestaa af bogstaver og tal. Et mellemrum eller en
         * bindestreg ville blive taget imod og tiet ihjel.
         */
        bool rent = (navne[i][0] >= 'a' && navne[i][0] <= 'z');
        for (const char *c = navne[i]; *c != '\0' && rent; c++) {
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
                  || (*c >= '0' && *c <= '9'))) {
                rent = false;
            }
        }
        CHECK("og indeholder kun bogstaver og tal, med lille forbogstav", rent);
    }

    /*
     * DEN VIGTIGSTE. To felter med samme navn ville betyde at det ene
     * overskriver det andet paa serveren, og paa dashboardet ville et
     * tal hoppe mellem to vaerdier uden grund. En kopieret linje er nem
     * at lave og umulig at se.
     */
    size_t dubletter = 0;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (strcmp(navne[i], navne[j]) == 0) {
                dubletter++;
            }
        }
    }
    CHECK("ingen af dem gaar igen", dubletter == 0);
}

/*
 * Udgangspunktet er bundet til ÉN inverter.
 *
 * HVORFOR DEN HER PROEVE FINDES.
 *
 * Udgangspunktet ligger i flashen og overlever en genstart. Det er
 * meningen: ellers kostede hver genstart en time. Men det overlever ogsaa
 * at kunden skifter inverter, eller vaelger en anden i indstillingerne,
 * eller at skaermen bliver flyttet til et andet anlaeg.
 *
 * Uden en binding til serienummeret ville den foerste time efter et skift
 * blive regnet som den NYE inverters livstaeller minus den GAMLES. Er den
 * nye hoejere, fordi anlaegget er aeldre eller stoerre, sender vi et tal
 * paa mange tusinde kilowattimer for én time. Det bliver gemt paa
 * serveren for evigt og oedelaegger hver graf og hver sum bagefter.
 *
 * Og det er netop den slags fejl der ikke ser ud som en fejl: den sker én
 * gang, den ser ud som et stort tal, og ingen kan bagefter sige hvorfor.
 */
void test_energi_inverterskift(void)
{
    zs_energi_basis_t b;
    float ud[ZS_E_ANTAL];
    bool  har[ZS_E_ANTAL];
    int8_t t_ud = -1;

    float  t[ZS_E_ANTAL] = { 1000.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    bool   h[ZS_E_ANTAL] = { true, true, true, true, true };

    ZS_SUITE("Timeenergi: udgangspunktet hører til ÉN inverter");

    memset(&b, 0, sizeof(b));

    /* En time hos inverter A. */
    CHECK("første måling hos A sætter udgangspunktet",
          zs_energi_tik(&b, t, h, "11111111", 8, 20000, ud, har, &t_ud)
          == ZS_ENERGI_FOERSTE);
    CHECK_STR("og serienummeret er gemt med", b.serienr, "11111111");

    float t2[ZS_E_ANTAL] = { 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    CHECK("en time senere hos A kan regnes",
          zs_energi_tik(&b, t2, h, "11111111", 9, 20000, ud, har, &t_ud)
          == ZS_ENERGI_KLAR);
    CHECK_F("og giver de 3000 Wh", ud[ZS_E_PRODUCERET], 3000.0f, 0.1);

    /*
     * DEN FARLIGE. Nu sidder der en anden inverter, og dens livstæller
     * står meget højere, fordi anlægget er ældre. Uden bindingen ville
     * den time blive sendt som 996 kWh.
     */
    float stor[ZS_E_ANTAL] = { 1000000.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    zs_energi_t r = zs_energi_tik(&b, stor, h, "99999999", 10, 20000,
                                  ud, har, &t_ud);
    CHECK("en ANDEN inverter giver ikke en time", r != ZS_ENERGI_KLAR);
    CHECK("den siger at inverteren er ny", r == ZS_ENERGI_NY_INVERTER);
    CHECK_STR("og udgangspunktet er nu den nyes", b.serienr, "99999999");

    /* Og timen efter kan regnes igen, nu paa den nyes egen taeller. */
    float stor2[ZS_E_ANTAL] = { 1002500.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    CHECK("timen efter kan regnes på den nye",
          zs_energi_tik(&b, stor2, h, "99999999", 11, 20000, ud, har, &t_ud)
          == ZS_ENERGI_KLAR);
    CHECK_F("og giver de 2500 Wh", ud[ZS_E_PRODUCERET], 2500.0f, 0.1);

    /* Tilbage til A igen, fx fordi kunden skiftede tilbage i
     * indstillingerne. Ogsaa det skal starte forfra. */
    CHECK("tilbage til A starter også forfra",
          zs_energi_tik(&b, t, h, "11111111", 12, 20000, ud, har, &t_ud)
          == ZS_ENERGI_NY_INVERTER);

    ZS_SUITE("Timeenergi: uden et serienummer regnes der ikke");

    /*
     * Serienummeret laeses ved hver forbindelse, saa er det tomt, er der
     * noget galt. Saa springer vi timen over i stedet for at tilskrive
     * den en inverter vi ikke kan navngive.
     */
    memset(&b, 0, sizeof(b));
    CHECK("tomt serienummer afvises",
          zs_energi_tik(&b, t, h, "", 8, 20000, ud, har, &t_ud)
          == ZS_ENERGI_ARG);
    CHECK("og der blev ikke sat noget udgangspunkt", !b.gyldig);
    CHECK("NULL som serienummer afvises",
          zs_energi_tik(&b, t, h, NULL, 8, 20000, ud, har, &t_ud)
          == ZS_ENERGI_ARG);
    CHECK("stadig intet udgangspunkt", !b.gyldig);

    /*
     * Et serienummer der er praecis saa langt som der er plads til. Her
     * falder en kopi der ikke afslutter med nul, eller som klipper ét
     * tegn for lidt.
     */
    char lang[ZS_ENERGI_SERIENR_MAX];
    memset(lang, 'A', sizeof(lang) - 1);
    lang[sizeof(lang) - 1] = '\0';
    memset(&b, 0, sizeof(b));
    CHECK("et serienummer der fylder feltet helt",
          zs_energi_tik(&b, t, h, lang, 8, 20000, ud, har, &t_ud)
          == ZS_ENERGI_FOERSTE);
    CHECK_STR("gemmes helt, og er afsluttet", b.serienr, lang);
    CHECK("og genkendes som den samme næste time",
          zs_energi_tik(&b, t2, h, lang, 9, 20000, ud, har, &t_ud)
          == ZS_ENERGI_KLAR);

    /* Og ét tegn galt helt ude i enden skal vaere et ANDET serienummer.
     * Det er her en sammenligning der stopper for tidligt falder. */
    char naesten[ZS_ENERGI_SERIENR_MAX];
    memcpy(naesten, lang, sizeof(naesten));
    naesten[sizeof(naesten) - 2] = 'B';
    CHECK("ét tegn galt i enden er en anden inverter",
          zs_energi_tik(&b, t, h, naesten, 10, 20000, ud, har, &t_ud)
          == ZS_ENERGI_NY_INVERTER);
}

/*
 * En tæller der forsvinder og kommer igen.
 *
 * HVORFOR DEN HER PROEVE FINDES.
 *
 * En mutationstest aendrede "||" til "&&" i vagten der kraever at BEGGE
 * tidspunkter har taelleren, og ikke én af de mange proever sagde fra.
 *
 * Med den aendring: havde vi IKKE taelleren sidste time og har den NU, saa
 * staar der nul i udgangspunktet, og forskellen bliver hele inverterens
 * livstaeller. Tusinde kilowattimer sendt som én times energi, gemt paa
 * serveren for evigt.
 *
 * Og det er ikke teori. Elmaaleren sidder paa sin EGEN Modbus-unit, saa
 * den kan vaere uden svar et oejeblik mens inverteren svarer fint. Saa
 * mangler koebt og solgt i én aflaesning og er der igen i den naeste.
 * Det sker paa et rigtigt anlaeg.
 */
void test_energi_taeller_forsvinder(void)
{
    zs_energi_basis_t b;
    float ud[ZS_E_ANTAL];
    bool  har[ZS_E_ANTAL];
    int8_t t_ud = -1;

    /* Alle fem til stede, og alle fem mangler. */
    bool alle[ZS_E_ANTAL]  = { true, true, true, true, true };
    bool uden_maaler[ZS_E_ANTAL] = { true, false, false, true, true };

    float lavt[ZS_E_ANTAL]  = { 1000.0f,  2000.0f,  3000.0f, 100.0f, 200.0f };
    float hoejt[ZS_E_ANTAL] = { 4000.0f,  5000.0f,  6000.0f, 400.0f, 500.0f };

    ZS_SUITE("Timeenergi: elmåleren forsvinder og kommer igen");

    /* Time 8: elmaaleren svarer ikke, saa koebt og solgt mangler. */
    memset(&b, 0, sizeof(b));
    CHECK("udgangspunkt sat uden elmåler",
          zs_energi_tik(&b, lavt, uden_maaler, "31234567", 8, 20000,
                        ud, har, &t_ud) == ZS_ENERGI_FOERSTE);
    CHECK("og vi husker at vi ikke havde dem", !b.har[ZS_E_KOEBT]);

    /*
     * DEN FARLIGE. Time 9: elmaaleren er tilbage, og dens livstaeller
     * staar paa 5000 Wh. Uden vagten ville de 5000 blive sendt som denne
     * times forbrug.
     */
    CHECK("timen kan regnes",
          zs_energi_tik(&b, hoejt, alle, "31234567", 9, 20000,
                        ud, har, &t_ud) == ZS_ENERGI_KLAR);
    CHECK("produktionen kan regnes, den var der hele tiden",
          har[ZS_E_PRODUCERET]);
    CHECK_F("og den er de 3000 Wh", ud[ZS_E_PRODUCERET], 3000.0f, 0.1);

    CHECK("købt SENDES IKKE, for vi havde den ikke sidste time",
          !har[ZS_E_KOEBT]);
    CHECK("og der står ikke hele livstælleren i den", ud[ZS_E_KOEBT] == 0.0f);
    CHECK("solgt sendes heller ikke", !har[ZS_E_SOLGT]);
    CHECK("og heller ikke der", ud[ZS_E_SOLGT] == 0.0f);

    /* Og timen EFTER, hvor vi har haft den i begge ender, skal den med. */
    float hoejere[ZS_E_ANTAL] = { 7000.0f, 5500.0f, 6200.0f, 700.0f, 800.0f };
    CHECK("timen efter kan den regnes",
          zs_energi_tik(&b, hoejere, alle, "31234567", 10, 20000,
                        ud, har, &t_ud) == ZS_ENERGI_KLAR);
    CHECK("nu er købt med", har[ZS_E_KOEBT]);
    CHECK_F("og den er de 500 Wh", ud[ZS_E_KOEBT], 500.0f, 0.1);

    ZS_SUITE("Timeenergi: en tæller der forsvinder");

    /* Den anden vej: havde den, har den ikke. Mindre slem, men den skal
     * ogsaa springes over og ikke sendes som nul. */
    memset(&b, 0, sizeof(b));
    CHECK("udgangspunkt med alle fem",
          zs_energi_tik(&b, lavt, alle, "31234567", 8, 20000,
                        ud, har, &t_ud) == ZS_ENERGI_FOERSTE);
    CHECK("timen kan regnes selv om måleren faldt ud",
          zs_energi_tik(&b, hoejt, uden_maaler, "31234567", 9, 20000,
                        ud, har, &t_ud) == ZS_ENERGI_KLAR);
    CHECK("produktionen er med", har[ZS_E_PRODUCERET]);
    CHECK("men købt sendes ikke som nul", !har[ZS_E_KOEBT]);
    CHECK("og solgt heller ikke", !har[ZS_E_SOLGT]);
    CHECK("og batteriet er stadig med", har[ZS_E_BAT_IND] && har[ZS_E_BAT_UD]);
}
