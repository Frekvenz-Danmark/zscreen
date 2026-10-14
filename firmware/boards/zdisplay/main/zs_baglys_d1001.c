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

    /*
     * OG SAA SLUKKES LYSDIODEN.
     *
     * Der sidder en trefarvet lysdiode ved kameraet, paa GPIO 22, 36 og
     * 23. Den lyste paa hver enhed fra opstart, og den lyste GROENT ved
     * siden af kameraet - saa det ser ud som om kameraet optager. Det
     * goer det ikke. En skaerm der haenger i et hjem og ser ud som om
     * den filmer, er ikke en kosmetisk fejl.
     *
     * HVOR DET KOMMER FRA. Seeeds bsp_display_brightness_init()
     * opretter de tre LEDC-kanaler med
     *
     *     .duty = 100
     *
     * og lysdioden er AKTIV LAV:
     *
     *     duty_cycle = (1024 * (100 - percent)) / 100
     *
     * Altsaa er duty 100 ud af 1024 ikke "10 % lys", det er omkring 90
     * %. Og ingenting slukker dem bagefter: bsp_led_*_set() findes, men
     * BSP'en kalder dem ikke selv.
     *
     * Vi retter det HER og ikke i Seeeds fil. Submodulet skal kunne
     * opdateres uden at vores rettelser forsvinder - og det her er
     * desuden en beslutning om PRODUKTET: skal dioden bruges til noget
     * en dag, er det det her sted der skal give den lov.
     *
     * Svaret ses ikke efter med vilje: bsp_led_*_set returnerer void.
     * Virker LEDC ikke, er baglyset ovenfor allerede faldet igennem med
     * en besked.
     */
    bsp_led_red_set(false);
    bsp_led_green_set(false);
    bsp_led_blue_set(false);

    ESP_LOGI(TAG, "baglys klar, %u %%, og lysdioden ved kameraet er slukket",
             (unsigned)start_pct);
    return true;
}

void zs_baglys_set(uint8_t pct)
{
    if (!s_klar) {
        return;
    }

    /*
     * DEN GROENNE KNAP SLUKKER SKAERMEN, OG DET SKAL VI RESPEKTERE.
     *
     * Seeeds BSP saetter selv en haandtering paa knappen: ét tryk
     * saetter lysstyrken til 0 OG slukker panelet med
     * esp_lcd_panel_disp_on_off(disp_panel, false). Se
     * bsp_button_single_click_event_cb i deres kilde.
     *
     * Vores side ved ingenting om det. Reglerne i zs_display kalder
     * herned ved hvert tick - natdaempning, alarmpuls, opvaagning efter
     * et tryk - og det foerste af dem ville taende BAGLYSET igen paa et
     * panel der er slukket.
     *
     * Resultatet er det vaerste af to verdener: en skaerm der lyser
     * kraftigt og viser INGENTING. Kunden har slukket den, og den staar
     * og lyser hvidt paa vaeggen.
     *
     * bsp_backlight_get_enable_state() er deres egen maade at spoerge
     * om brugeren har slukket. Har de det, roerer vi ikke lyset.
     */
    if (!bsp_backlight_get_enable_state()) {
        return;
    }

    /* BSP'en klipper selv til 0-100 og regner om til 10 bits duty. */
    (void)bsp_display_brightness_set((int)pct);
}
