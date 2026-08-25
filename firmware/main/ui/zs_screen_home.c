#include "zs_screen_home.h"
#include "zs_theme.h"
#include "zs_tile.h"
#include "zs_flow.h"
#include "zs_format.h"

#include <stdio.h>
#include <string.h>

/* Under saa mange watt kalder vi det hvile. En stikkontakt der staar og
 * traekker tyve watt er ikke noget nogen skal forholde sig til, og et
 * kort der skifter mellem "lader" og "aflader" hvert andet sekund
 * fordi tallet vipper omkring nul, er bare uroligt at kigge paa. */
#define IDLE_W   25.0f

#define PAGE_COUNT   2
#define DOT_SIZE     10
/*
 * Afstanden mellem prikkerne er ikke pynt.
 *
 * Prikkerne kan trykkes paa, og fingerfladen er bredere end selve
 * prikken. Med 12 px imellem blev de to fingerflader 207..251 og
 * 228..272, altsaa OVERLAPPENDE: et tryk lige til hoejre for den
 * foerste prik ramte den anden, fordi den ligger oeverst.
 *
 * Med 30 px imellem og 38 px fingerflade bliver de 201..239 og
 * 240..278. De roerer hinanden og overlapper ikke.
 *
 * Hoejden er 28, altsaa under de 44 vi ellers kraever. Det er
 * acceptabelt her: prikkerne er en genvej, og at traekke siden til
 * side virker altid.
 */
#define DOT_GAP      30
#define DOT_HIT_W    38

static lv_obj_t      *s_root;
static lv_obj_t      *s_pager;
static lv_obj_t      *s_page[PAGE_COUNT];
static lv_obj_t      *s_dot[PAGE_COUNT];
static zs_statusbar_t s_bar;
static zs_tile_t      s_solar;
static zs_tile_t      s_house;
static zs_tile_t      s_battery;
static zs_tile_t      s_grid;
static zs_flow_t      s_flow;
static int            s_page_now;
static bool           s_created;

lv_obj_t *zs_screen_home_root(void)
{
    return s_root;
}

/* Maler prikkerne om, saa den man staar paa er orange og bredere. */
static void update_dots(void)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        bool on = (i == s_page_now);
        /* Kun farven skifter, ikke stoerrelsen. En prik der vokser
         * skubber afstanden skaev, og saa staar de to prikker ikke
         * laengere symmetrisk om midten. */
        lv_obj_set_style_bg_color(s_dot[i],
            lv_color_hex(on ? ZS_C_ACCENT : ZS_C_BORDER), 0);
    }
}

/* Finder ud af hvilken side der staar i midten efter en bevaegelse. */
static void on_scroll_end(lv_event_t *e)
{
    (void)e;
    lv_coord_t x = lv_obj_get_scroll_x(s_pager);
    int idx = (int)((x + ZS_SCR_WIDTH / 2) / ZS_SCR_WIDTH);
    if (idx < 0)             { idx = 0; }
    if (idx >= PAGE_COUNT)   { idx = PAGE_COUNT - 1; }
    if (idx != s_page_now) {
        s_page_now = idx;
        update_dots();
    }
}

static void on_dot_clicked(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= PAGE_COUNT) {
        return;
    }
    lv_obj_scroll_to_x(s_pager, idx * ZS_SCR_WIDTH, LV_ANIM_ON);
    s_page_now = idx;
    update_dots();
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

    /*
     * Siderne ligger side om side i en beholder man kan traekke i.
     * Statuslinjen og prikkerne ligger UDENFOR den, saa de staar
     * stille mens siderne glider.
     *
     * LV_SCROLL_SNAP_CENTER goer at den altid lander praecis paa en
     * side. Uden det kan man blive staaende midt imellem to sider,
     * og saa ser skaermen i stykker ud.
     */
    s_pager = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_pager);
    lv_obj_set_size(s_pager, ZS_SCR_WIDTH, ZS_PAGE_HEIGHT);
    lv_obj_set_pos(s_pager, 0, ZS_BAR_HEIGHT);
    lv_obj_set_scroll_dir(s_pager, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(s_pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(s_pager, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(s_pager, 0, 0);
    lv_obj_set_style_pad_column(s_pager, 0, 0);
    lv_obj_add_event_cb(s_pager, on_scroll_end, LV_EVENT_SCROLL_END, NULL);

    for (int i = 0; i < PAGE_COUNT; i++) {
        s_page[i] = lv_obj_create(s_pager);
        lv_obj_remove_style_all(s_page[i]);
        lv_obj_set_size(s_page[i], ZS_SCR_WIDTH, ZS_PAGE_HEIGHT);
        lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(s_page[i], LV_SCROLLBAR_MODE_OFF);
        /* Uden dette klipper flex-beholderen siderne smallere for at
         * faa dem til at passe, og saa staar kortene skaevt. */
        lv_obj_set_style_flex_grow(s_page[i], 0, 0);
    }

    /* Side 1: de fire kasser */
    zs_tile_create(&s_solar,   s_page[0], 0, 0, "SOLCELLER", ZS_ICON_SUN);
    zs_tile_create(&s_house,   s_page[0], 1, 0, "FORBRUG",   ZS_ICON_HOUSE);
    zs_tile_create(&s_battery, s_page[0], 0, 1, "BATTERI",   ZS_ICON_BATTERY);
    zs_tile_create(&s_grid,    s_page[0], 1, 1, "NETTET",    ZS_ICON_ZAP);

    /* Side 2: energiflow */
    zs_flow_create(&s_flow, s_page[1]);

    /* Prikkerne nederst, midt paa:
     *   10 + 30 + 10 = 50, saa den foerste starter paa 240 - 25 = 215. */
    lv_coord_t total = PAGE_COUNT * DOT_SIZE + (PAGE_COUNT - 1) * DOT_GAP;
    lv_coord_t x0 = (ZS_SCR_WIDTH - total) / 2;
    lv_coord_t y  = ZS_SCR_HEIGHT - ZS_DOTS_HEIGHT + (ZS_DOTS_HEIGHT - DOT_SIZE) / 2;

    for (int i = 0; i < PAGE_COUNT; i++) {
        lv_coord_t cx = x0 + i * (DOT_SIZE + DOT_GAP);

        lv_obj_t *hit = lv_btn_create(s_root);
        lv_obj_remove_style_all(hit);
        lv_obj_set_size(hit, DOT_HIT_W, ZS_DOTS_HEIGHT);
        lv_obj_set_pos(hit, cx + DOT_SIZE / 2 - DOT_HIT_W / 2,
                       ZS_SCR_HEIGHT - ZS_DOTS_HEIGHT);
        lv_obj_add_event_cb(hit, on_dot_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        s_dot[i] = lv_obj_create(s_root);
        lv_obj_remove_style_all(s_dot[i]);
        lv_obj_set_size(s_dot[i], DOT_SIZE, DOT_SIZE);
        lv_obj_set_pos(s_dot[i], cx, y);
        lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_dot[i], LV_OBJ_FLAG_CLICKABLE);
    }
    s_page_now = 0;
    update_dots();
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
    zs_statusbar_set_demo(&s_bar, d->demo);

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
        zs_flow_update(&s_flow, d);
        return;
    }

    update_solar(d);
    update_house(d);
    update_battery(d);
    update_grid(d);

    /* Side 2 faar PRAECIS de samme data. To sider, ét sandhedsbegreb:
     * de kan ikke komme til at sige hver sit om samme maaling. */
    zs_flow_update(&s_flow, d);
}
