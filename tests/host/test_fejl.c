/*
 * Vagten der afgoer hvad der er galt.
 *
 * Hele pointen med at reglen er et eget modul er at den kan koeres
 * igennem her. Paa en rigtig skaerm kraever ét af tilfaeldene nedenfor
 * at man tager wifi'et fra den, venter en time, saetter det paa igen og
 * ser godt efter de foerste to sekunder. Her tager det ingen tid, og
 * det bliver proevet hver gang nogen bygger.
 */

#include "zs_test.h"
#include "../../firmware/kerne/app/zs_fejl.h"
#include "../../firmware/kerne/zs_config.h"

#define MIN_MS   (60 * 1000)

/* Alt virker. Udgangspunktet for hvert tilfaelde nedenfor. */
static zs_fejl_ind_t alt_godt(void)
{
    zs_fejl_ind_t i;
    memset(&i, 0, sizeof(i));
    /*
     * paa_stroem SKAL saettes her.
     *
     * Structen memsettes til nul, og nul betyder "ingen stroem". Uden
     * den her linje ville HVER proeve i filen melde stroemsvigt efter
     * tredive sekunder - og det er netop derfor feltet er defineret
     * saadan at sand er "alt er fint": en kalder der glemmer det,
     * bliver fanget af proeverne med det samme.
     */
    i.paa_stroem = true;
    i.paa_wifi = true;
    i.inverter_svarer = true;
    i.inverter_opsat = true;
    i.inverter_tilstand = 4;      /* producerer */
    i.nat = false;
    i.demo = false;
    return i;
}

