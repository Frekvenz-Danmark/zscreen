/*
 * zScreen - engangskoden der binder skaermen til en kunde. Fase B.
 *
 * HVORFOR DEN FINDES.
 *
 * Skaermen melder sig selv ind paa flaaden med sit certifikat, og det
 * siger HVILKEN skaerm det er. Men ikke hvis kunde den staar hos. Nogen
 * skal sige "den her skaerm hoerer til Hansen paa Testvej 1", og det er
 * tilmeldingen.
 *
 * Den gaar saadan: skaermen laver en engangskode og viser den som seks
 * tegn og en QR. En montoer laeser koden og giver den til Frekvenz
 * sammen med kundens oplysninger. Frekvenz kalder vores API med koden,
 * og saa bliver kundens mappe og anlaeg oprettet.
 *
 * HVAD KODEN IKKE ER.
 *
 * Den er ikke adgang. Den siger kun HVILKEN skaerm, og den er opbrugt
 * bagefter. Hvem der maa laese kundens tal, afgoeres af en servicebruger
 * med adgang til netop de kunder de maa se. Identitet og adgang er to
 * ting, og de maa ikke smelte sammen: var koden adgang, kunne enhver der
 * fotograferede en skaerm laese kundens forbrug.
 *
 * DERFOR STAAR ENHEDENS IDENTITET IKKE I QR'EN.
 *
 * QR'en baerer KUN de seks tegn. Stod enhedens id i den, kunne enhver der
 * fotograferer en skaerm i et vindue tilmelde den senere, ogsaa naar
 * koden er doed. Og der staar ingen adresse i den: en adresse der er
 * forkert eller flytter, er en QR der peger paa ingenting i tusind
 * skaerme der allerede er sat op.
 *
 * FIRE TING HOLDER KODEN TAET, OG DEN FOERSTE ER IKKE LAENGDEN.
 *
 * Seks tegn af 32 er en milliard muligheder. Det lyder af meget, men en
 * maskine proever en milliard gange, saa laengden alene er ikke nok:
 *
 *   1. den UDLOEBER efter ZS_TILMELD_LEVETID_MIN minutter
 *   2. den virker KUN mens skaermen faktisk viser den. Holder skaermen op,
 *      ryddes den ogsaa paa serveren
 *   3. den kan bruges ÉN gang
 *   4. serveren lukker ned efter faa forsoeg
 *
 * De tre foerste staar her. Den fjerde er serverens, for kun den kan
 * taelle forsoeg fra hele verden.
 *
 * OG DEN LIGGER ALDRIG I FLASHEN.
 *
 * Koden lever i RAM og kun der. Genstarter skaermen, er den vaek, og der
 * laves en ny. En kode der overlevede en genstart, ville vaere en kode
 * man kunne finde i en skaerm der har staaet slukket i et halvt aar.
 *
 * INGEN KODE FOER INVERTEREN ER FUNDET.
 *
 * Besluttet med Jakob 8. oktober 2026. En skaerm kan ikke tilmeldes foer
 * den har fundet anlaegget, for et anlaeg uden inverter er ikke et
 * anlaeg. Det koster: en montoer kan staa med en skaerm der ikke vil give
 * en kode. Derfor SKAL skaermen sige tydeligt hvorfor der ikke er nogen
 * endnu, og hvad man goer - se zs_tilmeld_stand_t.
 */

#ifndef ZS_TILMELD_H
#define ZS_TILMELD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seks tegn. Se noten om alfabetet i zs_tilmeld.c. */
#define ZS_TILMELD_KODE_LEN   6

/* Hvor mange tilfaeldige bytes zs_tilmeld_kode skal have. Seks tegn af
 * 32 er 5 bit hver, altsaa 30 bit, og det er fire bytes. */
#define ZS_TILMELD_TILF_BYTES 4

/* Hvor laenge en kode lever. Lang nok til at en montoer kan skrive den
 * ind i roligt tempo, kort nok til at den er doed naar han er gaaet. */
#define ZS_TILMELD_LEVETID_MIN 10

/*
 * Hvad skaermen skal vise. Raekkefoelgen er den man moeder dem i.
 */
typedef enum {
    /* Ingen kode: inverteren er ikke fundet endnu. Skaermen skal sige
     * HVORFOR, og hvad man goer: find anlaegget foerst. */
    ZS_TILMELD_MANGLER_INVERTER = 0,
    /* Ingen kode: skaermen er ikke paa flaaden endnu, saa serveren kan
     * ikke se koden, og saa kunne den ikke bruges til noget. */
    ZS_TILMELD_MANGLER_FLAADE,
    /* Koden er klar og skal vises. */
    ZS_TILMELD_KLAR,
    /* Koden er udloebet. Der laves en ny naar nogen beder om det. */
    ZS_TILMELD_UDLOEBET,
    /*
     * HER STOD ÉN MERE: ZS_TILMELD_TILMELDT, "skaermen er tilmeldt en
     * kunde, der er ikke mere at vise".
     *
     * Den er taget ud fordi den ikke kunne NAAS. Funktionen der satte
     * den blev kun kaldt fra proeverne, og firmwaren har ingen maade at
     * vide at tilmeldingen lykkedes: det sker paa serveren, og skaermen
     * faar ikke noget at vide.
     *
     * En tilstand brugerfladen lover og firmwaren aldrig naar, er vaerre
     * end ingen tilstand: den ser proevet ud.
     *
     * SAADAN KOMMER DEN TILBAGE. Serveren rydder selv kodefeltet naar
     * koden bliver brugt - det er braend() i scripts/tilmeld.py. Skaermen
     * kan altsaa abonnere paa sit eget enrollCode, praecis som den i dag
     * lytter efter firmwareTarget, og saa betyder "feltet blev tomt uden
     * at JEG ryddede det" at tilmeldingen lykkedes. Mekanismen findes
     * allerede, det er bare ikke koblet paa. Det hoerer i den fase hvor
     * serveren skal sende ned.
     */
} zs_tilmeld_stand_t;

