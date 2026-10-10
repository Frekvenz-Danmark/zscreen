/*
 * zScreen - ladeplanen mod noterne, raekke for raekke.
 *
 * HVORFOR TALLENE STAAR SKREVET UD HER.
 *
 * Testen bruger IKKE modulets egne konstanter til at forvente med. Den
 * skriver tallene som de staar i docs/batteristyring.md: 3, 10000, 9900,
 * 500. Brugte vi ZS_LADE_FULD_RATE paa begge sider, ville testen bestaa
 * uanset hvad tallet blev aendret til, og saa provede den ingenting. Den
 * er her netop for at sige fra hvis koden holder op med at passe med
 * noterne - det er den eneste kilde vi har indtil nogen maaler paa et
 * anlaeg.
 *
 * Skal tallene aendres fordi en MAALING siger noget andet, skal baade
 * noterne, koden og de her linjer rettes. Tre steder er med vilje: en
 * skalafaktor der aendres i tavshed, er punkt 2 paa maalelisten.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_ladeplan.h"

#include <stdlib.h>
#include <string.h>

/* Noternes tal. Skrevet ud, ikke laant fra headeren. */
#define NOTE_FULD       10000
#define NOTE_RESERVE_HOEJ 9900
#define NOTE_RESERVE_LAV   500
#define NOTE_STYRET          3
#define NOTE_AUTOMATIK       0

/* En gyldig timer, godt inde i det tilladte. */
#define TIMER            600

static void raekke(const char *hvad, zs_lade_tilstand_t t, int32_t rate,
                   int32_t stor, int32_t ud_w, int32_t ind_w, int32_t rsv)
{
    zs_ladeplan_t p;
    char navn[96];
    memset(&p, 0, sizeof(p));
    snprintf(navn, sizeof(navn), "%s: planen blev lavet", hvad);
    CHECK(navn, zs_ladeplan_lav(&p, t, rate, TIMER));
    snprintf(navn, sizeof(navn), "%s: StorCtl_Mod", hvad);
    CHECK_INT(navn, p.stor_ctl_mod, stor);
    snprintf(navn, sizeof(navn), "%s: OutWRte", hvad);
    CHECK_INT(navn, p.out_w_rte, ud_w);
    snprintf(navn, sizeof(navn), "%s: InWRte", hvad);
    CHECK_INT(navn, p.in_w_rte, ind_w);
    snprintf(navn, sizeof(navn), "%s: MinRsvPct", hvad);
    CHECK_INT(navn, p.min_rsv_pct, rsv);
    snprintf(navn, sizeof(navn), "%s: timeren kom med uaendret", hvad);
    CHECK_INT(navn, p.doedmand_s, TIMER);
}

