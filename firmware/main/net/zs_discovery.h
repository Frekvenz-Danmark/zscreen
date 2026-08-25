/*
 * zScreen - find inverteren paa netvaerket.
 *
 * Kunden skal ikke skulle finde en IP-adresse frem. Skaermen leder
 * selv: den gaar hele undernettet igennem, ser efter hvem der lytter
 * paa Modbus-porten, og spoerger dem der goer om de taler SunSpec.
 *
 * Fremgangsmaade:
 *   1. Prøv den adresse vi kender i forvejen, hvis der er én. En
 *      skaerm der har koert i et aar og faar en ny IP fra routeren,
 *      skal ikke bruge otte sekunder paa at finde ud af det.
 *   2. Gaa .1 til .254 igennem og se hvem der lytter paa port 502.
 *      Mange adresser ad gangen, med kort taalmodighed, saa hele
 *      undernettet er klaret paa faa sekunder.
 *   3. Spoerg hver af dem om de taler SunSpec, og laes fabrikat,
 *      model og serienummer.
 *
 * Vi bruger IKKE mDNS. Fronius' Gen24 annoncerer sig ikke paalideligt,
 * og et opslag der somme tider virker er vaerre end ét der aldrig
 * goer: saa bygger man en fejlsoegning oven paa noget ustabilt.
 */

#ifndef ZS_DISCOVERY_H
#define ZS_DISCOVERY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "zs_fronius.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZS_DISCOVERY_MAX  8

typedef struct {
    char         ip[16];
    zs_fr_info_t info;
} zs_found_t;

/*
 * Kaldes undervejs saa brugeren kan se at der sker noget.
 *   done   hvor mange adresser der er proevet
 *   total  hvor mange der er i alt
 *   found  hvor mange invertere der er fundet indtil videre
 * Maa vaere NULL.
 */
typedef void (*zs_discovery_progress_fn)(void *ctx, int done, int total, int found);

/*
 * Leder efter invertere. Blokerer, saa den skal koeres fra en egen
 * opgave og ikke fra LVGL's.
 *
 * subnet er fx "192.168.1.0". prefer maa vaere NULL, ellers en adresse
 * der proeves foerst.
 *
 * Returnerer antallet der blev fundet, eller -1 ved fejl.
 */
int zs_discovery_scan(const char *subnet, const char *prefer,
                      zs_found_t *out, size_t max,
                      zs_discovery_progress_fn progress, void *ctx);

/* Afbryder en scanning der er i gang. Bruges naar brugeren gaar
 * tilbage, saa skaermen ikke skal vente paa at den bliver faerdig. */
void zs_discovery_abort(void);

/*
 * Blev den sidste soegning afbrudt?
 *
 * Kalderen SKAL spoerge om det foer den viser resultatet. Ellers
 * bliver brugeren revet tilbage til listen over invertere et halvt
 * sekund efter at have trykket paa tilbage, fordi soegningen foerst
 * naaede at blive faerdig bagefter.
 */
bool zs_discovery_was_aborted(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_DISCOVERY_H */
