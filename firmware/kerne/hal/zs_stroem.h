/*
 * zScreen - stroemmen og batteriet, det ene sted hvor det er hardware.
 *
 * HVORFOR FILEN FINDES.
 *
 * De to produkter er ikke ens her, og det er ikke en detalje:
 *
 *   zScreen   (SenseCAP Indicator D1)  har INTET batteri. Ryger
 *             stroemmen, er skaermen vaek, og der er ingen at sige det
 *             til.
 *   ZDisplay  (reTerminal D1001)       har batteri. Ryger stroemmen,
 *             bliver den ved med at koere i timer - og saa VISER den
 *             tal som om alt var i orden, mens den langsomt loeber
 *             toer. Det er den stilhed den her fil findes for at bryde.
 *
 * Reglerne om hvad der saa skal ske, hoerer ikke til her. De staar i
 * app/zs_fejl.c sammen med wifi og inverteren, fordi en skaerm der
 * blinker, skal blinke efter ÉN regel. Her staar kun maalingen.
 *
 * ENHEDEN ER PROCENT OG MILLIVOLT, ikke raa ADC-tal. En ADC-vaerdi
 * afhaenger af referencen og af to modstande paa netop det board, og
 * saa ville reglerne kende hardwaren igen.
 */

#ifndef ZS_STROEM_H
#define ZS_STROEM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Det maalte. Samlet i én struct, saa to flag ikke kan bytte plads. */
typedef struct {
    /*
     * Har DET HER board et batteri. Er den falsk, siger de andre felter
     * ingenting, og baade fladen og fejlvagten skal lade som om feltet
     * ikke var der.
     *
     * Den staar her og ikke som en #ifdef, fordi motoren skal kunne
     * oversaettes ens til begge produkter - og fordi en proeve paa
     * vaerten saa kan koere BEGGE tilfaelde igennem.
     */
    bool findes;
    /*
     * Batteriets stand, 0 til 100. -1 betyder "ved det ikke endnu":
     * maalingen koerer i en egen opgave og er ikke klar i det foerste
     * sekund efter opstart.
     */
    int  procent;
    /*
     * Er der stroem paa stikket. MAALT paa spaendingen og IKKE paa
     * ladestatus - se noten i zs_stroem_d1001.c. Et fuldt batteri paa
     * stroem melder "lader ikke", og det ville se ud som et stroemsvigt.
     */
    bool paa_stroem;
    /* Lader den lige nu. Kun til visning, aldrig til at alarmere paa. */
    bool lader;
    /* Spaendingen paa stikket i millivolt. Til loggen og til Detaljer. */
    int  stik_mv;
    /* Batteriets spaending i millivolt. Samme formaal. */
    int  batteri_mv;
} zs_stroem_t;

/*
 * Saetter maalingen i gang. Svarer falsk hvis boardet ikke har noget at
 * maale - og det er ikke en fejl, det er svaret for zScreen.
 */
bool zs_stroem_init(void);

/*
 * Det sidst maalte. Blokerer ikke og laeser ikke hardware selv: tallene
 * kommer fra den opgave Seeeds BSP allerede koerer, saa den her maa
 * kaldes fra app-loekken uden at koste noget.
 *
 * Skriver et nulstillet svar hvis ud er NULL-sikret forkert, saa en
 * kalder der glemmer at se paa 'findes', ikke kommer til at vise
 * tilfaeldige tal.
 */
void zs_stroem_laes(zs_stroem_t *ud);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STROEM_H */
