#include "zs_fejl.h"
#include "zs_config.h"

#include <string.h>

/*
 * De to ting holdes i hver sin boette, og det er HELE grunden til at
 * reglen blev flyttet herind.
 *
 * Foerste udgave havde ét ur og én "hvad er galt nu"-vaerdi. Den slog
 * fejl paa en maade man ikke taenker over foer man ser den: er wifi vaek
 * i en time og kommer tilbage, saa er inverteren ogsaa vaek - den naas
 * jo over wifi - og med ét ur havde inverterens ur ogsaa koert i en
 * time. I det sekund wifi'et kom tilbage, sprang skaermen derfor fra
 * "ikke paa netvaerket" til "anlaegget svarer ikke", selvom inverteren
 * bare havde brug for to sekunder til at tage imod en forbindelse.
 *
 * Derfor: inverterens ur koerer KUN naar vi er paa nettet. Kan vi ikke
 * naa noget som helst, er det ikke anlaeggets skyld, og saa skal det
 * ikke have skudt i skoene.
 */

static bool sover(int32_t tilstand)
{
    /*
     * SunSpec's egne tal: 1 er slukket, 2 er sover. Begge betyder at
     * inverteren selv har besluttet at holde fri, og saa er det ikke en
     * fejl at den holder op med at svare.
     *
     * NUL er ukendt og taeller IKKE som soevn. Det er vigtigt: nul er
     * ogsaa hvad et felt staar paa naar det ikke blev laest, og hvad
     * vagten har foer den har set inverteren én gang. En skaerm der
     * lige er genstartet, maa ikke paastaa at anlaegget sover - den ved
     * det ikke. Se ogsaa noten om natten i zs_fejl_tik.
     */
    return tilstand == 1 || tilstand == 2;
}

void zs_fejl_init(zs_fejl_vagt_t *v)
{
    if (v == NULL) {
        return;
    }
    memset(v, 0, sizeof(*v));
    v->type = ZS_FEJL_INGEN;
}

void zs_fejl_glem_inverteren(zs_fejl_vagt_t *v)
{
    if (v == NULL) {
        return;
    }
    v->inv_tabt = false;
    v->inv_vaek_ms = 0;
    v->sidste_tilstand = 0;
    /*
     * Alarmen slippes ogsaa, saa bip-taelleren begynder forfra hvis det
     * nye valg heller ikke virker. Ellers ville en kunde der vaelger
     * igen og igen, kun blive bippet ad den foerste gang.
     */
    if (v->type == ZS_FEJL_INVERTER || v->type == ZS_FEJL_INVERTER_SOVER) {
        v->type = ZS_FEJL_INGEN;
        v->alarm = false;
    }
}

void zs_fejl_ser_ikke(zs_fejl_vagt_t *v, int64_t nu_ms)
{
    if (v == NULL) {
        return;
    }
    /*
     * Kun uret flyttes frem. Intet andet: hverken hvad der er galt,
     * hvor laenge det har vaeret galt, eller hvor mange gange der er
     * bippet. Den tid der gaar her, bliver bare ikke skrevet paa
     * inverterens regning - for vi spurgte den ikke.
     */
    v->har_tikket = true;
    v->sidst_tik_ms = nu_ms;
}

