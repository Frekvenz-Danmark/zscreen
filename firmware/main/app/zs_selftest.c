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

void zs_selftest_run(void)
{
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
        zs_theme_mode_t t = (runde % 2 == 0) ? ZS_THEME_LIGHT : ZS_THEME_DARK;
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
