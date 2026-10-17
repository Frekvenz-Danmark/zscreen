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

    /* ── stroemmen: eget ur ── */
    if (ind->paa_stroem) {
        v->stroem_tabt = false;
    } else if (!v->stroem_tabt) {
        v->stroem_tabt = true;
        v->stroem_tabt_ms = nu_ms;
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

    bool stroem_fejl = v->stroem_tabt
        && (nu_ms - v->stroem_tabt_ms >= ZS_FEJL_STROEM_GRAENSE_MS);
    bool wifi_fejl = v->wifi_tabt
        && (nu_ms - v->wifi_tabt_ms >= ZS_FEJL_WIFI_GRAENSE_MS);
    bool inv_fejl = v->inv_tabt
        && (v->inv_vaek_ms >= ZS_FEJL_INVERTER_GRAENSE_MS);

    /*
     * SOVER INVERTEREN BARE?
     *
     * Regnet ÉT sted, fordi to steder bliver uenige. Den bruges baade
     * til rangordenen nedenfor og til hvilken af de to inverterfejl det
     * er.
     */
    bool inv_sover = sover(v->sidste_tilstand)
        && (v->inv_vaek_ms < ZS_FEJL_SOVER_MAKS_MS);

    /*
     * ALARMEN SLAAET FRA NEDPRIORITERER STROEMFEJLEN - DEN TIER IKKE
     * DE ANDRE.
     *
     * Stroemmen vinder over wifi nedenfor. Havde kunden slaaet
     * "Batteri-alarm" fra, blev blinket derfor slaaet fra for HELE
     * siden, ogsaa naar wifi samtidig var vaek - og det er ikke en
     * sjaelden sammenfald: et stroemsvigt i huset tager baade skaermens
     * stroemforsyning OG routeren. Saa stod der en skaerm uden net, der
     * hverken blinkede eller bippede, fordi nogen havde slaaet
     * BATTERIets alarm fra.
     *
     * "Batteri-alarm" slaar batteriets alarm fra. Derfor faar
     * stroemfejlen kun forrangen naar alarmen er slaaet til. Er den
     * ikke, falder vi igennem til wifi og inverter - og er der ikke
     * andet galt, vises stroemsiden stadig, bare stille. Se blinket
     * nedenfor.
     */
    bool stroem_foerst = stroem_fejl && !ind->alarm_fra;

    if (ind->demo) {
        ud.type = ZS_FEJL_INGEN;
    } else if (stroem_foerst) {
        /*
         * STROEMMEN FOERST, FOER WIFI.
         *
         * Er stroemmen vaek, er skaermen vaek om nogle timer, og saa er
         * der ingenting at vise overhovedet. Wifi og inverter kan
         * fortsaette med at vaere i orden hele vejen ned - ZDisplay har
         * batteri og bliver paa netvaerket - saa uden den her gren ville
         * en skaerm paa vej mod sort melde "alt virker" indtil den gik
         * ud.
         *
         * Og det er den ENESTE af de tre en kunde kan rette paa stedet
         * uden vaerktoej: saet kablet i.
         */
        ud.type = ZS_FEJL_INGEN_STROEM;
    } else if (wifi_fejl) {
        /*
         * Wifi foerst. Er nettet vaek, er inverteren det ogsaa, og saa
         * er der kun én af de to beskeder der peger paa noget kunden kan
         * goere noget ved.
         */
        ud.type = ZS_FEJL_WIFI;
    } else if (stroem_fejl && inv_sover) {
        /*
         * EN SOVENDE INVERTER ER IKKE "NOGET GALT".
         *
         * Maalt paa et rigtigt anlaeg 17. oktober 2026: Fronius meldte
         * "Sleeping" en oktoberaften, som den skal. Havde kunden
         * samtidig slaaet Batteri-alarm fra, faldt stroemsvigtet
         * igennem til inverter-grenen nedenfor og blev
         * ZS_FEJL_INVERTER_SOVER. Den er ikke alvorlig, saa zs_app
         * satte har_side = false og sendte skaermen TILBAGE til
         * forsiden. Stroemsvigtet forsvandt altsaa helt: ingen side,
         * intet blink, ingen lyd, mens batteriet blev draenet.
         *
         * Alarmen slaaet fra skal NEDPRIORITERE stroemfejlen, ikke
         * skjule den - det staar i noten ved stroem_foerst ovenfor, og
         * Avanceret-siden lover det med ord: "Beskeden bliver vist
         * uanset hvad." Solen der er gaaet ned er ikke "noget andet
         * galt", saa den viger for stroemmen. En inverter der er
         * holdt op med at svare, gaar stadig foran.
         */
        ud.type = ZS_FEJL_INGEN_STROEM;
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
        ud.type = inv_sover ? ZS_FEJL_INVERTER_SOVER : ZS_FEJL_INVERTER;
    } else if (stroem_fejl) {
        /*
         * Alarmen er slaaet fra, og der er ikke andet galt. Siden
         * vises stadig - kunden skal kunne se HVORFOR skaermen gaar ud
         * om nogle timer - men den blinker og bipper ikke. Se
         * stroem_foerst ovenfor.
         */
        ud.type = ZS_FEJL_INGEN_STROEM;
    } else {
        ud.type = ZS_FEJL_INGEN;
    }

    /* Hvad den var FOER. Bruges af lyden nedenfor: en ny fejltype skal
     * have sine bip, ogsaa naar den afloeser en der allerede alarmerer. */
    zs_fejl_type_t forrige = v->type;
    v->type = ud.type;

    /* ── blinket ── Reglen staar i zs_fejl_alvorlig, ÉT sted. ── */
    ud.blink = zs_fejl_alvorlig(ud.type);

    /*
     * Og saa kunden selv. Har hun slaaet alarmen fra, blinker og
     * bipper det ikke - men ud.type staar uroert, saa fejlsiden og
     * statuslinjen stadig siger hvad der er galt.
     *
     * Det staar HER og ikke ude i app-loekken, fordi lyden nedenfor
     * foelger blinket. Slog vi den fra to steder, ville en af dem en
     * dag blive glemt, og saa bippede en skaerm med alarmen slaaet
     * fra.
     */
    if (ind->alarm_fra && ud.type == ZS_FEJL_INGEN_STROEM) {
        ud.blink = false;
    }

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
    } else if (!v->alarm || ud.type != forrige) {
        /*
         * Alarmen begynder nu - ELLER den skiftede til en anden fejl.
         *
         * Her stod kun "!v->alarm", altsaa en kant fra ikke-alarm til
         * alarm. Bevist ved koersel: doer inverteren klokken ét, er
         * alle fire bip brugt en time senere (pauserne er 5, 10 og 30
         * minutter). Rives stroemkablet ud derefter, vinder
         * stroem_foerst og typen skifter - men blinket har staaet
         * uafbrudt, saa v->alarm var stadig sand, lyd_nr stadig fire,
         * og summeren lod IKKE en lyd for stroemsvigtet. Netop den
         * fejl som hele zs_stroem-laget blev lavet for at raabe om.
         *
         * Typeskiftet taeller derfor som en ny alarm. De tre alvorlige
         * typer skifter ikke frem og tilbage i sekunder - wifi laaser
         * efter 20 sek, inverteren efter 120, og stroemmen efter 30 -
         * saa det her kan ikke blive en summer der gaar i ring.
         */
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
    /*
     * INGEN TITEL I SIDEHOVEDET. Overskriften siger det hele i to
     * linjer, og "Ingen strøm" ovenover var det samme sagt en gang
     * mere med andre ord. To overskrifter paa den samme side er ikke
     * to oplysninger.
     */
    case ZS_FEJL_INGEN_STROEM:    break;
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
    /*
     * TO LINJER, OG DEN ANDEN ER DET MAN SKAL GOERE.
     *
     * Her stod "Skærmen kører på batteri", og det er en iagttagelse.
     * Den fortaeller hvad skaermen er i gang med, ikke hvad der er
     * sket eller hvad man gOEr ved det. Nu siger foerste linje hvad
     * der ER sket og anden linje hvad man skal - og linjeskiftet er
     * med vilje, saa de to laeses som to ting.
     *
     * Maalt i zs_font_stor paa 760: 583 og 329 px. Begge paa ÉN linje
     * hver, saa skiftet falder hvor der staar \n og ikke et tilfaeldigt
     * sted.
     */
    case ZS_FEJL_INGEN_STROEM:    return "Strømforsyningen er fjernet\n"
                                         "Tilslut den igen.";
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
    case ZS_FEJL_INGEN_STROEM:
        /*
         * MAALT MED tools/maal-tekst.py, saa "to linjer" ikke er
         * oejemaal: 675 og 619 px paa ZDisplays 760, 415 og 381 paa
         * zScreens 456. To naesten lige linjer paa BEGGE skaerme, fordi
         * baade skriften og bredden skalerer ens.
         */
        return "Sæt venligst strømkablet i igen. Skærmen kører videre "
               "på batteriet, men slukker af sig selv, når det er "
               "brugt op.";
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
    /*
     * HER STOD "" - INGEN KNAP.
     *
     * Begrundelsen var rigtig saa langt den raekker: skaermen kan ikke
     * saette kablet i. Men siden er alvorlig, saa appen tvinger den
     * frem og HOLDER den der, og den har hverken tilbagepil eller
     * tandhjul. Uden knap var hele fladen altsaa laast saa laenge
     * batteriet holdt - timer - og det ene sted kunden kan tie den,
     * Indstillinger -> Avancerede -> Batteri-alarm, var ikke til at
     * naa. Kravet til den knap er skrevet ned: hun skal kunne slaa DEN
     * fra uden at skulle ringe nogen op.
     *
     * Knappen foerer derfor til Indstillinger. Appen tvinger kun
     * fejlsiden frem fra FORSIDEN, saa hun bliver ikke revet tilbage
     * mens hun staar derinde.
     */
    case ZS_FEJL_INGEN_STROEM:    return "Indstillinger";
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
    case ZS_FEJL_INGEN_STROEM:
    case ZS_FEJL_WIFI:
    case ZS_FEJL_INVERTER:        return true;
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return false;
}