void test_fejl(void)
{
    ZS_SUITE("Fejlvagten: hvad er galt");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    /* ── alt virker ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    ud = zs_fejl_tik(&v, &ind, 1000);
    CHECK("alt virker: ingen fejl", ud.type == ZS_FEJL_INGEN);
    CHECK("alt virker: intet blink", !ud.blink);
    CHECK("alt virker: ingen lyd", !ud.lyd);

    /* ── wifi forsvinder ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    ud = zs_fejl_tik(&v, &ind, 0);
    ind.paa_wifi = false;
    ind.inverter_svarer = false;      /* uden net svarer den heller ikke */

    ud = zs_fejl_tik(&v, &ind, 1000);
    CHECK("wifi vaek i 1 s: endnu ingen fejl", ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_WIFI_GRAENSE_MS - 1);
    CHECK("wifi vaek lige under graensen: stadig ingen fejl",
          ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_WIFI_GRAENSE_MS);
    CHECK("wifi vaek paa graensen: fejlen er wifi", ud.type == ZS_FEJL_WIFI);
    CHECK("wifi-fejl blinker", ud.blink);
    CHECK("wifi-fejl bipper med det samme", ud.lyd);

    /* ── og kommer tilbage ── */
    ind.paa_wifi = true;
    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_WIFI_GRAENSE_MS + 1);
    CHECK("wifi tilbage: fejlen slipper STRAKS, ingen ventetid",
          ud.type == ZS_FEJL_INGEN);
    CHECK("wifi tilbage: blinket stopper", !ud.blink);

    /* ── inverteren forsvinder mens den producerer ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;

    /*
     * TABT er det foerste tik der opdagede det. Graensen regnes derfra
     * og ikke fra det oejeblik inverteren faktisk gik ned, for det
     * oejeblik kender skaermen ikke. App-loekken tikker hvert andet
     * sekund, saa forskellen er hoejst to sekunder.
     */
    const int64_t tabt = 2000;
    ud = zs_fejl_tik(&v, &ind, tabt);
    CHECK("inverter lige forsvundet: ingen fejl endnu", ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, tabt + 30 * 1000);
    CHECK("inverter vaek i 30 s: ingen fejl endnu - skaermen leder selv",
          ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, tabt + ZS_FEJL_INVERTER_GRAENSE_MS - 1);
    CHECK("inverter vaek lige under graensen: stadig ingen fejl",
          ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, tabt + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("inverter vaek paa graensen: fejlen er inverteren",
          ud.type == ZS_FEJL_INVERTER);
    CHECK("inverter-fejl blinker", ud.blink);
    CHECK("inverter-fejl bipper", ud.lyd);

    /*
     * Graenserne skal vaere forskellige, og inverterens skal vaere den
     * store. Er de ens, kommer siden frem mens skaermen stadig er ved at
     * finde en inverter der bare har skiftet adresse.
     */
    CHECK("inverterens graense er laengere end wifi's",
          ZS_FEJL_INVERTER_GRAENSE_MS > ZS_FEJL_WIFI_GRAENSE_MS);

    /* ── inverteren forsvinder mens den sover: det er natten ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 2;        /* sover */
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("sov da den forsvandt: det er soevn og ikke en fejl",
          ud.type == ZS_FEJL_INVERTER_SOVER);
    CHECK("soevn blinker IKKE", !ud.blink);
    CHECK("soevn bipper IKKE", !ud.lyd);

    /* Slukket, altsaa tilstand 1, taeller ogsaa som soevn. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 1;
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("slukket da den forsvandt: ogsaa soevn",
          ud.type == ZS_FEJL_INVERTER_SOVER);

    /*
     * UKENDT tilstand er IKKE soevn.
     *
     * Nul er baade "vi har aldrig set den" og "feltet blev ikke laest".
     * En skaerm der lige er genstartet maa ikke paastaa at anlaegget
     * sover - saa ville en inverter der er gaaet i fejl mens skaermen
     * var slukket, blive meldt som en der holder fri.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 0;
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("ukendt tilstand er en fejl og ikke soevn",
          ud.type == ZS_FEJL_INVERTER);

    /* En inverter i FEJL, tilstand 7, der derefter forsvinder: fejl. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 7;
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("stod i fejl da den forsvandt: en fejl",
          ud.type == ZS_FEJL_INVERTER);

    /* ── wifi vejer tungest ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.paa_wifi = false;
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("baade wifi og inverter vaek: der staar wifi",
          ud.type == ZS_FEJL_WIFI);

    /*
     * ── DEN VIGTIGE: wifi vaek i en time, og saa tilbage ──
     *
     * Inverteren naas over wifi, saa den har ogsaa vaeret vaek i en
     * time. Alligevel maa skaermen IKKE skifte til "anlaegget svarer
     * ikke" i det sekund nettet kommer tilbage: inverteren har ikke
     * faaet en chance endnu. Den skal have sine egne to minutter.
     *
     * Det var den fejl der gjorde at reglen blev to ure i stedet for et.
     */
    ind.paa_wifi = true;
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS);
    CHECK("wifi tilbage efter en time: IKKE straks en inverterfejl",
          ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS + ZS_FEJL_INVERTER_GRAENSE_MS - 1);
    CHECK("inverteren faar sine egne to minutter", ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("og derefter er det en inverterfejl", ud.type == ZS_FEJL_INVERTER);

    /* ── en skaerm uden inverter er ikke en skaerm med en fejl ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_opsat = false;
    ind.inverter_svarer = false;
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("ingen inverter valgt endnu: ingen fejl", ud.type == ZS_FEJL_INGEN);

    /* ── demoen tier ── */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.paa_wifi = false;
    ind.inverter_svarer = false;
    ind.demo = true;
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("demo: ingen fejl uanset hvad", ud.type == ZS_FEJL_INGEN);
    CHECK("demo: intet blink", !ud.blink);
    CHECK("demo: ingen lyd", !ud.lyd);

    /*
     * ── ÉN REGEL FOR ALVOREN, og baade blinket og FARVEN bruger den ──
     *
     * Overskriftens farve paa fejlsiden slaas op med
     * zs_fejl_alvorlig. Faldt den og blinket fra hinanden, ville en
     * skaerm staa med en roed overskrift der ikke pulserede - eller
     * omvendt - og det ser ud som en fejl i skaermen og ikke som en
     * besked.
     */
    CHECK("wifi er alvorligt", zs_fejl_alvorlig(ZS_FEJL_WIFI));
    CHECK("en inverter der er vaek er alvorligt",
          zs_fejl_alvorlig(ZS_FEJL_INVERTER));
    CHECK("ingen stroem er alvorligt",
          zs_fejl_alvorlig(ZS_FEJL_INGEN_STROEM));
    CHECK("en inverter der SOVER er ikke alvorligt",
          !zs_fejl_alvorlig(ZS_FEJL_INVERTER_SOVER));
    CHECK("ingen fejl er ikke alvorligt", !zs_fejl_alvorlig(ZS_FEJL_INGEN));

    /* Og de to foelges ad, uanset hvad vagten svarer. Koert igennem
     * alle fire slags, ikke stikproever. */
    {
        bool enige = true;
        const zs_fejl_type_t fire[] = {
            ZS_FEJL_INGEN, ZS_FEJL_INGEN_STROEM, ZS_FEJL_WIFI,
            ZS_FEJL_INVERTER, ZS_FEJL_INVERTER_SOVER,
        };
        for (unsigned i = 0; i < sizeof(fire) / sizeof(fire[0]); i++) {
            /* Byg en verden der giver netop den type. */
            zs_fejl_vagt_t w;
            zs_fejl_init(&w);
            zs_fejl_ind_t ii = alt_godt();
            switch (fire[i]) {
            case ZS_FEJL_INGEN_STROEM: ii.paa_stroem = false; break;
            case ZS_FEJL_WIFI:   ii.paa_wifi = false; break;
            case ZS_FEJL_INVERTER:
                ii.inverter_tilstand = 4; break;
            case ZS_FEJL_INVERTER_SOVER:
                ii.inverter_tilstand = 2; break;
            case ZS_FEJL_INGEN:  break;
            }
            (void)zs_fejl_tik(&w, &ii, 0);
            if (fire[i] != ZS_FEJL_INGEN) {
                ii.inverter_svarer = false;
                (void)zs_fejl_tik(&w, &ii, 2000);
            }
            zs_fejl_ud_t uu = zs_fejl_tik(&w, &ii, 2000 + 10 * MIN_MS);
            if (uu.blink != zs_fejl_alvorlig(uu.type)) {
                ZS_FAIL("blinket og alvoren er uenige om %s",
                        zs_fejl_navn(uu.type));
                enige = false;
            }
        }
        CHECK("blinket foelger alvoren for alle fem slags", enige);
    }

    /*
     * ── STROEMMEN ──
     *
     * ZDisplay har batteri og bliver derfor ved med at virke naar
     * stroemmen ryger: wifi er der, inverteren svarer, tallene er
     * rigtige. Uden den her tilstand ville skaermen melde "alt virker"
     * hele vejen ned mod sort.
     */
    ZS_SUITE("Stroemmen: skaermen koerer paa batteri");

    zs_fejl_init(&v);
    ind = alt_godt();
    ud = zs_fejl_tik(&v, &ind, 1000);
    CHECK("paa stroem: ingen fejl", ud.type == ZS_FEJL_INGEN);

    ind.paa_stroem = false;
    ud = zs_fejl_tik(&v, &ind, 1000);
    CHECK("stroemmen lige gaaet: endnu ingen fejl",
          ud.type == ZS_FEJL_INGEN);

    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_STROEM_GRAENSE_MS - 1);
    CHECK("lige under graensen: stadig ingen fejl",
          ud.type == ZS_FEJL_INGEN);

    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_STROEM_GRAENSE_MS);
    CHECK("paa graensen: fejlen er stroemmen",
          ud.type == ZS_FEJL_INGEN_STROEM);
    CHECK("og den blinker", ud.blink);
    CHECK("og den bipper med det samme", ud.lyd);

    /*
     * KABLET SAT I: fejlen skal slippe STRAKS. Der er ingen grund til
     * at lade en skaerm blinke efter at mennesket har gjort det rigtige
     * - saa ville man tro at det ikke hjalp.
     */
    ind.paa_stroem = true;
    ud = zs_fejl_tik(&v, &ind, 1000 + ZS_FEJL_STROEM_GRAENSE_MS + 1);
    CHECK("kablet sat i: fejlen slipper straks",
          ud.type == ZS_FEJL_INGEN);
    CHECK("og blinket stopper", !ud.blink);

    /*
     * STROEMMEN FOERST. Ryger stroemmen OG nettet, er det stroemmen der
     * skal staa paa skaermen: den er den eneste af de to der ender med
     * en sort skaerm, og den eneste en kunde kan rette med haenderne.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.paa_stroem = false;
    ind.paa_wifi = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("baade stroem og net vaek: stroemmen staar foerst",
          ud.type == ZS_FEJL_INGEN_STROEM);

    /*
     * OG EN SKAERM UDEN BATTERI MAA ALDRIG MELDE DET. zScreen svarer
     * altid paa_stroem = sand, for koerer den, ER der stroem. Proeven
     * her er paa REGLEN: er flaget sandt, findes fejlen ikke, uanset
     * hvor laenge der gaar.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, 48 * 60 * MIN_MS);
    CHECK("to doegn paa stroem: aldrig et stroemsvigt",
          ud.type == ZS_FEJL_INGEN);

    /* Teksterne skal findes, ellers staar siden tom. */
    CHECK("stroemfejlen har INGEN titel i sidehovedet - overskriften "
          "siger det hele i to linjer",
          zs_fejl_titel(ZS_FEJL_INGEN_STROEM)[0] == '\0');
    CHECK("og en overskrift",
          zs_fejl_overskrift(ZS_FEJL_INGEN_STROEM)[0] != '\0');
    CHECK("og en forklaring",
          zs_fejl_forklaring(ZS_FEJL_INGEN_STROEM)[0] != '\0');
    /*
     * MEN INGEN KNAP. Der er ingenting skaermen kan goere - kablet skal
     * i. En knap uden handling er vaerre end ingen knap.
     */
    CHECK("og INGEN knap, for der er ingenting at trykke paa",
          zs_fejl_knap(ZS_FEJL_INGEN_STROEM)[0] == '\0');

    /*
     * ── BATTERI-ALARMEN KAN SLAAS FRA, OG KUN DEN ──
     *
     * Kontakten i Avancerede indstillinger hedder "Batteri-alarm".
     * Slog den ALT fra, var navnet en loegn: man slaar den fra for
     * batteriet og hoerer derefter aldrig om en doed inverter.
     */
    ZS_SUITE("Batteri-alarmen slaaet fra");

    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = true;
    ind.paa_stroem = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, ZS_FEJL_STROEM_GRAENSE_MS);
    CHECK("fejlen er der stadig", ud.type == ZS_FEJL_INGEN_STROEM);
    CHECK("men den blinker ikke", !ud.blink);
    CHECK("og den bipper ikke", !ud.lyd);

    /* Og de to andre blinker videre, uanset kontakten. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = true;
    ind.paa_wifi = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("wifi-fejlen melder stadig", ud.type == ZS_FEJL_WIFI);
    CHECK("og den blinker STADIG - kontakten er kun batteriets",
          ud.blink);

    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = true;
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 1000);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("inverter-fejlen melder stadig", ud.type == ZS_FEJL_INVERTER);
    CHECK("og den blinker ogsaa stadig", ud.blink);

    /*
     * ── OG BEGGE PAA ÉN GANG. DET ER DEN ALMINDELIGE. ──
     *
     * De to proever ovenfor koerer hver for sig, med stroemmen i
     * orden. Men et stroemsvigt i huset tager BAADE skaermens
     * stroemforsyning og routeren - saa de to fejl kommer naesten
     * altid sammen.
     *
     * Her var hullet: stroemmen vinder forrangen, saa hele siden blev
     * sat til INGEN_STROEM, og kontakten slog derefter blinket fra
     * for det hele. En skaerm uden net, tavs, fordi nogen havde slaaet
     * BATTERIets alarm fra.
     *
     * Nu nedprioriterer kontakten stroemfejlen i stedet.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = true;
    ind.paa_stroem = false;
    ind.paa_wifi = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("stroem OG wifi vaek med alarmen fra: wifi melder",
          ud.type == ZS_FEJL_WIFI);
    CHECK("og den blinker - kontakten er kun batteriets", ud.blink);

    /* Med alarmen SLAAET TIL vinder stroemmen stadig, som foer. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = false;
    ind.paa_stroem = false;
    ind.paa_wifi = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, 10 * MIN_MS);
    CHECK("med alarmen til vinder stroemmen stadig",
          ud.type == ZS_FEJL_INGEN_STROEM);
    CHECK("og den blinker", ud.blink);

    /* Og er stroemmen det ENESTE, vises siden stadig - bare stille. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.alarm_fra = true;
    ind.paa_stroem = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, ZS_FEJL_STROEM_GRAENSE_MS);
    CHECK("kun stroemmen vaek: siden vises stadig",
          ud.type == ZS_FEJL_INGEN_STROEM);
    CHECK("men stille", !ud.blink && !ud.lyd);

    /*
     * ── HVAD SIDEN SKAL SIGE OM TALLENE ──
     *
     * Fejlsiden saetter "Tallene paa forsiden er fra kl. X og staar
     * stille." under forklaringen, og den saetning skal vaere sand.
     *
     * HER STOD TRE PROEVER DER SPURGTE FEJLTYPEN:
     *
     *     !zs_fejl_tal_staar_stille(ZS_FEJL_INGEN_STROEM)
     *      zs_fejl_tal_staar_stille(ZS_FEJL_WIFI)
     *
     * De bestod, og reglen var alligevel forkert. Stroemsvigt ALENE
     * fryser ikke tallene - skaermen koerer paa batteriet, er paa
     * nettet og laeser inverteren. Men et stroemsvigt i huset tager
     * ogsaa routeren, og saa ER de frosne. Stroemfejlen vinder
     * forrangen, saa siden tiede om det ene sande om dataen netop naar
     * det gjaldt. Proeverne daekkede hver fejl for sig og ikke
     * sammenfaldet - igen.
     *
     * Reglen spoerger nu uret, og proeverne spoerger den om tid.
     */
    ZS_SUITE("Staar forsidens tal stille");

    CHECK("friske tal: nej",
          !zs_fejl_tal_staar_stille(100000, 100000));
    CHECK("et sekund gamle: stadig nej",
          !zs_fejl_tal_staar_stille(101000, 100000));
    CHECK("praecis paa graensen: nej",
          !zs_fejl_tal_staar_stille(100000 + ZS_STALE_AFTER_MS, 100000));
    CHECK("ét ms over graensen: ja",
          zs_fejl_tal_staar_stille(100001 + ZS_STALE_AFTER_MS, 100000));
    /*
     * ALDRIG NOGEN TAL ER OGSAA "STAAR STILLE". En skaerm der lige er
     * startet og ikke har naaet inverteren, viser nuller - og de
     * nuller staar stille. Var svaret "friske", ville siden love at
     * nullerne var rigtige.
     */
    CHECK("aldrig nogen tal: ja", zs_fejl_tal_staar_stille(100000, 0));
    /*
     * OG LIGE EFTER OPSTART, hvor uret stadig er lavt. DET er det
     * tilfaelde vagten findes for.
     *
     * Proeven ovenfor med nu_ms = 100000 fanger den ikke: uden
     * vagten bliver regnestykket 100000 - 0 > 10000, altsaa "staar
     * stille" alligevel - det rigtige svar ad en forkert vej. Maalt
     * med en mutation: "sidst_gode_ms <= 0" lavet til "< 0" slap
     * igennem.
     *
     * Fem sekunder efter opstart giver 5000 - 0 > 10000, altsaa
     * falsk, og saa ville siden love at nullerne var friske tal.
     */
    CHECK("fem sekunder efter opstart, aldrig nogen tal: ja",
          zs_fejl_tal_staar_stille(5000, 0));
    CHECK("og et negativt tidsstempel ogsaa",
          zs_fejl_tal_staar_stille(100000, -5));
    /*
     * OG DET SAMMENFALD DER VAR HULLET: stroem og wifi vaek paa én
     * gang. Reglen kender ikke fejltypen laengere, saa den kan ikke
     * tie om det - men proeven staar her, fordi det var DEN vej fejlen
     * kom ind.
     */
    CHECK("stroem OG wifi vaek, tallene er gamle: ja",
          zs_fejl_tal_staar_stille(200000, 100000));

    /* Og ikonet. Kun stroemsvigt har et. */
    CHECK("stroemsvigt har batteri-ikonet",
          zs_fejl_ikon(ZS_FEJL_INGEN_STROEM) == ZS_FEJL_IKON_BATTERI);
    CHECK("wifi har intet ikon",
          zs_fejl_ikon(ZS_FEJL_WIFI) == ZS_FEJL_IKON_INTET);
    CHECK("inverter har intet ikon",
          zs_fejl_ikon(ZS_FEJL_INVERTER) == ZS_FEJL_IKON_INTET);

    /* ── NULL maa ikke faa en skaerm til at blinke ── */
    ud = zs_fejl_tik(NULL, &ind, 0);
    CHECK("NULL-vagt: ingen fejl og intet blink",
          ud.type == ZS_FEJL_INGEN && !ud.blink && !ud.lyd);
    ud = zs_fejl_tik(&v, NULL, 0);
    CHECK("NULL-inddata: ingen fejl og intet blink",
          ud.type == ZS_FEJL_INGEN && !ud.blink && !ud.lyd);
    zs_fejl_init(NULL);      /* maa ikke styrte */
    CHECK("zs_fejl_init(NULL) styrter ikke", true);
}

