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
/* start_pct er den gemte lysstyrke. Baglyset taender direkte paa den,
 * saa der ikke er et spring fra standardvaerdien. */
void zs_display_init(uint8_t start_pct);

/* Lysstyrke i procent, 5 til 100. Under 5 er skaermen i praksis sort,
 * og en skaerm brugeren ikke kan se er en skaerm brugeren ikke kan
 * skrue op igen. Derfor er 5 bunden, ogsaa om natten. */
void zs_display_set_brightness(uint8_t pct);
uint8_t zs_display_get_brightness(void);

/*
 * Er klokken inde i natperioden, altsaa mellem ZS_NIGHT_START_HOUR og
 * ZS_NIGHT_END_HOUR.
 *
 * Den staar HER fordi natten allerede var defineret her, til baglyset.
 * Alarmen skal tie i samme timer som skaermen daemper, og to steder der
 * hver har deres egen idé om hvornaar det er nat, ville med sikkerhed
 * komme ud af trit en dag hvor nogen retter det ene tal.
 *
 * Giver FALSK naar uret ikke er sat. I praksis kan det kun ske paa en
 * skaerm der aldrig har haft internet, og den er ikke sat op endnu, saa
 * der er ingen alarm der kan tie.
 *
 * Svarer uanset om natdaempningen er slaaet til: det er et spoergsmaal
 * om klokken, ikke om en indstilling.
 */
bool zs_display_er_nat(void);

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
 * Lad alarmen overtage baglyset.
 *
 * Kaldes af fejlsiden naar dens animation starter og stopper - IKKE af
 * app-loekken. Ejerskabet foelger dermed animationen praecis: niveauet
 * kommer kun derfra, og naar siden forlades, stopper animationen og
 * giver baglyset tilbage i samme aandedrag.
 *
 * Det var foer app-loekkens opgave, og saa skulle hver gren i den huske
 * at slukke. Én gren gjorde ikke: trykkede kunden "Vælg netværk",
 * forlod loekken aldrig den gren hvor der blev slukket, og skaermen
 * pulserede gennem hele opsaetningen - midt i det hun gjorde for at
 * reparere det. Nu kan det ikke ske, for den der taender er den samme
 * der slukker.
 *
 * Mens alarmen ejer baglyset, rOErer zs_display_tick, skyderen og et
 * tryk kun MAALET, aldrig lysstyrken. Saa er der én skriver.
 */
void zs_display_set_alarm(bool on);

/*
 * Pulsens hoejde: 0 nederst, 255 paa den lysstyrke kunden har valgt.
 *
 * Kaldes fra fejlsidens animation, ét kald pr. billede, med den SAMME
 * vaerdi der saetter farven paa fladen. Derfor aander lys og farve
 * sammen - to ure ville glide fra hinanden i loebet af minutter.
 *
 * Bunden er ZS_ALARM_DIM_PCT af kundens lysstyrke, ikke et fast tal:
 * staar skaermen paa 20 om aftenen, pulserer den mellem 20 og 7, ikke op
 * til 100.
 *
 * GOER INGENTING hvis alarmen ikke ejer baglyset. Saa kan et sent kald
 * fra en animation der er ved at blive stoppet, ikke efterlade
 * lysstyrken et tilfaeldigt sted.
 */
void zs_display_alarm_niveau(uint8_t hoejde);

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
