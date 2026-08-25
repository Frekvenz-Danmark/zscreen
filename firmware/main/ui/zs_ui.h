/*
 * zScreen - brugerfladen udadtil.
 *
 * Kun zs_app.c kalder herind. Alle funktioner tager selv LVGL-laasen,
 * saa der ikke er ét sted hvor nogen har glemt det. LVGL taaler ikke
 * at blive kaldt fra to traade paa én gang, og en skaerm der gaar i
 * staa en gang om ugen er praktisk talt umulig at fejlsoege bagefter.
 */

#ifndef ZS_UI_H
#define ZS_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "zs_wifi.h"
#include "zs_discovery.h"
#include "zs_screen_home.h"
#include "zs_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZS_SCREEN_WELCOME = 0,   /* logo og "Kom i gang"                */
    ZS_SCREEN_WIFI_LIST,     /* vælg netværk                        */
    ZS_SCREEN_PASSWORD,      /* tastatur til kodeordet              */
    ZS_SCREEN_CONNECTING,    /* forbinder til netværket             */
    ZS_SCREEN_INVERTER_SCAN, /* søger efter inverteren              */
    ZS_SCREEN_INVERTER_LIST, /* vælg inverter                       */
    ZS_SCREEN_HOME,          /* de fire kort                        */
    ZS_SCREEN_SETTINGS,      /* indstillinger                       */
    ZS_SCREEN_DETAILS,       /* detaljer om anlægget                */
} zs_screen_id_t;

/* Bygger alle skaerme. Kaldes én gang fra app_main, med LVGL-laasen
 * taget af kalderen. */
void zs_ui_init(void);

/* Skifter side. Tager selv laasen. */
void zs_ui_show(zs_screen_id_t id);
zs_screen_id_t zs_ui_current(void);

/* ── data ind i skaermene ──────────────────────────────────────────
 * Alle tager selv laasen og maa kaldes fra app-opgaven.            */

void zs_ui_set_wifi_list(const zs_ap_t *aps, int n);
void zs_ui_set_wifi_scanning(bool scanning);

/* Vises paa "forbinder"-siden. is_error faerver teksten. */
void zs_ui_set_connect_status(const char *text, bool is_error, bool can_retry);

void zs_ui_set_scan_progress(int done, int total, int found);
void zs_ui_set_inverter_list(const zs_found_t *list, int n);

void zs_ui_set_home(const zs_home_data_t *d);

/* Fylder Indstillinger og Detaljer. */
void zs_ui_set_settings(const zs_settings_t *s, const char *ip);
void zs_ui_set_demo(bool demo);
void zs_ui_set_details(const zs_fr_t *fr, const char *own_ip, int rssi);

#ifdef __cplusplus
}
#endif

#endif /* ZS_UI_H */