void test_fejl_glem(void)
{
    ZS_SUITE("Fejlvagten: et nyt inverter-valg faar en ren tavle");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    /* En inverter der har vaeret vaek laenge. */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS);
    CHECK("der ER en inverterfejl foerst", ud.type == ZS_FEJL_INVERTER);

    /*
     * Kunden vaelger en inverter. Den nye svarer ikke endnu - den har
     * ikke haft en chance - og saa maa siden IKKE komme frem med det
     * samme. Uden nulstillingen stod uret fra for en time siden, og
     * graensen var for laengst passeret.
     */
    zs_fejl_glem_inverteren(&v);
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS + 2000);
    CHECK("efter et valg er der ingen fejl, selvom den stadig ikke svarer",
          ud.type == ZS_FEJL_INGEN);
    CHECK("og intet blink", !ud.blink);

    ud = zs_fejl_tik(&v, &ind,
                     60 * MIN_MS + 2000 + ZS_FEJL_INVERTER_GRAENSE_MS - 1);
    CHECK("den nye faar sine egne to minutter", ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind,
                     60 * MIN_MS + 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("og foerst derefter er det en fejl", ud.type == ZS_FEJL_INVERTER);

    /*
     * DEN FARLIGE: den gamle inverter meldte at den sov. Vaelger kunden
     * en ny, og den er helt doed, maa den IKKE blive meldt som sovende -
     * for saa er alarmen slaaet fra, og det sker uden en lyd.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 2;            /* den gamle sov */
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("den gamle meldes som sovende", ud.type == ZS_FEJL_INVERTER_SOVER);

    zs_fejl_glem_inverteren(&v);
    /* Den nye svarer slet ikke, saa vi faar aldrig en tilstand fra den. */
    (void)zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS + 1000);
    ud = zs_fejl_tik(&v, &ind,
                     2000 + 2 * ZS_FEJL_INVERTER_GRAENSE_MS + 2000);
    CHECK("men den NYE meldes som en FEJL, ikke som sovende",
          ud.type == ZS_FEJL_INVERTER);
    CHECK("og den blinker", ud.blink);

    /* Wifi-uret maa IKKE roeres: et inverter-valg siger ingenting om
     * netvaerket. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.paa_wifi = false;
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 0);
    ud = zs_fejl_tik(&v, &ind, ZS_FEJL_WIFI_GRAENSE_MS);
    CHECK("wifi-fejlen staar", ud.type == ZS_FEJL_WIFI);
    zs_fejl_glem_inverteren(&v);
    ud = zs_fejl_tik(&v, &ind, ZS_FEJL_WIFI_GRAENSE_MS + 1000);
    CHECK("og den bliver staaende efter et inverter-valg",
          ud.type == ZS_FEJL_WIFI);

    CHECK("zs_fejl_glem_inverteren(NULL) styrter ikke",
          (zs_fejl_glem_inverteren(NULL), true));
}

void test_fejl_blip(void)
{
    ZS_SUITE("Fejlvagten: korte wifi-udfald maa ikke nulstille inverter-uret");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    /*
     * EN DAARLIG ROUTER: to sekunder uden net, hvert halve minut. Hver
     * gang kortere end wifi-graensen, saa der kommer aldrig en
     * wifi-fejl. Og inverteren er HELT doed hele vejen.
     *
     * Foer nulstillede hvert udfald inverterens ur, saa den naaede
     * aldrig sine to minutter: et doedt anlaeg gav hverken side, blink
     * eller lyd. Nogensinde.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;

    int64_t t = 2000;
    for (int runde = 0; runde < 30; runde++) {
        /* 28 sekunder med net */
        for (int i = 0; i < 14; i++) {
            ind.paa_wifi = true;
            ud = zs_fejl_tik(&v, &ind, t);
            t += 2000;
        }
        /* 2 sekunder uden */
        ind.paa_wifi = false;
        ud = zs_fejl_tik(&v, &ind, t);
        t += 2000;
    }
    ind.paa_wifi = true;
    ud = zs_fejl_tik(&v, &ind, t);
    CHECK("et doedt anlaeg meldes, ogsaa med et wifi-udfald hvert halve minut",
          ud.type == ZS_FEJL_INVERTER);
    CHECK("og det blinker", ud.blink);

    /*
     * Og et enkelt udfald midt i en staaende inverterfejl maa ikke
     * lukke siden. FOer blev typen INGEN i det tik, og kunden blev smidt
     * tilbage paa forsiden - hvorefter nedtaellingen og bippene begyndte
     * forfra.
     */
    ind.paa_wifi = false;
    ud = zs_fejl_tik(&v, &ind, t + 2000);
    CHECK("et udfald midt i en inverterfejl lukker ikke siden",
          ud.type == ZS_FEJL_INVERTER || ud.type == ZS_FEJL_WIFI);
    CHECK("og der er stadig en fejl at se", ud.type != ZS_FEJL_INGEN);

    /*
     * MEN det oprindelige formaal skal stadig holde: har nettet vaeret
     * vaek LAENGE, skal inverteren have sine egne to minutter naar det
     * kommer tilbage - den har ikke haft en chance imens.
     */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.paa_wifi = false;
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS);
    CHECK("en time uden net: det er wifi der meldes", ud.type == ZS_FEJL_WIFI);
    ind.paa_wifi = true;
    ud = zs_fejl_tik(&v, &ind, 60 * MIN_MS + 2000);
    CHECK("nettet tilbage: ikke straks en inverterfejl",
          ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind,
                     60 * MIN_MS + 2000 + ZS_FEJL_INVERTER_GRAENSE_MS - 1);
    CHECK("inverteren faar sine egne to minutter", ud.type == ZS_FEJL_INGEN);
    ud = zs_fejl_tik(&v, &ind,
                     60 * MIN_MS + 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("og derefter er det en inverterfejl", ud.type == ZS_FEJL_INVERTER);
}

