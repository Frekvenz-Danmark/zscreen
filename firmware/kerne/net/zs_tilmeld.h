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
 *
 * OG DEN KOMMER AF SIG SELV, SAA LAENGE SIDEN STAAR AABEN.
 *
 * Her blev der foer spurgt ÉN gang, nemlig da siden blev aabnet. Var
 * forudsaetningerne ikke paa plads i netop det oejeblik, var siden en
 * blindgyde: der kom aldrig en kode, heller ikke naar skaermen kom paa
 * flaaden faa sekunder senere, for der var ingen der spurgte igen.
 *
 * Nu spoerger kalderen ved hvert tik mens siden staar aaben. Det er
 * zs_tilmeld_bed_om der afgoer om der SKAL laves en ny - den giver den
 * gyldige kode tilbage uroert - saa gentagne kald koster ingenting og
 * kan ikke skifte koden under montoeren.
 *
 * Reglen om at koden kun lever mens skaermen viser den, bliver ikke
 * svagere af det. Den bliver staerkere: den bliver til naar siden er
 * aaben, og den ryddes naar siden lukkes, og de to ting er nu det samme
 * sted i stedet for hver sit.
 */

#ifndef ZS_TILMELD_H
#define ZS_TILMELD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zs_wifi.h"    /* zs_wifi_state_t  */
#include "zs_fleet.h"   /* zs_fleet_state_t */

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
 * VEJEN TIL SERVEREN. Tre ting, ikke én.
 *
 * Her stod der foer ét bool: "paa_flaaden". Det var maalt forkert paa
 * rigtigt jern 11. oktober 2026: en skaerm der var paa wifi med IP og
 * laeste fra inverteren, sagde "Skaermen er ikke paa nettet endnu" og
 * sendte montoeren ud at tjekke et netvaerk der virkede fint. Grunden
 * var at indmeldelsen paa flaaden spredes tilfaeldigt over op til et
 * minut efter opstart (ZS_FLEET_START_SPREAD_MS, maalt noedvendig mod
 * en rigtig server), og i hele det minut var det ene bool falsk.
 *
 * Ét bool kan ikke kende forskel paa "der er intet net", "nettet er
 * der, serveren er paa vej" og "serveren siger nej". De tre kraever tre
 * FORSKELLIGE ting af montoeren: sæt net op, vent, ring til Frekvenz.
 * Derfor er det her en enum og ikke et flag.
 */
typedef enum {
    /* Ikke paa wifi. Her hjaelper det at tjekke netvaerket. */
    ZS_TILMELD_VEJ_INTET_NET = 0,
    /* Paa wifi, og paa vej ind paa flaaden. Det her er det normale i de
     * foerste minutter efter en opstart, og det er IKKE en fejl. */
    ZS_TILMELD_VEJ_VENTER,
    /* Serveren vil ikke have skaermen, eller skaermen har intet
     * certifikat. Det gaar ikke vaek af sig selv, og det skal siges. */
    ZS_TILMELD_VEJ_NEJ,
    /* Indmeldt. Serveren kan se koden, saa den kan bruges. */
    ZS_TILMELD_VEJ_FLAADE,
} zs_tilmeld_vej_t;

/*
 * De to tilstande oversat til ÉN vej, og hvorfor det ligger her.
 *
 * Det stod i zs_app.c, hvor det ikke kunne proeves: app-laget bliver
 * ikke oversat paa maskinen, saa de eneste maader at se om den
 * oversaettelse var rigtig, var at flashe en skaerm og saette sig til at
 * vente. Netop den slags gaetteri var fejlen: bool'en der sagde "ikke
 * paa nettet" om et net der virkede, var aldrig blevet proevet med
 * nettet oppe og flaaden nede paa samme tid.
 *
 * Her er den en ren funktion af to enum'er, og proeven koerer ALLE
 * kombinationer af dem igennem - ogsaa tal der ikke findes i nogen af
 * dem. Se test_tilmeld.c.
 */
zs_tilmeld_vej_t zs_tilmeld_vejen(zs_wifi_state_t net,
                                  zs_fleet_state_t flaade);

/*
 * Hvad skaermen skal vise. Raekkefoelgen er den man moeder dem i.
 */
