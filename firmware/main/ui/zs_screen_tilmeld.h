/*
 * zScreen - siden hvor anlaegget tilmeldes en kunde. Fase B.
 *
 * Skaermen viser en engangskode paa seks tegn. En montoer laeser den og
 * giver den til Frekvenz sammen med kundens oplysninger, og saa bliver
 * kundens mappe og anlaeg oprettet.
 *
 * SIDEN ER OGSAA DER NAAR DER IKKE ER NOGEN KODE, OG DET ER MENINGEN.
 *
 * En kode kraever at inverteren er fundet og at skaermen er paa flaaden.
 * Mangler én af dem, staar montoeren med en skaerm der ikke vil give en
 * kode, og saa SKAL der staa hvorfor og hvad man goer. Ellers leder han
 * efter fejlen i Frekvenz' app, hvor den ikke er.
 *
 * Det var den dyre halvdel af beslutningen 8. oktober: Jakob valgte det
 * strammere - ingen tilmelding foer anlaegget er fundet - og prisen er
 * netop den montoer. Derfor er beskederne her ikke pynt.
 */

#ifndef ZS_SCREEN_TILMELD_H
#define ZS_SCREEN_TILMELD_H

#include "lvgl.h"
#include "zs_tilmeld.h"

#ifdef __cplusplus
extern "C" {
#endif

void zs_tilmeld_page_create(void);
lv_obj_t *zs_tilmeld_page_root(void);

/*
 * Tegner siden om.
 *
 *   stand       hvad der skal staa
 *   kode        de seks tegn, eller en tom streng
 *   min_igen    minutter til koden doer. Kun brugt ved ZS_TILMELD_KLAR
 */
void zs_tilmeld_page_update(zs_tilmeld_stand_t stand, const char *kode,
                            int min_igen);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_TILMELD_H */