/*
 * Det vi holder paa. En nulstillet struct betyder "ingen kode endnu",
 * saa den kan laves med memset eller som en statisk variabel.
 *
 * Den maa ALDRIG gemmes i flashen. Se noten i headerens top.
 */
typedef struct {
    char    kode[ZS_TILMELD_KODE_LEN + 1];
    int64_t doer_ms;        /* hvornaar den udloeber, samme ur som nu_ms */
    bool    har_kode;
} zs_tilmeld_t;

/*
 * Laver koden ud af tilfaeldige bytes. Ren funktion: de samme bytes
 * giver altid den samme kode, og derfor kan den proeves.
 *
 *   ud       mindst ZS_TILMELD_KODE_LEN + 1 tegn
 *   tilf     ZS_TILMELD_TILF_BYTES tilfaeldige bytes
 *
 * Giver false hvis noget er NULL eller for lille, og roerer da ikke ud.
 */
bool zs_tilmeld_kode(char *ud, size_t ud_len,
                     const uint8_t *tilf, size_t tilf_len);

/*
 * Hvad skal skaermen vise lige nu. Ren funktion af tilstanden.
 *
 *   inverter_fundet  har skaermen en inverter at maale paa
 *   paa_flaaden      er den indmeldt, saa serveren kan se koden
 *   nu_ms            millisekunder siden start, samme ur som i tik
 */
zs_tilmeld_stand_t zs_tilmeld_stand(const zs_tilmeld_t *s,
                                    bool inverter_fundet, bool paa_flaaden,
                                    int64_t nu_ms);

/*
 * Beder om en kode. Laver en ny hvis der ikke er en gyldig.
 *
 * Giver true hvis der ER en kode at vise bagefter, altsaa hvis standen
 * blev ZS_TILMELD_KLAR. Giver false hvis forudsaetningerne mangler, og
 * laver da ingen kode: en kode serveren ikke kan se, er en kode
 * montoeren proever forgaeves.
 */
bool zs_tilmeld_bed_om(zs_tilmeld_t *s, bool inverter_fundet,
                       bool paa_flaaden, int64_t nu_ms,
                       const uint8_t *tilf, size_t tilf_len);

/*
 * Rydder koden. Kaldes naar skaermen holder op med at vise den, saa den
 * ogsaa holder op med at virke. Det er regel 2.
 */
void zs_tilmeld_ryd(zs_tilmeld_t *s);

/*
 * Koden, eller en tom streng hvis der ikke er en gyldig at vise.
 *
 * Det er den der sendes til serveren, og en tom streng betyder
 * udtrykkeligt "ryd den hos dig ogsaa".
 */
const char *zs_tilmeld_kode_nu(const zs_tilmeld_t *s,
                               bool inverter_fundet, bool paa_flaaden,
                               int64_t nu_ms);

/*
 * HVORNAAR KODEN SENDES TIL SERVEREN.
 *
 * "Har den aendret sig" er ikke nok, praecis som ved inverterfeltet i
 * zs_fleet_msg.h. Melder skaermen sig ind IGEN - fordi enheden er
 * oprettet forfra, eller fordi beskeden gik tabt - saa staar feltet tomt
 * paa serveren, mens vi stadig tror vi har sagt det. Og saa ville
 * montoeren se en kode paa skaermen som serveren ikke kendte.
 *
 * Derfor: har vi sagt noget, og hvad sagde vi saa. Og nulstil ved en ny
 * indmeldelse.
 *
 * En nulstillet struct betyder "aldrig sagt noget".
 */
typedef struct {
    bool sagt;
    char sidst[ZS_TILMELD_KODE_LEN + 1];
} zs_tilmeld_sendt_t;

/*
 * Skal koden sendes nu?
 *
 *   kode  det zs_tilmeld_kode_nu gav. En TOM streng er ikke ingenting:
 *         den betyder "ryd den hos dig ogsaa", og den skal ogsaa sendes
 *         én gang, ellers bliver koden ved med at virke paa serveren
 *         efter skaermen er holdt op med at vise den.
 *
 * Giver true hvis der skal sendes, og opdaterer s. Kaldes ved hver
 * aflaesning og tier naesten altid.
 */
bool zs_tilmeld_skal_sende(zs_tilmeld_sendt_t *s, const char *kode);

/*
 * Hvor mange minutter koden gaelder endnu, rundet OP.
 *
 * Rundet op med vilje: staar der "0 minutter" paa en kode der stadig
 * virker, tror montoeren den er doed og laver en ny uden grund. Giver 0
 * naar der ikke er en gyldig kode.
 */
int zs_tilmeld_min_igen(const zs_tilmeld_t *s, int64_t nu_ms);

/* Glem hvad vi har sagt. Kaldes ved en ny indmeldelse. */
void zs_tilmeld_sendt_nulstil(zs_tilmeld_sendt_t *s);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TILMELD_H */
