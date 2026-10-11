/*
 * zScreen - QR-koden til tilmeldingen.
 *
 * Et tynd lag over qrcodegen i vendor/. Det findes af to grunde.
 *
 * 1. GEOMETRIEN SKAL KUNNE PROEVES PAA EN ALMINDELIG MASKINE.
 *
 * Hvor mange pixels per modul, og hvor meget luft rundt om. Begge er ren
 * regning, og begge kan vaere forkerte paa en maade man ikke kan se: en
 * QR med for lidt luft rundt om bliver IKKE laest af en telefon, og den
 * ser helt rigtig ud paa skaermen. Derfor ligger de to tal i rene
 * funktioner her, og de bliver proevet i tests/host/test_qr.c.
 *
 * 2. LUFTEN RUNDT OM ER ET KRAV, IKKE PYNT.
 *
 * QR-standarden kraever en lys kant paa FIRE moduler hele vejen rundt.
 * Uden den kan laeseren ikke finde kodens kant. Det er den hyppigste
 * grund til at en QR paa en skaerm ikke virker, og den er gratis at
 * undgaa.
 *
 * OG KODEN TEGNES ALTID MOERKT PAA LYST.
 *
 * Ogsaa i moerkt tema. En omvendt QR - lyse moduler paa moerk bund -
 * bliver afvist af mange laesere, og vi kan ikke vide hvilken telefon
 * montoeren har. Derfor ligger der en hvid plade bag, uanset tema.
 */

#ifndef ZS_QR_H
#define ZS_QR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Luft rundt om, i moduler. Staar i QR-standarden. */
#define ZS_QR_KANT_MODULER  4

/*
 * HVOR STOR QR'EN TEGNES PAA SKAERMEN, I PIXELS.
 *
 * Tallet staar HER og ikke i zs_screen_tilmeld.c, fordi to ting skal
 * vaere enige om det: siden der tegner den, og tools/qr-laes der beviser
 * at en telefon kan laese den. Stod det to steder, kunne siden blive
 * gjort stoerre uden at nogen proevede om den nye stoerrelse virkede -
 * og saa beviste proeven en stoerrelse der ikke laengere findes.
 *
 * 232 er 8 pixels per modul for en kode paa seks tegn (21 moduler plus
 * 2 x 4 moduler luft = 29). Det er MAALT paa jern 11. oktober 2026, og
 * ikke regnet: skaermen skriver selv i loggen hvor mange pixels siden
 * bruger, se maal_stakken i zs_screen_tilmeld.c.
 *
 *   145 px  -  den gamle stoerrelse
 *   203 px  -  maalt: hele siden bruger 354 af de 424 der er
 *   232 px  -  maalt: 383 af 424. Det er den der staar her
 *   261 px  -  ville give 412 af 424, altsaa 12 px tilbage. For lidt
 *              til at en skrifttype eller en tekst maa vokse
 *
 * Mit eget overslag sagde at 232 ikke kunne vaere der. Overslaget var
 * 44 px galt, fordi det regnede med to tekstlinjer hvor der kun blev
 * én. Derfor staar maalingen i loggen nu: saa skal den naeste ikke
 * gaette heller.
 */
#define ZS_QR_SKAERM_MAAL_PX  232

/*
 * Hvor mange pixels hvert modul maa vaere, hvis hele koden med luft skal
 * kunne staa i px pixels.
 *
 *   moduler  kodens bredde i moduler, UDEN luft (21, 25, 29 ...)
 *   px       hvor mange pixels vi har at tegne i
 *
 * Giver 0 hvis det ikke kan gaa op med mindst ét pixel per modul. Der
 * rundes NED: et halvt pixel per modul findes ikke, og tegner man et
 * modul paa 3,5 pixels, bliver hvert andet modul en pixel bredere end
 * de andre, og saa kan laeseren ikke maale rastet.
 */
int zs_qr_skala(int moduler, int px);

/*
 * Hvor mange pixels hele koden fylder med luft, ved en given skala.
 * Altid et helt antal moduler gange skalaen, saa den aldrig bliver skaev.
 */
int zs_qr_px(int moduler, int skala);

/*
 * Hvor langt inde fra kanten af det tegnede, det foerste MODUL begynder.
 * Det er luften, og den er lige stor paa alle fire sider.
 */
int zs_qr_kant_px(int skala);

/*
 * Laver koden. ud skal vaere ZS_QR_BUF bytes.
 *
 * Giver true og saetter *moduler til bredden i moduler. Giver false hvis
 * teksten ikke kan vaere i den stoerste udgave vi tillader - og saa er ud
 * uberoert.
 */
#define ZS_QR_MAKS_UDGAVE  4
#define ZS_QR_BUF          ((17 + ZS_QR_MAKS_UDGAVE * 4) * \
                            (17 + ZS_QR_MAKS_UDGAVE * 4) / 8 + 8)

bool zs_qr_lav(const char *tekst, uint8_t *ud, size_t ud_len, int *moduler);

/* Er modulet paa (x, y) moerkt? x og y er moduler, ikke pixels. */
bool zs_qr_modul(const uint8_t *qr, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* ZS_QR_H */