void test_ladeplan(void)
{
    ZS_SUITE("Ladeplanen mod noterne i docs/batteristyring.md");

    /*
     * | 1. Oplad | 3 | -rate (gulv) | 10000 | 9900 |
     *
     * Tre rater, saa gulvet skal FOELGE raten og ikke bare vaere negativt.
     * Med ét tal ville "-5000 altid" bestaa.
     */
    raekke("oplad 1", ZS_LADE_OPLAD, 1,
           NOTE_STYRET, -1, NOTE_FULD, NOTE_RESERVE_HOEJ);
    raekke("oplad 3700", ZS_LADE_OPLAD, 3700,
           NOTE_STYRET, -3700, NOTE_FULD, NOTE_RESERVE_HOEJ);
    raekke("oplad fuld", ZS_LADE_OPLAD, NOTE_FULD,
           NOTE_STYRET, -NOTE_FULD, NOTE_FULD, NOTE_RESERVE_HOEJ);

    /* | 2. Aflad | 3 | 10000 | -rate (gulv) | 500 |  - spejlvendt */
    raekke("aflad 1", ZS_LADE_AFLAD, 1,
           NOTE_STYRET, NOTE_FULD, -1, NOTE_RESERVE_LAV);
    raekke("aflad 3700", ZS_LADE_AFLAD, 3700,
           NOTE_STYRET, NOTE_FULD, -3700, NOTE_RESERVE_LAV);
    raekke("aflad fuld", ZS_LADE_AFLAD, NOTE_FULD,
           NOTE_STYRET, NOTE_FULD, -NOTE_FULD, NOTE_RESERVE_LAV);

    /*
     * | 3. Normal | 0 | 10000 | 10000 | 500 |
     *
     * StorCtl_Mod 0 er det vigtige: inverteren tilbage til sin egen
     * automatik. Og de tre andre NULSTILLES, saa et gulv fra en
     * tidligere kommando ikke bliver staaende - derfor 10000 og ikke 0.
     */
    raekke("normal", ZS_LADE_NORMAL, 0,
           NOTE_AUTOMATIK, NOTE_FULD, NOTE_FULD, NOTE_RESERVE_LAV);

    /* | 5. Laas | 3 | 0 | 10000 | 500 | */
    raekke("laas", ZS_LADE_LAAS, 0,
           NOTE_STYRET, 0, NOTE_FULD, NOTE_RESERVE_LAV);

    /*
     * Tilstand 4: "zero export er bare at lade med 100 %. Altsaa samme
     * registre som tilstand 1, med fuld rate."
     *
     * Det staar som en paastand i noterne, saa den proeves som en
     * paastand: de to planer skal vaere ENS, hele strukturen, ikke felt
     * for felt. Det er ogsaa det der holder udfyldningsbytene i hullet
     * ryddet - uden memset i modulet er to ens planer forskellige.
     */
    raekke("zero export", ZS_LADE_ZERO_EXPORT, 0,
           NOTE_STYRET, -NOTE_FULD, NOTE_FULD, NOTE_RESERVE_HOEJ);
    {
        zs_ladeplan_t z, o;
        memset(&z, 0x11, sizeof(z));
        memset(&o, 0x22, sizeof(o));
        CHECK("zero export kunne laves", zs_ladeplan_lav(&z, ZS_LADE_ZERO_EXPORT, 0, TIMER));
        CHECK("oplad med fuld rate kunne laves", zs_ladeplan_lav(&o, ZS_LADE_OPLAD, NOTE_FULD, TIMER));
        CHECK("zero export ER oplad med fuld rate, hele strukturen",
              memcmp(&z, &o, sizeof(z)) == 0);
    }

    /* Navnene, til loggen. */
    CHECK_STR("navn oplad",       zs_ladeplan_navn(ZS_LADE_OPLAD), "oplad");
    CHECK_STR("navn aflad",       zs_ladeplan_navn(ZS_LADE_AFLAD), "aflad");
    CHECK_STR("navn normal",      zs_ladeplan_navn(ZS_LADE_NORMAL), "normal");
    CHECK_STR("navn zeroExport", zs_ladeplan_navn(ZS_LADE_ZERO_EXPORT), "zeroExport");
    CHECK_STR("navn laas",        zs_ladeplan_navn(ZS_LADE_LAAS), "laas");
}

