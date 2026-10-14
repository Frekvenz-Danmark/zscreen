/*
 * zScreen - ladeplanen, oversat til registervaerdier. Fase E.
 *
 * LAES DET HER FOERST.
 *
 * Modulet sender INGENTING. Det regner kun ud HVILKE vaerdier de fire
 * registre skulle have, hvis man ville saette en tilstand.
 *
 * HVOR LANGT VEJEN UD ER BYGGET, efterset 10. oktober 2026. Jeg havde
 * foerst skrevet at Modbus-laget ikke kunne skrive, og det var FORKERT:
 *
 *   zs_modbus_tcp   KAN skrive. zs_mb_build_write_request bygger en
 *                   FC16-ramme, zs_mb_parse_write_response laeser svaret,
 *                   og zs_mb_write_verified skriver og LAESER TILBAGE -
 *                   netop fordi Fronius svarer uden fejl ogsaa naar en
 *                   skrivning bliver afvist. Det er proevet i
 *                   test_modbus_skriv
 *   zs_fronius      er skrivebeskyttet. Den kalder ingen af dem, og det
 *                   staar i dens egen header: "Alt er skrivebeskyttet"
 *   ingen           kalder zs_mb_write_verified. Nul steder i hele
 *                   firmwaren
 *
 * Saa vejen findes, men den er ikke forbundet, og det bliver den indtil
 * fem ting er maalt paa rigtig hardware - se docs/batteristyring.md.
 *
 * DET HER ER DEN DEL DER KAN PROEVES UDEN EN INVERTER.
 *
 * Tabellen er Jakobs noter, givet 9. oktober 2026, og han sagde selv at
 * det ikke er sikkert det er rigtigt. Den staar som udgangspunkt. Men
 * selve OVERSAETTELSEN - tilstand plus rate bliver fire tal - er ren
 * regning, og den kan bevises paa en almindelig maskine. Saa den dag
 * nogen staar ved et anlaeg, er det kun ADRESSERNE og SKALAEN der skal
 * maales, ikke ogsaa logikken.
 *
 * HVAD DER IKKE STAAR HER, OG HVORFOR.
 *
 *   registeradresserne   OutWRte og InWRte er ALDRIG fundet, og vores
 *                        offsets er spec minus to. Et gaet paa en adresse
 *                        er en skrivning i et tilfaeldigt register
 *   skalafaktoren        10000 og 500 LIGNER hundrededele procent, men
 *                        det skal laeses af inverteren, ikke antages
 *   om negative tal kan skrives  og hvordan de kodes
 *
 * Tre ting vi ikke ved, og derfor er de her vaerdier RAA tal fra noterne
 * og ikke procenter. Modulet oversaetter ikke til noget det ikke kender.
 *
 * DOEDMANDSKNAPPEN ER ET KRAV, IKKE ET VALG.
 *
 * Besluttet 8. oktober 2026: nulstillingstimeren SKAL saettes paa hver
 * plan, saa batteriet gaar tilbage til normal hvis Platformen tier. En
 * mistet forbindelse maa ikke kunne efterlade en kundes batteri
 * fastlaast.
 *
 * Derfor NAEGTER zs_ladeplan_lav at lave en plan uden en timer. Reglen
 * staar i koden i stedet for i en kommentar, saa den ikke kan glemmes.
 * Timeren skal stadig FINDES i registrene og maales - det er punkt 4 paa
 * listen - men ingen plan kan forlade det her modul uden den.
 *
 * OG DEN SKAL HAVE ET LOFT, ikke kun et gulv. Foer stod der kun "over
 * nul", og maalt 9. oktober 2026 blev 65535 sekunder godtaget: 18 timer
 * og 12 minutter. En doedmandsknap der foerst slaar til i morgen, er
 * ingen doedmandsknap. Loftet staar nedenfor.
 */

#ifndef ZS_LADEPLAN_H
#define ZS_LADEPLAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Den fulde rate i noternes raa tal. 10000 ser ud som 100,00 procent,
 * men det er IKKE maalt - se headerens top.
 *
 * TALLET STAAR KUN HER. Foer stod 10000 ogsaa som literal i tabellen i
 * .c-filen, og maalt 9. oktober 2026: saettes den her til 1000, bliver
 * raten begraenset til 1000 mens InWRte stadig staar 10000. To skalaer i
 * samme struktur, uden en advarsel. Og punkt 2 paa maalelisten er
 * praecis "hvad er skalafaktoren", saa tallet SKAL kunne aendres ét sted.
 */
