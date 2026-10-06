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

int8_t zs_price_find_hour(const zs_price_day_t *d, int lokal_time, int offset_h)
{
    if (d == NULL || lokal_time < 0 || lokal_time > 23) {
        return -1;
    }

    /*
     * FOERSTE runde: kraev at BAADE klokketimen og forskydningen passer.
     *
     * Den nat sommertiden slutter staar klokken to to gange i filen, én
     * med forskydning 2 og én med 1. Uden forskydningen tog vi den
     * foerste begge gange.
     */
    for (uint8_t i = 0; i < d->antal; i++) {
        if (d->timer[i].hour == (uint8_t)lokal_time
            && d->timer[i].utc_offset_h != ZS_PRICE_OFFSET_UKENDT
            && d->timer[i].utc_offset_h == (int8_t)offset_h) {
            return (int8_t)i;
        }
    }

    /*
     * ANDEN runde: kun klokketimen.
     *
     * Her ender vi med data fra en kilde der ikke oplyser forskydningen,
     * og med gemte priser fra foer feltet fandtes. Saa opfoerer vi os
     * praecis som foer, i stedet for pludselig ikke at vise nogen pris.
     */
    for (uint8_t i = 0; i < d->antal; i++) {
        if (d->timer[i].hour == (uint8_t)lokal_time) {
            return (int8_t)i;
        }
    }
    return -1;
}

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
    struct tm lt, ut;
    localtime_r(&t, &lt);
    gmtime_r(&t, &ut);

    /*
     * Vores egen forskydning fra UTC, regnet ud af forskellen mellem
     * lokal tid og UTC.
     *
     * Vi bruger IKKE tm_gmtoff: den er en udvidelse, og den findes ikke
     * i ESP-IDF's newlib. Efterset, ikke antaget.
     *
     * Dagsforskellen skal med, ellers giver natten mellem to datoer 23
     * timers forskydning i stedet for minus én.
     */
    int offset_h = lt.tm_hour - ut.tm_hour;
    int dagdiff  = lt.tm_yday - ut.tm_yday;
    if (dagdiff == 1 || dagdiff < -1) {
        offset_h += 24;          /* lokalt er vi i morgen */
    } else if (dagdiff == -1 || dagdiff > 1) {
        offset_h -= 24;          /* lokalt er vi i gaar */
    }

    d->nu = zs_price_find_hour(d, lt.tm_hour, offset_h);
}
