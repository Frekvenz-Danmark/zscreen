/*
 * zScreen - ordren fra Platformen, holdt og sat paa tid. Fase E.
 *
 * HVAD DEN GOER, OG HVAD DEN IKKE GOER.
 *
 * Platformen skriver en plan paa batteriet hos OpenRemote, og serveren
 * baerer den over paa skaermens eget asset. Her kommer den ind som TRE
 * SEPARATE BESKEDER, én per felt:
 *
 *     ladeplanTilstand    "oplad" / "aflad" / "normal" / "zeroExport" / "laas"
 *     ladeplanSekunder    nulstillingstimeren, 1 til ZS_LADE_DOEDMAND_MAKS_S
 *     powerSetpoint       den oenskede effekt i KILOWATT, fortegnet
 *
 * Modulet samler dem, holder dem, og siger hvornaar der kan dannes en
 * plan. Det SENDER ingenting og laeser ingenting - det er ren regning,
 * og derfor kan hele beslutningen proeves paa en almindelig maskine.
 *
 * EN PLAN KAN IKKE BLANDES. DET ER DEN VIGTIGSTE REGEL HER.
 *
 * Felterne kommer hver for sig, og der er ingen raekkefoelge lovet. Kom
 * tilstanden "oplad" ind mens timeren stadig stod paa den FORRIGE plans
 * 900 sekunder, ville vi have en ordre ingen har bedt om - ny tilstand,
 * gammel timer. Og den slags kan ikke ses udefra: alle tre felter ser
 * gyldige ud hver for sig.
 *
 * Derfor: en NY tilstand nulstiller de to andre felter. Platformen skal
 * sende hele saettet. Sendes den samme tilstand igen, beholdes de - det
 * er et livstegn, ikke en ny plan.
 *
 * DOEDMANDSKNAPPEN ER REGNET AF DET MONOTONE UR.
 *
 * Udloebet er "sidste felt kom for mere end `sekunder` sekunder siden",
 * og tiden tages af uret der taeller fra opstart - IKKE af vaeguret. Et
 * NTP-spring flytter vaeguret med timer, og saa ville knappen slaa til
 * for tidligt eller aldrig. Det er samme fejlklasse som dengang
 * dagnummeret kom fra UTC og timen fra lokal tid, og den kostede 730
 * timers historik om aaret.
 *
 * OG EN UDLOEBET PLAN BLIVER IKKE SENDT NOGEN STEDER I DAG.
 *
 * Det skal staa hoejt, for "doedmandsknappen er bygget" er en farlig halv
 * sandhed. Modulet holder op med at give en plan naar tiden er gaaet.
 * Det skriver IKKE normal til inverteren, for der er ingen vej derud:
 * zs_fronius er skrivebeskyttet, og registeradresserne er aldrig fundet.
 * Batteriet er altsaa ikke beskyttet af det her endnu - kun regningen er
 * paa plads, saa den dag vejen aabnes, er det ikke ogsaa logikken der
 * skal findes.
 *
 * TO AF DE FEM TILSTANDE KAN IKKE BLIVE REGISTRE ENDNU.
 *
 * `oplad` og `aflad` bruger en rate, og raten kommer af et setpunkt i
 * KILOWATT. Registret vil have noternes raa tal, og omregningen kraever
 * skalafaktoren - punkt 2 paa maalelisten i docs/batteristyring.md, og
 * den er ikke maalt. Vi gaetter ikke paa en skala.
 *
 * Saa for de to giver modulet ZS_ORDRE_MANGLER_SKALA: ordren er modtaget
 * og gyldig, og den kan ikke blive til fire tal. De tre andre -
 * `normal`, `zeroExport` og `laas` - ser slet ikke paa raten, og de
 * bliver en hel plan.
 */

#ifndef ZS_LADEORDRE_H
#define ZS_LADEORDRE_H

#include <stdbool.h>
#include <stdint.h>

#include "zs_ladeplan.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Loftet paa setpunktet i kilowatt. Samme tal som paa serveren, hvor
 * grænsen staar som en constraint paa feltet og HAANDHAEVES - maalt 10.
 * oktober 2026: 99999, -99999 og 501 kW giver alle HTTP 406.
 *
 * Firmwaren efterser det ALLIGEVEL. Serverens graense er serverens, og
 * den kan aendres af en der ikke taenker paa skaermen. Et setpunkt vi
 * ikke kan forstaa, skal afvises her ogsaa.
 */
#define ZS_ORDRE_SETPUNKT_MAKS_KW   500

/* Hvor ordren staar. Raekkefoelgen er fra "intet" mod "klar". */
typedef enum {
    ZS_ORDRE_INGEN = 0,         /* der er ikke kommet noget            */
    ZS_ORDRE_UFULD,             /* der mangler et felt                 */
    ZS_ORDRE_UDLOEBET,          /* doedmandsknappen er gaaet           */
    ZS_ORDRE_MANGLER_SKALA,     /* gyldig, men raten kan ikke omregnes */
    ZS_ORDRE_KLAR,              /* en plan kan dannes                  */
} zs_ordre_stand_t;

typedef struct {
    bool               har_tilstand;
    bool               har_sekunder;
    bool               har_setpunkt;
    zs_lade_tilstand_t tilstand;
    uint16_t           sekunder;
    float              setpunkt_kw;
    int64_t            sidst_ms;    /* monotont, se headerens top */
} zs_ladeordre_t;

/* Tom ordre. Skal kaldes foer brug. */
void zs_ladeordre_nulstil(zs_ladeordre_t *o);

/*
 * Tilstanden som tekst, praecis som den staar paa serveren.
 *
 * Giver false hvis navnet ikke er ét af de fem, og roerer saa ingenting:
 * en ordre vi ikke forstaar, maa ikke kunne slaa den forrige i stykker.
 *
 * ER TILSTANDEN NY, NULSTILLES DE TO ANDRE FELTER. Se headerens top.
 */
bool zs_ladeordre_tilstand(zs_ladeordre_t *o, const char *navn, int64_t nu_ms);

/* Timeren i sekunder, 1 til ZS_LADE_DOEDMAND_MAKS_S. */
bool zs_ladeordre_sekunder(zs_ladeordre_t *o, int32_t sek, int64_t nu_ms);

/*
 * Setpunktet i kilowatt, fortegnet.
 *
 * Afviser NaN og uendelig. Maalt paa serversiden 10. oktober 2026: NaN
 * slap igennem HVER graensekontrol, fordi enhver sammenligning med NaN
 * er falsk. Den fejl skal ikke kunne komme ind her ad bagdoeren.
 */
bool zs_ladeordre_setpunkt(zs_ladeordre_t *o, float kw, int64_t nu_ms);

/* Hvor ordren staar lige nu. Aldrig to svar paa samme tilstand. */
zs_ordre_stand_t zs_ladeordre_stand(const zs_ladeordre_t *o, int64_t nu_ms);

/*
 * Danner planen, hvis den kan dannes.
 *
 * Giver true KUN naar standen er ZS_ORDRE_KLAR, og roerer ikke ud
 * ellers. Alt andet - ufuld, udloebet, manglende skala - er false, og
 * standen siger hvorfor.
 */
bool zs_ladeordre_plan(const zs_ladeordre_t *o, int64_t nu_ms,
                       zs_ladeplan_t *ud);

/* Standens navn, til en log. Aldrig NULL. */
const char *zs_ladeordre_stand_navn(zs_ordre_stand_t stand);

#ifdef __cplusplus
}
#endif

#endif /* ZS_LADEORDRE_H */
