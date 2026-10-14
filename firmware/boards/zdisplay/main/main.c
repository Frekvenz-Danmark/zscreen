/*
 * ZDisplay - opstart.
 *
 * Samme raekkefoelge som zScreen, og den er vigtig af samme grund:
 * skaermen skal vaere vaagen foer LVGL kan tegne paa den, og LVGL skal
 * vaere klar foer noget som helst forsoeger at lave en knap.
 *
 * Alt der roerer LVGL skal ske mellem lv_port_sem_take() og
 * lv_port_sem_give(). LVGL er ikke bygget til at blive kaldt fra flere
 * traade paa én gang, og en skaerm der gaar i staa en gang om ugen er
 * praktisk talt umulig at fejlsoege bagefter.
 *
 * DET ENE TRIN DER ER NYT: bsp_power_init().
 *
 * Paa zScreen taender skaermen naar der kommer stroem. Den her enhed har
 * et batteri og en blød taendknap, og VDD_3V3 holdes af et ben paa
 * benudvideren (BSP_PWR_HOLD). Bliver det ben ikke sat, slipper enheden
 * sin egen stroem igen.
 *
 * bsp_power_init() er Seeeds egen, og den laver mere end navnet siger:
 * den saetter en PMU-spaendingsindstilling, taender I2C, benudvider,
 * skaerm-, kamera- og 4G-stroem, batterimaalingen, uret, IMU'en og den
 * groenne knap - og den SAETTER DRIVESTYRKEN NED paa de seks SDIO-ben
 * ud til C6'eren for at daempe overshoot. Det sidste er den slags der
 * ikke kan gaettes, og grunden til at vi bruger deres kilde uaendret i
 * stedet for at skrive opstarten selv. Se
 * firmware/boards/zdisplay/components/zd_bsp/CMakeLists.txt.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#include "esp32_p4_re_terminal_d1001.h"
#include "lv_port.h"
#include "lvgl.h"

#include "zs_config.h"
#include "zs_display.h"
#include "zs_theme.h"
#include "zs_ui.h"
#include "zs_app.h"
#include "zs_lyd.h"

static const char *TAG = "zdisplay";

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
    ESP_LOGI(TAG, "%s %s", ZS_PRODUCT_NAME, zs_version());

    nvs_init_once();

    /* Indstillingerne foerst. Baade baglyset og brugerfladen skal vide
     * hvad kunden har valgt inden de bygges, ellers taender skaermen i
     * standardtilstand og retter sig selv et oejeblik senere. */
    zs_app_load_settings();

    /* Stroem, I2C, benudvider, batteri, ur og knap. Se noten oeverst. */
    ESP_ERROR_CHECK(bsp_power_init());

    lv_port_init();
    zs_display_init(zs_app_saved_brightness());

    /* Brugerfladen bygges under laasen. Derefter tager hver funktion i
     * zs_ui selv laasen, saa ingen kalder skal huske det. */
    lv_port_sem_take();
    zs_ui_init();
    lv_port_sem_give();

    /* Lyden maa fejle. Se zs_lyd_d1001.c. */
    (void)zs_lyd_init();

    if (!zs_app_start()) {
        ESP_LOGE(TAG, "kunne ikke starte hovedopgaven");
    }

    ESP_LOGI(TAG, "intern hukommelse ledig: %u KB, PSRAM ledig: %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    /* Herfra sker alting i zs_app-opgaven og i LVGL's egen. Den her
     * opgave har ikke mere at lave, men den maa ikke slutte: FreeRTOS
     * rydder stakken op naar app_main returnerer. */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
