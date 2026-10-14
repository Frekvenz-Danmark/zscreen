/*
 * Baglyset paa reTerminal D1001, altsaa ZDisplay.
 *
 * Modsat zScreen saetter vi ikke selv en PWM op. Benet er GPIO 14, men
 * stroemmen til baglyset sidder paa benudvideren (BSP_LCD_BACKLIGHT_EN),
 * og Seeeds BSP styrer baade det og LEDC-kanalen. Satte vi vores egen
 * timer op oveni, ville to kanaler skrive paa samme ben.
 *
 * Derfor er filen kort: den giver procenten videre.
 */

#include "zs_baglys.h"

#include "esp_log.h"
#include "esp32_p4_re_terminal_d1001.h"
#include "display.h"

static const char *TAG = "baglys";

static bool s_klar = false;

bool zs_baglys_init(uint8_t start_pct)
{
    /*
     * bsp_display_new() kalder den her i forvejen, og opstarten kalder
     * lv_port_init() foer zs_display_init(). Den staar alligevel, saa
     * filen ikke lydloest afhaenger af en raekkefoelge et andet sted:
     * at saette LEDC op igen er harmloest, og uden den ville et kald i
     * den modsatte raekkefoelge give et baglys der ikke kunne styres.
     */
    esp_err_t err = bsp_display_brightness_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke saette baglyset op: %s", esp_err_to_name(err));
        return false;
    }

    s_klar = true;
    zs_baglys_set(start_pct);
    ESP_LOGI(TAG, "baglys klar, %u %%", (unsigned)start_pct);
    return true;
}

void zs_baglys_set(uint8_t pct)
{
    if (!s_klar) {
        return;
    }
    /* BSP'en klipper selv til 0-100 og regner om til 10 bits duty. */
    (void)bsp_display_brightness_set((int)pct);
}
