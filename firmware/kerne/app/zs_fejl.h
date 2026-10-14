/*
 * zScreen - vagten der afgoer HVAD der er galt, og hvor meget der skal
 * raabes op om det.
 *
 * HVORFOR DEN ER SIT EGET MODUL.
 *
 * Reglen laa foer som tyve linjer inde i app-loekken (nonet_tik), og
 * den kunne kun proeves ved at tage wifi'et fra en rigtig skaerm og
 * sidde og vente tyve sekunder. Det blev den aldrig proevet med mere
 * end én gang, og den kunne kun én ting: wifi.
 *
 * Her er den en ren funktion. Ingen LVGL, intet netvaerk, ingen
 * esp_timer: den faar at vide hvordan verden ser ud, og svarer hvad
 * skaermen skal vise. Derfor kan hele reglen - ogsaa natten, ogsaa
 * bip-takten over tre kvarter - koeres igennem paa en Mac i
 * millisekunder. Se tests/host/test_fejl.c.
 *
 * Det er samme princip som i resten af projektet: beslutningen bor et
 * sted der kan maales, og kun det der RØRER hardware bliver tilbage i
 * app-loekken.
 *
 * DEN TAELLER TID I MILLISEKUNDER FRA OPSTART, ikke i klokketid.
 * Kalderen giver esp_timer_get_time()/1000. Det monotone ur bliver
 * ikke rykket af NTP, og en skaerm der faar sit ur sat ti minutter
 * tilbage midt i en fejl, skal ikke begynde forfra. Natten er den ene
 * ting der kommer fra klokken, og den kommer ind som et flag.
 */

#ifndef ZS_FEJL_H
#define ZS_FEJL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HVAD DER ER GALT. Raekkefoelgen er ogsaa prioriteringen.
 *
 * Der er med vilje FIRE og ikke to. "Inverteren svarer ikke" og
 * "inverteren sover" ser helt ens ud paa netvaerket - der kommer
 * ingenting i begge tilfaelde - men det ene er en fejl nogen skal gaa
 * hen og se paa, og det andet er tirsdag aften. Blandes de sammen,
 * blinker hver skaerm uden batteri roedt hver nat, og saa holder folk
 * op med at tro paa blinket. Det er den dyreste fejl en alarm kan
 * lave.
 */
typedef enum {
    ZS_FEJL_INGEN = 0,          /* alt virker                           */
    /*
     * Skaermen koerer paa batteri. Kun ZDisplay kan naa den her: den
     * har et batteri og bliver derfor ved med at vise tal som om alt
     * var i orden, mens den loeber toer. zScreen melder altid "paa
     * stroem", for koerer den, ER der stroem - se zs_stroem_ingen.c.
     *
     * Den staar FOERST af de alvorlige, fordi den er den eneste der
     * ender med en sort skaerm hvis ingen gOEr noget.
     */
    ZS_FEJL_INGEN_STROEM,
    ZS_FEJL_WIFI,               /* skaermen er ikke paa netvaerket       */
    ZS_FEJL_INVERTER,           /* paa nettet, men anlaegget svarer ikke */
    ZS_FEJL_INVERTER_SOVER,     /* forsvandt mens den selv meldte soevn  */
} zs_fejl_type_t;

/*
 * ── DOEREN DER SKAL STAA AABEN: NAAR SKAERMEN BEGYNDER AT SKRIVE ──
 *
 * I dag LAESER skaermen kun. Derfor kan "anlaegget holder fri" afgoeres
 * af det ene vi ved: hvad inverteren SELV meldte foer den forsvandt.
 *
 * Det holder ikke naar ladeplanen kommer til. Platformen sender
 * ladeordrer ned - zs_ladeordre samler dem allerede, se net/zs_fleet.c
 * - og de arbejder NETOP om natten: det er dér batteriet skal lades
 * naar stroemmen er billig. Saa snart vi sender en kommando og
 * forventer at anlaegget gOEr noget, er tavshed en FEJL - ogsaa klokken
 * tre, ogsaa hvis det sidste vi hoerte var "sover".
 *
 * HVORDAN DEN KOMMER IND, den dag skrivningen gOEr:
 *
 *   1. ét felt mere herunder:
 *
 *          bool forventer_vaagen;   // der er en levende ladeordre
 *
 *   2. fodret fra ordren i fejl_tik, fx
 *
 *          ind.forventer_vaagen =
 *              (zs_ladeordre_stand(&ordre, nu) == ZS_ORDRE_KLAR);
 *
 *   3. og ÉN linje i sover() i zs_fejl.c: forventer vi den vaagen, er
 *      soevn ikke et gyldigt svar.
 *
 * Det er med vilje IKKE bygget endnu. Et felt ingen saetter, er et felt
 * der staar falsk den dag nogen tror det virker - og den rigtige kilde
 * findes foerst naar skrivningen findes. Men reglen og stedet staar
 * her, saa de ikke skal findes forfra.
 *
 * Doedmandsknappen betyder i oevrigt at svaret er KORTVARIGT sandt:
 * tier Platformen, falder ordren tilbage af sig selv efter faa
 * minutter, og saa maa anlaegget gerne sove igen. Derfor skal det
 * laeses af ordrens stand ved hvert tik og ikke huskes.
 */

