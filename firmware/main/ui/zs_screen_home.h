/*
 * zScreen - hovedskaermen.
 *
 *     ┌────────────────────────────────────────┐
 *     │ [Z]                      14:32   ᯤ  ⚙  │
 *     ├──────────────────┬─────────────────────┤
 *     │ SOLCELLER     ☀  │ FORBRUG          ⌂  │
 *     │                  │                     │
 *     │ 4,2 kW           │ 1,8 kW              │
 *     │                  │                     │
 *     │ Producerer nu    │ Bruger nu           │
 *     ├──────────────────┼─────────────────────┤
 *     │ BATTERI       ▤  │ NETTET           ⚡ │
 *     │                  │                     │
 *     │ 78 %             │ 2,1 kW              │
 *     │                  │                     │
 *     │ ↓ Lader 1,4 kW   │ ↑ Saelger           │
 *     └──────────────────┴─────────────────────┘
 */

#ifndef ZS_SCREEN_HOME_H
#define ZS_SCREEN_HOME_H

#include "lvgl.h"
#include "zs_fronius.h"
#include "zs_statusbar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alt hovedskaermen skal bruge for at tegne sig selv. Samlet i én
 * struct, saa der kun er ét sted at kigge naar noget mangler. */
typedef struct {
    zs_fr_live_t    live;
    bool            have_data;      /* har vi nogensinde faaet en maaling */
    bool            stale;          /* er den sidste maaling for gammel   */
    bool            has_meter;
    bool            has_battery;
    zs_link_state_t link;
    int             rssi;
    const char     *time_text;      /* NULL naar uret ikke er sat         */
    bool            demo;           /* opdigtede tal, vises tydeligt      */
} zs_home_data_t;

/* Bygger skaermen paa den aktive LVGL-skaerm. Kaldes én gang. */
void zs_screen_home_create(lv_event_cb_t gear_cb, void *user_data);

/* Tegner de fire kort og statuslinjen om. Kaldes hver gang der er nye
 * tal, altsaa hvert andet sekund. Skal kaldes med LVGL-laasen taget. */
void zs_screen_home_update(const zs_home_data_t *d);

/* Skaermens rod, saa den kan skjules naar en anden skaerm vises. */
lv_obj_t *zs_screen_home_root(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_HOME_H */
