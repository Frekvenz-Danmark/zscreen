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
#include "zs_screen_tilmeld.h"
#include "zs_ui_maal.h"
#include "zs_screen_fejl.h"
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

static void show_locked(zs_screen_id_t id);

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
    zs_tilmeld_page_create();
    zs_fejl_page_create();

    s_ready = true;
    /* Vi viser velkomstsiden med den laase-frie udgave.
     *
     * zs_ui_show() tager selv LVGL-laasen, og kalderen holder den
     * allerede. To gange fra samme opgave er en doedvande, saa den
     * offentlige udgave maa ikke kaldes herindefra. */
    show_locked(ZS_SCREEN_WELCOME);
    ESP_LOGI(TAG, "brugerfladen er klar");

    /*
     * MAAL FLADEN, hver gang.
     *
     * Den er tavs naar der ikke er noget at sige, og den koster én
     * gennemgang af traeet ved opstart. Til gengaeld siger den fra med
     * tal naar noget stikker ud af sin foraelder eller er for lille at
     * ramme - og det er praecis de fejl der ellers bliver fundet af en
     * kunde, fordi de kun KAN ses.
     */
    (void)zs_ui_maal_graenser();
}

/* Alle sidernes rodobjekter, i samme raekkefoelge som zs_screen_id_t.
 * Én liste betyder at der ikke er en switch der kan komme ud af trit
 * med opregningen naar der tilfoejes en side. */
lv_obj_t *zs_ui_root_of(zs_screen_id_t id)
{
    switch (id) {
    case ZS_SCREEN_WELCOME:       return zs_setup_root_welcome();
    case ZS_SCREEN_GUIDE:         return zs_setup_root_guide();
    case ZS_SCREEN_WIFI_LIST:     return zs_setup_root_wifi();
    case ZS_SCREEN_PASSWORD:      return zs_setup_root_password();
    case ZS_SCREEN_CONNECTING:    return zs_setup_root_connecting();
    case ZS_SCREEN_INVERTER_SCAN: return zs_setup_root_scan();
    case ZS_SCREEN_INVERTER_LIST: return zs_setup_root_inverter();
    case ZS_SCREEN_INVERTER_MANUAL: return zs_setup_root_inverter_manual();
    case ZS_SCREEN_HOME:          return zs_screen_home_root();
    case ZS_SCREEN_SETTINGS:      return zs_settings_root();
    case ZS_SCREEN_DETAILS:       return zs_details_root();
    case ZS_SCREEN_THEME:         return zs_theme_page_root();
    case ZS_SCREEN_TILMELD:       return zs_tilmeld_page_root();
    case ZS_SCREEN_FEJL:          return zs_fejl_page_root();
    }
    return NULL;
}

#define SCREEN_COUNT  (ZS_SCREEN_FEJL + 1)

/*
 * Skifter side UDEN at tage laasen. Kalderen skal holde den.
 *
 * Findes fordi zs_ui_init() bygger skaermene mens app_main allerede
 * holder laasen. Kaldte den den offentlige zs_ui_show(), ville laasen
 * blive taget to gange af samme opgave, og enheden ville staa stille
 * uden en eneste fejl i loggen. Det skete i praksis.
 */
static void show_locked(zs_screen_id_t id)
{
    for (int i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_t *r = zs_ui_root_of((zs_screen_id_t)i);
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
    /*
     * Pulsen paa fejlsiden koerer kun mens siden vises.
     *
     * En animation bliver ved med at tikke paa en skjult side, og saa
     * ville skaermen blive tegnet om i hvert billede resten af tiden -
     * for en bevaegelse ingen kan se. Det staar HER og ikke i siden
     * selv, saa den ikke kan glemmes naar nogen skifter side ad en ny
     * vej i fremtiden.
     */
    zs_fejl_page_set_active(id == ZS_SCREEN_FEJL);
}

void zs_ui_show(zs_screen_id_t id)
{
    if (!s_ready) {
        return;
    }
    lv_port_sem_take();
    show_locked(id);
    lv_port_sem_give();
}

zs_screen_id_t zs_ui_current(void)
{
    return s_current;
}

void zs_ui_set_theme(zs_theme_mode_t m)
{
    if (!s_ready || m == zs_theme_mode()) {
        return;
    }
    lv_port_sem_take();

    /* Hvor stod vi. Vi skal tilbage til samme side bagefter, ellers
     * hopper skaermen til velkomstsiden midt i en indstilling. */
    zs_screen_id_t hvor = s_current;

    /*
     * Siderne bygges helt om.
     *
     * De delte stilarter kunne godt bare faa nye farver, men langt de
     * fleste farver er sat direkte paa hvert objekt da det blev lavet,
     * og de sidder fast. At bygge om er den eneste maade der ikke
     * efterlader noget i det gamle tema, og det er vigtigere end de faa
     * hundrede millisekunder det tager.
     */
    zs_screen_home_destroy();
    zs_setup_destroy();
    zs_settings_destroy();
    /*
     * Fejlsiden skal OGSAA bygges om, og den blev glemt her i den
     * udgave hvor den kun kunne wifi. Farverne paa dens overskrift og
     * ramme er sat direkte paa objekterne, saa et temaskift efterlod en
     * side i det gamle temas farver - usynligt indtil den dag noget gik
     * i stykker, og saa stod der en roed der ikke passede til resten.
     */
    zs_fejl_page_destroy();

    zs_theme_set_mode(m);

    zs_screen_home_create(on_gear, NULL);
    zs_setup_create();
    zs_settings_create();
    zs_fejl_page_create();

    show_locked(hvor);
    lv_port_sem_give();

    ESP_LOGI(TAG, "tema skiftet til %s", zs_theme_name(m));
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

void zs_ui_set_demo(bool demo)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_settings_set_demo(demo);
    lv_port_sem_give();
}

void zs_ui_set_ota(const zs_ota_status_t *o)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_settings_set_ota(o);
    lv_port_sem_give();
}

void zs_ui_set_price(const zs_price_day_t *d)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_screen_home_set_price((const struct zs_price_day *)d);
    lv_port_sem_give();
}

void zs_ui_set_details(const zs_fr_t *fr, const char *own_ip, int rssi)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_details_update(fr, own_ip, rssi);
    lv_port_sem_give();
}

void zs_ui_set_fejl(zs_fejl_type_t type, const char *tid, bool blink)
{
    if (!s_ready) { return; }
    lv_port_sem_take();
    zs_fejl_page_update(type, tid, blink);
    lv_port_sem_give();
}

void zs_ui_set_tilmeld(zs_tilmeld_stand_t stand, const char *kode)
{
    if (!s_ready) { return; }
    /*
     * Kun naar siden faktisk vises.
     *
     * App'en kalder herind ved HVERT tik, altsaa hvert andet sekund, og
     * uden den her linje tog vi LVGL-laasen, satte tre labels og kodede
     * QR'en forfra - 441 pixels - hele tiden, ogsaa mens skaermen stod
     * paa hovedsiden. Kommentaren i tilmeld_tik lovede at det var gratis
     * naar siden ikke vises. Det var den ikke, foer nu.
     *
     * Siden bliver ikke gammel af det: naar den aabnes, sender
     * brugerfladen ZS_CMD_TILMELD_VIS, og app'en tegner den om med det
     * samme i svaret paa den kommando.
     */
    if (zs_ui_current() != ZS_SCREEN_TILMELD) {
        return;
    }
    lv_port_sem_take();
    zs_tilmeld_page_update(stand, kode);
    lv_port_sem_give();
}
