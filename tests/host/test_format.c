/*
 * Test af tal-formatering paa dansk.
 *
 * Det ser ud som pynt, men det er det ikke: en skaerm der viser
 * "4.2 kW" med punktum, eller hopper mellem "1000 W" og "1,0 kW" for
 * den samme maaling, ligner et halvfaerdigt produkt.
 */

#include "zs_test.h"
#include "../../firmware/main/app/zs_format.h"

#include <math.h>
#include <string.h>

static void chk_power(const char *what, float w, const char *val, const char *unit)
{
    zs_num_t n;
    zs_fmt_power(w, &n);
    zs_tests_run++;
    if (strcmp(n.value, val) == 0 && strcmp(n.unit, unit) == 0) {
        ZS_PASS(what);
    } else {
        ZS_FAIL("%s: fik \"%s %s\", forventede \"%s %s\"", what, n.value, n.unit, val, unit);
    }
}

void test_format(void)
{
    ZS_SUITE("Tal paa dansk: effekt");

    chk_power("0 W",                        0.0f,     "0",   "W");
    chk_power("1 W",                        1.0f,     "1",   "W");
    chk_power("850 W",                    850.0f,   "850",   "W");
    chk_power("999 W",                    999.0f,   "999",   "W");

    /* Graensen. Med 1000 som skillelinje ville 999,6 blive til
     * "1000 W", altsaa fire cifre i et tre-cifret felt. */
    chk_power("999,4 W bliver 999 W",     999.4f,   "999",   "W");
    chk_power("999,6 W springer til kW",  999.6f,   "1,0",  "kW");
    chk_power("1000 W",                  1000.0f,   "1,0",  "kW");

    chk_power("4200 W",                  4200.0f,   "4,2",  "kW");
    chk_power("4249 W runder ned",       4249.0f,   "4,2",  "kW");
    chk_power("4250 W runder op",        4250.0f,   "4,3",  "kW");
    chk_power("12345 W",                12345.0f,  "12,3",  "kW");

    /* Over 100 kW dropper vi decimalet. Ellers fylder tallet fem tegn. */
    chk_power("99900 W",                99900.0f,  "99,9",  "kW");
    chk_power("100000 W",              100000.0f,   "100",  "kW");
    chk_power("250000 W",              250000.0f,   "250",  "kW");

    /* Fortegnet bliver vist med ord under tallet, ikke med et minus. */
    chk_power("-850 W vises som 850 W",  -850.0f,   "850",   "W");
    chk_power("-4200 W vises som 4,2 kW", -4200.0f, "4,2",  "kW");

    {
        zs_num_t n;
        zs_fmt_power(NAN, &n);
        CHECK_STR("NaN giver en streg", n.value, "-");
        zs_fmt_power(INFINITY, &n);
        CHECK_STR("uendelig giver en streg", n.value, "-");
    }

    {
        zs_num_t n;
        zs_fmt_power(4200.0f, &n);
        CHECK("decimaltegnet er komma, ikke punktum", strchr(n.value, '.') == NULL);
        CHECK("og der ER et komma",                   strchr(n.value, ',') != NULL);
    }

    ZS_SUITE("Tal paa dansk: procent og energi");

    {
        zs_num_t n;
        zs_fmt_percent(78.0f, &n);
        CHECK_STR("78 procent", n.value, "78");
        CHECK_STR("enheden er procent", n.unit, "%");

        zs_fmt_percent(77.6f, &n);
        CHECK_STR("77,6 runder til 78", n.value, "78");

        zs_fmt_percent(-5.0f, &n);
        CHECK_STR("negativ klippes til 0", n.value, "0");

        zs_fmt_percent(105.0f, &n);
        CHECK_STR("over 100 klippes til 100", n.value, "100");

        zs_fmt_percent(NAN, &n);
        CHECK_STR("NaN giver en streg", n.value, "-");
    }

    {
        zs_num_t n;
        zs_fmt_energy_wh(12500.0, &n);
        CHECK_STR("12500 Wh er 12,5", n.value, "12,5");
        CHECK_STR("i kWh", n.unit, "kWh");

        zs_fmt_energy_wh(2500000.0, &n);
        CHECK_STR("2,5 millioner Wh er 2,5", n.value, "2,5");
        CHECK_STR("i MWh", n.unit, "MWh");

        zs_fmt_energy_wh(NAN, &n);
        CHECK_STR("NaN giver en streg", n.value, "-");
    }

    ZS_SUITE("Tal på dansk: tal der ikke er maalinger");

    /*
     * Et forvansket register kan give et tal saa stort at lround ikke
     * kan holde det i en long. Det er udefineret opfoersel, ikke bare
     * et grimt tal, og strengen ville desuden blive skaaret over midt i
     * saa den lignede en rigtig maaling. Vi viser ingen data i stedet.
     *
     * Vi tjekker ogsaa at der altid er plads i value[12]. Bliver et loft
     * haevet uden omtanke, faejler den her test foer nogen ser et
     * halvskrevet tal paa vaeggen.
     */
    {
        zs_num_t n;

        zs_fmt_power(1.0e30f, &n);
        CHECK_STR("urealistisk effekt giver en streg", n.value, "-");

        zs_fmt_energy_wh(1.0e30, &n);
        CHECK_STR("urealistisk energi giver en streg", n.value, "-");

        zs_fmt_energy_wh(-1.0e30, &n);
        CHECK_STR("ogsaa negativ", n.value, "-");

        char b[16];
        zs_fmt_kroner(1.0e30f, b, sizeof(b));
        CHECK_STR("urealistisk pris giver en streg", b, "-");

        /* Lige under loftet skal stadig give et tal, ikke en streg. */
        zs_fmt_power(9.0e8f, &n);
        CHECK("lige under effektloftet er stadig et tal", n.value[0] != '-');
        CHECK("og der er plads i feltet", strlen(n.value) < sizeof(n.value));

        zs_fmt_energy_wh(9.0e11, &n);
        CHECK("lige under energiloftet er stadig et tal", n.value[0] != '-');
        CHECK("og der er plads i feltet", strlen(n.value) < sizeof(n.value));
    }

    ZS_SUITE("Tal på dansk: kroner");

    {
        char b[16];
        struct { float kr; const char *vent; const char *hvad; } t[] = {
            { 1.58f,   "1,58",  "almindelig pris" },
            { 0.62f,   "0,62",  "under en krone" },
            { 0.0f,    "0,00",  "nul" },
            { 12.05f,  "12,05", "to cifre foran kommaet" },
            { 1.005f,  "1,01",  "runder halve op" },
            /*
             * De fire vigtige. Negative timepriser forekommer flere
             * gange om aaret naar det blaeser, og regner man bare
             * oere/100 forsvinder minusset for alt mellem -1 og 0,
             * fordi heltalsdivisionen giver nul.
             */
            { -0.05f,  "-0,05", "lille negativ pris beholder minus" },
            { -0.99f,  "-0,99", "knap en krone negativ beholder minus" },
            { -1.00f,  "-1,00", "praecis minus én krone" },
            { -2.34f,  "-2,34", "stoerre negativ pris" },
        };
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            zs_fmt_kroner(t[i].kr, b, sizeof(b));
            CHECK_STR(t[i].hvad, b, t[i].vent);
        }
        zs_fmt_kroner(NAN, b, sizeof(b));
        CHECK_STR("NaN giver en streg", b, "-");

        /* Maa ikke skrive uden for en lille buffer. */
        char lille[4];
        zs_fmt_kroner(-123.45f, lille, sizeof(lille));
        CHECK("for lille buffer nul-afsluttes",
              lille[sizeof(lille) - 1] == '\0');
    }

    {
        zs_num_t n;
        zs_fmt_none(&n);
        CHECK_STR("ingen data er en streg", n.value, "-");
        CHECK_STR("uden enhed", n.unit, "");
    }

    {
        char buf[] = "4.2";
        CHECK_STR("punktum bliver til komma", zs_fmt_da_decimal(buf), "4,2");
    }
}
