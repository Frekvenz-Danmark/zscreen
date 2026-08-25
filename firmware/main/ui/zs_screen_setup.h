/*
 * zScreen - opsaetningen, trin for trin paa selve skaermen.
 *
 *   1. Velkomst          logo og "Kom i gang"
 *   2. Vælg netværk      liste over wifi i nærheden
 *   3. Kodeord           tastatur med æ, ø og å
 *   4. Forbinder         mens der ventes, og hvis det gik galt
 *   5. Søger             gennemgår netværket efter inverteren
 *   6. Vælg inverter     de fundne, med model og serienummer
 *
 * Ingen telefon, ingen computer, ingen QR-kode. Kunden skal kunne
 * saette skaermen op med det den kom med i kassen.
 */

#ifndef ZS_SCREEN_SETUP_H
#define ZS_SCREEN_SETUP_H

#include "lvgl.h"
#include "zs_wifi.h"
#include "zs_discovery.h"
#include "zs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

void zs_setup_create(void);

/* Siderne, saa zs_ui kan vise og skjule dem. */
lv_obj_t *zs_setup_root_welcome(void);
lv_obj_t *zs_setup_root_wifi(void);
lv_obj_t *zs_setup_root_password(void);
lv_obj_t *zs_setup_root_connecting(void);
lv_obj_t *zs_setup_root_scan(void);
lv_obj_t *zs_setup_root_inverter(void);
lv_obj_t *zs_setup_root_zone(void);

void zs_setup_set_wifi_list(const zs_ap_t *aps, int n);
void zs_setup_set_wifi_scanning(bool scanning);
void zs_setup_set_connect_status(const char *text, bool is_error, bool can_retry);
void zs_setup_set_scan_progress(int done, int total, int found);
void zs_setup_set_inverter_list(const zs_found_t *list, int n);

/* Siger hvor "tilbage" foerer hen fra siden med prisomraade. */
void zs_setup_zone_set_return(zs_screen_id_t hvorhen);


/* River opsaetningens sider ned. Se noten ved zs_screen_home_destroy. */
void zs_setup_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_SETUP_H */