#define ZS_LADE_FULD_RATE   10000

/*
 * Loftet over nulstillingstimeren. VORES BESLUTNING, IKKE EN MAALING.
 *
 * 900 sekunder er ét kvarter, samme rytme som skaermens egne tal (se
 * zs_kvarter.c). En plan der lever laengere end det kvarter den blev
 * lavet i, kan ikke begrundes med noget vi ved. Skal den kunne vaere
 * laengere, skal nogen sige hvorfor - og saa staar det her, ikke i et
 * kald et sted ude i koden.
 *
 * Der er med vilje INGEN nedre graense ud over nul. En timer paa ét
 * sekund ville betyde at batteriet skifter mellem plan og normal hvert
 * sekund, og det er en rigtig driftsrisiko - men vi har ikke maalt
 * registret endnu, og et tal vi finder paa, er et gaet. Det staar paa
 * listen i docs/batteristyring.md i stedet.
 */
#define ZS_LADE_DOEDMAND_MAKS_S  900

/* Tilstandene. Numrene er noternes, saa de kan sammenlignes med dem. */
typedef enum {
    ZS_LADE_OPLAD = 1,
    ZS_LADE_AFLAD = 2,
    ZS_LADE_NORMAL = 3,
    /*
     * Tilstand 4, zero export: oplyst som "lad med 100 procent", altsaa
     * samme registre som oplad med fuld rate.
     *
     * DET ER IKKE ZERO EXPORT I STRENG FORSTAND. Er batteriet fuldt,
     * gaar overskuddet ud paa nettet alligevel. Vil man have en haard
     * graense paa hvad der sendes ud, uanset batteriets stand, er det et
     * andet register der skal findes for sig.
     */
    ZS_LADE_ZERO_EXPORT = 4,
    ZS_LADE_LAAS = 5,
} zs_lade_tilstand_t;

/* De fire registre, i raa vaerdier, plus timeren. */
typedef struct {
    int32_t  stor_ctl_mod;
    int32_t  out_w_rte;
    int32_t  in_w_rte;
    int32_t  min_rsv_pct;
    uint16_t doedmand_s;    /* nulstillingstimeren, altid > 0 */
} zs_ladeplan_t;

/*
 * Laver planen. Giver false og roerer ikke ud hvis noget ikke kan passe.
 *
 *   tilstand     en af ovenstaaende
 *   rate         den oenskede effekt i noternes raa tal.
 *                  oplad og aflad:  1 til ZS_LADE_FULD_RATE
 *                  alle andre:      0, for de ser ikke paa den
 *   doedmand_s   nulstillingstimeren i sekunder, 1 til
 *                ZS_LADE_DOEDMAND_MAKS_S
 *
 * Giver false hvis:
 *   - doedmand_s er nul, eller over loftet. Se headerens top: begge
 *     ender er en beslutning
 *   - tilstanden ikke findes
 *   - raten er uden for 0..ZS_LADE_FULD_RATE
 *   - der er givet en rate til en tilstand der IKKE bruger den. En rate
 *     der tavst blev kastet vaek, ville se ud som en plan der virkede
 *   - RATEN ER NUL til oplad eller aflad. Maalt 9. oktober 2026, og det
 *     var ikke til at se:
 *
 *       oplad med rate 0   gav OutWRte 0 og MinRsvPct 9900. Altsaa "al
 *                          afladning blokeret, og hold 99 % reserve" -
 *                          og noterne siger selv at 99 % skubber
 *                          inverteren mod at lade, altsaa KOEBE fra
 *                          nettet. Den der bad om nul effekt, fik en
 *                          plan der handler stroem
 *       aflad med rate 0   gav InWRte 0, altsaa "al LADNING blokeret".
 *                          Det modsatte af ingenting
 *
 *     Vil man ingenting, er det ZS_LADE_NORMAL. Det er den samme regel
 *     som ovenfor, blot den anden vej: et tal der ikke betyder hvad det
 *     ser ud til, er vaerre end et afslag.
 */
bool zs_ladeplan_lav(zs_ladeplan_t *ud, zs_lade_tilstand_t tilstand,
                     int32_t rate, uint16_t doedmand_s);