void test_fejl_ser_ikke(void)
{
    ZS_SUITE("Fejlvagten: tid hvor vi ikke ser efter, taelles ikke");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    /* Inverteren har vaeret vaek i et minut - under graensen. */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    int64_t t = 2000;
    for (; t <= 62000; t += 2000) {
        ud = zs_fejl_tik(&v, &ind, t);
    }
    CHECK("et minut: endnu ingen fejl", ud.type == ZS_FEJL_INGEN);

    /*
     * EN TIMES DEMO PAA EN MESSE. Appen ser ikke efter inverteren, saa
     * den siger det - og den time maa ikke skrives paa inverterens
     * regning.
     *
     * FOer sprang fejlsiden frem i samme sekund demoen sluttede, fordi
     * det foerste tik bagefter havde en hel time i sig.
     */
    for (int64_t d = t; d <= t + 60 * MIN_MS; d += 2000) {
        zs_fejl_ser_ikke(&v, d);
    }
    int64_t efter = t + 60 * MIN_MS + 2000;
    ud = zs_fejl_tik(&v, &ind, efter);
    CHECK("efter en times demo er der STADIG ingen fejl",
          ud.type == ZS_FEJL_INGEN);

    /* Og resten af minuttet skal stadig gaa foer det er en fejl. */
    for (int64_t d = efter; d <= efter + 56000; d += 2000) {
        ud = zs_fejl_tik(&v, &ind, d);
    }
    CHECK("og der mangler stadig lidt af de to minutter",
          ud.type == ZS_FEJL_INGEN);
    for (int64_t d = efter + 58000; d <= efter + 70000; d += 2000) {
        ud = zs_fejl_tik(&v, &ind, d);
    }
    CHECK("men saa kommer fejlen", ud.type == ZS_FEJL_INVERTER);

    /* Et spring hvor vi FAKTISK ser efter, taelles med - fx en
     * netvaerkssoegning paa fire minutter. */
    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    ud = zs_fejl_tik(&v, &ind, 2000 + 4 * MIN_MS);
    CHECK("en soegning paa fire minutter taelles MED", ud.type == ZS_FEJL_INVERTER);

    CHECK("zs_fejl_ser_ikke(NULL) styrter ikke",
          (zs_fejl_ser_ikke(NULL, 0), true));
}

