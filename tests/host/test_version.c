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
