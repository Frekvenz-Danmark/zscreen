#include "zs_screen_home.h"
#include "zs_theme.h"
#include "zs_tile.h"
#include "zs_format.h"

#include <stdio.h>
#include <string.h>

/* Under saa mange watt kalder vi det hvile. En stikkontakt der staar og
 * traekker tyve watt er ikke noget nogen skal forholde sig til, og et
 * kort der skifter mellem "lader" og "aflader" hvert andet sekund
 * fordi tallet vipper omkring nul, er bare uroligt at kigge paa. */
#define IDLE_W   25.0f

static lv_obj_t      *s_root;
static zs_statusbar_t s_bar;
static zs_tile_t      s_solar;
static zs_tile_t      s_house;
static zs_tile_t      s_battery;
static zs_tile_t      s_grid;
static bool           s_created;

lv_obj_t *zs_screen_home_root(void)
{
    return s_root;
}

void zs_screen_home_create(lv_event_cb_t gear_cb, void *user_data)
{
    if (s_created) {
        return;
    }
    s_created = true;

    s_root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, ZS_SCR_WIDTH, ZS_SCR_HEIGHT);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    zs_statusbar_create(&s_bar, s_root, gear_cb, user_data);

    zs_tile_create(&s_solar,   s_root, 0, 0, "SOLCELLER", ZS_ICON_SUN);
    zs_tile_create(&s_house,   s_root, 1, 0, "FORBRUG",   ZS_ICON_HOUSE);
    zs_tile_create(&s_battery, s_root, 0, 1, "BATTERI",   ZS_ICON_BATTERY);
    zs_tile_create(&s_grid,    s_root, 1, 1, "NETTET",    ZS_ICON_ZAP);
}

/* ------------------------------------------------------------------ */
/* De fire kort                                                        */
/* ------------------------------------------------------------------ */

static void update_solar(const zs_home_data_t *d)
{
    zs_val_t w = d->live.solar_w;

    if (!w.ok) {
        zs_tile_set_none(&s_solar, "Ingen måling");
        return;
    }
    bool active = w.v > IDLE_W;
    zs_tile_set_power(&s_solar, w, active);
    zs_tile_set_sub(&s_solar, NULL,
                    active ? "Producerer nu" : "Ingen sol lige nu",
                    ZS_C_LABEL);
}

static void update_house(const zs_home_data_t *d)
{
    if (!d->has_meter) {
        /* Uden elmaaler kan forbruget ikke udledes. Vi siger det, i
         * stedet for at vise et nul der ligner en maaling. */
        zs_tile_set_none(&s_house, "Ingen elmåler");
        return;
    }
    zs_val_t w = d->live.house_w;
    if (!w.ok) {
        zs_tile_set_none(&s_house, "Ingen måling");
        return;
    }
    zs_tile_set_power(&s_house, w, w.v > IDLE_W);
    zs_tile_set_sub(&s_house, NULL, "Bruger nu", ZS_C_LABEL);
}

static void update_battery(const zs_home_data_t *d)
{
    if (!d->has_battery) {
        zs_tile_set_none(&s_battery, "Intet batteri");
        return;
    }

    zs_val_t soc = d->live.soc_pct;
    zs_val_t pw  = d->live.battery_w;

    bool charging    = pw.ok && pw.v < -IDLE_W;
    bool discharging = pw.ok && pw.v >  IDLE_W;

    zs_tile_set_percent(&s_battery, soc, charging || discharging);

    /* Ikonet i hjoernet skifter mellem et batteri i hvile og et der
     * arbejder. Det er den eneste bevaegelse paa kortet, og den siger
     * noget. */
    lv_label_set_text(s_battery.head_icon,
                      (charging || discharging) ? ZS_ICON_BATTERY_CHARGE
                                                : ZS_ICON_BATTERY);

    if (!pw.ok) {
        zs_tile_set_sub(&s_battery, NULL, "Effekt ukendt", ZS_C_LABEL);
        return;
    }

    zs_num_t n;
    zs_fmt_power(pw.v, &n);
    char txt[40];

    if (charging) {
        snprintf(txt, sizeof(txt), "Lader %s %s", n.value, n.unit);
        zs_tile_set_sub(&s_battery, ZS_ICON_ARROW_DOWN, txt, ZS_C_GOOD);
    } else if (discharging) {
        snprintf(txt, sizeof(txt), "Aflader %s %s", n.value, n.unit);
        zs_tile_set_sub(&s_battery, ZS_ICON_ARROW_UP, txt, ZS_C_ACCENT);
    } else if (soc.ok && soc.v >= 99.0f) {
        zs_tile_set_sub(&s_battery, NULL, "Fuldt opladt", ZS_C_LABEL);
    } else {
        zs_tile_set_sub(&s_battery, NULL, "Hviler", ZS_C_LABEL);
    }
}

static void update_grid(const zs_home_data_t *d)
{
    if (!d->has_meter) {
        zs_tile_set_none(&s_grid, "Ingen elmåler");
        return;
    }
    zs_val_t w = d->live.grid_w;
    if (!w.ok) {
        zs_tile_set_none(&s_grid, "Ingen måling");
        return;
    }

    bool buying  = w.v >  IDLE_W;
    bool selling = w.v < -IDLE_W;

    zs_tile_set_power(&s_grid, w, buying || selling);

    if (buying) {
        zs_tile_set_sub(&s_grid, ZS_ICON_ARROW_DOWN, "Køber fra nettet", ZS_C_BAD);
    } else if (selling) {
        zs_tile_set_sub(&s_grid, ZS_ICON_ARROW_UP, "Sælger til nettet", ZS_C_GOOD);
    } else {
        zs_tile_set_sub(&s_grid, NULL, "Hverken køb eller salg", ZS_C_LABEL);
    }
}

/* ------------------------------------------------------------------ */

void zs_screen_home_update(const zs_home_data_t *d)
{
    if (!s_created || d == NULL) {
        return;
    }

    zs_statusbar_set_time(&s_bar, d->time_text);
    zs_statusbar_set_link(&s_bar, d->link, d->rssi);

    /* Daempningen saettes FOER tallene skrives, saa farvevalget inde i
     * kortene ser den rigtige tilstand med det samme. Goer man det
     * bagefter, blinker tallene i fuld styrke i et enkelt billede. */
    bool stale = d->stale || !d->have_data;
    zs_tile_set_stale(&s_solar,   stale);
    zs_tile_set_stale(&s_house,   stale);
    zs_tile_set_stale(&s_battery, stale);
    zs_tile_set_stale(&s_grid,    stale);

    if (!d->have_data) {
        /* Foer den foerste maaling. Ikke en fejl, bare endnu ikke noget
         * at vise, og det skal der staa. */
        zs_tile_set_none(&s_solar,   "Henter ...");
        zs_tile_set_none(&s_house,   "Henter ...");
        zs_tile_set_none(&s_battery, "Henter ...");
        zs_tile_set_none(&s_grid,    "Henter ...");
        return;
    }

    update_solar(d);
    update_house(d);
    update_battery(d);
    update_grid(d);
}
