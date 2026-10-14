#include "zs_ladeplan.h"

#include <string.h>

/*
 * Reserven, MinRsvPct, i noternes raa tal.
 *
 * Den staar for sig og IKKE i samme skala som raten, selvom begge tal
 * ligner hundrededele procent. Det er to forskellige stoerrelser: raten
 * er procent af EFFEKT, reserven er procent af KAPACITET, og SunSpec kan
 * have hver sin skalafaktor paa dem. Punkt 2 paa maalelisten gaelder dem
 * begge, hver for sig.
 */
#define RESERVE_HOEJ    9900    /* skubber mod ladning, noternes 99 %  */
#define RESERVE_LAV      500    /* "tving ikke ladning", noternes 5 %  */

/*
 * Et registerfelt er ENTEN gulvet - altsaa -rate - ELLER et fast tal.
 *
 * Foer stod de to ting i samme kolonne med et flag ved siden af, og saa
 * var den faste vaerdi DOED i de raekker hvor gulvet laa. Maalt 9.
 * oktober 2026: oplads og zero exports faste OutWRte kunne aendres fra 0
 * til 12345 uden at ÉN af 80 maalinger flyttede sig. Og vaerre: en der
 * laeste tabellen, saa "oplad OutWRte 0", mens noterne siger -rate.
 *
 * Nu staar det i feltet selv, saa tabellen kan laeses linje for linje mod
 * noterne og ingen celle kan vaere pynt.
 */
typedef struct {
    bool    gulv;   /* true: vaerdien er -rate              */
    int32_t fast;   /* ellers det her. ULAEST naar gulv=true */
} felt_t;

#define GULV        { .gulv = true,  .fast = 0 }
#define FAST(v)     { .gulv = false, .fast = (v) }

/*
 * Tabellen fra docs/batteristyring.md, tilstand for tilstand.
 *
 * `bruger_rate` siger om tilstanden overhovedet ser paa raten. Gjorde vi
 * ikke forskel, ville en rate givet til "normal" blive kastet vaek i
 * tavshed, og kalderen ville tro han havde sat en effekt.
 */
typedef struct {
    zs_lade_tilstand_t tilstand;
    int32_t stor_ctl_mod;
    int32_t min_rsv_pct;
    bool    bruger_rate;
    felt_t  ud;             /* OutWRte */
    felt_t  ind;            /* InWRte  */
    const char *navn;
} raekke_t;

static const raekke_t TABEL[] = {
    /* Oplad: gulvet paa OutWRte, og MinRsvPct 99 % skubber mod ladning */
    { ZS_LADE_OPLAD, 3, RESERVE_HOEJ, true,
      GULV, FAST(ZS_LADE_FULD_RATE), "oplad" },
    /* Aflad: spejlvendt, gulvet paa InWRte */
    { ZS_LADE_AFLAD, 3, RESERVE_LAV, true,
      FAST(ZS_LADE_FULD_RATE), GULV, "aflad" },
    /*
     * Normal: StorCtl_Mod 0 giver inverteren tilbage til sin egen
     * automatik. De tre andre NULSTILLES, saa et gulv fra en tidligere
     * kommando ikke bliver staaende.
     */
    { ZS_LADE_NORMAL, 0, RESERVE_LAV, false,
      FAST(ZS_LADE_FULD_RATE), FAST(ZS_LADE_FULD_RATE), "normal" },
    /*
     * Zero export: samme registre som oplad, med fuld rate. Se noten i
     * headeren om hvad det IKKE er. Raten skal gives som 0, fordi
     * tilstanden ikke laeser den - fulde rate er indbygget her.
     */
    { ZS_LADE_ZERO_EXPORT, 3, RESERVE_HOEJ, false,
      GULV, FAST(ZS_LADE_FULD_RATE), "zeroExport" },
    /*
     * Laas: OutWRte 0 blokerer AL afladning, baade til installationen og
     * til nettet. MinRsvPct 5 % betyder "tving ikke opladning", saa den
     * ikke koeber fra nettet. InWRte 10000 er intet ladeloft, saa solen
     * maa lade frit.
     */
    { ZS_LADE_LAAS, 3, RESERVE_LAV, false,
      FAST(0), FAST(ZS_LADE_FULD_RATE), "laas" },
};

static const raekke_t *find(zs_lade_tilstand_t t)
{
    for (size_t i = 0; i < sizeof(TABEL) / sizeof(TABEL[0]); i++) {
        if (TABEL[i].tilstand == t) {
            return &TABEL[i];
        }
    }
    return NULL;
}

static int32_t vaerdi(felt_t f, int32_t rate)
{
    return f.gulv ? -rate : f.fast;
}