/*
 * Hvad vagten faar at vide. Samlet i én struct, saa to flag ikke kan
 * bytte plads i et kald.
 */
typedef struct {
    /*
     * Er der stroem paa stikket. Fra zs_stroem_laes().
     *
     * SAND betyder "alt er fint", og det er med vilje: et board uden
     * batteri svarer altid sandt, saa en skaerm der ikke KAN maale,
     * aldrig kommer til at melde stroemsvigt. Et felt der er nul som
     * standard ville melde fejl paa hver skaerm den dag nogen glemte at
     * fylde det.
     */
    bool    paa_stroem;
    bool    paa_wifi;           /* zs_wifi_state() == CONNECTED          */
    bool    inverter_svarer;    /* zs_fr_is_connected()                  */
    bool    inverter_opsat;     /* er der overhovedet valgt en inverter  */
    /*
     * Inverterens egen driftstilstand, som SunSpec melder den:
     * 1 slukket, 2 sover, 4 producerer, 7 fejl. NUL betyder ukendt.
     *
     * Den laeses KUN naar inverter_svarer er sand. Vagten husker selv
     * den sidste den fik, for det er netop vaerdien fra foer den
     * forsvandt der afgoer om det er natten eller en fejl.
     *
     * Kalderen skal give 0 hvis inverteren ikke udfylder statusfelterne
     * (zs_fr_live_t.status_ok er falsk). Et felt der ikke blev laest,
     * staar paa nul og ville ellers blive laest som "slukket".
     */
    int32_t inverter_tilstand;
    bool    nat;                /* klokken er i natperioden              */
    /*
     * Demoen. Den naar aldrig herind i dag, fordi app-loekken springer
     * videre foer kaldet - men flaget er her alligevel, for den dag
     * nogen flytter kaldet, maa en fremvisning paa en messe ikke
     * begynde at bippe foran en kunde.
     */
    bool    demo;
    /*
     * Kunden har slaaet alarmen fra i Avancerede indstillinger.
     *
     * FELTET HEDDER "FRA" OG IKKE "TIL", og det er med vilje. Structen
     * memsettes til nul hos kalderne, og nul skal betyde NORMAL. Hed
     * det alarm_til, ville en glemt linje goere hver skaerm i marken
     * tavs - og en alarm der er slaaet fra ved et uheld, opdages
     * foerst den dag den skulle have lydt. Det modsatte, en alarm der
     * lyder for meget, opdages med det samme.
     *
     * Den slaar KUN blinket og lyden fra. ud.type bliver som den er,
     * saa fejlsiden og statuslinjen stadig siger hvad der er galt -
     * ellers havde vi lavet en knap der skjuler fejl.
     */
    bool    alarm_fra;
} zs_fejl_ind_t;

/* Hvad skaermen skal goere. */
typedef struct {
    zs_fejl_type_t type;
    /*
     * Skal siden blinke. Falsk ved soevn: der er ingenting at haste med
     * naar solen er nede.
     */
    bool blink;
    /*
     * Skal der bippes NU. Sand i ét enkelt tik, ikke saa laenge som
     * fejlen varer: kalderen sender ét bip hver gang den er sand.
     */
    bool lyd;
} zs_fejl_ud_t;

/* Vagtens hukommelse. Kalderen ejer den og roerer ikke felterne. */
typedef struct {
    zs_fejl_type_t type;
    /*
     * Hvornaar hver af de to ting forsvandt, og OM de er vaek.
     *
     * Der er et flag ved siden af tidspunktet, og det er ikke
     * overfloedigt. Foerste udgave brugte nul som "den er der", og det
     * er den samme vaerdi som det monotone ur staar paa i det sekund
     * skaermen taender. Forsvandt noget paa det foerste tik, blev uret
     * sat til nul, hvilket blev laest som "ikke vaek", og saa begyndte
     * nedtaellingen forfra paa det naeste tik. Fejlen var paa to
     * sekunder og derfor usynlig i drift - men den stod i en test, og
     * en sentinel der kan forveksles med en rigtig vaerdi er den slags
     * der en dag bliver et rigtigt problem et andet sted.
     */
    /* Stroemmen: eget ur, samme form som wifi'ets. */
    bool    stroem_tabt;
    int64_t stroem_tabt_ms;
    bool    wifi_tabt;
    int64_t wifi_tabt_ms;
    /*
     * INVERTERENS UR ER EN SUM, ikke et tidspunkt.
     *
     * Her stod et tidspunkt, som wifi'et nulstillede. Det kunne ikke
     * holde: en router der falder ud i to sekunder hvert halve minut,
     * nulstillede uret oftere end de to minutter det skal naa - saa et
     * anlaeg der var HELT doedt, blev aldrig meldt. Og en nulstilling
     * ved hvert tik uden net var endnu vaerre.
     *
     * Nu laegges tiden SAMMEN, og kun mens der er net. Et udfald paa to
     * sekunder koster to sekunder, ikke to minutter. Og har nettet
     * vaeret vaek en time, staar summen stille i den time: inverteren
     * naas over wifi, saa den skal ikke have tid skudt i skoene mens der
     * ikke var en vej til den.
     */
    bool    inv_tabt;
    int64_t inv_vaek_ms;
    /* Sidste tik, saa der kan laegges et stykke tid til summen. */
    bool    har_tikket;
    int64_t sidst_tik_ms;
    /* Sidste driftstilstand vi saa mens inverteren svarede. 0 = ukendt */
    int32_t sidste_tilstand;
    /* Blinker vi lige nu, hvor mange bip er der sendt, og naar kommer  */
    /* det naeste.                                                      */
    bool    alarm;
    uint8_t lyd_nr;
    int64_t naeste_lyd_ms;
} zs_fejl_vagt_t;