void test_fejl_sover_graense(void)
{
    ZS_SUITE("Fejlvagten: soevn holder ikke i al evighed");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 2;           /* meldte soevn */
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    const int64_t tabt = 2000;
    (void)zs_fejl_tik(&v, &ind, tabt);

    ud = zs_fejl_tik(&v, &ind, tabt + ZS_FEJL_INVERTER_GRAENSE_MS);
    CHECK("om aftenen er det soevn", ud.type == ZS_FEJL_INVERTER_SOVER);
    CHECK("og den blinker ikke", !ud.blink);

    /* En lang vinternat: stadig soevn. */
    ud = zs_fejl_tik(&v, &ind, tabt + 16 * 60 * MIN_MS);
    CHECK("efter seksten timer er det stadig soevn - det er en vinternat",
          ud.type == ZS_FEJL_INVERTER_SOVER);

    /* Men ikke i al evighed. */
    ud = zs_fejl_tik(&v, &ind, tabt + ZS_FEJL_SOVER_MAKS_MS - 1);
    CHECK("lige under graensen er det soevn",
          ud.type == ZS_FEJL_INVERTER_SOVER);
    ud = zs_fejl_tik(&v, &ind, tabt + ZS_FEJL_SOVER_MAKS_MS);
    CHECK("paa graensen er det en FEJL - saa er det ikke solen",
          ud.type == ZS_FEJL_INVERTER);
    CHECK("og saa blinker den", ud.blink);

    /* Og graensen skal vaere laengere end den laengste nat. */
    CHECK("graensen er laengere end sytten timer",
          ZS_FEJL_SOVER_MAKS_MS > 17 * 60 * 60 * 1000);

    /* Den vaagner den naeste morgen: ingen fejl, og soevnen er glemt. */
    zs_fejl_init(&v);
    ind = alt_godt();
    ind.inverter_tilstand = 2;
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.inverter_svarer = false;
    (void)zs_fejl_tik(&v, &ind, 2000);
    (void)zs_fejl_tik(&v, &ind, 2000 + ZS_FEJL_INVERTER_GRAENSE_MS);
    ind.inverter_svarer = true;
    ind.inverter_tilstand = 4;           /* producerer igen */
    ud = zs_fejl_tik(&v, &ind, 10 * 60 * MIN_MS);
    CHECK("naar solen kommer, er der ingen fejl", ud.type == ZS_FEJL_INGEN);
}

