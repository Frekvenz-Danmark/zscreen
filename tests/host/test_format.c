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
