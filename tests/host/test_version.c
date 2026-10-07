/*
 * Test af versionssammenligning.
 *
 * Den her funktion afgoer om en skaerm paa en vaeg henter ny firmware.
 * To fejl er mulige, og begge er slemme:
 *
 *   for streng   skaermen opdaterer aldrig, og en fejlrettelse naar
 *                aldrig ud til kunden
 *   for slap     skaermen henter den samme udgave igen og igen, eller
 *                ruller tilbage til noget aeldre
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_version.h"

void test_version(void)
{
    ZS_SUITE("Versionsnumre: kan de laeses");

    {
        unsigned v[3];
        struct { const char *s; bool ok; unsigned a, b, c; const char *hvad; } t[] = {
            { "1.2.3",     true,  1, 2, 3,  "almindelig" },
            { "v1.2.3",    true,  1, 2, 3,  "med v foran" },
            { "V1.2.3",    true,  1, 2, 3,  "med stort V" },
            { "0.0.0",     true,  0, 0, 0,  "nuller" },
            { "0.10.0",    true,  0, 10, 0, "to cifre i midten" },
            { "10.20.30",  true,  10, 20, 30, "to cifre alle steder" },

            /* De her SKAL afvises. Teksten kommer fra et maerke paa
             * GitHub, altsaa udefra. */
            { "1.2",       false, 0, 0, 0,  "kun to led" },
            { "1.2.3.4",   false, 0, 0, 0,  "fire led" },
            { "1.2.3-rc1", false, 0, 0, 0,  "forhaandsudgave" },
            { "1.2.x",     false, 0, 0, 0,  "bogstav i et led" },
            { " 1.2.3",    false, 0, 0, 0,  "mellemrum foran" },
            { "1.2.3 ",    false, 0, 0, 0,  "mellemrum bagved" },
            { "-1.0.0",    false, 0, 0, 0,  "minus foran" },
            { "+1.0.0",    false, 0, 0, 0,  "plus foran" },
            { "1..3",      false, 0, 0, 0,  "tomt led" },
            { "..",        false, 0, 0, 0,  "kun punktummer" },
            { "",          false, 0, 0, 0,  "tom tekst" },
            { "v",         false, 0, 0, 0,  "kun et v" },
            { "999999.0.0",false, 0, 0, 0,  "urimeligt stort tal" },
        };
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            bool ok = zs_version_parse(t[i].s, v);
            CHECK(t[i].hvad, ok == t[i].ok);
            if (t[i].ok && ok) {
                CHECK(t[i].hvad,
                      v[0] == t[i].a && v[1] == t[i].b && v[2] == t[i].c);
            }
        }
        CHECK("NULL er ikke en version", !zs_version_parse(NULL, v));
    }

    ZS_SUITE("Versionsnumre: hvad er nyest");

    {
        struct { const char *a, *b; int vent; const char *hvad; } t[] = {
            { "0.2.0", "0.1.0",  1, "hoejere rettelsestal er nyere" },
            { "0.1.0", "0.2.0", -1, "og omvendt" },
            { "0.1.0", "0.1.0",  0, "ens er ens" },
            { "v0.2.0", "0.2.0", 0, "v foran aendrer ingenting" },

            /*
             * Den vigtigste i hele filen.
             *
             * 0.10.0 er nyere end 0.9.0, men staar FOER den
             * alfabetisk. Sammenlignede vi teksterne, ville hver eneste
             * skaerm i marken staa paa 0.9.0 for altid, og ingen ville
             * opdage det foer nogen kiggede paa en af dem.
             */
            { "0.10.0", "0.9.0",  1, "0.10.0 er nyere end 0.9.0" },
            { "0.9.0",  "0.10.0", -1, "og 0.9.0 er aeldre end 0.10.0" },
            { "1.0.0",  "0.99.99", 1, "stoerste led vejer tungest" },
            { "0.1.10", "0.1.9",   1, "ogsaa i sidste led" },
            { "0.10.0", "0.10.0", 0, "lige store giver nul" },
            { "v0.10.0", "0.9.0", 1, "et v foran aendrer ingenting" },

            /* Kan én af dem ikke laeses, opdaterer vi ikke. */
            { "hvadsomhelst", "0.1.0", -1, "ulaeselig ny version" },
            { "0.2.0", "hvadsomhelst", -1, "ulaeselig koerende version" },
            { "-1.0.0", "0.1.0",  -1, "minus regnes ikke som kaempestort" },
        };
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            CHECK(t[i].hvad, zs_version_cmp(t[i].a, t[i].b) == t[i].vent);
        }
        CHECK("NULL opdaterer ikke", zs_version_cmp(NULL, "0.1.0") == -1);
        CHECK("NULL begge veje",     zs_version_cmp("0.1.0", NULL) == -1);
    }

    ZS_SUITE("Versionsnumre: v foran");

    {
        CHECK_STR("v fjernes", zs_version_strip_v("v1.2.3"), "1.2.3");
        CHECK_STR("stort V ogsaa", zs_version_strip_v("V1.2.3"), "1.2.3");
        CHECK_STR("uden v roeres den ikke", zs_version_strip_v("1.2.3"), "1.2.3");
    }
}

