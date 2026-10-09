#include "zs_tilmeld.h"

#include <string.h>

/*
 * Alfabetet: Douglas Crockfords base32. Cifrene og de store bogstaver,
 * men UDEN I, L, O og U.
 *
 * To grunde, og den foerste er den vigtigste.
 *
 * 1. INGEN SKAEVHED. 32 er en potens af to, saa fem bit bliver ét tegn
 *    uden rest. Havde vi taget alle 36 tegn og brugt modulo, ville de
 *    foerste tegn i alfabetet komme lidt oftere end de sidste. Det er
 *    ikke en stor skaevhed, men den er gratis at undgaa, og en kode der
 *    ikke er jaevnt fordelt er svagere end sin laengde lover.
 *
 * 2. INGEN FORVEKSLING. Koden bliver LAEST paa en skaerm og SKREVET et
 *    andet sted af et menneske. 0 og O, 1 og I og L ligner hinanden i
 *    enhver skrifttype, og U bliver V i haandskrift. De fire er ude.
 *
 * Det koster: 32 i stedet for 36 giver en milliard muligheder i stedet
 * for 2,2 milliarder. Det er i orden, fordi laengden ALDRIG var det der
 * holdt koden taet. Udloeb, engangsbrug, at den kun virker mens skaermen
 * viser den, og at serveren lukker ned efter faa forsoeg - DET er
 * vagterne. Se headeren.
 */
static const char ALFABET[32] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

bool zs_tilmeld_kode(char *ud, size_t ud_len,
                     const uint8_t *tilf, size_t tilf_len)
{
    if (ud == NULL || ud_len < (size_t)ZS_TILMELD_KODE_LEN + 1
        || tilf == NULL || tilf_len < (size_t)ZS_TILMELD_TILF_BYTES) {
        return false;
    }

    /*
     * Fire bytes samles til ét 32 bit tal, og der tages fem bit ad
     * gangen fra oven. Seks tegn bruger 30 bit, og de sidste to bit
     * bliver ikke brugt. Det er med vilje: brugte vi dem til et syvende
     * halvt tegn, ville det tegn vaere skaevt.
     */
    uint32_t bits = ((uint32_t)tilf[0] << 24) | ((uint32_t)tilf[1] << 16)
                  | ((uint32_t)tilf[2] << 8)  | (uint32_t)tilf[3];

    for (int i = 0; i < ZS_TILMELD_KODE_LEN; i++) {
        unsigned skub = (unsigned)(27 - i * 5);      /* 27, 22, ... 2 */
        ud[i] = ALFABET[(bits >> skub) & 0x1Fu];
    }
    ud[ZS_TILMELD_KODE_LEN] = '\0';
    return true;
}

zs_tilmeld_stand_t zs_tilmeld_stand(const zs_tilmeld_t *s,
                                    bool inverter_fundet, bool paa_flaaden,
                                    int64_t nu_ms)
{
    if (s == NULL) {
        return ZS_TILMELD_MANGLER_INVERTER;
    }
    /*
     * Tilmeldt gaar FOERST. Er kunden oprettet, er der ikke mere at
     * vise, og det gaelder ogsaa hvis inverteren senere forsvinder: en
     * skaerm der mister sin inverter i fem minutter, skal ikke begynde
     * at tilbyde en ny tilmelding af en kunde der allerede findes.
     */
    if (s->tilmeldt) {
        return ZS_TILMELD_TILMELDT;
    }
    if (!inverter_fundet) {
        return ZS_TILMELD_MANGLER_INVERTER;
    }
    if (!paa_flaaden) {
        return ZS_TILMELD_MANGLER_FLAADE;
    }
    if (!s->har_kode) {
        /* Ingen endnu. Det er ikke en fejl, der er bare ikke bedt om en. */
        return ZS_TILMELD_UDLOEBET;
    }
    if (nu_ms >= s->doer_ms) {
        return ZS_TILMELD_UDLOEBET;
    }
    return ZS_TILMELD_KLAR;
}

bool zs_tilmeld_bed_om(zs_tilmeld_t *s, bool inverter_fundet,
                       bool paa_flaaden, int64_t nu_ms,
                       const uint8_t *tilf, size_t tilf_len)
{
    if (s == NULL) {
        return false;
    }
    /*
     * Forudsaetningerne tjekkes FOER der laves en kode. En kode lavet
     * uden inverter eller uden flaade, ville staa paa skaermen og se
     * rigtig ud, mens serveren ikke kendte den. Montoeren ville proeve
     * den, faa nej, og lede efter fejlen et helt andet sted.
     */
    if (s->tilmeldt || !inverter_fundet || !paa_flaaden) {
        return false;
    }
    /* Er der en gyldig, bliver den staaende. Ellers ville koden skifte
     * under montoeren hver gang skaermen tegnede sig selv om. */
    if (s->har_kode && nu_ms < s->doer_ms) {
        return true;
    }
    if (!zs_tilmeld_kode(s->kode, sizeof(s->kode), tilf, tilf_len)) {
        return false;
    }
    s->doer_ms  = nu_ms + (int64_t)ZS_TILMELD_LEVETID_MIN * 60 * 1000;
    s->har_kode = true;
    return true;
}

void zs_tilmeld_ryd(zs_tilmeld_t *s)
{
    if (s == NULL) {
        return;
    }
    /* Hele bufferen nulstilles, ikke kun det foerste tegn: koden skal
     * ikke ligge og kunne laeses i RAM bagefter. */
    memset(s->kode, 0, sizeof(s->kode));
    s->doer_ms  = 0;
    s->har_kode = false;
}

void zs_tilmeld_tilmeldt(zs_tilmeld_t *s)
{
    if (s == NULL) {
        return;
    }
    zs_tilmeld_ryd(s);
    s->tilmeldt = true;
}

const char *zs_tilmeld_kode_nu(const zs_tilmeld_t *s,
                               bool inverter_fundet, bool paa_flaaden,
                               int64_t nu_ms)
{
    if (zs_tilmeld_stand(s, inverter_fundet, paa_flaaden, nu_ms)
        != ZS_TILMELD_KLAR) {
        return "";
    }
    return s->kode;
}

bool zs_tilmeld_skal_sende(zs_tilmeld_sendt_t *s, const char *kode)
{
    if (s == NULL || kode == NULL) {
        return false;
    }
    if (s->sagt && strncmp(s->sidst, kode, sizeof(s->sidst)) == 0) {
        return false;           /* uaendret, og serveren ved det */
    }
    /* strncpy med haanden paa nulbyten: koden er kort og kendt, men en
     * afkortet kode ville vaere en kode montoeren proever forgaeves. */
    size_t n = strlen(kode);
    if (n >= sizeof(s->sidst)) {
        return false;           /* kan ikke vaere en kode vi har lavet */
    }
    memcpy(s->sidst, kode, n + 1);
    s->sagt = true;
    return true;
}

void zs_tilmeld_sendt_nulstil(zs_tilmeld_sendt_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s->sidst, 0, sizeof(s->sidst));
    s->sagt = false;
}
