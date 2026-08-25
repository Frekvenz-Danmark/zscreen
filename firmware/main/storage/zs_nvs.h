/*
 * zScreen - gemte indstillinger.
 *
 * Alt hvad skaermen skal huske hen over en stroemafbrydelse ligger i
 * ESP32'ens NVS-omraade: hvilket wifi den er paa, hvilken inverter den
 * laeser fra, og hvor lys den skal vaere.
 *
 * SIKKERHED, som skal med naar der skal saelges:
 *   Wifi-kodeordet ligger i klartekst i flash, som det goer paa
 *   praktisk talt alt IoT-udstyr fra hylden. Enhver der kan skille
 *   kabinettet ad og saette en programmer paa, kan laese det.
 *   Foer serieproduktion skal flash-kryptering og sikker opstart slaas
 *   til. Vi goer det ikke i udviklingsfasen, fordi det braender
 *   sikringer i chippen der ikke kan braendes tilbage, og saa kan
 *   boardet ikke bruges til at proeve ting af paa.
 *   Fremgangsmaaden staar i docs/hardware.md.
 */

#ifndef ZS_NVS_H
#define ZS_NVS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wifi-navne kan vaere 32 tegn, kodeord op til 63. Plus plads til
 * afslutningen. Tallene kommer fra wifi-standarden, ikke fra et gaet. */
#define ZS_SSID_MAX     33
#define ZS_PASS_MAX     65
#define ZS_IP_MAX       16      /* "255.255.255.255" plus afslutning */

typedef struct {
    char     wifi_ssid[ZS_SSID_MAX];
    char     wifi_pass[ZS_PASS_MAX];

    char     inverter_ip[ZS_IP_MAX];
    uint16_t inverter_port;
    uint8_t  inverter_unit;

    /* Vender elmaalerens fortegn. Se noten i zs_fronius.h om hvorfor
     * det er en indstilling og ikke en konstant. */
    bool     meter_import_positive;

    /* Prisområde: "DK1" vest for Storebælt, "DK2" øst for. Tomt
     * betyder at kunden ikke har valgt, og så vises prissiden ikke. */
    char     price_zone[4];

    uint8_t  brightness;        /* 5 til 100                          */
    bool     night_dimming;

    /* Er skaermen sat op? Er den ikke, starter vi i opsaetningen i
     * stedet for at vise fire tomme kort. */
    bool     configured;
} zs_settings_t;

/* Fylder s med standardvaerdier. Bruges naar der ikke er gemt noget,
 * og naar der nulstilles. */
void zs_nvs_defaults(zs_settings_t *s);

/*
 * Laeser de gemte indstillinger.
 *
 * Er der ikke gemt noget, eller er noget af det ulaeseligt, faar man
 * standardvaerdier og false retur. Skaermen starter saa i opsaetningen
 * i stedet for at gaa i staa.
 */
bool zs_nvs_load(zs_settings_t *s);

/* Gemmer. Returnerer false hvis det ikke lykkedes at skrive. */
bool zs_nvs_save(const zs_settings_t *s);

/*
 * Sletter alt og gaar tilbage til fabriksindstillinger.
 * Kraever en bekraeftelse i brugerfladen foerst: efter dette skal
 * skaermen saettes op forfra, inklusive wifi.
 */
bool zs_nvs_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_NVS_H */
