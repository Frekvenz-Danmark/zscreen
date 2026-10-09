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
    CHECK_STR("navn zero export", zs_ladeplan_navn(ZS_LADE_ZERO_EXPORT), "zero export");
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