void test_fejl_lyd(void)
{
    ZS_SUITE("Fejlvagten: hvor mange gange der bippes");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.paa_wifi = false;
    ind.inverter_svarer = false;

    int64_t t0 = ZS_FEJL_WIFI_GRAENSE_MS;

    /*
     * Hvert andet sekund, som app-loekken gOEr det, i et doegn. Vi
     * taeller bippene og hvornaar de kom.
     */
    int bip = 0;
    int64_t hvornaar[8];
    memset(hvornaar, 0, sizeof(hvornaar));

    for (int64_t t = 0; t <= 24 * 60 * MIN_MS; t += 2000) {
        ud = zs_fejl_tik(&v, &ind, t);
        if (ud.lyd) {
            if (bip < 8) { hvornaar[bip] = t; }
            bip++;
        }
    }

    CHECK_INT("der bippes fire gange for én fejl, ikke mere", bip, 4);
    CHECK_INT("foerste bip kommer naar fejlen kommer", hvornaar[0], t0);
    /* Pauserne staar i ZS_LYD_PAUSER_MS: 5, 10 og 30 minutter. */
    CHECK_INT("andet bip fem minutter senere", hvornaar[1] - hvornaar[0],
              5 * MIN_MS);
    CHECK_INT("tredje bip ti minutter efter det andet",
              hvornaar[2] - hvornaar[1], 10 * MIN_MS);
    CHECK_INT("fjerde bip en halv time efter det tredje",
              hvornaar[3] - hvornaar[2], 30 * MIN_MS);
    CHECK("og saa er der stille resten af doegnet, mens det stadig blinker",
          ud.blink && bip == 4);

    /* En NY fejl bipper igen. Ellers ville en skaerm kun kunne sige fra
     * én gang i sit liv. */
    ind.paa_wifi = true;
    (void)zs_fejl_tik(&v, &ind, 24 * 60 * MIN_MS + 2000);
    ind.paa_wifi = false;
    bip = 0;
    for (int64_t t = 24 * 60 * MIN_MS + 4000;
         t <= 25 * 60 * MIN_MS; t += 2000) {
        ud = zs_fejl_tik(&v, &ind, t);
        if (ud.lyd) { bip++; }
    }
    CHECK("en ny fejl bipper igen", bip > 0);
}

