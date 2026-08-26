/*
 * zScreen - selvtest af sideombygning og temaskift.
 *
 * Slaas til med ZS_SELFTEST i zs_config.h. Den er slaaet FRA i det der
 * sendes ud: den tegner sider om i et tempo ingen bruger skal se, og
 * den skal ikke ligge og koere paa en vaeg.
 *
 * Hvorfor den findes:
 *   Et temaskift river alle sider ned og bygger dem op igen. Det er den
 *   eneste maade at faa farver der sidder paa hvert enkelt objekt med
 *   over, men det er ogsaa den slags der efterlader en doed pegepind
 *   eller en bid hukommelse der aldrig kommer tilbage. Begge dele viser
 *   sig foerst efter mange skift, og aldrig mens man sidder og tester i
 *   haanden.
 *
 *   Derfor: byg alle sider om mange gange, gaa hver eneste side
 *   igennem hver gang, og maal hukommelsen foer og efter. Siver der
 *   noget, staar det i loggen med et tal.
 */

#include "zs_config.h"

#if ZS_SELFTEST

#include "zs_selftest.h"
#include "zs_ui.h"
#include "zs_theme.h"
#include "zs_screen_home.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "selftest";

/* Alle sider, i den raekkefoelge de staar i zs_screen_id_t. */
static const zs_screen_id_t ALLE[] = {
    ZS_SCREEN_WELCOME, ZS_SCREEN_WIFI_LIST, ZS_SCREEN_PASSWORD,
    ZS_SCREEN_CONNECTING, ZS_SCREEN_INVERTER_SCAN, ZS_SCREEN_INVERTER_LIST,
    ZS_SCREEN_PRICE_ZONE, ZS_SCREEN_HOME, ZS_SCREEN_SETTINGS,
    ZS_SCREEN_DETAILS,
};
#define ANTAL_SIDER (sizeof(ALLE) / sizeof(ALLE[0]))

/* Antal gange hele saettet bygges om. To skift er ét par, saa vi ender
 * i det tema vi startede i og kan sammenligne hukommelsen aerligt. */
#define RUNDER  6

static size_t fri(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
         + heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

/*
 * Prøver de fire opsaetninger af kasser paa hovedskaermen.
 *
 * Maalene er allerede efterproevet af tests/host/test_tilegrid.c. Det
 * her tjekker den anden halvdel: at LVGL kan aendre stoerrelse paa og
 * skjule kasserne igen og igen uden at gaa ned eller lade noget ligge.
 */
static void proev_kasserne(void)
{
    static const struct { bool maaler, batteri; const char *hvad; } saet[] = {
        { true,  true,  "elmaaler og batteri, fire kasser" },
        { true,  false, "elmaaler uden batteri, tre kasser" },
        { false, true,  "batteri uden elmaaler, to kasser" },
        { false, false, "kun inverter, én kasse" },
    };

    size_t foer = fri();
    for (int runde = 0; runde < 6; runde++) {
        for (size_t i = 0; i < sizeof(saet) / sizeof(saet[0]); i++) {
            zs_home_data_t d;
            memset(&d, 0, sizeof(d));
            d.have_data   = true;
            d.has_meter   = saet[i].maaler;
            d.has_battery = saet[i].batteri;
            d.link        = ZS_LINK_OK;
            d.rssi        = -55;
            d.live.solar_w   = (zs_val_t){ .ok = true, .v = 4200.0f };
            d.live.house_w   = (zs_val_t){ .ok = saet[i].maaler,  .v = 1800.0f };
            d.live.grid_w    = (zs_val_t){ .ok = saet[i].maaler,  .v = -2400.0f };
            d.live.battery_w = (zs_val_t){ .ok = saet[i].batteri, .v = -1000.0f };
            d.live.soc_pct   = (zs_val_t){ .ok = saet[i].batteri, .v = 78.0f };

            zs_ui_show(ZS_SCREEN_HOME);
            zs_ui_set_home(&d);
            vTaskDelay(pdMS_TO_TICKS(250));
            if (runde == 0) {
                ESP_LOGI(TAG, "  %s: tegnet", saet[i].hvad);
            }
        }
    }
    long tabt = (long)foer - (long)fri();
    ESP_LOGI(TAG, "kasser: 24 skift, %ld bytes forskel", tabt);
}

void zs_selftest_run(void)
{
    proev_kasserne();

    zs_theme_mode_t start_tema = zs_theme_mode();

    /* Én opvarmning foerst. Den allerfoerste ombygning bruger lidt
     * hukommelse der bliver liggende med vilje, fx skrifttypernes
     * mellemlager. Den skal ikke taelle som et sivende hul. */
    zs_ui_set_theme(start_tema == ZS_THEME_DARK ? ZS_THEME_LIGHT
                                                : ZS_THEME_DARK);
    zs_ui_set_theme(start_tema);
    vTaskDelay(pdMS_TO_TICKS(300));

    size_t foer = fri();
    ESP_LOGI(TAG, "start: %u KB fri", (unsigned)(foer / 1024));

    for (int runde = 0; runde < RUNDER; runde++) {
        /* Gaa alle temaer igennem, ikke kun to. Tilfoejes et, kommer
         * det med af sig selv. */
        zs_theme_mode_t t = (zs_theme_mode_t)(runde % ZS_THEME_COUNT);
        zs_ui_set_theme(t);

        for (size_t i = 0; i < ANTAL_SIDER; i++) {
            zs_ui_show(ALLE[i]);
            /* Lang nok til at LVGL naar at tegne siden faerdig. Sker
             * det ikke, tester vi kun at objekterne blev lavet, ikke at
             * de kan tegnes. */
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        ESP_LOGI(TAG, "runde %d i %s: %u KB fri, alle %u sider tegnet",
                 runde + 1, zs_theme_name(t),
                 (unsigned)(fri() / 1024), (unsigned)ANTAL_SIDER);
    }

    zs_ui_set_theme(start_tema);
    vTaskDelay(pdMS_TO_TICKS(300));

    size_t efter = fri();
    long tabt = (long)foer - (long)efter;

    ESP_LOGI(TAG, "slut: %u KB fri", (unsigned)(efter / 1024));
    if (tabt > 4096) {
        ESP_LOGE(TAG, "SIVER: %ld bytes vaek efter %d ombygninger, "
                      "ca. %ld bytes pr. gang",
                 tabt, RUNDER, tabt / RUNDER);
    } else if (tabt > 0) {
        ESP_LOGI(TAG, "OK: %ld bytes forskel efter %d ombygninger, "
                      "det er stoej og ikke et hul", tabt, RUNDER);
    } else {
        ESP_LOGI(TAG, "OK: ingen hukommelse tabt efter %d ombygninger",
                 RUNDER);
    }
}

#endif /* ZS_SELFTEST */