bool zs_ladeplan_lav(zs_ladeplan_t *ud, zs_lade_tilstand_t tilstand,
                     int32_t rate, uint16_t doedmand_s)
{
    if (ud == NULL) {
        return false;
    }
    /*
     * DOEDMANDSKNAPPEN FOERST, BEGGE ENDER. Uden en timer bliver der
     * ingen plan, og med en der raekker ind i naeste kvarter heller
     * ikke: en mistet forbindelse maa ikke kunne efterlade en kundes
     * batteri fastlaast. Reglen staar her og ikke i en kommentar, saa
     * den ikke kan glemmes.
     */
    if (doedmand_s == 0 || doedmand_s > ZS_LADE_DOEDMAND_MAKS_S) {
        return false;
    }
    const raekke_t *r = find(tilstand);
    if (r == NULL) {
        return false;
    }
    if (rate < 0 || rate > ZS_LADE_FULD_RATE) {
        return false;
    }
    /*
     * En rate til en tilstand der ikke bruger den, er en fejl og ikke
     * noget vi retter i tavshed. Kalderen tror han har sat en effekt.
     */
    if (!r->bruger_rate && rate != 0) {
        return false;
    }
    /*
     * Og nul til en tilstand der SKAL have en rate, er lige saa slemt.
     * Den gav en plan der lignede "ingenting" og var alt andet - se
     * maalingen i headeren. Vil man ingenting, er det ZS_LADE_NORMAL.
     */
    if (r->bruger_rate && rate == 0) {
        return false;
    }

    /*
     * memset FOERST, og den er ikke til pynt: strukturen er 20 bytes mens
     * felterne kun fylder 18, og de to udfyldningsbytes bliver aldrig
     * skrevet af linjerne nedenfor. Maalt 9. oktober 2026: uden den her
     * linje gav memcmp paa to ENS planer "forskellig", fordi affaldet fra
     * kalderens stak laa tilbage i hullet. Ingen proeve paa felterne kan
     * se det, saa grunden staar her.
     */
    memset(ud, 0, sizeof(*ud));
    ud->stor_ctl_mod = r->stor_ctl_mod;
    ud->min_rsv_pct  = r->min_rsv_pct;
    ud->doedmand_s   = doedmand_s;

    /* Tilstande uden rate har den fulde rate indbygget, se tabellen. */
    int32_t virksom = r->bruger_rate ? rate : ZS_LADE_FULD_RATE;
    ud->out_w_rte = vaerdi(r->ud, virksom);
    ud->in_w_rte  = vaerdi(r->ind, virksom);
    return true;
}

const char *zs_ladeplan_navn(zs_lade_tilstand_t tilstand)
{
    const raekke_t *r = find(tilstand);
    return r ? r->navn : "ukendt";
}

bool zs_ladeplan_tilstand(const char *navn, zs_lade_tilstand_t *ud)
{
    if (navn == NULL || ud == NULL) {
        return false;
    }
    /*
     * strcmp og ikke strncmp: "oplad" og "opladning" maa ikke vaere det
     * samme. Og ingen tolerance for store bogstaver eller mellemrum -
     * serveren godtager kun den praecise streng, saa firmwaren skal ikke
     * vaere mere tilgivende end den.
     */
    for (size_t i = 0; i < sizeof(TABEL) / sizeof(TABEL[0]); i++) {
        if (strcmp(TABEL[i].navn, navn) == 0) {
            *ud = TABEL[i].tilstand;
            return true;
        }
    }
    return false;
}

/*
 * Kvitteringernes navne. Samme liste som paa serveren, og de staar i
 * samme raekkefoelge som enummen, saa de ikke kan komme ud af trit.
 */
static const char *const KVIT_NAVNE[] = {
    "ok", "doedmand", "tilstand", "ud", "ind", "reserve", "ingenSvar",
};

zs_lade_kvit_t zs_ladeplan_kvitter(const zs_ladeplan_t *oensket,
                                   const zs_ladeplan_t *laest)
{
    /*
     * Intet svar daekker ogsaa det kalderen ikke burde goere. Et manglende
     * oensket ER en programfejl, men svaret maa aldrig blive OK af den
     * grund: det eneste der maa give OK, er fire tal og en timer der
     * stemmer.
     */
    if (oensket == NULL || laest == NULL) {
        return ZS_LADE_KVIT_INTET_SVAR;
    }
    /* Timeren foerst. Se headeren: den er den ene der ikke maa fejle. */
    if (laest->doedmand_s != oensket->doedmand_s) {
        return ZS_LADE_KVIT_DOEDMAND;
    }
    if (laest->stor_ctl_mod != oensket->stor_ctl_mod) {
        return ZS_LADE_KVIT_TILSTAND;
    }
    if (laest->out_w_rte != oensket->out_w_rte) {
        return ZS_LADE_KVIT_UD;
    }
    if (laest->in_w_rte != oensket->in_w_rte) {
        return ZS_LADE_KVIT_IND;
    }
    if (laest->min_rsv_pct != oensket->min_rsv_pct) {
        return ZS_LADE_KVIT_RESERVE;
    }
    return ZS_LADE_KVIT_OK;
}

const char *zs_ladeplan_kvit_navn(zs_lade_kvit_t kvit)
{
    if ((int)kvit < 0 ||
        (size_t)kvit >= sizeof(KVIT_NAVNE) / sizeof(KVIT_NAVNE[0])) {
        return "ukendt";
    }
    return KVIT_NAVNE[kvit];
}
