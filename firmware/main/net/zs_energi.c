/*
 * zScreen - timens energi. Se zs_energi.h for hvorfor.
 *
 * Ingenting herinde roerer net, ur eller lager. Det er med vilje: netop
 * den her regning er svaer at proeve af paa en skaerm, fordi man skulle
 * vente en time hver gang. Som ren funktion tager den et oejeblik.
 */

#include "zs_energi.h"
#include "zs_tekst.h"

#include <string.h>

static const char *NAVNE[ZS_E_ANTAL] = {
    "energyProduced",
    "energyImported",
    "energyExported",
    "energyBatteryIn",
    "energyBatteryOut",
};

bool zs_energi_time_ok(int time)
{
    return time >= 0 && time < ZS_ENERGI_TIMER_PR_DOEGN;
}

const char *zs_energi_felt_navn(zs_energi_felt_t f)
{
    return (f >= 0 && f < ZS_E_ANTAL) ? NAVNE[f] : "";
}

/* Saetter udgangspunktet til det vi staar med nu. */
static void saet_basis(zs_energi_basis_t *b, const float *taeller,
                       const bool *har, const char *serienr,
                       int time, uint16_t dagnr)
{
    b->gyldig = true;
    b->time   = (int8_t)time;
    b->dagnr  = dagnr;
    for (size_t i = 0; i < ZS_E_ANTAL; i++) {
        b->har[i] = har[i];
        b->wh[i]  = har[i] ? taeller[i] : 0.0f;
    }
    /* Hele bufferen nulstilles foerst. Den ligger i flashen, og to
     * serienumre af forskellig laengde maa ikke kunne blande sig saadan
     * at resten af det gamle staar tilbage bag nulbyten. */
    memset(b->serienr, 0, sizeof(b->serienr));
    zs_tekst_kopi(b->serienr, sizeof(b->serienr), serienr);
}

zs_energi_t zs_energi_tik(zs_energi_basis_t *basis,
                          const float *taeller, const bool *har,
                          const char *serienr,
                          int time, uint16_t dagnr,
                          float *ud, bool *ud_har, int8_t *ud_time)
{
    if (basis == NULL || taeller == NULL || har == NULL
        || ud == NULL || ud_har == NULL || ud_time == NULL
        || !zs_energi_time_ok(time)) {
        return ZS_ENERGI_ARG;
    }

    /*
     * Uden et serienummer ved vi ikke hvis taeller vi staar med, og saa
     * regner vi ikke. Det er ikke en teoretisk vagt: serienummeret
     * laeses ved hver forbindelse, saa er det tomt, er der noget galt,
     * og det er bedre at springe timen over end at tilskrive den en
     * inverter vi ikke kan navngive.
     */
    if (serienr == NULL || serienr[0] == '\0') {
        return ZS_ENERGI_ARG;
    }

    if (!basis->gyldig) {
        saet_basis(basis, taeller, har, serienr, time, dagnr);
        return ZS_ENERGI_FOERSTE;
    }

    /*
     * ER DET DEN SAMME INVERTER?
     *
     * Udgangspunktet ligger i flashen og overlever en genstart. Det
     * overlever ogsaa at kunden skifter inverter eller vaelger en anden i
     * indstillingerne, og saa ville den foerste time blive regnet som den
     * nye livstaeller minus den gamles. Se noten paa
     * zs_energi_basis_t.serienr for hvad det koster.
     */
    if (strncmp(basis->serienr, serienr, sizeof(basis->serienr)) != 0) {
        saet_basis(basis, taeller, har, serienr, time, dagnr);
        return ZS_ENERGI_NY_INVERTER;
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
        saet_basis(basis, taeller, har, serienr, time, dagnr);
        return ZS_ENERGI_HUL;
    }

    /* Præcis én time er gaaet. Så kan vi regne. */
    *ud_time = basis->time;
    for (size_t i = 0; i < ZS_E_ANTAL; i++) {
        ud[i] = 0.0f;
        ud_har[i] = false;
        /*
         * ELLER, ikke OG. Begge tidspunkter skal have taelleren.
         *
         * Det ser ud som en detalje og er det ikke. Havde vi den IKKE
         * sidste time og har den NU, saa staar der nul i udgangspunktet,
         * og forskellen bliver hele inverterens livstaeller. Den ville
         * blive sendt som én times energi, altsaa maaske tusinde
         * kilowattimer, og gemt paa serveren for evigt.
         *
         * Og det er ikke en fjern mulighed: elmaaleren sidder paa sin
         * EGEN Modbus-unit, se ZS_FR_METER_UNIT, og den kan sagtens vaere
         * uden svar i et oejeblik mens inverteren svarer fint. Saa
         * mangler koebt og solgt i én aflaesning og er der igen i den
         * naeste.
         *
         * Den anden vej, havde den og har den ikke, er mindre slem: saa
         * er taelleren nul, forskellen bliver negativ, og feltet bliver
         * sprunget over nedenfor. Men vi skal ikke vaere afhaengige af
         * det held.
         */
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

    saet_basis(basis, taeller, har, serienr, time, dagnr);
    return ZS_ENERGI_KLAR;
}