void test_ladeplan_vagter(void)
{
    zs_ladeplan_t p;
    ZS_SUITE("Ladeplanen: hvad den naegter, og hvorfor");

    /*
     * DOEDMANDSKNAPPEN, BEGGE ENDER.
     *
     * Nul er en beslutning fra 8. oktober: ingen plan uden en timer.
     * Loftet er vores eget, og det kom af en maaling: foer stod der kun
     * "over nul", og 65535 sekunder - 18 timer - blev godtaget.
     */
    CHECK("timer 0 afvises: ingen plan uden doedmandsknap",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, 0));
    CHECK("timer 1 godtages", zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, 1));
    CHECK("timer paa loftet godtages",
          zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, ZS_LADE_DOEDMAND_MAKS_S));
    CHECK("ét sekund over loftet afvises",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, ZS_LADE_DOEDMAND_MAKS_S + 1));
    CHECK("en time afvises: raekker ind i naeste kvarter",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, 3600));
    CHECK("65535 sekunder afvises: 18 timer er ingen doedmandsknap",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 5000, 65535));
    CHECK_INT("loftet er ét kvarter", ZS_LADE_DOEDMAND_MAKS_S, 900);

    /*
     * RATE NUL TIL EN TILSTAND DER SKAL HAVE EN.
     *
     * Maalt 9. oktober 2026, og det her er grunden til at den er en fejl
     * og ikke bare et tal: oplad med rate 0 gav OutWRte 0 og MinRsvPct
     * 9900. Altsaa "al afladning blokeret, hold 99 % reserve" - og
     * noterne siger selv at 99 % skubber inverteren mod at LADE, altsaa
     * koebe fra nettet. Den der bad om nul effekt, fik en plan der
     * handler stroem. Aflad med 0 gav InWRte 0: al LADNING blokeret.
     */
    CHECK("oplad med rate 0 afvises: det er ikke ingenting, det er 99 % reserve",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 0, TIMER));
    CHECK("aflad med rate 0 afvises: det er ikke ingenting, det blokerer ladning",
          !zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 0, TIMER));
    CHECK("vil man ingenting, er det normal, og den virker",
          zs_ladeplan_lav(&p, ZS_LADE_NORMAL, 0, TIMER));

    /*
     * EN RATE TIL EN TILSTAND DER IKKE SER PAA DEN.
     * Den ville blive kastet vaek i tavshed, og kalderen ville tro han
     * havde sat en effekt.
     */
    CHECK("rate til normal afvises",
          !zs_ladeplan_lav(&p, ZS_LADE_NORMAL, 5000, TIMER));
    CHECK("rate til laas afvises",
          !zs_ladeplan_lav(&p, ZS_LADE_LAAS, 5000, TIMER));
    CHECK("rate til zero export afvises: fuld rate er indbygget",
          !zs_ladeplan_lav(&p, ZS_LADE_ZERO_EXPORT, 5000, TIMER));
    CHECK("ogsaa en rate paa 1 til normal afvises",
          !zs_ladeplan_lav(&p, ZS_LADE_NORMAL, 1, TIMER));

    /* Raten uden for graenserne, begge ender og det vilde. */
    CHECK("rate -1 afvises",     !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, -1, TIMER));
    CHECK("rate over fuld afvises",
          !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, NOTE_FULD + 1, TIMER));
    CHECK("rate INT32_MAX afvises", !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, 2147483647, TIMER));
    CHECK("rate INT32_MIN afvises", !zs_ladeplan_lav(&p, ZS_LADE_OPLAD, -2147483647 - 1, TIMER));

    /*
     * Tilstande der ikke findes. 0 er med med vilje: en nulstillet
     * variabel er ikke en gyldig tilstand, og det skal den heller ikke
     * blive ved et uheld.
     */
    const int ukendte[] = { 0, 6, 7, -1, 255, 1000000 };
    for (size_t i = 0; i < sizeof(ukendte) / sizeof(ukendte[0]); i++) {
        char navn[64];
        snprintf(navn, sizeof(navn), "tilstand %d afvises", ukendte[i]);
        CHECK(navn, !zs_ladeplan_lav(&p, (zs_lade_tilstand_t)ukendte[i], 0, TIMER));
        snprintf(navn, sizeof(navn), "navn paa tilstand %d er \"ukendt\"", ukendte[i]);
        CHECK_STR(navn, zs_ladeplan_navn((zs_lade_tilstand_t)ukendte[i]), "ukendt");
    }

    CHECK("ud == NULL afvises", !zs_ladeplan_lav(NULL, ZS_LADE_OPLAD, 5000, TIMER));

    /*
     * EN AFVIST PLAN MAA IKKE ROERE NOGET.
     *
     * Headeren lover det, og det er vigtigere end det ser ud: en kalder
     * der genbruger samme struktur til hver plan, skal ikke kunne staa
     * med en halv plan efter et afslag. En halv plan er den vaerste
     * slags - det er den der bliver sendt.
     */
    {
        zs_ladeplan_t foer;
        memset(&p, 0, sizeof(p));
        CHECK("en gyldig plan til at maale paa",
              zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 3700, TIMER));
        foer = p;
        (void)zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 3700, 0);
        (void)zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 3700, 65535);
        (void)zs_ladeplan_lav(&p, (zs_lade_tilstand_t)9, 0, TIMER);
        (void)zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 99999, TIMER);
        (void)zs_ladeplan_lav(&p, ZS_LADE_AFLAD, 0, TIMER);
        (void)zs_ladeplan_lav(&p, ZS_LADE_NORMAL, 1, TIMER);
        CHECK("seks afslag roerte ikke den plan der stod der",
              memcmp(&foer, &p, sizeof(p)) == 0);
    }
}


