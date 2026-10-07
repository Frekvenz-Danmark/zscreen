/*
 * zScreen - timens energi, regnet af inverterens egne livstaellere.
 *
 * HVORFOR DEN FINDES.
 *
 * Skaermen sender live-tal hvert andet sekund, saa dashboardet er
 * levende. Men at GEMME hvert andet sekund er noget helt andet: for
 * tusind skaerme er det maalt til 183 gigabyte paa to uger, mod 142
 * megabyte hvis vi gemmer én vaerdi i timen.
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
typedef struct {
    bool     gyldig;              /* er der overhovedet et udgangspunkt */
    int8_t   time;                /* 0 til 23                           */
    uint16_t dagnr;               /* dage siden 1970, til at se doegnskift */
    bool     har[ZS_E_ANTAL];     /* havde inverteren taelleren         */
    float    wh[ZS_E_ANTAL];      /* taellerstand i wattimer            */
} zs_energi_basis_t;

/* Hvad der skete ved det her tik. */
typedef enum {
    ZS_ENERGI_VENTER = 0,  /* samme time endnu, intet at sende          */
    ZS_ENERGI_KLAR,        /* en hel time er gaaet, ud er fyldt         */
    ZS_ENERGI_FOERSTE,     /* foerste maaling, kun udgangspunktet sat   */
    ZS_ENERGI_HUL,         /* der er gaaet mere end én time, sprunget over */
    ZS_ENERGI_ARG,         /* kalderen gav noget ubrugeligt             */
} zs_energi_t;

/*
 * Ét tik. Kaldes saa tit man vil, typisk ved hver aflaesning.
 *
 *   basis      laeses og opdateres. Gem den i flashen naar den aendrer
 *              sig, saa en genstart ikke koster timen.
 *   taeller    de fem taellerstande i wattimer
 *   har        for hver: havde inverteren den overhovedet
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
