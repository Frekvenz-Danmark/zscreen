/*
 * zScreen - hvilken time er det lige nu.
 *
 * Ligger for sig selv fordi resten af zs_price.c haenter over
 * internettet og trAEkker hele HTTP-laget med ind. Den her bruger kun
 * time.h, og saa kan baade den og demoen testes paa en almindelig
 * maskine.
 */

/*
 * localtime_r er POSIX, ikke C-standard. Uden det her er den skjult paa
 * glibc naar der oversaettes med -std=c11, og saa bygger filen paa Mac
 * men ikke paa Linux. Linjen skal staa FOER enhver include.
 */
#define _POSIX_C_SOURCE 200809L

#include "zs_price.h"

#include <time.h>

void zs_price_update_now(zs_price_day_t *d)
{
    if (d == NULL || !d->ok) {
        return;
    }
    d->nu = -1;

    time_t t = time(NULL);
    if (t < 1700000000) {
        return;   /* uret er ikke sat, saa vi ved ikke hvad klokken er */
    }
    struct tm lt;
    localtime_r(&t, &lt);

    for (uint8_t i = 0; i < d->antal; i++) {
        if (d->timer[i].hour == (uint8_t)lt.tm_hour) {
            d->nu = (int8_t)i;
            return;
        }
    }
}