zs_fejl_ud_t zs_fejl_tik(zs_fejl_vagt_t *v, const zs_fejl_ind_t *ind,
                         int64_t nu_ms)
{
    zs_fejl_ud_t ud;
    memset(&ud, 0, sizeof(ud));
    ud.type = ZS_FEJL_INGEN;

    if (v == NULL || ind == NULL) {
        return ud;
    }

    /* Husk tilstanden mens vi KAN laese den. Det er den her vaerdi der
     * om et oejeblik afgoer om et tab er natten eller en fejl. */
    if (ind->inverter_svarer && ind->inverter_tilstand != 0) {
        v->sidste_tilstand = ind->inverter_tilstand;
    }

    /* ── wifi: eget ur ── */
    if (ind->paa_wifi) {
        v->wifi_tabt = false;
    } else if (!v->wifi_tabt) {
        v->wifi_tabt = true;
        v->wifi_tabt_ms = nu_ms;
    }

    /*
     * ── inverteren: en SUM, og den vokser kun mens der er net ──
     *
     * Summen nulstilles naar der ikke ER valgt en inverter. Ellers ville
     * en skaerm midt i sin foerste opsaetning begynde at taelle ned mod
     * en fejl om et anlaeg der endnu ikke er fundet.
     *
     * HVORFOR EN SUM OG IKKE ET TIDSPUNKT. To forsoeg gik forud:
     *
     * 1. Et tidspunkt, nulstillet ved hvert tik uden wifi. En router der
     *    faldt ud i to sekunder lidt oftere end hvert andet minut, holdt
     *    uret paa nul for evigt: et HELT doedt anlaeg blev aldrig meldt,
     *    og hverken side, blink eller lyd kom nogensinde.
     *
     * 2. Et tidspunkt, nulstillet naar nettet KOM TILBAGE. Bedre, men
     *    stadig forkert: et udfald hvert halve minut gav stadig en
     *    nulstilling hvert halve minut, og de to minutter blev aldrig
     *    naaet. Det stod sort paa hvidt i en proeve.
     *
     * Summen klarer begge: et udfald paa to sekunder koster to
     * sekunder. Og det oprindelige formaal holder stadig, for summen
     * vokser IKKE mens nettet er vaek - saa en inverter faar sine fulde
     * to minutter efter en lang wifi-nedtur, hvor den ikke havde en vej
     * at svare ad.
     *
     * Et spring i tiden - fx de op mod fire minutter hvor loekken staar
     * i en netvaerksscanning - taelles MED, og det er rigtigt: der var
     * net, og inverteren svarede ikke.
     *
     * Men det gaelder kun naar vi FAKTISK ser efter. I demoen og under
     * opsaetningen gOEr loekken ikke, og dér bliver fejl_tik slet ikke
     * kaldt. Et kvarters demo gav derfor ét tik med et kvarter i, og
     * fejlsiden sprang frem i samme sekund demoen sluttede. Loesningen
     * er ikke at klippe alle spring - saa ville en lovlig scanning ogsaa
     * blive snydt - men at appen SIGER det: se zs_fejl_ser_ikke.
     */
    int64_t siden_sidst = 0;
    if (v->har_tikket && nu_ms > v->sidst_tik_ms) {
        siden_sidst = nu_ms - v->sidst_tik_ms;
    }
    v->har_tikket = true;
    v->sidst_tik_ms = nu_ms;

    if (!ind->inverter_opsat || ind->inverter_svarer) {
        v->inv_tabt = false;
        v->inv_vaek_ms = 0;
    } else if (ind->paa_wifi) {
        if (!v->inv_tabt) {
            v->inv_tabt = true;
            v->inv_vaek_ms = 0;
        } else {
            v->inv_vaek_ms += siden_sidst;
        }
    }
    /* Uden wifi: summen staar stille, men inverteren er stadig vaek. */

    bool wifi_fejl = v->wifi_tabt
        && (nu_ms - v->wifi_tabt_ms >= ZS_FEJL_WIFI_GRAENSE_MS);
    bool inv_fejl = v->inv_tabt
        && (v->inv_vaek_ms >= ZS_FEJL_INVERTER_GRAENSE_MS);

    if (ind->demo) {
        ud.type = ZS_FEJL_INGEN;
    } else if (wifi_fejl) {
        /*
         * Wifi foerst. Er nettet vaek, er inverteren det ogsaa, og saa
         * er der kun én af de to beskeder der peger paa noget kunden kan
         * goere noget ved.
         */
        ud.type = ZS_FEJL_WIFI;
    } else if (inv_fejl) {
        /*
         * SOEVN HOLDER IKKE I AL EVIGHED.
         *
         * sidste_tilstand bliver ikke nyere af at tiden gaar. En
         * inverter der gik i stykker ét minut efter at have meldt
         * "sover", ser ud som en der sover - og blev meldt sovende for
         * evigt: graat maerke, intet blink, ingen lyd, ogsaa i juli.
         *
         * Efter ZS_FEJL_SOVER_MAKS_MS er det ikke solen laengere.
         */
        bool for_laenge = (v->inv_vaek_ms >= ZS_FEJL_SOVER_MAKS_MS);
        ud.type = (sover(v->sidste_tilstand) && !for_laenge)
                ? ZS_FEJL_INVERTER_SOVER : ZS_FEJL_INVERTER;
    } else {
        ud.type = ZS_FEJL_INGEN;
    }

    v->type = ud.type;

    /* ── blinket ── Reglen staar i zs_fejl_alvorlig, ÉT sted. ── */
    ud.blink = zs_fejl_alvorlig(ud.type);

    /*
     * ── lyden ──
     *
     * Natten tier HELT. Ogsaa naar vi ikke ved om inverteren sov:
     * blinket staar og venter paa fladen til næste morgen, og ingen
     * skal vaekkes klokken tre af noget de ikke kan lave om paa foer det
     * bliver lyst.
     *
     * Bemaerk at taelleren IKKE skrider frem mens der er stille. Derfor
     * kommer det foerste bip i det oejeblik natten slutter, hvis fejlen
     * stadig er der - og saa er der fem minutter til det naeste, praecis
     * som hvis fejlen var sket klokken syv. Havde takten vaeret regnet
     * fra fejlens begyndelse i stedet, ville alle fire bip blive sendt
     * paa under et sekund klokken seks om morgenen.
     */
    static const int32_t pauser[] = ZS_LYD_PAUSER_MS;
    const int antal = (int)(sizeof(pauser) / sizeof(pauser[0]));

    if (!ud.blink) {
        v->alarm = false;
    } else if (!v->alarm) {
        /* Alarmen begynder nu. */
        v->alarm = true;
        v->lyd_nr = 0;
        v->naeste_lyd_ms = nu_ms;
    }

    if (ud.blink && !ind->nat && (int)v->lyd_nr < antal
        && nu_ms >= v->naeste_lyd_ms) {
        ud.lyd = true;
        v->naeste_lyd_ms = nu_ms + pauser[v->lyd_nr];
        v->lyd_nr++;
    }

    return ud;
}

