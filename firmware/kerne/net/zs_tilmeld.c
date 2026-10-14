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

/*
 * Er koden loebet ud? ÉT sted, saa de to kaldere ikke kan komme til at
 * svare forskelligt paa det samme spoergsmaal.
 *
 * Den stod foer begge steder. Mutationstesten pegede paa det: mutanten
 * ">=" -> ">" i min_igen kunne ikke slaas ihjel, fordi regnestykket
 * bagefter alligevel gav nul ved praecis udloeb. Grænsen var altsaa
 * proevet ét sted og ikke det andet, og de to kunne skride fra hinanden
 * uden at nogen saa det.
 */
static bool udloebet(const zs_tilmeld_t *s, int64_t nu_ms)
{
    return nu_ms >= s->doer_ms;
}

zs_tilmeld_vej_t zs_tilmeld_vejen(zs_wifi_state_t net,
                                  zs_fleet_state_t flaade)
{
    if (net != ZS_WIFI_CONNECTED) {
        /*
         * Ikke engang en IP. ZS_WIFI_CONNECTED saettes paa
         * IP_EVENT_STA_GOT_IP, ikke naar radioen har fundet
         * adgangspunktet, saa det her er "der er en vej ud af huset".
         * Det er den ene gang hvor det hjaelper at gaa ind i
         * netvaerksindstillingerne.
         */
        return ZS_TILMELD_VEJ_INTET_NET;
    }
    switch (flaade) {
    case ZS_FLEET_READY:
        return ZS_TILMELD_VEJ_FLAADE;

    case ZS_FLEET_REJECTED:
    case ZS_FLEET_OFF:
        /*
         * De to gaar IKKE vaek af sig selv. OFF betyder at der ikke er
         * noget certifikat paa enheden, altsaa at den aldrig blev gjort
         * klar hos Frekvenz, og REJECTED at serveren siger nej til det
         * certifikat der er. Begge kraever et menneske hos Frekvenz, og
         * en montoer der venter paa dem, venter for evigt.
         */
        return ZS_TILMELD_VEJ_NEJ;

    case ZS_FLEET_CONNECTING:
    case ZS_FLEET_ENROLLING:
    case ZS_FLEET_ERROR:
        /*
         * ERROR er med her, og det er med vilje. Modulet proever igen af
         * sig selv efter ZS_FLEET_RECONNECT_MS, saa tilstanden er "paa
         * vej" og ikke "gik i staa". Maalt paa jern 11. oktober 2026:
         * foerste forsoeg faldt kl. 17,5 sekunder, og nummer to kom ind
         * fjorten sekunder senere uden at nogen roerte skaermen.
         */
        return ZS_TILMELD_VEJ_VENTER;
    }
    /*
     * En tilstand vi ikke kender. Lov ingenting: en kode serveren ikke
     * kender, er vaerre end ingen kode.
     */
    return ZS_TILMELD_VEJ_NEJ;
}

/*
 * Vejen oversat til det skaermen skal vise, naar der ikke er en kode.
 *
 * Hver vej har sin egen besked, fordi hver vej kraever noget forskelligt
 * af montoeren. Se noten ved zs_tilmeld_vej_t.
 */
static zs_tilmeld_stand_t vejens_stand(zs_tilmeld_vej_t vej)
{
    switch (vej) {
    case ZS_TILMELD_VEJ_INTET_NET: return ZS_TILMELD_MANGLER_NET;
    case ZS_TILMELD_VEJ_VENTER:    return ZS_TILMELD_VENTER_PAA_SERVER;
    case ZS_TILMELD_VEJ_NEJ:       return ZS_TILMELD_SERVER_NEJ;
    case ZS_TILMELD_VEJ_FLAADE:    break;
    }
    /*
     * En vej vi ikke kender, behandles som den vaerste af dem: ingen
     * kode, og sig det. Et ukendt tal her maa ALDRIG blive en kode
     * serveren ikke kender - saa ville montoeren proeve den forgaeves.
     */
    return ZS_TILMELD_SERVER_NEJ;
}

