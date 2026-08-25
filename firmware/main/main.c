/*
 * zScreen - opstart.
 *
 * Raekkefoelgen er vigtig og foelger Seeeds egen: skaermen skal vaere
 * vaagen foer LVGL kan tegne paa den, og LVGL skal vaere klar foer noget
 * som helst forsoeger at lave en knap.
 *
 * Alt der roerer LVGL skal ske mellem lv_port_sem_take() og
 * lv_port_sem_give(). LVGL er ikke bygget til at blive kaldt fra flere
 * traade paa én gang, og en skaerm der gaar i staa en gang om ugen er
 * praktisk talt umulig at fejlsoege bagefter.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#include "bsp_board.h"
#include "lv_port.h"
#include "lvgl.h"

#include "zs_config.h"
#include "zs_display.h"
#include "zs_theme.h"
#include "zs_screen_home.h"

static const char *TAG = "zscreen";

/* Skaermens tilstand som UI'et tegner ud fra. Ét sted, saa der ikke er
 * tvivl om hvad der er sandt lige nu. */
static zs_home_data_t s_home;

static void on_gear_clicked(lv_event_t *e)
{
    (void)e;
    /* Indstillinger kommer i naeste skridt. */
    ESP_LOGI(TAG, "tandhjul trykket");
}

static void nvs_init_once(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Enten er der ikke plads mere, eller ogsaa er formatet skiftet
         * med en firmwareopdatering. Begge dele loeses ved at rydde
         * omraadet. Vi mister kundens wifi-kode og valgte inverter, og
         * saa skal skaermen saettes op igen. Det er bedre end en enhed
         * der ikke kan starte. */
        ESP_LOGW(TAG, "gemte indstillinger kunne ikke laeses, rydder dem");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    ESP_LOGI(TAG, "%s %s", ZS_PRODUCT_NAME, ZS_VERSION);

    nvs_init_once();

    ESP_ERROR_CHECK(bsp_board_init());
    lv_port_init();
    zs_display_init();

    lv_port_sem_take();
    zs_theme_init();
    zs_screen_home_create(on_gear_clicked, NULL);
    lv_port_sem_give();

    /* Foer der er en forbindelse: ingen tal, ingen wifi. Det er den
     * aerlige starttilstand, ikke en fejl. */
    s_home.have_data = false;
    s_home.stale = true;
    s_home.link = ZS_LINK_NO_WIFI;
    s_home.time_text = NULL;

    lv_port_sem_take();
    zs_screen_home_update(&s_home);
    lv_port_sem_give();

    ESP_LOGI(TAG, "intern hukommelse ledig: %u KB, PSRAM ledig: %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    /* Hovedloekken holder styr paa lysstyrken. Alt andet sker i sine
     * egne opgaver og gennem LVGL's tidsstyring. */
    while (1) {
        zs_display_tick();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
