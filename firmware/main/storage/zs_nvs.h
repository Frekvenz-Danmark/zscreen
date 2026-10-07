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

#include "zs_energi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wifi-navne kan vaere 32 tegn, kodeord op til 63. Plus plads til
 * afslutningen. Tallene kommer fra wifi-standarden, ikke fra et gaet. */
#define ZS_SSID_MAX     33
#define ZS_PASS_MAX     65
#define ZS_IP_MAX       16      /* "255.255.255.255" plus afslutning */
/* SunSpec model 1 SN er 16 registre, altsaa 32 tegn, plus afslutning. */
#define ZS_SERIAL_MAX   33

typedef struct {
    char     wifi_ssid[ZS_SSID_MAX];
    char     wifi_pass[ZS_PASS_MAX];

    char     inverter_ip[ZS_IP_MAX];
    uint16_t inverter_port;
    uint8_t  inverter_unit;

    /*
     * Inverterens serienummer. DEN RIGTIGE IDENTITET.
     *
     * En IP-adresse er ikke en identitet. Routeren uddeler dem paa laan,
     * og naar inverteren har vaeret slukket laenge nok, kan den komme
     * tilbage paa en anden. Saa stod skaermen og bankede paa en adresse
     * hvor der ikke var nogen, for evigt, og kunden skulle selv ind i
     * indstillingerne og scanne forfra.
     *
     * Vaerre: adressen kan vaere givet til en ANDEN enhed imens. Saa
     * svarede der noget, og skaermen ville vise en fremmed inverters
     * tal som om de var kundens.
     *
     * Serienummeret kommer fra SunSpec model 1 felt SN og sidder i
     * hardwaren. Det laeses ved hver forbindelse og sammenlignes. Passer
     * det ikke, er det ikke vores inverter, uanset hvad adressen siger.
     *
     * Tomt betyder at vi ikke kender det endnu: enten en ny skaerm, eller
     * en der er opdateret fra en udgave der ikke gemte det. Saa tages det
     * fra den foerste inverter vi faar forbindelse til.
     */
    char     inverter_serial[ZS_SERIAL_MAX];

    /* Vender elmaalerens fortegn. Se noten i zs_fronius.h om hvorfor
     * det er en indstilling og ikke en konstant. */
    bool     meter_import_positive;

    /* Prisområde: "DK1" vest for Storebælt, "DK2" øst for. Tomt
     * betyder at kunden ikke har valgt, og så vises prissiden ikke. */
    char     price_zone[4];

    uint8_t  brightness;        /* 5 til 100                          */
    bool     night_dimming;

    /* 0 er moerkt, 1 er lyst. Gemt som tal og ikke som zs_theme_mode_t,
     * saa lageret ikke afhaenger af brugerfladens opregning. */
    uint8_t  theme;

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

/* ------------------------------------------------------------------ */
/* Timeenergiens udgangspunkt                                          */
/* ------------------------------------------------------------------ */

/*
 * Taellerstanden da den igangvaerende time begyndte.
 *
 * HVORFOR DEN SKAL I FLASHEN. En times forbrug er taelleren nu minus
 * taelleren ved timens start. Laa udgangspunktet kun i hukommelsen,
 * ville enhver genstart koste den igangvaerende time: en
 * stroemafbrydelse eller en firmwareopdatering klokken 14.30 ville give
 * et hul i historikken netop der.
 *
 * Gemt i flashen bliver timen 14 til 15 stadig rigtig, for inverterens
 * egne taellere har taelt videre imens.
 *
 * Det koster én skrivning i timen, altsaa cirka 8800 om aaret. NVS
 * fordeler slid selv, og flashen paa den her skaerm taaler
 * stoerrelsesordner mere.
 *
 * Gemmes i sit EGET navnerum, ikke sammen med kundens indstillinger. De
 * to har helt forskellig levetid: indstillingerne aendrer sig naesten
 * aldrig, det her hver time.
 */
bool zs_nvs_save_energi(const zs_energi_basis_t *b);

/* Laeser udgangspunktet. false betyder at der ikke er et, fx foerste
 * gang eller efter en nulstilling. Saa staar b som ugyldig. */
bool zs_nvs_load_energi(zs_energi_basis_t *b);

#ifdef __cplusplus
}
#endif

#endif /* ZS_NVS_H */
