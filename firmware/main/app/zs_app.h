/*
 * zScreen - den der holder styr paa det hele.
 *
 * Arbejdsdelingen er den samme som i Seeeds egne eksempler, bare med
 * faerre led:
 *
 *     zs_app   én opgave der laver alt det der tager tid: wifi,
 *              soegning efter inverter, og aflaesning hvert andet
 *              sekund. Den roerer aldrig LVGL direkte uden at tage
 *              laasen foerst.
 *
 *     zs_ui    tegner. Den roerer aldrig netvaerket og venter aldrig
 *              paa noget. Skal brugeren have noget til at ske, sender
 *              den en besked til zs_app og gaar videre.
 *
 * Det er derfor der er en koe imellem: trykker man paa "Forbind" mens
 * en wifi-scanning koerer, maa skaermen ikke fryse i to sekunder. Uden
 * den opdeling ville hvert eneste netvaerkskald blokere tegningen.
 */

#ifndef ZS_APP_H
#define ZS_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "zs_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZS_CMD_SETUP_CONTINUE = 0, /* "Kom i gang": brug gemt wifi hvis der er */
    ZS_CMD_WIFI_SCAN,         /* søg efter netværk                  */
    ZS_CMD_WIFI_CONNECT,      /* ssid og pass er udfyldt            */
    ZS_CMD_INVERTER_SCAN,     /* gennemgå undernettet               */
    ZS_CMD_INVERTER_SELECT,   /* ip er udfyldt                      */
    ZS_CMD_SETUP_RESTART,     /* gå tilbage til valg af netværk     */
    /*
     * Fortryd opsaetningen.
     *
     * HVORFOR DEN FINDES. Trykker en kunde paa Netvaerk i
     * indstillingerne og fortryder, skal de kunne komme hjem igen. Foer
     * foerte tilbage-knappen til velkomstsiden, hvor den eneste knap er
     * "Kom i gang", som foerer tilbage til netvaerkslisten. Saa var der
     * ingen vej hjem uden at gaa hele opsaetningen igennem, inklusive en
     * ny soegning efter inverteren.
     *
     * Og brugerfladen kan ikke bare vise hovedskaermen selv: appen staar
     * i ST_SETUP, hvor den hverken forbinder eller aflaeser, saa
     * skaermen ville staa doed. Derfor gaar fortryd gennem appen, som
     * ejer tilstanden.
     */
    ZS_CMD_SETUP_CANCEL,
    ZS_CMD_SET_PRICE_ZONE,    /* ssid bruges til "DK1" eller "DK2"   */
    ZS_CMD_SET_NIGHT_DIM,     /* flag                               */
    ZS_CMD_SET_METER_SIGN,    /* flag: positiv betyder køb          */
    ZS_CMD_SET_THEME,         /* u8 er zs_theme_mode_t              */
    ZS_CMD_DEMO_START,        /* vis hovedskærmen med opdigtede tal */
    ZS_CMD_DEMO_STOP,         /* tilbage til opsætning eller drift   */
    ZS_CMD_FACTORY_RESET,
    ZS_CMD_CHECK_UPDATE,      /* se efter ny firmware nu            */
    ZS_CMD_REBOOT,
} zs_cmd_type_t;

typedef struct {
    zs_cmd_type_t type;
    char          ssid[ZS_SSID_MAX];
    char          pass[ZS_PASS_MAX];
    char          ip[ZS_IP_MAX];
    uint8_t       u8;
    bool          flag;
} zs_cmd_t;

/*
 * Laeser de gemte indstillinger og saetter temaet.
 *
 * Skal kaldes FOER zs_display_init og zs_ui_init, saa skaermen taender
 * med kundens lysstyrke og tema med det samme.
 */
void zs_app_load_settings(void);

/* Den gemte lysstyrke. Kun rigtig efter zs_app_load_settings. */
uint8_t zs_app_saved_brightness(void);

/*
 * Lysstyrken fra skyderen. GAAR IKKE GENNEM KOEEN.
 *
 * HVORFOR. En skyder er en kontinuerlig kontrol: LVGL sender en
 * haendelse for hvert trin under et traek, saa et traek fra 5 til 100
 * giver op mod halvfems vaerdier paa under et sekund. Koeen holder otte,
 * og xQueueSend uden ventetid smider resten vaek uden at nogen ser det.
 *
 * Var den sidste vaerdi blandt dem der blev smidt vaek, stod skyderen paa
 * ét og skaermen lyste som noget andet. Og det gemte ogsaa.
 *
 * Her skrives i stedet ÉN plads, hvor nyeste vaerdi vinder. Hovedopgaven
 * ser efter den i hver runde og retter skaermen hvis den er anderledes
 * end den der staar nu. Ingen vaerdi kan gaa tabt, og der er ingen
 * koetrafik.
 *
 * Maa kaldes fra skaermopgaven. En enkelt justeret skrivning er udelelig
 * paa den her maskine, og der er kun én skriver.
 */
void zs_app_set_brightness(uint8_t pct);

/* Starter opgaven. Kaldes fra app_main efter at skaermen er klar. */
bool zs_app_start(void);

/*
 * Firmwarens version, fx "0.1.0".
 *
 * Kommer fra firmware/version.txt gennem den byggede fils egen
 * beskrivelse. Der er derfor kun ét sted at rette den, og det tal
 * skaermen viser er med sikkerhed det samme som opdateringen
 * sammenligner med.
 */
const char *zs_version(void);

/*
 * Sender en besked fra brugerfladen.
 *
 * Blokerer aldrig. Er koen fuld, kastes beskeden vaek og der
 * returneres false: det sker kun hvis nogen trykker mange gange i
 * traek, og saa er den foerste besked allerede paa vej.
 */
bool zs_app_send(const zs_cmd_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* ZS_APP_H */
