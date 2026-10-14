/*
 * zScreen - hvad der er FORSKELLIGT fra maerke til maerke.
 *
 * HVORFOR DEN FINDES.
 *
 * SunSpec er en standard, og det meste af det vi laeser er ens paa alle
 * invertere der taler den: effekt, spaending, driftstilstand,
 * fejlflagene i Evt1, DCEvt og modellernes adresser. Det hoerer IKKE
 * hjemme her.
 *
 * Men standarden har ogsaa felter hvor producenten selv bestemmer
 * betydningen, og saa er to invertere ikke enige:
 *
 *   EvtVnd1 til EvtVnd4   producentens egne fejlbits. Bit 1 betyder
 *                         noget hos Fronius og noget andet hos de
 *                         andre.
 *   StVnd                 producentens egen driftstilstand.
 *   DC-kanalerne          hvordan MPPT-blokkene deles mellem solstrenge
 *                         og batteri, naar de ikke har navne.
 *
 * FEJLEN DET RETTER. Foer blev Fronius' fejltabeller brugt paa ENHVER
 * inverter der forbandt. En Huawei med bit 1 sat ville faa teksten
 * "Netfejl" og henvisningen "Fronius-kode 101" paa skaermen. Altsaa en
 * forkert fejl, skrevet med fuld sikkerhed. Det er vaerre end at sige at
 * vi ikke ved det.
 *
 * SAADAN UDVIDES DEN. Ét sted, én raekke. Du skal bruge producentens
 * egen Modbus-dokumentation, og du maa IKKE gaette: en forkert bittekst
 * er en loegn paa en vaeg hos en kunde. Kender vi ikke maerket, falder vi
 * tilbage paa standarden alene, og producentens egne felter vises raat
 * med den raa vaerdi. Det er altid rigtigt, bare mindre hjaelpsomt.
 */

#ifndef ZS_MAERKER_H
#define ZS_MAERKER_H

#include "zs_fronius_codes.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hvordan DC-kanalerne deles, naar de ikke har navne.
 *
 * Har kanalerne navne, bruger vi dem, og saa er det her ligegyldigt.
 * Det er SunSpecs egen maade og den er at foretraekke. Det her er kun
 * reserven.
 */
typedef enum {
    /* Vi kender ikke maerkets opdeling. Saa gaetter vi ikke: har
     * anlaegget et batteri og ingen navne, siger vi det ikke kan laeses. */
    ZS_DC_UKENDT = 0,
    /* De TO SIDSTE kanaler er batteriets, resten er solstrenge. Fronius
     * skriver det i deres Modbus-manual 42,0410,2649: "For devices with
     * a storage solution, there are two additional blocks (charging
     * (MPP3) and discharging (MPP4))." */
    ZS_DC_BATTERI_SIDST,
} zs_dc_layout_t;

typedef struct {
    /* Matches mod SunSpecs Mn-felt fra model 1. Sammenligningen ser
     * bort fra store og smaa bogstaver, for Fronius skriver selv baade
     * "Fronius" og "FRONIUS" alt efter firmwareudgave. */
    const char *maerke;

    zs_dc_layout_t dc_layout;

    /* Producentens egne fejlbits. NULL betyder at vi ikke har dem, og
     * saa vises bittene raat i stedet for med en opdigtet tekst. */
    const zs_bit_text_t *evtvnd1;  size_t n_evtvnd1;
    const zs_bit_text_t *evtvnd2;  size_t n_evtvnd2;
    const zs_bit_text_t *evtvnd3;  size_t n_evtvnd3;

    /* Hvad der staar foran koden paa skaermen, fx "Fronius-kode 101".
     * Kunden skal kunne slaa den op i producentens egen manual. */
    const char *kode_praefiks;
} zs_maerke_t;

/*
 * Finder maerket ud fra SunSpecs Mn-felt.
 *
 * Returnerer ALDRIG NULL. Kender vi ikke maerket, kommer der en raekke
 * uden tabeller og med ZS_DC_UKENDT, saa alt det standarden daekker
 * stadig virker, og resten vises raat.
 */
const zs_maerke_t *zs_maerke_find(const char *manufacturer);

/* Hvor mange maerker vi kender. Til proever. */
size_t zs_maerke_antal(void);

/* Maerke nummer i, eller NULL. Til proever. */
const zs_maerke_t *zs_maerke_nr(size_t i);

#ifdef __cplusplus
}
#endif

#endif /* ZS_MAERKER_H */
