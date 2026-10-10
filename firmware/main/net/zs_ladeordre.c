#include "zs_ladeordre.h"

#include <math.h>
#include <string.h>

/*
 * Bruger tilstanden en rate? De to der goer, kan ikke blive registre
 * foer skalafaktoren er maalt - se headerens top.
 */
static bool bruger_rate(zs_lade_tilstand_t t)
{
    return t == ZS_LADE_OPLAD || t == ZS_LADE_AFLAD;
}

void zs_ladeordre_nulstil(zs_ladeordre_t *o)
{
    if (o == NULL) {
        return;
    }
    memset(o, 0, sizeof(*o));
}

bool zs_ladeordre_tilstand(zs_ladeordre_t *o, const char *navn, int64_t nu_ms)
{
    if (o == NULL) {
        return false;
    }
    zs_lade_tilstand_t ny;
    if (!zs_ladeplan_tilstand(navn, &ny)) {
        return false;       /* ukendt navn roerer ingenting */
    }
    /*
     * EN NY TILSTAND NULSTILLER DE ANDRE FELTER.
     *
     * Ellers kunne en ny tilstand blive parret med den FORRIGE plans
     * timer, og det er en ordre ingen har bedt om. Den samme tilstand
     * igen er derimod et livstegn: felterne beholdes, og uret stilles.
     */
    if (!o->har_tilstand || o->tilstand != ny) {
        o->har_sekunder = false;
        o->har_setpunkt = false;
    }
    o->tilstand = ny;
    o->har_tilstand = true;
    o->sidst_ms = nu_ms;
    return true;
}

bool zs_ladeordre_sekunder(zs_ladeordre_t *o, int32_t sek, int64_t nu_ms)
{
    if (o == NULL) {
        return false;
    }
    if (sek <= 0 || sek > ZS_LADE_DOEDMAND_MAKS_S) {
        return false;
    }
    o->sekunder = (uint16_t)sek;
    o->har_sekunder = true;
    o->sidst_ms = nu_ms;
    return true;
}

bool zs_ladeordre_setpunkt(zs_ladeordre_t *o, float kw, int64_t nu_ms)
{
    if (o == NULL) {
        return false;
    }
    /*
     * isnan og IKKE "kw != kw". Det er den samme proeve, men den sidste
     * udloeser -Wfloat-equal, og en advarsel man skal laere at se forbi,
     * er en advarsel der en dag bliver set forbi naar den betyder noget.
     *
     * isinf staar her ogsaa selvom graensen nedenfor ville fange
     * uendelig: reglen "et tal vi ikke kan forstaa, afvises" hoerer ét
     * sted, ikke spredt ud over to.
     */
    if (isnan(kw) || isinf(kw)) {
        return false;
    }
    if (kw < -(float)ZS_ORDRE_SETPUNKT_MAKS_KW ||
        kw > (float)ZS_ORDRE_SETPUNKT_MAKS_KW) {
        return false;
    }
    o->setpunkt_kw = kw;
    o->har_setpunkt = true;
    o->sidst_ms = nu_ms;
    return true;
}

zs_ordre_stand_t zs_ladeordre_stand(const zs_ladeordre_t *o, int64_t nu_ms)
{
    if (o == NULL || !o->har_tilstand) {
        return ZS_ORDRE_INGEN;
    }
    if (!o->har_sekunder) {
        return ZS_ORDRE_UFULD;
    }
    /* Setpunktet kraeves KUN af de tilstande der ser paa raten. */
    if (bruger_rate(o->tilstand) && !o->har_setpunkt) {
        return ZS_ORDRE_UFULD;
    }
    /*
     * DOEDMANDSKNAPPEN, paa det monotone ur. Udloebet efterses FOER
     * skalaen: en udloebet ordre er udloebet, uanset om vi kunne have
     * regnet den om.
     *
     * >= og ikke >: staar der 600 sekunder, er ordren doed NAAR de 600
     * er gaaet, ikke et millisekund efter. Ved en graense der afgoer om
     * et batteri staar fastlaast, vaelges den strenge ende.
     */
    if (nu_ms - o->sidst_ms >= (int64_t)o->sekunder * 1000) {
        return ZS_ORDRE_UDLOEBET;
    }
    if (bruger_rate(o->tilstand)) {
        return ZS_ORDRE_MANGLER_SKALA;
    }
    return ZS_ORDRE_KLAR;
}

bool zs_ladeordre_plan(const zs_ladeordre_t *o, int64_t nu_ms,
                       zs_ladeplan_t *ud)
{
    if (o == NULL || ud == NULL) {
        return false;
    }
    if (zs_ladeordre_stand(o, nu_ms) != ZS_ORDRE_KLAR) {
        return false;
    }
    /*
     * Raten gives som 0: standen er KLAR, og de tilstande der naar
     * hertil er netop dem der IKKE ser paa raten. zs_ladeplan_lav
     * afviser selv en rate til en tilstand der ikke bruger den, saa
     * reglen staar ét sted og haandhaeves det samme sted.
     */
    return zs_ladeplan_lav(ud, o->tilstand, 0, o->sekunder);
}

const char *zs_ladeordre_stand_navn(zs_ordre_stand_t stand)
{
    switch (stand) {
    case ZS_ORDRE_INGEN:          return "ingen";
    case ZS_ORDRE_UFULD:          return "ufuld";
    case ZS_ORDRE_UDLOEBET:       return "udloebet";
    case ZS_ORDRE_MANGLER_SKALA:  return "mangler skala";
    case ZS_ORDRE_KLAR:           return "klar";
    }
    /*
     * Ingen default: i switchen, med vilje. Med -Wswitch-enum siger
     * oversaetteren fra hvis der kommer en ny stand og den ikke bliver
     * haandteret - og et default ville netop skjule det. Et tal uden for
     * enummen lander her i stedet.
     */
    return "ukendt";
}
