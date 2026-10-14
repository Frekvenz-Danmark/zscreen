/*
 * zScreen - lyden. Ét kort bip naar noget er galt.
 *
 * HVORFOR DET ER SVAERERE END DET LYDER.
 *
 * Summeren sidder IKKE paa ESP32'en. Den sidder paa RP2040'eren, den
 * anden chip paa boardet, paa dens GPIO 19, og den er passiv: den skal
 * have en PWM-tone, ikke bare spaending. ESP32'en kan ikke naa den.
 *
 * Det eneste der gaar mellem de to chips, er en seriel ledning:
 *
 *     ESP32-S3 GPIO 19  (TX)  ->  RP2040 GPIO 17  (RX)
 *     ESP32-S3 GPIO 20  (RX)  <-  RP2040 GPIO 16  (TX)
 *     115200 baud, 8N1, pakker i COBS
 *
 * Et bip er altsaa en PAKKE vi sender til den anden chip, og saa er det
 * dens firmware der laver lyden. Det er ogsaa derfor der er to trin for
 * at faa lyd paa en skaerm: ZS_LYD_ENABLED i zs_config.h, OG den
 * firmware i tools/rp2040/. Fabrikkens egen RP2040-firmware svarer ikke
 * paa bip-kommandoen - den kender den, men kalder den aldrig.
 *
 * DE TO BEN ER IKKE USB. Paa en almindelig ESP32-S3 er GPIO 19 og 20
 * chippens USB-ben, og saa ville det her slaa baade konsollen og
 * flashningen ud. Paa D1'eren gaar de to ben til RP2040'eren, og der er
 * intet USB-stik paa dem: skaermen flashes over UART0. Derfor er
 * CONFIG_ESP_CONSOLE_SECONDARY_NONE sat i sdkconfig.defaults - ellers
 * holder USB-blokken fast paa benene, og pakken kommer aldrig ud.
 *
 * MODULET BLOKERER ALDRIG. Et bip er to toner med en pause imellem, og
 * pausen holdes af en esp_timer, ikke af en vTaskDelay. Kaldet kommer
 * fra app-loekken, som ogsaa skal passe et netvaerk og en skaerm.
 */

#ifndef ZS_LYD_H
#define ZS_LYD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pakketyperne i Seeeds egen protokol mellem de to chips. Tallene er
 * deres, ikke vores: de staar i deres esp32_rp2040.h og i RP2040'erens
 * egen firmware, og de maa derfor ikke vaelges om. */
#define ZS_RP_BEEP_ON    0xA1   /* med uint32 ms som nyttelast */
/*
 * SLUK SENDES ALDRIG, og det er ikke en forglemmelse.
 *
 * RP2040'eren slukker tonen selv naar de millisekunder vi bad om, er
 * gaaet. Sendte vi ogsaa et sluk, ville det kappes med dens egen
 * nedtaelling og kunne klippe en tone over. Den staar her fordi den er
 * en del af Seeeds protokol og den anden chip svarer paa den - ikke
 * fordi vi bruger den.
 */
#define ZS_RP_BEEP_OFF   0xA2

/*
 * Saetter UART'en op. Kaldes én gang ved opstart.
 *
 * Giver false hvis det ikke lykkedes, eller hvis lyden er slaaet fra i
 * byggningen. Det er ikke en fejl der skal standse noget: en skaerm
 * uden lyd blinker stadig.
 */
bool zs_lyd_init(void);

/*
 * Bipper ÉN gang, altsaa ZS_LYD_TONER korte toner.
 *
 * Blokerer ikke. Kaldes et nyt bip mens et er i gang, bliver det nye
 * ignoreret: to bip oven i hinanden bliver én lang ulyd, og det er
 * ikke det der er meningen.
 */
void zs_lyd_bip(void);

/* Er der lyd i den her byggning. Til loggen og til Indstillinger. */
bool zs_lyd_findes(void);

/*
 * Bygger pakken der skal ud paa ledningen: kommandobyte, nyttelast,
 * COBS, og nul til sidst som skilletegn.
 *
 * REN FUNKTION, og det er med vilje. Den roerer ingen hardware, saa
 * hele indpakningen kan proeves paa en Mac - se tests/host/test_lyd.c.
 * Det er praecis den slags der ellers kun kan proeves ved at sidde med
 * en logikanalysator paa to ben, og derfor aldrig bliver proevet.
 *
 * ms er tonens laengde. Nul betyder "ingen nyttelast", altsaa den korte
 * pakke Seeeds eget eksempel sender.
 *
 * Giver laengden i bytes, eller -1 hvis der ikke er plads eller ud er
 * NULL. Der skal vaere mindst 8 bytes.
 */
int zs_lyd_pakke(uint8_t *ud, size_t plads, uint8_t cmd, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* ZS_LYD_H */