void test_fejl_nat(void)
{
    ZS_SUITE("Fejlvagten: natten");

    zs_fejl_vagt_t v;
    zs_fejl_ind_t ind;
    zs_fejl_ud_t ud;

    zs_fejl_init(&v);
    ind = alt_godt();
    (void)zs_fejl_tik(&v, &ind, 0);
    ind.paa_wifi = false;
    ind.inverter_svarer = false;
    ind.nat = true;

    int bip = 0;
    for (int64_t t = 0; t <= 6 * 60 * MIN_MS; t += 2000) {
        ud = zs_fejl_tik(&v, &ind, t);
        if (ud.lyd) { bip++; }
    }
    CHECK("om natten blinker den", ud.blink);
    CHECK_INT("om natten bipper den ikke én gang paa seks timer", bip, 0);

    /*
     * Og naar det bliver morgen, kommer der ÉT bip - ikke fire paa en
     * gang. Takten regnes fra det forrige bip, ikke fra fejlens
     * begyndelse, og det er praecis derfor.
     */
    ind.nat = false;
    int64_t morgen = 6 * 60 * MIN_MS + 2000;
    ud = zs_fejl_tik(&v, &ind, morgen);
    CHECK("foerste bip kommer naar natten slutter", ud.lyd);

    bip = 0;
    for (int64_t t = morgen + 2000; t < morgen + 4 * MIN_MS; t += 2000) {
        ud = zs_fejl_tik(&v, &ind, t);
        if (ud.lyd) { bip++; }
    }
    CHECK_INT("og de tre andre vaelter IKKE ud bagefter", bip, 0);

    ud = zs_fejl_tik(&v, &ind, morgen + 5 * MIN_MS);
    CHECK("naeste bip kommer fem minutter efter det foerste", ud.lyd);
}