/*
 * NAVNENE PAA TRAADEN.
 *
 * Platformen skriver tilstanden som tekst i et felt paa serveren, og
 * serveren haandhaever en liste over tilladte vaerdier. Maalt 10. oktober
 * 2026: den afviser "OPLAD", "oplad " med mellemrum og den tomme streng
 * med HTTP 406, og den gamle vaerdi bliver staaende.
 *
 * De fem navne staar skrevet ud her, saa listen i zScreen-server
 * (scripts/anlaeg.py) kan sammenlignes med ét blik. Staar de ikke ens,
 * kan Platformen skrive en tilstand firmwaren ikke forstaar - eller
 * omvendt, og saa bliver planen aldrig sat.
 */
void test_ladeplan_navne(void)
{
    zs_lade_tilstand_t t;
    ZS_SUITE("Ladeplanen: navnene paa traaden");

    /* Serverens allowedValues, skrevet ud. Samme fem, samme stavemaade. */
    const char *liste[] = { "oplad", "aflad", "normal", "zeroExport", "laas" };
    const zs_lade_tilstand_t svarer[] = {
        ZS_LADE_OPLAD, ZS_LADE_AFLAD, ZS_LADE_NORMAL,
        ZS_LADE_ZERO_EXPORT, ZS_LADE_LAAS,
    };
    for (size_t i = 0; i < sizeof(liste) / sizeof(liste[0]); i++) {
        char navn[80];
        snprintf(navn, sizeof(navn), "\"%s\" bliver den rigtige tilstand", liste[i]);
        t = (zs_lade_tilstand_t)0;
        CHECK(navn, zs_ladeplan_tilstand(liste[i], &t) && t == svarer[i]);
        snprintf(navn, sizeof(navn), "og tilbage igen: %s", liste[i]);
        CHECK_STR(navn, zs_ladeplan_navn(svarer[i]), liste[i]);
    }

    /*
     * INGEN TOLERANCE. Serveren godtager kun den praecise streng, saa
     * firmwaren maa ikke vaere mere tilgivende: et navn vi "retter" i
     * tavshed, er en tilstand kunden ikke bad om.
     */
    const char *nej[] = {
        "OPLAD", "Oplad", "oplad ", " oplad", "oplad\n", "",
        "opladning", "opl", "zero export", "zeroexport", "ZeroExport",
        "noedstop", "ukendt", "3", "normal2",
    };
    for (size_t i = 0; i < sizeof(nej) / sizeof(nej[0]); i++) {
        char navn[80];
        snprintf(navn, sizeof(navn), "\"%s\" afvises", nej[i]);
        t = ZS_LADE_LAAS;
        CHECK(navn, !zs_ladeplan_tilstand(nej[i], &t));
        snprintf(navn, sizeof(navn), "og \"%s\" roerte ikke ud", nej[i]);
        CHECK(navn, t == ZS_LADE_LAAS);
    }
    CHECK("NULL-navn afvises", !zs_ladeplan_tilstand(NULL, &t));
    CHECK("NULL-ud afvises", !zs_ladeplan_tilstand("oplad", NULL));
}

/*
 * KVITTERINGEN.
 *
 * Besluttet 10. oktober 2026: enheden skal efterse at skrivningen blev
 * udfoert. Det her er dommen, og den kan proeves uden en inverter.
 *
 * Navnene er ogsaa serverens, fordi OpenRemotes egen executionStatus
 * ikke kunne bruges: maalt 10. oktober er dens tilladte vaerdier
 * REQUEST_START, REQUEST_REPEATING, REQUEST_CANCEL, READY, COMPLETED,
 * RUNNING og CANCELLED - og ingen af dem betyder "inverteren gjorde det
 * ikke".
 */