/*
 * Maalversionen kommer fra SERVEREN og ender inde i en URL.
 *
 * Det gOEr den til det eneste sted hvor en tekst udefra bliver til en
 * adresse skaermen henter firmware fra. Slipper en skraastreg eller et
 * punktum-punktum igennem, kan den peges et andet sted hen. Underskriften
 * ville stadig redde os, for en fremmed firmware bliver afvist, men en
 * skaerm der henter fra et forkert sted igen og igen er ogsaa en fejl.
 *
 * Derfor er den streng: noejagtig tal.tal.tal, med et valgfrit v foran.
 */
void test_version_tag(void)
{
    ZS_SUITE("Målversionen fra serveren");

    CHECK("almindelig version", zs_version_tag_ok("0.9.0") == true);
    CHECK("med v foran, som mærket på GitHub", zs_version_tag_ok("v0.9.0") == true);
    CHECK("stort V virker også", zs_version_tag_ok("V1.2.3") == true);
    CHECK("flercifret", zs_version_tag_ok("10.20.30") == true);
    CHECK("nuller", zs_version_tag_ok("0.0.0") == true);

    CHECK("tom afvises", zs_version_tag_ok("") == false);
    CHECK("NULL afvises", zs_version_tag_ok(NULL) == false);
    CHECK("kun v afvises", zs_version_tag_ok("v") == false);
    CHECK("to tal er ikke nok", zs_version_tag_ok("1.2") == false);
    CHECK("fire tal er for mange", zs_version_tag_ok("1.2.3.4") == false);
    CHECK("bogstaver afvises", zs_version_tag_ok("1.2.3a") == false);
    CHECK("mellemrum afvises", zs_version_tag_ok("1.2.3 ") == false);

    /* Det der kunne pille ved adressen. */
    CHECK("skråstreg afvises", zs_version_tag_ok("1.2.3/evil") == false);
    CHECK("punktum-punktum afvises", zs_version_tag_ok("../1.2.3") == false);
    CHECK("to punktummer i træk afvises", zs_version_tag_ok("1..3") == false);
    CHECK("punktum forrest afvises", zs_version_tag_ok(".1.2") == false);
    CHECK("punktum bagerst afvises", zs_version_tag_ok("1.2.") == false);
    CHECK("spørgsmålstegn afvises", zs_version_tag_ok("1.2.3?a=b") == false);
    CHECK("kolon afvises", zs_version_tag_ok("1.2.3:80") == false);
    CHECK("procent afvises", zs_version_tag_ok("1.2.%2e") == false);

    /* En uendelig lang streng maa ikke kunne fylde feltet op. */
    char lang[200];
    memset(lang, '1', sizeof(lang) - 1);
    lang[sizeof(lang) - 1] = '\0';
    CHECK("alt for lang afvises", zs_version_tag_ok(lang) == false);
    CHECK("for mange cifre i ét tal afvises", zs_version_tag_ok("1234567.1.1") == false);

    /*
     * De angreb teksten kunne baere, hvis nogen fik fat i dashboardet.
     *
     * Maalversionen kommer fra serveren og ender i en URL, saa den her
     * funktion ER graensen. Vognretur og linjeskift staar foerst, fordi
     * det er den der kunne lave to HTTP-headere ud af én linje.
     */
    CHECK("vognretur og linjeskift afvises",
          zs_version_tag_ok("1.2.3\r\nHost: evil") == false);
    CHECK("linjeskift alene afvises", zs_version_tag_ok("1.2.3\nX") == false);
    CHECK("bagstreg afvises", zs_version_tag_ok("1.2.3\\evil") == false);
    CHECK("en hel URL afvises", zs_version_tag_ok("http://evil/x.bin") == false);
    CHECK("tabulator afvises", zs_version_tag_ok("1.2.3\t") == false);
    CHECK("et tegn over 127 afvises", zs_version_tag_ok("1.2.3\xff") == false);
    CHECK("semikolon afvises", zs_version_tag_ok("1.2.3;rm -rf") == false);
    CHECK("havelaage afvises", zs_version_tag_ok("1.2.3#frag") == false);
    CHECK("minus foran afvises", zs_version_tag_ok("-1.2.3") == false);
    CHECK("plus foran afvises", zs_version_tag_ok("+1.2.3") == false);
    CHECK("to v foran afvises", zs_version_tag_ok("vv1.2.3") == false);

    /* Og graensen den anden vej: fem cifre i hvert tal er lovligt. */
    CHECK("fem cifre i hvert tal er lovligt",
          zs_version_tag_ok("99999.99999.99999") == true);
    CHECK("seks cifre er ikke", zs_version_tag_ok("999999.1.1") == false);
}

