/*
 * zScreen - skifter mellem skaermene og lukker data ind i dem.
 *
 * Alle skaerme bygges én gang ved opstart og bliver liggende. Vi skjuler
 * og viser dem i stedet for at bygge dem om hver gang.
 *
 * Hvorfor: LVGL bruger tid paa at lave og slette objekter, og en
 * skaerm der skal bygges forfra hver gang man trykker "tilbage",
 * blinker synligt. Prisen er lidt hukommelse, og den har vi rigeligt
 * af med 8 MB PSRAM.
 *
 * Alle funktioner her tager selv LVGL-laasen. Det er med vilje samlet
 * ét sted: skal hver kalder huske det, glemmer nogen det en dag, og
 * resultatet er en skaerm der fryser en gang om ugen uden moenster.
 */

#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_screen_home.h"
#include "zs_screen_setup.h"
#include "zs_screen_settings.h"
#include "zs_app.h"
#include "lv_port.h"

#include "esp_log.h"

#include <string.h>

static const char *TAG = "ui";

static zs_screen_id_t s_current = ZS_SCREEN_WELCOME;
static bool           s_ready = false;

static void on_gear(lv_event_t *e)
{
    (void)e;
    zs_ui_show(ZS_SCREEN_SETTINGS);
}

void zs_ui_init(void)
{
    if (s_ready) {
        return;
    }
    zs_theme_init();

    zs_screen_home_create(on_gear, NULL);
    zs_setup_create();
    zs_settings_create();

    s_ready = true;
    /* Alt er skjult indtil zs_app har fundet ud af hvor vi skal starte. */
    zs_ui_show(ZS_SCREEN_WELCOME);
    ESP_LOGI(TAG, "brugerfladen er klar");
}

/* Alle sidernes rodobjekter, i samme raekkefoelge som zs_screen_id_t.
 * Én liste betyder at der ikke er en switch der kan komme ud af trit
 * med opregningen naar der tilfoejes en side. */
static lv_obj_t *root_of(zs_screen_id_t id)
{
    switch (id) {
    case ZS_SCREEN_WELCOME:       return zs_setup_root_welcome();
    case ZS_SCREEN_WIFI_LIST:     return zs_setup_root_wifi();
    case ZS_SCREEN_PASSWORD:      return zs_setup_root_password();
    case ZS_SCREEN_CONNECTING:    return zs_setup_root_connecting();
    case ZS_SCREEN_INVERTER_SCAN: return zs_setup_root_scan();
    case ZS_SCREEN_INVERTER_LIST: return zs_setup_root_inverter();
    case ZS_SCREEN_HOME:          return zs_screen_home_root();
    case ZS_SCREEN_SETTINGS:      return zs_settings_root();
    case ZS_SCREEN_DETAILS:       return zs_details_root();
    }
    return NULL;
}

#define SCREEN_COUNT  (ZS_SCREEN_DETAILS + 1)

void zs_ui_show(zs_screen_id_t id)
{
    if (!s_ready) {
        return;
    }
    lv_port_sem_take();
    for (int i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_t *r = root_of((zs_screen_id_t)i);
        if (r == NULL) {
            continue;
        }
        if ((zs_screen_id_t)i == id) {
            lv_obj_clear_flag(r, LV_OBJ_FLAG_HIDDEN);
            /* Frem foran de andre, saa et tryk rammer den side der er
             * synlig og ikke en der ligger ovenpaa uden at vaere det. */
            lv_obj_move_foreground(r);
        } else {
            lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN);
        }
    }
    s_current = id;
    lv_port_sem_give();
}

zs_screen_id_t zs_ui_current(void)
{
    return s_current;
}

/* ------------------------------------------------------------------ */
/* Data ind i skaermene                                                */
/* ------------------------------------------------------------------ */

void zs_ui_set_wifi_list(const zs_ap_t *aps, int n)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_setup_set_wifi_list(aps, n);
    lv_port_sem_give();
}

void zs_ui_set_wifi_scanning(bool scanning)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_setup_set_wifi_scanning(scanning);
    lv_port_sem_give();
}

void zs_ui_set_connect_status(const char *text, bool is_error, bool can_retry)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_setup_set_connect_status(text, is_error, can_retry);
    lv_port_sem_give();
}

void zs_ui_set_scan_progress(int done, int total, int found)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_setup_set_scan_progress(done, total, found);
    lv_port_sem_give();
}

void zs_ui_set_inverter_list(const zs_found_t *list, int n)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_setup_set_inverter_list(list, n);
    lv_port_sem_give();
}

void zs_ui_set_home(const zs_home_data_t *d)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_screen_home_update(d);
    lv_port_sem_give();
}

void zs_ui_set_settings(const zs_settings_t *s, const char *ip)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_settings_update(s, ip);
    lv_port_sem_give();
}

void zs_ui_set_details(const zs_fr_t *fr, const char *own_ip, int rssi)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_details_update(fr, own_ip, rssi);
    lv_port_sem_give();
}