void test_ladeplan_kvittering(void)
{
    zs_ladeplan_t vil, fik;
    ZS_SUITE("Ladeplanen: blev planen faktisk sat");

    CHECK("en plan at efterse", zs_ladeplan_lav(&vil, ZS_LADE_OPLAD, 3700, TIMER));

    fik = vil;
    CHECK_INT("alt stemmer -> ok", zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_OK);

    /*
     * ÉT felt forkert ad gangen. Hver af dem skal udpeges ved navn, for
     * den der staar med en log skal vide HVILKET register der ikke tog
     * imod - ikke bare at noget gik galt.
     */
    fik = vil; fik.doedmand_s = (uint16_t)(vil.doedmand_s - 1);
    CHECK_INT("timeren tog ikke -> doedmand",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_DOEDMAND);
    fik = vil; fik.stor_ctl_mod = 0;
    CHECK_INT("StorCtl_Mod forkert -> tilstand",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_TILSTAND);
    fik = vil; fik.out_w_rte = 0;
    CHECK_INT("OutWRte forkert -> ud",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_UD);
    fik = vil; fik.in_w_rte = 0;
    CHECK_INT("InWRte forkert -> ind",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_IND);
    fik = vil; fik.min_rsv_pct = 0;
    CHECK_INT("MinRsvPct forkert -> reserve",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_RESERVE);

    /*
     * TIMEREN FOERST. Er baade timeren og et register forkert, skal
     * svaret vaere timeren: den er den ene der kan efterlade en kundes
     * batteri fastlaast hvis forbindelsen gaar.
     */
    fik = vil;
    fik.doedmand_s = 1;
    fik.out_w_rte = 0;
    fik.min_rsv_pct = 0;
    CHECK_INT("timeren vejer tungest naar flere er forkerte",
              zs_ladeplan_kvitter(&vil, &fik), ZS_LADE_KVIT_DOEDMAND);

    /* Ingen tilbagelaesning. Og det maa ALDRIG se ud som held. */
    CHECK_INT("ingen tilbagelaesning -> ingenSvar",
              zs_ladeplan_kvitter(&vil, NULL), ZS_LADE_KVIT_INTET_SVAR);
    CHECK_INT("heller ikke uden en oensket plan",
              zs_ladeplan_kvitter(NULL, &fik), ZS_LADE_KVIT_INTET_SVAR);
    CHECK_INT("og slet ikke med to NULL",
              zs_ladeplan_kvitter(NULL, NULL), ZS_LADE_KVIT_INTET_SVAR);

    /*
     * ALLE FEM FELTER SKAL VEJE. Et felt der ikke blev efterset, ville
     * se ud som en kvittering og vaere en loegn. Her aendres hvert felt
     * for sig og INGEN af dem maa give ok.
     */
    for (int i = 0; i < 5; i++) {
        char navn[72];
        fik = vil;
        switch (i) {
        case 0: fik.stor_ctl_mod += 1; break;
        case 1: fik.out_w_rte    += 1; break;
        case 2: fik.in_w_rte     += 1; break;
        case 3: fik.min_rsv_pct  += 1; break;
        default: fik.doedmand_s  = (uint16_t)(fik.doedmand_s + 1); break;
        }
        snprintf(navn, sizeof(navn), "felt %d taelder med: ét forkert tal giver ikke ok", i);
        CHECK(navn, zs_ladeplan_kvitter(&vil, &fik) != ZS_LADE_KVIT_OK);
    }

    /* Navnene, som de ogsaa staar paa serveren. */
    CHECK_STR("kvit ok",        zs_ladeplan_kvit_navn(ZS_LADE_KVIT_OK), "ok");
    CHECK_STR("kvit doedmand",  zs_ladeplan_kvit_navn(ZS_LADE_KVIT_DOEDMAND), "doedmand");
    CHECK_STR("kvit tilstand",  zs_ladeplan_kvit_navn(ZS_LADE_KVIT_TILSTAND), "tilstand");
    CHECK_STR("kvit ud",        zs_ladeplan_kvit_navn(ZS_LADE_KVIT_UD), "ud");
    CHECK_STR("kvit ind",       zs_ladeplan_kvit_navn(ZS_LADE_KVIT_IND), "ind");
    CHECK_STR("kvit reserve",   zs_ladeplan_kvit_navn(ZS_LADE_KVIT_RESERVE), "reserve");
    CHECK_STR("kvit ingenSvar", zs_ladeplan_kvit_navn(ZS_LADE_KVIT_INTET_SVAR), "ingenSvar");
    CHECK_STR("kvit uden for listen", zs_ladeplan_kvit_navn((zs_lade_kvit_t)99), "ukendt");
    CHECK_STR("og negativ",     zs_ladeplan_kvit_navn((zs_lade_kvit_t)-1), "ukendt");
}
