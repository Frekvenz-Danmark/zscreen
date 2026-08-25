/*
 * zScreen - Indstillinger og Detaljer.
 *
 * Indstillinger er til kunden: skift netværk, skift inverter, skru på
 * lyset. Detaljer er til den der skal fejlsøge i marken: alt hvad
 * skærmen ved om anlægget, samlet ét sted, så man kan læse det op i
 * telefonen uden at skulle hente en computer.
 */

#ifndef ZS_SCREEN_SETTINGS_H
#define ZS_SCREEN_SETTINGS_H

#include "lvgl.h"
#include "zs_nvs.h"
#include "zs_fronius.h"

#ifdef __cplusplus
extern "C" {
#endif

void zs_settings_create(void);

lv_obj_t *zs_settings_root(void);
lv_obj_t *zs_details_root(void);

void zs_settings_update(const zs_settings_t *s, const char *ip);

/* Viser eller skjuler raden "Afslut demo". */
void zs_settings_set_demo(bool demo);
void zs_details_update(const zs_fr_t *fr, const char *own_ip, int rssi);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_SETTINGS_H */