/* Er vejen helt fremme? ÉT sted, saa stand og bed_om ikke kan komme til
 * at svare forskelligt paa om der maa laves en kode. */
static bool fremme(zs_tilmeld_vej_t vej)
{
    return vej == ZS_TILMELD_VEJ_FLAADE;
}

zs_tilmeld_stand_t zs_tilmeld_stand(const zs_tilmeld_t *s,
                                    bool inverter_fundet,
                                    zs_tilmeld_vej_t vej,
                                    int64_t nu_ms)
{
    if (s == NULL) {
        return ZS_TILMELD_MANGLER_INVERTER;
    }
    if (!inverter_fundet) {
        return ZS_TILMELD_MANGLER_INVERTER;
    }
    if (!fremme(vej)) {
        return vejens_stand(vej);
    }
    if (!s->har_kode) {
        /* Ingen endnu. Det er ikke en fejl, der er bare ikke bedt om en. */
        return ZS_TILMELD_UDLOEBET;
    }
    if (udloebet(s, nu_ms)) {
        return ZS_TILMELD_UDLOEBET;
    }
    return ZS_TILMELD_KLAR;
}

bool zs_tilmeld_bed_om(zs_tilmeld_t *s, bool inverter_fundet,
                       zs_tilmeld_vej_t vej, int64_t nu_ms,
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
    if (!inverter_fundet || !fremme(vej)) {
        return false;
    }
    /* Er der en gyldig, bliver den staaende. Ellers ville koden skifte
     * under montoeren hver gang skaermen tegnede sig selv om. */
    if (s->har_kode && !udloebet(s, nu_ms)) {
        return true;
    }
    if (!zs_tilmeld_kode(s->kode, sizeof(s->kode), tilf, tilf_len)) {
        return false;
    }
    s->doer_ms  = nu_ms + (int64_t)ZS_TILMELD_LEVETID_MIN * 60 * 1000;
    s->har_kode = true;
    return true;
}

int zs_tilmeld_min_igen(const zs_tilmeld_t *s, int64_t nu_ms)
{
    if (s == NULL || !s->har_kode || udloebet(s, nu_ms)) {
        return 0;
    }
    int64_t igen_ms = s->doer_ms - nu_ms;
    /* Rundet OP: 61 sekunder er "2 minutter" og ikke "1". Se headeren. */
    return (int)((igen_ms + 59999) / 60000);
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

const char *zs_tilmeld_kode_nu(const zs_tilmeld_t *s,
                               bool inverter_fundet, zs_tilmeld_vej_t vej,
                               int64_t nu_ms)
{
    if (zs_tilmeld_stand(s, inverter_fundet, vej, nu_ms)
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

bool zs_tilmeld_hjem_nu(bool aaben, int64_t nu_ms, int64_t vis_ms,
                        int64_t *uret)
{
    if (uret == NULL) {
        return false;
    }
    if (!aaben) {
        /* Uret maaler tid PAA SIDEN. Se noten i headeren for hvad det
         * kostede at maale tid siden navnet kom i stedet. */
        *uret = 0;
        return false;
    }
    if (*uret == 0) {
        /*
         * Nul betyder "ikke startet", saa et ur der tilfaeldigvis
         * starter paa millisekund nul ville blive startet igen ved
         * naeste tik. Det er 0 ms efter opstart, og da er der ingen
         * flaade og derfor intet navn, saa det kan ikke naas. Men
         * bufferen skal ikke staa paa en tilfaeldighed: nu_ms kan ikke
         * vaere nul HER, for saa var vi faldet i den foerste gren ved
         * forrige tik.
         */
        *uret = (nu_ms == 0) ? 1 : nu_ms;
        return false;
    }
    return (nu_ms - *uret) > vis_ms;
}
