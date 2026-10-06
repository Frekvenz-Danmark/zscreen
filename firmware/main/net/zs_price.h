/*
 * zScreen - elprisen i dag.
 *
 * Priserne hentes fra elprisenligenu.dk, som udgiver dem som en
 * statisk JSON-fil pr. dag og prisområde:
 *
 *   https://www.elprisenligenu.dk/api/v1/prices/2026/08-25_DK2.json
 *
 * Frekvenz har licens til dataene, saa der skal ikke staa en
 * kildeangivelse paa skaermen.
 *
 * VIGTIGT: det er SPOTPRISEN, altsaa den raa elpris paa boersen. Den
 * er IKKE det kunden betaler: oven i kommer transport, afgifter og
 * moms, som typisk mere end fordobler den. Derfor staar ordet
 * "spotpris" baade som overskrift og som en linje nederst paa siden.
 * Uden det ville folk tro de betaler 62 oere i timen hvor de i
 * virkeligheden betaler over to kroner.
 *
 * Prisomraader:
 *   DK1  vest for Storebaelt, Aarhus
 *   DK2  oest for Storebaelt, Koebenhavn
 */

#ifndef ZS_PRICE_H
#define ZS_PRICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 25 timer, ikke 24.
 *
 * Den nat sommertiden slutter, har doegnet 25 timer, og filen fra
 * elprisenligenu.dk har 25 poster. Med plads til kun 24 ville den
 * sidste time blive tabt netop den ene nat om aaret, og det er en fejl
 * ingen ville opdage foer den dag.
 */
#define ZS_PRICE_MAX_HOURS   25

#define ZS_PRICE_ZONE_LEN    4     /* "DK1" plus afslutning */


typedef struct {
    uint8_t hour;    /* lokal time, 0 til 23 */
    /*
     * Forskydningen fra UTC i hele timer, fx 2 om sommeren og 1 om
     * vinteren. ZS_PRICE_OFFSET_UKENDT hvis den ikke kunne laeses.
     *
     * Den er HER fordi klokketimen alene ikke er nok. Den nat
     * sommertiden slutter findes klokken to to gange, én gang med
     * forskydning 2 og én gang med 1, og filen har begge. Uden det her
     * felt tog opslaget den foerste begge gange, og saa viste skaermen
     * den forkerte times pris i en hel time. Én gang om aaret, og helt
     * umuligt at opdage uden at vide det.
     */
    int8_t  utc_offset_h;
    float   dkk;     /* kroner pr. kWh, spotpris uden afgifter */
} zs_price_hour_t;

#define ZS_PRICE_OFFSET_UKENDT  ((int8_t)127)

typedef struct {
    bool    ok;                       /* er der brugbare priser        */
    char    zone[ZS_PRICE_ZONE_LEN];
    char    dato[11];                 /* "2026-08-25"                  */

    zs_price_hour_t timer[ZS_PRICE_MAX_HOURS];
    uint8_t antal;

    int8_t  nu;          /* plads i timer[] for den time vi er i, -1 = ukendt */
    uint8_t billigst;    /* plads i timer[]                                   */
    uint8_t dyrest;
    float   gennemsnit;

    char    fejl[72];    /* dansk tekst naar ok er false */
} zs_price_day_t;

/*
 * Hvilken plads i timer[] svarer til den time vi er i?
 *
 * Ren beslutning, uden ur og uden netvaerk, saa den kan proeves af paa en
 * almindelig maskine. Netop den beslutning var forkert i et aar: den saa
 * kun paa klokketimen.
 *
 *   lokal_time   0 til 23
 *   offset_h     vores egen forskydning fra UTC i hele timer
 *
 * Returnerer pladsen, eller -1 hvis timen ikke findes. Har en post
 * ZS_PRICE_OFFSET_UKENDT, taeller kun klokketimen for den, saa gamle
 * data og kilder uden forskydning virker som foer.
 */
int8_t zs_price_find_hour(const zs_price_day_t *d, int lokal_time, int offset_h);

/*
 * Er de her priser fra I DAG?
 *
 * Ren sammenligning uden ur, saa den kan proeves af. Datoen gives ind som
 * tal, praecis som struct tm har dem, altsaa aar-1900 og maaned 0 til 11.
 *
 * Den findes fordi gamle priser er FARLIGE: de ser rigtige ud. Fejler
 * hentningen efter midnat, stod gaarsdagens priser paa vaeggen som om de
 * var dagens, og den fremhaevede time var ogsaa forkert. Ingen dato paa
 * siden, saa intet at opdage det paa.
 */
bool zs_price_date_is_today(const char *dato, int tm_year, int tm_mon, int tm_mday);

/*
 * Henter dagens priser. Blokerer i op til nogle sekunder, saa den skal
 * koeres fra app-opgaven og ikke fra LVGL's.
 *
 * zone er "DK1" eller "DK2". Returnerer false ved fejl, og saa staar
 * der en dansk forklaring i ud->fejl.
 */
bool zs_price_fetch(const char *zone, zs_price_day_t *ud);

/* Finder ud af hvilken time vi er i lige nu. Kaldes hvert minut, saa
 * soejlen for den aktuelle time flytter sig uden at hente noget. */
void zs_price_update_now(zs_price_day_t *d);

/* Er priserne fra en anden dag end i dag? Saa skal der hentes igen. */
bool zs_price_is_stale(const zs_price_day_t *d);

#ifdef __cplusplus
}
#endif

#endif /* ZS_PRICE_H */
