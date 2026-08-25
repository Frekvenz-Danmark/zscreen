/*
 * zScreen - lysstyrke og natdaempning.
 *
 * Baglyset paa D1'eren sidder paa GPIO 45 og styres med PWM gennem
 * ESP32'ens LEDC-blok. Seeeds BSP kan kun taende og slukke, saa vi
 * saetter selv PWM'en op, paa samme maade som deres eget eksempel.
 *
 * Skaermen haenger paa en vaeg i en stue. Den skal kunne ses om dagen
 * og ikke lyse rummet op om aftenen, saa den daemper selv om natten.
 * Roerer man ved den, lyser den op igen i et minut og gaar saa tilbage.
 */

#ifndef ZS_DISPLAY_H
#define ZS_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Saetter PWM op og taender baglyset paa den gemte lysstyrke. */
void zs_display_init(void);

/* Lysstyrke i procent, 5 til 100. Under 5 er skaermen i praksis sort,
 * og en skaerm brugeren ikke kan se er en skaerm brugeren ikke kan
 * skrue op igen. Derfor er 5 bunden, ogsaa om natten. */
void zs_display_set_brightness(uint8_t pct);
uint8_t zs_display_get_brightness(void);

/* Slaar natdaempning til eller fra. */
void zs_display_set_night_dimming(bool enabled);
bool zs_display_get_night_dimming(void);

/*
 * Kaldes ved hvert tryk paa skaermen.
 *
 * Er skaermen daempet, lyser den op igen med det samme. Ellers goer den
 * ingenting. Skal vaere billig: den kaldes fra LVGL's inddatalaesning,
 * som koerer mange gange i sekundet.
 */
void zs_display_touch_wake(void);

/*
 * Kaldes cirka én gang i sekundet fra hovedloekken.
 *
 * Ser paa klokken og paa hvornaar der sidst blev roert ved skaermen, og
 * justerer lysstyrken derefter. Ligger her og ikke i en timer, saa der
 * kun er ét sted der bestemmer hvor lys skaermen er.
 */
void zs_display_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_DISPLAY_H */
