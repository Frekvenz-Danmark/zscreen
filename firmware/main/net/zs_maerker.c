/*
 * zScreen - tabellen over maerker. Se zs_maerker.h for hvorfor.
 *
 * Den her fil er med vilje kedelig: den indeholder DATA og én
 * sammenligning, ingen logik. Skal der et maerke mere paa, er det en
 * raekke i tabellen og ikke en ny gren i afkodningen.
 */

#include "zs_maerker.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define ANTAL(a) (sizeof(a) / sizeof((a)[0]))

/*
 * MAERKERNE.
 *
 * Fronius er det eneste vi har papir paa. Deres Modbus-manual har
 * nummeret 42,0410,2649, og baade fejlbittene og opdelingen af
 * DC-kanalerne staar der sort paa hvidt.
 *
 * Her maa der IKKE staa et maerke vi ikke har dokumentationen til. En
 * opdigtet bittekst er en forkert fejl skrevet med fuld sikkerhed, og
 * den staar paa en vaeg hos en kunde.
 */
static const zs_maerke_t MAERKER[] = {
    {
        .maerke        = "Fronius",
        .dc_layout     = ZS_DC_BATTERI_SIDST,
        .evtvnd1       = zs_evtvnd1_bits, .n_evtvnd1 = ANTAL(zs_evtvnd1_bits),
        .evtvnd2       = zs_evtvnd2_bits, .n_evtvnd2 = ANTAL(zs_evtvnd2_bits),
        .evtvnd3       = zs_evtvnd3_bits, .n_evtvnd3 = ANTAL(zs_evtvnd3_bits),
        .kode_praefiks = "Fronius-kode",
    },
};

/*
 * Den vi falder tilbage paa.
 *
 * Ingen tabeller og ingen opdeling. Alt det SunSpec selv definerer
 * virker stadig: effekt, spaending, driftstilstand, Evt1 og DCEvt. Kun
 * producentens egne felter vises raat, og det er det rigtige: vi ved
 * ikke hvad bit 1 betyder paa en inverter vi ikke har papir paa.
 */
static const zs_maerke_t UKENDT = {
    .maerke        = "",
    .dc_layout     = ZS_DC_UKENDT,
    .evtvnd1       = NULL, .n_evtvnd1 = 0,
    .evtvnd2       = NULL, .n_evtvnd2 = 0,
    .evtvnd3       = NULL, .n_evtvnd3 = 0,
    .kode_praefiks = NULL,
};

/* Sammenligner uden at skelne mellem store og smaa bogstaver.
 *
 * Fronius skriver selv baade "Fronius" og "FRONIUS" alt efter
 * firmwareudgave, og vi har set begge dele. Og vi leder efter maerket
 * SOM EN DEL af feltet: nogle skriver "Fronius International GmbH". */
static bool indeholder_uden_forskel(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return false;
    }
    size_t n = strlen(needle);
    size_t h = strlen(haystack);
    if (h < n) {
        return false;
    }
    for (size_t i = 0; i + n <= h; i++) {
        size_t k = 0;
        while (k < n) {
            char a = haystack[i + k];
            char b = needle[k];
            if (a >= 'A' && a <= 'Z') { a = (char)(a - 'A' + 'a'); }
            if (b >= 'A' && b <= 'Z') { b = (char)(b - 'A' + 'a'); }
            if (a != b) { break; }
            k++;
        }
        if (k == n) {
            return true;
        }
    }
    return false;
}

const zs_maerke_t *zs_maerke_find(const char *manufacturer)
{
    if (manufacturer != NULL && manufacturer[0] != '\0') {
        for (size_t i = 0; i < ANTAL(MAERKER); i++) {
            if (indeholder_uden_forskel(manufacturer, MAERKER[i].maerke)) {
                return &MAERKER[i];
            }
        }
    }
    return &UKENDT;
}

size_t zs_maerke_antal(void)
{
    return ANTAL(MAERKER);
}

const zs_maerke_t *zs_maerke_nr(size_t i)
{
    return (i < ANTAL(MAERKER)) ? &MAERKER[i] : NULL;
}