typedef enum {
    /* Ingen kode: inverteren er ikke fundet endnu. Skaermen skal sige
     * HVORFOR, og hvad man goer: find anlaegget foerst. */
    ZS_TILMELD_MANGLER_INVERTER = 0,
    /* Ingen kode: skaermen er slet ikke paa noget net. DET er den ene
     * gang hvor "tjek netvaerket" er det rigtige raad. */
    ZS_TILMELD_MANGLER_NET,
    /* Ingen kode ENDNU: nettet er der, og skaermen er paa vej ind paa
     * flaaden. Koden kommer af sig selv, og siden skal sige det i stedet
     * for at se ud som en fejl. Se noten ved zs_tilmeld_vej_t. */
    ZS_TILMELD_VENTER_PAA_SERVER,
    /* Ingen kode: serveren vil ikke have skaermen. Det bliver ikke
     * bedre af at vente, saa her skal der staa ring til Frekvenz. */
    ZS_TILMELD_SERVER_NEJ,
    /* Koden er klar og skal vises. */
    ZS_TILMELD_KLAR,
    /* Koden er udloebet. Der laves en ny naar nogen beder om det. */
    ZS_TILMELD_UDLOEBET,
    /*
     * Skaermen ER tilmeldt en kunde. Der er ikke mere at vise.
     *
     * DEN HAR VAERET TAGET UD, og det er vaerd at vide hvorfor den er
     * tilbage. Den blev fjernet fordi den ikke kunne NAAS: firmwaren
     * havde ingen maade at vide at tilmeldingen lykkedes - det sker paa
     * serveren, og skaermen fik ikke noget at vide. En tilstand
     * brugerfladen lover og firmwaren aldrig naar, er vaerre end ingen
     * tilstand: den ser proevet ud.
     *
     * Noten beskrev ogsaa vejen tilbage, og det er den der er bygget:
     * serveren skriver kundens navn paa skaermens eget asset naar
     * anlaegget er oprettet, og skaermen abonnerer paa det praecis som
     * den lytter efter prisomraadet. Se ZS_FLEET_KUNDE_FELT og
     * zs_fleet_kunde_ny.
     *
     * Forskellen fra den gamle plan: der lyttes paa et NAVN og ikke paa
     * at kodefeltet bliver ryddet. Saa kan skaermen sige HVEM den blev
     * tilmeldt, og det er den eneste maade montoeren kan opdage at
     * koden endte hos den forkerte kunde.
     */
    ZS_TILMELD_TILMELDT,
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
 *   vej              hvor langt skaermen er naaet mod serveren
 *   nu_ms            millisekunder siden start, samme ur som i tik
 */
zs_tilmeld_stand_t zs_tilmeld_stand(const zs_tilmeld_t *s,
                                    bool inverter_fundet,
                                    zs_tilmeld_vej_t vej,
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
                       zs_tilmeld_vej_t vej, int64_t nu_ms,
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
                               bool inverter_fundet, zs_tilmeld_vej_t vej,
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

/*
 * NAAR SKAERMEN ER TILMELDT: holder siden, eller gaar den hjem?
 *
 * Giver true naar hovedsiden skal vises. uret er kalderens eget ur og
 * bliver sat her: nul betyder "ikke startet".
 *
 *   aaben == false   uret nulstilles, og svaret er false
 *   uret == 0        uret startes paa nu_ms, svaret er false
 *   ellers           true naar der er gaaet mere end vis_ms
 *
 * DEN LIGGER HER OG IKKE I zs_app.c, af samme grund som
 * zs_tilmeld_vejen ovenfor: app-laget bliver ikke oversat paa maskinen,
 * saa en regel derinde kan kun proeves ved at flashe en skaerm og saette
 * sig til at vente.
 *
 * Og det kostede. Reglen stod i tilmeld_tik som "er uret ikke startet,
 * saa start det", uden at se paa om siden var aaben. Det tik koerer fem
 * gange i sekundet uanset hvilken side der vises, og navnet staar paa
 * serveren for altid - saa uret blev startet ved FOERSTE tik efter en
 * opstart. Foerste gang gik det godt, for siden stod aaben da navnet
 * kom. Men aabnede montoeren siden bagefter, for at se HVEM anlaegget
 * blev tilmeldt, var de tolv sekunder for laengst gaaet, og han blev
 * kastet hjem inden for 200 ms uden at kunne laese noget.
 *
 * Netop det navn er det eneste der kan vise at koden endte hos den
 * forkerte kunde. En side han ikke kan faa til at staa stille, er det
 * samme som ingen besked.
 *
 * Uret maaler altsaa tid PAA SIDEN. Se test_tilmeld.c.
 */
bool zs_tilmeld_hjem_nu(bool aaben, int64_t nu_ms, int64_t vis_ms,
                        int64_t *uret);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TILMELD_H */
