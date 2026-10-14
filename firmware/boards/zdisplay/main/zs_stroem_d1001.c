/*
 * Stroemmen og batteriet paa reTerminal D1001, altsaa ZDisplay.
 *
 * Seeeds BSP maaler det allerede. bsp_battery_manage_start() er kaldt
 * af bsp_i2c_init-vejen i opstarten og koerer en egen opgave, der
 * laeser to ADC-kanaler i en loekke:
 *
 *     bsp_usb_volt = bsp_adc_read(ADC_CHANNEL_1) * 2;   stikket
 *     bsp_bat_volt = bsp_adc_read(ADC_CHANNEL_2) * 2;   batteriet
 *
 * Derfor laeser vi ikke hardware her, vi henter bare det sidste tal.
 * Kaldet koster ingenting og kan staa i app-loekken.
 *
 * ── HVORFOR SPAENDINGEN OG IKKE LADESTATUS ──
 *
 * Det naermeste man kommer et "er der stroem"-flag i BSP'en er
 *
 *     bool bsp_battery_charge_status_read(void)
 *
 * og den maa IKKE bruges til det. Den laeser ladekredsens ben, og
 * kredsen holder op med at lade naar batteriet er fuldt. Det staar
 * sort paa hvidt i loggen paa den enhed vi har:
 *
 *     W (2713) ESP32_P4_EV: battery high, disable charge
 *
 * Et fuldt batteri paa stroem melder altsaa "lader ikke". Brugte vi
 * den, ville hver skaerm med fuldt batteri blinke "tilslut stroem"
 * mens den sad i stikket - og saa holder folk op med at tro paa
 * blinket. Det er den dyreste fejl en alarm kan lave.
 *
 * Spaendingen paa stikket siger det den skal: er der noget i stikket.
 */

#include "zs_stroem.h"

#include "esp_log.h"
#include "esp32_p4_re_terminal_d1001.h"

#include <string.h>

static const char *TAG = "stroem";

/*
 * GRAENSEN FOR "DER ER STROEM PAA".
 *
 * USB er 5 V, altsaa omkring 5000 mV, og uden noget i stikket laeser
 * ADC'en naer nul. Graensen er sat lavt med vilje: et langt eller
 * tyndt kabel giver et spaendingsfald, og en skaerm der melder
 * stroemsvigt fordi kablet er daarligt, er vaerre end en der ikke
 * melder det. 3500 mV ligger langt under enhver fungerende USB og
 * langt over et tomt stik.
 *
 * Tallet er efterproevet paa enheden - se zs_stroem_log.
 */
#define STIK_GRAENSE_MV  3500

bool zs_stroem_init(void)
{
    /*
     * Ingenting at saette i gang: BSP'ens egen opgave koerer fra
     * opstarten. Vi laeser ÉN gang og skriver det i loggen, saa de
     * raa tal staar der naar graensen ovenfor skal efterproeves.
     */
    zs_stroem_t s;
    zs_stroem_laes(&s);
    ESP_LOGI(TAG, "batteri %d %%, stik %d mV, batteri %d mV, %s%s",
             s.procent, s.stik_mv, s.batteri_mv,
             s.paa_stroem ? "paa stroem" : "PAA BATTERI",
             s.lader ? ", lader" : "");
    return true;
}

void zs_stroem_laes(zs_stroem_t *ud)
{
    if (ud == NULL) {
        return;
    }
    memset(ud, 0, sizeof(*ud));
    ud->findes = true;

    ud->stik_mv    = bsp_usb_voltage_read();
    ud->batteri_mv = bsp_battery_voltage_read();
    ud->lader      = bsp_battery_charge_status_read();
    ud->paa_stroem = (ud->stik_mv >= STIK_GRAENSE_MV);

    /*
     * Procenten er -1 indtil der er maalt. BSP'ens
     * bsp_battery_percent_read() regner paa bsp_bat_volt, og den staar
     * paa nul indtil opgaven har vaeret rundt én gang - saa et 0 %
     * ville komme frem paa skaermen i det foerste sekund efter en
     * opstart og se ud som et doeende batteri.
     */
    ud->procent = (ud->batteri_mv > 0) ? bsp_battery_percent_read() : -1;
}