zs_fejl_ikon_t zs_fejl_ikon(zs_fejl_type_t t)
{
    switch (t) {
    /* Et batteri, fordi det er det skaermen koerer PAA lige nu - og
     * det er den oplysning der gOEr at man rejser sig. */
    case ZS_FEJL_INGEN_STROEM:    return ZS_FEJL_IKON_BATTERI;
    /*
     * De to andre har ikke noget. Et ikon der ikke siger mere end
     * overskriften gOEr, er pynt paa en alarmside - og de to sider har
     * staaet i marken uden, saa der er ingen grund til at flytte dem.
     */
    case ZS_FEJL_WIFI:
    case ZS_FEJL_INVERTER:
    case ZS_FEJL_INVERTER_SOVER:
    case ZS_FEJL_INGEN:           break;
    }
    return ZS_FEJL_IKON_INTET;
}

bool zs_fejl_tal_staar_stille(int64_t nu_ms, int64_t sidst_gode_ms)
{
    /*
     * Aldrig nogen tal er ogsaa "staar stille". En skaerm der lige er
     * startet og ikke har naaet inverteren, viser nuller - og de
     * nuller staar stille.
     */
    if (sidst_gode_ms <= 0) {
        return true;
    }
    return (nu_ms - sidst_gode_ms) > ZS_STALE_AFTER_MS;
}

const char *zs_fejl_navn(zs_fejl_type_t t)
{
    switch (t) {
    case ZS_FEJL_INGEN:           return "ingen";
    case ZS_FEJL_INGEN_STROEM:    return "ingen stroem";
    case ZS_FEJL_WIFI:            return "wifi";
    case ZS_FEJL_INVERTER:        return "inverter";
    case ZS_FEJL_INVERTER_SOVER:  return "inverter sover";
    }
    return "ukendt";
}