/* ------------------------------------------------------------------ */
/* Teksterne                                                           */
/* ------------------------------------------------------------------ */
/*
 * Dansk uden fagsprog. Der staar ikke "Modbus", ikke "TCP" og ikke
 * "timeout" nogen steder: det er en skaerm i en stue, ikke en logfil.
 *
 * SOEVN HAR INGEN TEKSTER HER, og det er med vilje.
 *
 * Den havde dem foer, og siden tog over hele natten. Men siden har
 * hverken tilbageknap eller tandhjul - den er lavet til en fejl, hvor
 * der ikke er andet at gaa til - saa paa et anlaeg uden batteri var
 * PRISSIDEN og Indstillinger utilgaengelige fra solnedgang til
 * solopgang. Og prissiden er netop den der stadig har mening om natten:
 * i morgen har sine egne priser, uanset om solen er nede nu.
 *
 * Soevn staar derfor i STATUSLINJEN i stedet, som ZS_LINK_SOVER - samme
 * sted som alt andet om forbindelsen staar, og uden at spaerre noget.
 * Se zs_statusbar.h.
 *
 * Typen findes stadig, for den er det der slaar blinket og lyden FRA.
 * Det er hele dens arbejde.
 *
 * Og der staar hvad det BETYDER, ikke kun hvad der er sket. "Ingen
 * forbindelse" alene faar en kunde til at tro at skaermen er i stykker.
 * Det hun skal vide, er at tallene paa forsiden ikke er friske - det er
 * det eneste der har en foelge for hende.
 */

const char *zs_fejl_titel(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_WIFI:
    case ZS_FEJL_INVERTER:        return "Ingen forbindelse";
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return "";
}

const char *zs_fejl_overskrift(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_WIFI:            return "Skærmen er ikke på netværket";
    case ZS_FEJL_INVERTER:        return "Anlægget svarer ikke";
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return "";
}

const char *zs_fejl_forklaring(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_WIFI:
        return "Tallene fra anlægget kan ikke hentes, før skærmen er "
               "på netværket igen.";
    case ZS_FEJL_INVERTER:
        return "Skærmen er på netværket, men anlægget svarer ikke.";
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:
        break;
    }
    return "";
}

const char *zs_fejl_knap(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_WIFI:            return "Vælg netværk";
    case ZS_FEJL_INVERTER:        return "Søg efter anlægget";
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return "";
}

bool zs_fejl_alvorlig(zs_fejl_type_t t)
{
    /*
     * Soevn er ikke alvorligt. Der er ingenting at haste med naar solen
     * er nede, og en skaerm der blinker roedt hver aften er en skaerm man
     * holder op med at se paa.
     *
     * Reglen laa FOER inde i zs_fejl_tik, hvor den afgjorde blinket - og
     * saa var den ikke til at naa for siden, der skulle bruge den samme
     * regel til overskriftens FARVE. Resultatet var en roed overskrift
     * paa en side der sagde at anlaegget sov. Derfor er den her nu, og
     * begge spoerger den.
     */
    switch (t) {
    case ZS_FEJL_WIFI:
    case ZS_FEJL_INVERTER:        return true;
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return false;
}

const char *zs_fejl_navn(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_INGEN:           return "ingen";
    case ZS_FEJL_WIFI:            return "wifi";
    case ZS_FEJL_INVERTER:        return "inverter";
    case ZS_FEJL_INVERTER_SOVER:  return "inverter sover";
    }
    return "ukendt";
}