/*
 * Navnet. Aldrig NULL, og "ukendt" for en tilstand der ikke findes.
 *
 * DET ER OGSAA NAVNET PAA TRAADEN. Platformen skriver tilstanden som
 * tekst i et felt paa serveren, og serveren haandhaever en liste over
 * tilladte vaerdier - maalt 10. oktober 2026: den afviser baade "OPLAD",
 * "oplad " med et mellemrum og den tomme streng, med HTTP 406.
 *
 * Derfor er der ÉN stavemaade, og den kan staa i et felt: "zeroExport"
 * uden mellemrum. Foer stod der "zero export" fordi navnet kun skulle i
 * en log. To stavemaader af samme tilstand er en fejl der foerst viser
 * sig den dag nogen skriver den forkerte.
 *
 * Listen paa serveren staar i scripts/anlaeg.py i zScreen-server, og den
 * skal vaere PRAECIS de fem navne her. Begge sider har en proeve der
 * holder sin egen ende fast.
 */
const char *zs_ladeplan_navn(zs_lade_tilstand_t tilstand);

/*
 * Den anden vej: teksten fra Platformen bliver en tilstand.
 *
 * Giver false og roerer ikke ud hvis navnet ikke er ét af de fem. Der
 * bliver IKKE set bort fra store bogstaver eller mellemrum - serveren
 * godtager kun den praecise streng, og saa skal firmwaren ikke vaere
 * mere tilgivende end den. Et navn vi "retter" i tavshed, er en tilstand
 * kunden ikke bad om.
 */
bool zs_ladeplan_tilstand(const char *navn, zs_lade_tilstand_t *ud);

/*
 * KVITTERINGEN: blev planen faktisk sat?
 *
 * Besluttet 10. oktober 2026: enheden skal efterse at skrivningen blev
 * udfoert og melde tilbage. En skrivning der tavst ignoreres er den
 * vaerste udgave - det er punkt 5 paa maalelisten i
 * docs/batteristyring.md - og vi har en tilbagelaesning i Modbus-laget
 * der kan fange det.
 *
 * Det her er den del der kan proeves uden en inverter: SELVE DOMMEN.
 * Givet hvad vi ville saette, og hvad vi laeste tilbage, hvad er saa
 * svaret. Modulet laeser ikke selv noget.
 *
 * Navnene er ogsaa dem der staar paa serveren, af samme grund som
 * ovenfor. OpenRemotes egen executionStatus kunne IKKE bruges: dens
 * tilladte vaerdier er REQUEST_START, REQUEST_REPEATING, REQUEST_CANCEL,
 * READY, COMPLETED, RUNNING og CANCELLED - maalt 10. oktober 2026 - og
 * ingen af dem betyder "inverteren gjorde det ikke".
 */
typedef enum {
    ZS_LADE_KVIT_OK = 0,        /* alt stod som vi satte det        */
    ZS_LADE_KVIT_DOEDMAND,      /* timeren tog ikke. Vaerst af alle */
    ZS_LADE_KVIT_TILSTAND,      /* StorCtl_Mod                      */
    ZS_LADE_KVIT_UD,            /* OutWRte                          */
    ZS_LADE_KVIT_IND,           /* InWRte                           */
    ZS_LADE_KVIT_RESERVE,       /* MinRsvPct                        */
    ZS_LADE_KVIT_INTET_SVAR,    /* vi fik slet ikke laest tilbage    */
} zs_lade_kvit_t;

/*
 * Sammenligner den oenskede plan med den laeste.
 *
 *   laest == NULL   betyder "tilbagelaesningen lykkedes ikke", og svaret
 *                   er ZS_LADE_KVIT_INTET_SVAR
 *
 * Den giver ALDRIG OK uden at hvert felt stemmer. Det er hele pointen:
 * en kvittering man ikke kan stole paa, er vaerre end ingen.
 *
 * Doedmandsknappen efterses FOERST, fordi den er den ene der ikke maa
 * vaere forkert: er timeren ikke sat, kan en mistet forbindelse
 * efterlade batteriet fastlaast, uanset hvor rigtige de andre tre er.
 */
zs_lade_kvit_t zs_ladeplan_kvitter(const zs_ladeplan_t *oensket,
                                   const zs_ladeplan_t *laest);

/* Kvitteringens navn, til loggen og til serverens felt. Aldrig NULL. */
const char *zs_ladeplan_kvit_navn(zs_lade_kvit_t kvit);

#ifdef __cplusplus
}
#endif

#endif /* ZS_LADEPLAN_H */
