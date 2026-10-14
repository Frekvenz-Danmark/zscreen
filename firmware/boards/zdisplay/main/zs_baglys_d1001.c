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
     * HER KALDTE VI bsp_display_brightness_init() FOERST, og det var
     * forkert.
     *
     * Den er allerede kaldt af bsp_display_new() inde i lv_port_init(),
     * og opstarten kalder lv_port_init() foer zs_display_init(). Mit
     * ekstra kald var ment som en sikring mod den modsatte
     * raekkefoelge, men det er ikke harmloest: LEDC saetter de samme
     * fire ben op igen, og paa enheden gav det fire advarsler i loggen
     * ved hver opstart:
     *
     *   W ledc: GPIO 14 is not usable, maybe conflict with others
     *   W ledc: GPIO 22 is not usable, maybe conflict with others
     *   W ledc: GPIO 36 is not usable, maybe conflict with others
     *   W ledc: GPIO 23 is not usable, maybe conflict with others
     *
     * 14 er baglyset, 22, 36 og 23 er den trefarvede lysdiode. Fire
     * advarsler i et produkt der skal koere i aarevis er fire linjer
     * man skal laere at se forbi, og det er praecis saadan en rigtig
     * advarsel bliver overset.
     *
     * I stedet spoerger vi bare om lysstyrken KAN saettes.
     * bsp_display_brightness_set() svarer ESP_FAIL hvis opsaetningen
     * ikke er sket, saa en forkert raekkefoelge bliver opdaget her - med
     * en besked der siger hvad der er galt.
     */
    s_klar = true;
    esp_err_t err = bsp_display_brightness_set((int)start_pct);
    if (err != ESP_OK) {
        s_klar = false;
        ESP_LOGE(TAG, "baglyset svarer ikke (%s). Er lv_port_init() kaldt "
                      "foer zs_display_init()?", esp_err_to_name(err));
        return false;
    }

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
