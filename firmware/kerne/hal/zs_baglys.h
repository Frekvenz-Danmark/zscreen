/*
 * zScreen - baglyset, det ene sted hvor det er hardware.
 *
 * HVORFOR FILEN FINDES.
 *
 * zs_display.c er 300 linjer, og 270 af dem er regler: hvornaar
 * skaermen daemper om natten, hvor laenge den lyser op efter et tryk,
 * hvordan alarmen pulserer. De regler er de samme uanset hvilken
 * skaerm der hænger paa vaeggen.
 *
 * De sidste 30 linjer var en LEDC-timer paa GPIO 45. Det tal gaelder
 * KUN SenseCAP Indicator D1. Paa reTerminal D1001 sidder baglyset paa
 * GPIO 14, bag en benudvider, og bliver sat op af Seeeds egen BSP - saa
 * der skal vores side ikke roere en timer, bare sige en procent videre.
 *
 * Derfor er de to ting skilt: reglerne ét sted, benet pr. board.
 * zs_display.c kender kun procenterne.
 *
 * ENHEDEN ER PROCENT, 0 til 100, og ikke en duty-vaerdi. En duty
 * afhaenger af oploesningen - 13 bit paa den ene skaerm, 10 paa den
 * anden - og saa ville reglerne skulle kende hardwaren igen.
 */

#ifndef ZS_BAGLYS_H
#define ZS_BAGLYS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Saetter baglyset op og taender det paa start_pct.
 *
 * Giver false hvis det ikke lykkedes. Kalderen skal regne med det:
 * zs_display holder sig selv for "ikke klar" og roerer ikke lyset
 * bagefter, saa en skaerm med et doedt baglys stadig kan bruges af en
 * tekniker over loggen.
 */
bool zs_baglys_init(uint8_t start_pct);

/*
 * Saetter lysstyrken. 0 til 100.
 *
 * Kaldes op til 25 gange i sekundet mens en alarm pulserer, saa den maa
 * ikke blokere og ikke logge. Kalderen har allerede sorteret
 * gentagelser fra, saa hvert kald er en reel aendring.
 */
void zs_baglys_set(uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif /* ZS_BAGLYS_H */
