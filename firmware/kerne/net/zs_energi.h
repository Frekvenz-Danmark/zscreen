/*
 * zScreen - timens energi, regnet af inverterens egne livstaellere.
 *
 * HVORFOR DEN FINDES.
 *
 * Skaermen sender live-tal hvert andet sekund, saa dashboardet er
 * levende. Men at GEMME hvert andet sekund er noget helt andet.
 *
 * Serveren rydder selv op efter to uger, saa det der betyder noget er
 * hvad der ligger i de to uger. Raekkerne er det sikre tal, for de
 * foelger af takten og af ingenting andet:
 *
 *   hvert andet sekund:  5 felter * 43.200 * 14 dage = 3.024.000 raekker
 *   én gang i timen:     6 felter * 24 * 14 dage     =     2.016 raekker
 *
 * Altsaa 1500 gange faerre, pr. skaerm. Med en maalt pris paa 298 bytes
 * pr. raekke er det omkring 900 gigabyte mod under én, for tusind
 * skaerme, men den pris er maalt paa en lille tabel og er et overslag
 * opad. Raekkerne er facit, bytesne er et skoen. Se GEMMES i
 * zScreen-server/scripts/felter.py for hvordan det er maalt.
 *
 * Og en timevaerdi er ikke bare et gennemsnit. Den regnes som
 * taelleren nu minus taelleren ved timens start, og det giver et
 * PRAECIST tal:
 *
 *   - det taeller ogsaa med hvad der skete mens skaermen var slukket
 *   - det kan sammenlignes med en elregning, for det er samme slags tal
 *   - det kan ikke drive af, uanset hvor tit vi maaler
 *
 * Alle fem taellere findes i SunSpec. Batteriets to laa ikke hvor jeg
 * foerst troede: model 124 har ingen energifelter, men paa en Fronius
 * ligger lade- og afladesiden som to ekstra MPPT-kanaler, og HVER kanal
 * har sin egen DCWH-taeller i model 160.
 *
 * Modulet her roerer hverken net, ur eller lager. Det faar taellerne og
 * klokken udefra og siger hvad der skal sendes. Saa kan hele regningen
 * proeves af paa en almindelig maskine, ogsaa de tilfaelde der er
 * svaere at fremkalde: et doegnskifte, en genstart, en skaerm der har
 * vaeret slukket i tre timer, og en taeller der loeber rundt.
 */

#ifndef ZS_ENERGI_H
#define ZS_ENERGI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Tallene og navnene, samlet ét sted                                  */
/* ------------------------------------------------------------------ */

/*
 * Feltet der siger hvilken time vaerdierne daekker.
 *
 * Staar HER sammen med de fem andre feltnavne og ikke nede i
 * afsendelsen. De seks hoerer sammen: aendrer man ét navn, skal
 * serveren rettes samme sted, og saa skal de kunne findes samlet.
 */
#define ZS_ENERGI_TIME_FELT     "energyHour"

/*
 * Wattimer til kilowattimer.
 *
 * Vi regner i wattimer hele vejen, fordi det er det inverteren svarer,
 * og laver foerst om lige foer afsendelsen. kWh er det der staar paa en
 * elregning, og det kunden kan genkende.
 */
#define ZS_ENERGI_WH_PR_KWH     1000.0f

/* Sekunder i et doegn. Bruges til dagnummeret, se zs_energi_tik. */
#define ZS_ENERGI_SEK_PR_DOEGN  86400

/* Timer i et doegn. Graensen for en lovlig klokke er ANTAL minus én. */
#define ZS_ENERGI_TIMER_PR_DOEGN 24

/* De fem taellere, i den raekkefoelge de staar i alle tabeller her. */
typedef enum {
    ZS_E_PRODUCERET = 0,   /* inverterens samlede produktion   */
    ZS_E_KOEBT,            /* hentet fra nettet                */
    ZS_E_SOLGT,            /* leveret til nettet               */
    ZS_E_BAT_IND,          /* ind i batteriet                  */
    ZS_E_BAT_UD,           /* ud af batteriet                  */
    ZS_E_ANTAL
} zs_energi_felt_t;

/*
 * Udgangspunktet: taellerstanden da timen begyndte.
 *
 * Den gemmes i flashen, saa en genstart midt i timen ikke koster
 * timen. Ryger stroemmen klokken 14.30, er timen 14 til 15 stadig
 * rigtig naar skaermen kommer igen, for taellerne i inverteren har
 * taelt videre imens.
 */
/*
 * Saa langt et serienummer kan vaere. Samme maal som zs_fr_info_t.serial,
 * altsaa SunSpec model 1 felt SN: 32 tegn og et nul.
 */
#define ZS_ENERGI_SERIENR_MAX  33

typedef struct {
    bool     gyldig;              /* er der overhovedet et udgangspunkt */
    int8_t   time;                /* 0 til 23                           */
    uint16_t dagnr;               /* dage siden 1970, til at se doegnskift */
    bool     har[ZS_E_ANTAL];     /* havde inverteren taelleren         */
    float    wh[ZS_E_ANTAL];      /* taellerstand i wattimer            */
    /*
     * HVILKEN inverter taellerstanden kom fra.
     *
     * Den er her fordi udgangspunktet ligger i flashen og overlever en
     * genstart. Det er meningen. Men det overlever ogsaa at kunden
     * SKIFTER inverter, eller vaelger en anden i indstillingerne, eller
     * at skaermen bliver flyttet til et andet anlaeg.
     *
     * Uden det her felt ville den foerste time efter et skift blive
     * regnet som den nye inverters livstaeller minus den gamles. Er den
     * nye hoejere, fx fordi anlaegget er aeldre eller stoerre, sender vi
     * et tal paa mange tusinde kilowattimer for ÉN time. Det bliver gemt
     * paa serveren for evigt og oedelaegger hver graf og hver sum
     * bagefter. Er den lavere, bliver forskellen negativ og feltet
     * sprunget over, saa fejlen rammer kun den ene vej. Det er ikke en
     * troest: den ene vej er den der goer skade.
     *
     * Derfor: er serienummeret ikke det samme, er udgangspunktet ikke
     * vores, og vi starter forfra.
     */
    char     serienr[ZS_ENERGI_SERIENR_MAX];
} zs_energi_basis_t;

