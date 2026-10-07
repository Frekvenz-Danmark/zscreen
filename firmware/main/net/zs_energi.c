/*
 * zScreen - timens energi. Se zs_energi.h for hvorfor.
 *
 * Ingenting herinde roerer net, ur eller lager. Det er med vilje: netop
 * den her regning er svaer at proeve af paa en skaerm, fordi man skulle
 * vente en time hver gang. Som ren funktion tager den et oejeblik.
 */

#include "zs_energi.h"

#include <string.h>

static const char *NAVNE[ZS_E_ANTAL] = {
    "energyProduced",
    "energyImported",
    "energyExported",
    "energyBatteryIn",
    "energyBatteryOut",
};

const char *zs_energi_felt_navn(zs_energi_felt_t f)
{
    return (f >= 0 && f < ZS_E_ANTAL) ? NAVNE[f] : "";
}

/* Saetter udgangspunktet til det vi staar med nu. */
static void saet_basis(zs_energi_basis_t *b, const float *taeller,
                       const bool *har, int time, uint16_t dagnr)
{
    b->gyldig = true;
    b->time   = (int8_t)time;
    b->dagnr  = dagnr;
    for (size_t i = 0; i < ZS_E_ANTAL; i++) {
        b->har[i] = har[i];
        b->wh[i]  = har[i] ? taeller[i] : 0.0f;
    }
}

zs_energi_t zs_energi_tik(zs_energi_basis_t *basis,
                          const float *taeller, const bool *har,
                          int time, uint16_t dagnr,
                          float *ud, bool *ud_har, int8_t *ud_time)
{
    if (basis == NULL || taeller == NULL || har == NULL
        || ud == NULL || ud_har == NULL || ud_time == NULL
        || time < 0 || time > 23) {
        return ZS_ENERGI_ARG;
    }

    if (!basis->gyldig) {
        saet_basis(basis, taeller, har, time, dagnr);
        return ZS_ENERGI_FOERSTE;
    }

    /* Samme time paa samme dag: intet nyt. */
    if (basis->time == (int8_t)time && basis->dagnr == dagnr) {
        return ZS_ENERGI_VENTER;
    }

    /*
     * Hvor mange timer er der gaaet?
     *
     * Baade timen og dagen skal med. Uden dagen ville et doegnskift fra
     * 23 til 0 se ud som 23 timer baglaens, og saa ville hver nat blive
     * sprunget over.
     */
    long gaaet = ((long)dagnr - (long)basis->dagnr) * 24L
               + ((long)time - (long)basis->time);

    if (gaaet != 1) {
        /*
         * Enten mere end én time, altsaa skaermen var vaek, eller et
         * skridt baglaens fordi uret blev stillet. I begge tilfaelde
         * starter vi forfra i stedet for at sende et tal vi ikke kan
         * staa inde for.
         */
        saet_basis(basis, taeller, har, time, dagnr);
        return ZS_ENERGI_HUL;
    }

    /* Præcis én time er gaaet. Så kan vi regne. */
    *ud_time = basis->time;
    for (size_t i = 0; i < ZS_E_ANTAL; i++) {
        ud[i] = 0.0f;
        ud_har[i] = false;
        if (!basis->har[i] || !har[i]) {
            continue;       /* vi havde den ikke, saa vi opfinder den ikke */
        }
        float d = taeller[i] - basis->wh[i];
        if (d < 0.0f) {
            /*
             * Taelleren er gaaet BAGLAENS.
             *
             * Det sker naar en acc32 loeber rundt efter cirka 4,29
             * milliarder wattimer, og naar en inverter bliver nulstillet
             * eller skiftet. Vi kan ikke skelne de to, og et tal paa fire
             * millioner kilowattimer i én time ville vaere aabenlyst
             * forkert paa en skaerm. Saa springer vi feltet over.
             */
            continue;
        }
        ud[i] = d;
        ud_har[i] = true;
    }

    saet_basis(basis, taeller, har, time, dagnr);
    return ZS_ENERGI_KLAR;
}
