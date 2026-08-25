#include "zs_format.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

char *zs_fmt_da_decimal(char *s)
{
    if (s == NULL) {
        return s;
    }
    for (char *p = s; *p != '\0'; p++) {
        if (*p == '.') {
            *p = ',';
        }
    }
    return s;
}

void zs_fmt_none(zs_num_t *out)
{
    if (out == NULL) {
        return;
    }
    /* Almindelig bindestreg, ikke tankestreg. Den er smallere og
     * saetter pænere under et stort tal-font. */
    snprintf(out->value, sizeof(out->value), "-");
    out->unit[0] = '\0';
}

void zs_fmt_power(float watts, zs_num_t *out)
{
    if (out == NULL) {
        return;
    }
    if (isnan(watts) || isinf(watts)) {
        zs_fmt_none(out);
        return;
    }

    float w = fabsf(watts);

    /*
     * Graensen ligger paa 999,5 og ikke paa 1000.
     *
     * Med 1000 som graense ville 999,6 W blive rundet til "1000 W",
     * og saa staar der fire cifre i et felt der er sat op til tre,
     * lige indtil vaerdien kryber over 1000 og hopper til "1,0 kW".
     * Med 999,5 gaar den direkte fra "999 W" til "1,0 kW".
     */
    if (w < 999.5f) {
        snprintf(out->value, sizeof(out->value), "%ld", lroundf(w));
        snprintf(out->unit, sizeof(out->unit), "W");
        return;
    }

    /*
     * Vi runder selv og saetter tallet sammen i haanden i stedet for at
     * lade "%.1f" om det.
     *
     * printf runder nemlig halve vaerdier til naermeste LIGE ciffer.
     * 4250 W er praecis 4,25 kW, og saa skriver "%.1f" 4,2 fordi 2 er
     * lige. 4350 W bliver derimod til 4,4. Samme maaling, to forskellige
     * regler, og et tal der ikke stemmer med SolarWeb. lroundf runder
     * altid halve vaerdier vaek fra nul, som folk forventer.
     */
    long tenths = lroundf(w / 100.0f);   /* antal tiendedele kW */

    if (tenths < 1000) {
        snprintf(out->value, sizeof(out->value), "%ld,%ld", tenths / 10, tenths % 10);
    } else {
        /* Over 100 kW er decimalet stoej, og det er ogsaa den eneste
         * stoerrelse der ellers ville fylde fem tegn i et felt sat op
         * til fire. */
        snprintf(out->value, sizeof(out->value), "%ld", lroundf(w / 1000.0f));
    }
    snprintf(out->unit, sizeof(out->unit), "kW");
}

void zs_fmt_percent(float pct, zs_num_t *out)
{
    if (out == NULL) {
        return;
    }
    if (isnan(pct) || isinf(pct)) {
        zs_fmt_none(out);
        return;
    }
    if (pct < 0.0f)   { pct = 0.0f; }
    if (pct > 100.0f) { pct = 100.0f; }
    snprintf(out->value, sizeof(out->value), "%ld", lroundf(pct));
    snprintf(out->unit, sizeof(out->unit), "%%");
}

void zs_fmt_kroner(float kr, char *ud, size_t n)
{
    if (ud == NULL || n == 0) {
        return;
    }
    if (isnan(kr) || isinf(kr)) {
        snprintf(ud, n, "-");
        return;
    }
    long oere = lroundf(kr * 100.0f);
    const char *fortegn = "";
    if (oere < 0) {
        fortegn = "-";
        oere = -oere;
    }
    snprintf(ud, n, "%s%ld,%02ld", fortegn, oere / 100, oere % 100);
}

void zs_fmt_energy_wh(double wh, zs_num_t *out)
{
    if (out == NULL) {
        return;
    }
    if (isnan(wh) || isinf(wh)) {
        zs_fmt_none(out);
        return;
    }
    /* Samme haandrulning som i zs_fmt_power, og af samme grund. */
    double a = fabs(wh);
    if (a < 999950.0) {
        long tenths = lround(a / 100.0);
        snprintf(out->value, sizeof(out->value), "%ld,%ld", tenths / 10, tenths % 10);
        snprintf(out->unit, sizeof(out->unit), "kWh");
    } else {
        long tenths = lround(a / 100000.0);
        snprintf(out->value, sizeof(out->value), "%ld,%ld", tenths / 10, tenths % 10);
        snprintf(out->unit, sizeof(out->unit), "MWh");
    }
}
