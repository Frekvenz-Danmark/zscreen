/*
 * Baglyset paa SenseCAP Indicator D1, altsaa zScreen.
 *
 * GPIO og opsaetning er den samme som i Seeeds eget eksempel, saa vi ved
 * den passer til hardwaren. Tallene stod foer i zs_display.c; se
 * zs_baglys.h for hvorfor de er flyttet herud.
 */

#include "zs_baglys.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "baglys";

#define BL_GPIO         45
#define BL_MODE         LEDC_LOW_SPEED_MODE
#define BL_TIMER        LEDC_TIMER_0
#define BL_CHANNEL      LEDC_CHANNEL_0
#define BL_RES          LEDC_TIMER_13_BIT
#define BL_MAX_DUTY     ((1 << 13) - 1)
/* 5 kHz. Hoejt nok til at oejet ikke ser flimmer, lavt nok til at
 * LEDC'en kan levere fuld 13-bit oploesning. */
#define BL_FREQ_HZ      5000

static bool s_klar = false;

bool zs_baglys_init(uint8_t start_pct)
{
    ledc_timer_config_t timer = {
        .speed_mode      = BL_MODE,
        .timer_num       = BL_TIMER,
        .duty_resolution = BL_RES,
        .freq_hz         = BL_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&timer) != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke saette PWM-timer op til baglyset");
        return false;
    }

    ledc_channel_config_t ch = {
        .speed_mode = BL_MODE,
        .channel    = BL_CHANNEL,
        .timer_sel  = BL_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = BL_GPIO,
        .duty       = (uint32_t)BL_MAX_DUTY * start_pct / 100u,
        .hpoint     = 0,
    };
    if (ledc_channel_config(&ch) != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke saette PWM-kanal op til baglyset");
        return false;
    }

    s_klar = true;
    ESP_LOGI(TAG, "baglys klar paa GPIO %d, %u %%", BL_GPIO, (unsigned)start_pct);
    return true;
}

void zs_baglys_set(uint8_t pct)
{
    if (!s_klar) {
        return;
    }
    uint32_t duty = (uint32_t)BL_MAX_DUTY * pct / 100u;
    ledc_set_duty(BL_MODE, BL_CHANNEL, duty);
    ledc_update_duty(BL_MODE, BL_CHANNEL);
}
