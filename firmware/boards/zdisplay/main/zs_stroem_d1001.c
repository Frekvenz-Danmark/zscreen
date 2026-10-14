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
 * ER DER NOGET I STIKKET. Det er hele spoergsmaalet.
 *
 * HER STOD 3500 mV, og det var forkert taenkt: det laeste som en
 * indstillet graense, altsaa et tal nogen skulle vedligeholde og som
 * kunne vaere forkert paa den naeste enhed. Reglen er ikke "hvor hoej
 * er spaendingen" - reglen er "er kablet i", og hvor laenge det har
 * vaeret ude. Tiden staar i ZS_FEJL_STROEM_GRAENSE_MS, og DEN er
 * beslutningen.
 *
 * Tallet herunder er ikke indstillet, og der er ikke noget at
 * indstille. Maalt paa enheden:
 *
 *     kabel i:   4650 mV
 *     kabel ude: naer nul
 *
 * Der er altsaa ingen vaerdier i naerheden af hinanden, og enhver
 * graense mellem de to opfoerer sig ens. 1000 mV er valgt fordi det
 * ligger langt fra begge. Seeed bruger selv et spring paa 2000 mV til
 * at se at stikket har aendret sig, se loekken i
 * bsp_battery_manage_start.
 *
 * GRUNDEN TIL AT DER STAAR ET TAL HER OVERHOVEDET: benet er
 * BSP_USB_INSERT_DET, GPIO 17, og det er det SAMME ben som
 * ADC1_CHANNEL_1. Det er en spaendingsdeler og ikke en digital linje,
 * saa der er ikke et ja/nej at laese. Havde der vaeret, stod der
 * ingen millivolt her.
 */
#define STIK_TOMT_MV  1000

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
    ud->paa_stroem = (ud->stik_mv >= STIK_TOMT_MV);

    /*
     * Procenten er -1 indtil der er maalt. BSP'ens
     * bsp_battery_percent_read() regner paa bsp_bat_volt, og den staar
     * paa nul indtil opgaven har vaeret rundt én gang - saa et 0 %
     * ville komme frem paa skaermen i det foerste sekund efter en
     * opstart og se ud som et doeende batteri.
     */
    ud->procent = (ud->batteri_mv > 0) ? bsp_battery_percent_read() : -1;
}