/*
 * FORMEN PAA DEN HER STRUKTUR LIGGER I FLASHEN.
 *
 * zs_nvs_save_energi gemmer den som én klump bytes, og
 * zs_nvs_load_energi laeser den tilbage efter en genstart. Den afviser en
 * klump af en anden STOERRELSE, og saa koster det én time.
 *
 * Men bytter man to felter om uden at stoerrelsen aendrer sig, er der
 * ingen der siger fra. Saa bliver et gammelt serienummer laest som
 * taellerstande, eller en taellerstand som en dato, og timens energi
 * bliver et vildt tal der ikke ser forkert ud. Det er praecis den slags
 * fejl der blev lukket ved at binde udgangspunktet til serienummeret, og
 * den maa ikke komme ind ad bagdoeren.
 *
 * Derfor staar formen fast her. Aendrer nogen strukturen, falder
 * oversaettelsen med det samme, og saa er der en beslutning at tage i
 * stedet for en overraskelse:
 *
 *   - skal de gamle data kunne laeses? Saa skal der et versionsnummer med
 *     i klumpen og en omskrivning i zs_nvs_load_energi
 *   - eller er det i orden at hver skaerm mister én time? Saa opdateres
 *     tallene herunder, og det er et valg nogen har truffet
 *
 * Tallene er maalt paa en 64-bit maskine. Paa ESP32 er int, float og
 * pointer de samme stoerrelser, og der er ingen pointere i strukturen, saa
 * de passer begge steder. Falder det her paa en ny arkitektur, er det
 * netop det man vil vide.
 */
_Static_assert(sizeof(zs_energi_basis_t) == 68,
               "zs_energi_basis_t ligger i flashen: aendrer stoerrelsen sig, "
               "mister hver skaerm én time. Laes noten ovenfor.");
_Static_assert(offsetof(zs_energi_basis_t, gyldig) == 0, "formen er aendret");
_Static_assert(offsetof(zs_energi_basis_t, time) == 1, "formen er aendret");
_Static_assert(offsetof(zs_energi_basis_t, dagnr) == 2, "formen er aendret");
_Static_assert(offsetof(zs_energi_basis_t, har) == 4, "formen er aendret");
_Static_assert(offsetof(zs_energi_basis_t, wh) == 12, "formen er aendret");
_Static_assert(offsetof(zs_energi_basis_t, serienr) == 32, "formen er aendret");

/* Hvad der skete ved det her tik. */
typedef enum {
    ZS_ENERGI_VENTER = 0,  /* samme time endnu, intet at sende          */
    ZS_ENERGI_KLAR,        /* en hel time er gaaet, ud er fyldt         */
    ZS_ENERGI_FOERSTE,     /* foerste maaling, kun udgangspunktet sat   */
    ZS_ENERGI_HUL,         /* der er gaaet mere end én time, sprunget over */
    ZS_ENERGI_NY_INVERTER, /* et andet serienummer, udgangspunktet smidt væk */
    ZS_ENERGI_ARG,         /* kalderen gav noget ubrugeligt             */
} zs_energi_t;

/*
 * Ét tik. Kaldes saa tit man vil, typisk ved hver aflaesning.
 *
 *   basis      laeses og opdateres. Gem den i flashen naar den aendrer
 *              sig, saa en genstart ikke koster timen.
 *   taeller    de fem taellerstande i wattimer
 *   har        for hver: havde inverteren den overhovedet
 *   serienr    inverterens serienummer, saa vi ved om udgangspunktet er
 *              fra DEN SAMME inverter. Maa ikke vaere tom: uden den kan
 *              vi ikke sige hvis taeller vi regner paa, og saa regner vi
 *              ikke. Se noten paa zs_energi_basis_t.serienr.
 *   time       klokken nu, 0 til 23
 *   dagnr      dage siden 1970. Bruges til at se et doegnskift og til at
 *              maale hvor laenge der er gaaet.
 *   ud         de fem timevaerdier i wattimer. Kun fyldt ved KLAR.
 *   ud_har     for hver: kunne den regnes. Kun fyldt ved KLAR.
 *   ud_time    hvilken time vaerdierne daekker. Kun ved KLAR.
 *
 * VI GAETTER IKKE. Er der gaaet mere end én time, fx fordi skaermen var
 * slukket, springer vi over i stedet for at laegge flere timer sammen og
 * kalde det én. En soejle der daekker tre timer ser ud som en time med
 * tre gange forbrug, og det ville vaere en loegn man ikke kan gennemskue.
 */
zs_energi_t zs_energi_tik(zs_energi_basis_t *basis,
                          const float *taeller, const bool *har,
                          const char *serienr,
                          int time, uint16_t dagnr,
                          float *ud, bool *ud_har, int8_t *ud_time);

/* Er det en lovlig klokke? Ét sted, saa graensen ikke staar to steder
 * med hver sit tal. */
bool zs_energi_time_ok(int time);

/* Navnet paa feltet, som det hedder paa serveren. Aldrig NULL. */
const char *zs_energi_felt_navn(zs_energi_felt_t f);

#ifdef __cplusplus
}
#endif

#endif /* ZS_ENERGI_H */