void test_fejl_tekster(void)
{
    ZS_SUITE("Fejlvagten: teksterne");

    /*
     * KUN DE TO DER HAR EN SIDE.
     *
     * Soevn har ingen tekster mere. Siden har hverken tilbageknap eller
     * tandhjul, saa en overtagelse hele natten spaerrede prissiden og
     * Indstillinger paa ethvert anlaeg uden batteri. Soevn staar nu i
     * statuslinjen i stedet, som ZS_LINK_SOVER.
     */
    /*
     * OG OM DEN HAR EN KNAP.
     *
     * Her stod kun typerne, og saa var "hver fejl har en knap" en
     * paastand om alle fejl med en side. Det holder ikke laengere:
     * ZS_FEJL_INGEN_STROEM HAR en side - den er alvorlig og blinker -
     * men den har med vilje ingen knap, for kablet skal i, og det kan
     * skaermen ikke selv.
     *
     * Havde listen bare faaet den nye type med, ville knap-proeven
     * falde. Havde vi ladet den vaere ude, ville den nye sides TEKSTER
     * aldrig blive proevet, og en side uden overskrift ser ud som en
     * skaerm der er i stykker. Derfor staar det nu som en tabel: hver
     * type MED om den skal have en knap.
     */
    const struct { zs_fejl_type_t t; bool titel; bool knap; } alle[] = {
        { ZS_FEJL_WIFI,         true,  true  },
        { ZS_FEJL_INVERTER,     true,  true  },
        /*
         * Stroemsvigt har hverken titel eller knap, og begge er valg.
         * Overskriften siger det hele i to linjer - hvad der er sket,
         * og hvad man gOEr - saa en titel i sidehovedet ville vaere
         * det samme sagt en gang mere. Og der er ingenting at trykke
         * paa: kablet skal i.
         */
        { ZS_FEJL_INGEN_STROEM, false, false },
    };

    for (unsigned i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
        zs_fejl_type_t t = alle[i].t;
        if (alle[i].titel) {
            CHECK("hver fejl har en titel",
                  zs_fejl_titel(t) != NULL && zs_fejl_titel(t)[0] != '\0');
        } else {
            CHECK("og den uden titel har INGEN titel",
                  zs_fejl_titel(t) != NULL && zs_fejl_titel(t)[0] == '\0');
        }
        CHECK("hver fejl har en overskrift",
              zs_fejl_overskrift(t) != NULL && zs_fejl_overskrift(t)[0] != '\0');
        CHECK("hver fejl har en forklaring",
              zs_fejl_forklaring(t) != NULL
              && zs_fejl_forklaring(t)[0] != '\0');
        if (alle[i].knap) {
            CHECK("hver fejl har en knap der kan foere videre",
                  zs_fejl_knap(t) != NULL && zs_fejl_knap(t)[0] != '\0');
        } else {
            CHECK("og den uden handling har INGEN knap",
                  zs_fejl_knap(t) != NULL && zs_fejl_knap(t)[0] == '\0');
        }
        CHECK("hver fejl har et navn til loggen",
              zs_fejl_navn(t) != NULL && zs_fejl_navn(t)[0] != '\0');
    }

    /*
     * Teksterne maa IKKE vaere ens. To fejl der staar med samme ord paa
     * skaermen, er to fejl kunden ikke kan skelne - og saa er hele
     * opdelingen spildt.
     */
    CHECK("wifi og inverter siger ikke det samme",
          strcmp(zs_fejl_overskrift(ZS_FEJL_WIFI),
                 zs_fejl_overskrift(ZS_FEJL_INVERTER)) != 0);
    /*
     * TO LINJER I OVERSKRIFTEN, og linjeskiftet er en del af teksten.
     * Forsvinder det, staar der "Strømforsyningen er fjernet Tilslut
     * den igen." i én lang linje - og saa laeses de to ting som én.
     */
    CHECK("stroemmens overskrift har et linjeskift",
          strchr(zs_fejl_overskrift(ZS_FEJL_INGEN_STROEM), '\n') != NULL);
    CHECK("og den siger hvad man SKAL, ikke kun hvad der er",
          strstr(zs_fejl_overskrift(ZS_FEJL_INGEN_STROEM),
                 "Tilslut") != NULL);

    CHECK("og stroemmen siger noget tredje",
          strcmp(zs_fejl_overskrift(ZS_FEJL_INGEN_STROEM),
                 zs_fejl_overskrift(ZS_FEJL_WIFI)) != 0
          && strcmp(zs_fejl_overskrift(ZS_FEJL_INGEN_STROEM),
                    zs_fejl_overskrift(ZS_FEJL_INVERTER)) != 0);
    /*
     * Og soevn har INGEN sidetekster. Kommer der tekster paa den igen,
     * er det fordi nogen har givet den en side - og saa skal den ogsaa
     * have en vej ud, ellers er natten spaerret igen.
     */
    CHECK("soevn har ingen overskrift",
          zs_fejl_overskrift(ZS_FEJL_INVERTER_SOVER)[0] == '\0');
    CHECK("soevn har ingen forklaring",
          zs_fejl_forklaring(ZS_FEJL_INVERTER_SOVER)[0] == '\0');
    CHECK("soevn har ingen knap",
          zs_fejl_knap(ZS_FEJL_INVERTER_SOVER)[0] == '\0');
    /* Men den har stadig et navn til loggen. */
    CHECK("soevn har et navn til loggen",
          zs_fejl_navn(ZS_FEJL_INVERTER_SOVER)[0] != '\0');

    /*
     * OG DER SKAL VAERE PLADS TIL DEN PAA SIDEN.
     *
     * Fejlsiden saetter forklaringen sammen med en hale -
     * " Tallene på forsiden er fra kl. 14:05 og står stille." - i en
     * buffer paa 200 bytes, se skriv_teksten i zs_screen_fejl.c.
     * snprintf afkorter stille og roligt hvis der ikke er plads, og saa
     * mangler der en halv saetning paa en skaerm uden at nogen har
     * sagt det.
     *
     * Halen er 54 tegn, hvoraf to er to bytes i UTF-8.
     */
    {
        const size_t hale = 56;      /* bytes, inkl. mellemrummet foran */
        const size_t buffer = 200;
        bool plads = true;
        for (unsigned i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
            size_t n = strlen(zs_fejl_forklaring(alle[i].t)) + hale + 1;
            if (n > buffer) {
                ZS_FAIL("forklaringen til %s bliver afkortet: %u bytes af %u",
                        zs_fejl_navn(alle[i].t), (unsigned)n, (unsigned)buffer);
                plads = false;
            }
        }
        CHECK("forklaringen og klokkeslaettet kan vaere i sidens buffer",
              plads);
    }

    /*
     * Intet fagsprog. Det her er en skaerm i en stue, og ord som
     * "Modbus" eller "timeout" fortaeller kunden ingenting - de
     * fortaeller hende kun at det er for teknisk til hende.
     */
    static const char *forbudt[] = {
        "Modbus", "modbus", "TCP", "timeout", "socket", "SunSpec",
        "inverter_state", "DHCP", "RSSI",
    };
    bool rent = true;
    for (unsigned i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
        const char *tekster[] = {
            zs_fejl_titel(alle[i].t), zs_fejl_overskrift(alle[i].t),
            zs_fejl_forklaring(alle[i].t), zs_fejl_knap(alle[i].t),
        };
        for (unsigned j = 0; j < sizeof(tekster) / sizeof(tekster[0]); j++) {
            for (unsigned k = 0; k < sizeof(forbudt) / sizeof(forbudt[0]); k++) {
                if (strstr(tekster[j], forbudt[k]) != NULL) {
                    rent = false;
                    ZS_FAIL("fagsprog paa skaermen: \"%s\" i \"%s\"",
                            forbudt[k], tekster[j]);
                }
            }
        }
    }
    CHECK("ingen fagsprog i noget kunden laeser", rent);
}