/*
 * "VI SER IKKE EFTER INVERTEREN LIGE NU."
 *
 * Kaldes i stedet for et tik i de tilstande hvor appen slet ikke
 * forsoeger at naa anlaegget: demoen og opsaetningen. Den flytter kun
 * uret frem, saa den tid ikke bliver lagt til inverterens.
 *
 * Uden den fik vagten ét tik med hele demoen i - og fejlsiden sprang
 * frem i samme sekund demoen sluttede. Et spring i tiden taelles ellers
 * MED, og det skal det: en netvaerkssoegning kan tage fire minutter hvor
 * vi faktisk forsoeger.
 */
void zs_fejl_ser_ikke(zs_fejl_vagt_t *v, int64_t nu_ms);

/* Nulstiller vagten. Skal kaldes én gang foer det foerste tik. */
void zs_fejl_init(zs_fejl_vagt_t *v);

/*
 * Ét tik. Kaldes saa ofte kalderen har lyst - en gang i sekundet eller
 * hvert andet sekund - og svarer hvad skaermen skal vise lige nu.
 *
 * Giver ZS_FEJL_INGEN og intet blink hvis v eller ind er NULL, saa et
 * kald der gaar galt ikke kan faa en skaerm til at blinke uden grund.
 */
zs_fejl_ud_t zs_fejl_tik(zs_fejl_vagt_t *v, const zs_fejl_ind_t *ind,
                         int64_t nu_ms);

/*
 * Teksterne. De staar HER og ikke i siden, saa den samme fejl hedder
 * det samme i loggen, paa skaermen og i en test. Aldrig NULL.
 */
const char *zs_fejl_titel(zs_fejl_type_t t);       /* linjen foeroven   */
const char *zs_fejl_overskrift(zs_fejl_type_t t);  /* den store linje  */
const char *zs_fejl_forklaring(zs_fejl_type_t t);  /* hvad det betyder */
const char *zs_fejl_knap(zs_fejl_type_t t);        /* hvad knappen hedder */

/*
 * GLEMMER ALT OM INVERTEREN: uret, og hvad den sidst meldte.
 *
 * Kaldes naar kunden VAELGER en inverter, ogsaa den samme igen. To ting
 * gaar galt uden den:
 *
 * 1. Uret bliver ved at loebe fra dengang den gamle forsvandt, saa
 *    graensen er for laengst passeret. Siden kommer altsaa frem igen i
 *    naeste tik - et par hundrede millisekunder efter at kunden har
 *    valgt - selvom den nye inverter ikke har haft en chance endnu.
 *
 * 2. Den sidste driftstilstand er den GAMLE inverters. Meldte den
 *    "sover", bliver en ny og helt doed inverter klassificeret som
 *    sovende, og saa er alarmen slaaet fra. Tavst.
 *
 * Wifi-uret roeres IKKE: et inverter-valg siger ingenting om
 * netvaerket.
 */
void zs_fejl_glem_inverteren(zs_fejl_vagt_t *v);

/*
 * Er det her noget der skal raabes op om.
 *
 * ÉN regel, ÉT sted. Den afgoer baade om siden pulserer, om baglyset
 * pulserer, om der bippes - og hvilken FARVE overskriften har. Stod den
 * fire steder, ville en skaerm en dag staa med en roed overskrift der
 * ikke pulserede, eller omvendt, og den slags ser ud som en fejl i
 * skaermen og ikke som en besked.
 *
 * Soevn er ikke alvorligt: der er ingenting at haste med naar solen er
 * nede, og en skaerm der raaber op hver aften er en skaerm man holder op
 * med at se paa.
 */
bool zs_fejl_alvorlig(zs_fejl_type_t t);

/* Til loggen. Kort og paa dansk. */
const char *zs_fejl_navn(zs_fejl_type_t t);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FEJL_H */
