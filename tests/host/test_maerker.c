/*
 * Tabellen over maerker.
 *
 * Det her er stedet der afgoer om vi kan udvide til flere invertere uden
 * at rode i afkodningen. Og det er stedet hvor en fejl er sveer at se:
 * bruger vi det forkerte maerkes fejltabel, staar der en forkert fejl paa
 * skaermen, skrevet med fuld sikkerhed.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_maerker.h"

#include <string.h>

void test_maerker(void)
{
    ZS_SUITE("Mærker: Fronius genkendes som de skriver sig selv");

    /* Fronius skriver selv baade med stort og smaat, alt efter
     * firmwareudgave, og vi har set begge dele paa rigtige enheder. */
    const zs_maerke_t *m = zs_maerke_find("Fronius");
    CHECK("Fronius", m->dc_layout == ZS_DC_BATTERI_SIDST);
    CHECK("FRONIUS", zs_maerke_find("FRONIUS")->dc_layout == ZS_DC_BATTERI_SIDST);
    CHECK("fronius med smaat",
          zs_maerke_find("fronius")->dc_layout == ZS_DC_BATTERI_SIDST);
    /* Nogle skriver hele firmanavnet i feltet. */
    CHECK("Fronius International GmbH",
          zs_maerke_find("Fronius International GmbH")->dc_layout
          == ZS_DC_BATTERI_SIDST);
    CHECK("med mellemrum foran",
          zs_maerke_find("  Fronius  ")->dc_layout == ZS_DC_BATTERI_SIDST);

    CHECK("og den har sine fejltabeller med",
          m->evtvnd1 != NULL && m->n_evtvnd1 > 0);
    CHECK("og et praefiks saa kunden kan slaa koden op",
          m->kode_praefiks != NULL && m->kode_praefiks[0] != '\0');

    ZS_SUITE("Mærker: et mærke vi ikke kender");

    /*
     * DET VIGTIGSTE I HELE FILEN.
     *
     * Foer blev Fronius' fejltabeller brugt paa ENHVER inverter. En
     * Huawei med bit 1 sat ville faa teksten "Netfejl" og henvisningen
     * "Fronius-kode 101" paa skaermen. En forkert fejl, skrevet med fuld
     * sikkerhed, om en inverter vi ikke har papir paa.
     */
    const zs_maerke_t *u = zs_maerke_find("Huawei");
    CHECK("Huawei faar IKKE Fronius' fejltabeller", u->evtvnd1 == NULL);
    CHECK("heller ikke den anden", u->evtvnd2 == NULL);
    CHECK("eller den tredje", u->evtvnd3 == NULL);
    CHECK("og ingen laengder", u->n_evtvnd1 == 0 && u->n_evtvnd2 == 0
                               && u->n_evtvnd3 == 0);
    CHECK("og vi gaetter ikke paa kanalopdelingen",
          u->dc_layout == ZS_DC_UKENDT);
    CHECK("og intet kode-praefiks der peger paa en forkert manual",
          u->kode_praefiks == NULL);

    CHECK("SMA ogsaa", zs_maerke_find("SMA Solar")->evtvnd1 == NULL);
    CHECK("SolarEdge ogsaa", zs_maerke_find("SolarEdge")->evtvnd1 == NULL);

    ZS_SUITE("Mærker: det der ikke må vælte");

    CHECK("NULL giver ukendt og ikke et nedbrud",
          zs_maerke_find(NULL)->dc_layout == ZS_DC_UKENDT);
    CHECK("tom streng ogsaa",
          zs_maerke_find("")->dc_layout == ZS_DC_UKENDT);
    CHECK("og den returnerer ALDRIG NULL",
          zs_maerke_find(NULL) != NULL && zs_maerke_find("") != NULL
          && zs_maerke_find("hvadsomhelst") != NULL);

    /* Et navn der er kortere end maerket maa ikke ramme. */
    CHECK("Fron er ikke Fronius",
          zs_maerke_find("Fron")->dc_layout == ZS_DC_UKENDT);
    /* Og et der kun ligner. */
    CHECK("Froniusx taeller som Fronius, for det INDEHOLDER navnet",
          zs_maerke_find("Froniusx")->dc_layout == ZS_DC_BATTERI_SIDST);

    ZS_SUITE("Mærker: reglerne for selve tabellen");

    CHECK("der er mindst ét mærke", zs_maerke_antal() >= 1);
    CHECK("ud over enden giver NULL", zs_maerke_nr(zs_maerke_antal()) == NULL);

    for (size_t i = 0; i < zs_maerke_antal(); i++) {
        const zs_maerke_t *e = zs_maerke_nr(i);
        CHECK("hvert maerke har et navn",
              e != NULL && e->maerke != NULL && e->maerke[0] != '\0');
        /*
         * Har et maerke en fejltabel, SKAL det ogsaa have et praefiks.
         * Ellers staar der "Kode 101" paa skaermen uden at sige hvis
         * manual kunden skal slaa op i.
         */
        if (e != NULL && e->evtvnd1 != NULL) {
            CHECK("et maerke med fejltabel har ogsaa et praefiks",
                  e->kode_praefiks != NULL && e->kode_praefiks[0] != '\0');
            CHECK("og en laengde der passer", e->n_evtvnd1 > 0);
        }
        /* Og omvendt: ingen tabel, ingen laengde. */
        if (e != NULL && e->evtvnd1 == NULL) {
            CHECK("ingen tabel betyder ingen laengde", e->n_evtvnd1 == 0);
        }
    }
}

/*
 * Tabellen holdt op mod sig selv.
 *
 * Her staar ingen navne skrevet ind. Proeven gaar igennem HVER raekke i
 * MAERKER og holder den op mod reglerne, saa den ogsaa daekker det maerke
 * nummer to vi laegger ind, uden at nogen husker at skrive en proeve.
 *
 * Grunden: en mutationstest viste at ombytningen af store bogstaver havde
 * en graense der aldrig blev ramt, fordi det eneste maerke i tabellen
 * hedder "Fronius" og ikke har et Z. Et maerke som Zeversolar eller ZCS,
 * der begge findes, kunne ikke findes af koden. Den slags skal fanges af
 * tabellen selv, ikke af at nogen gaetter rigtigt.
 */
void test_maerke_tabel(void)
{
    ZS_SUITE("Mærker: hver række i tabellen holdt op mod reglerne");

    size_t antal = zs_maerke_antal();
    CHECK("der er mindst ét mærke i tabellen", antal > 0);

    int tom = 0, ikke_sig_selv = 0, ikke_smaat = 0, ikke_stort = 0, ikke_del = 0;

    for (size_t i = 0; i < antal; i++) {
        const zs_maerke_t *m = zs_maerke_nr(i);
        if (m == NULL || m->maerke == NULL || m->maerke[0] == '\0') {
            tom++;
            continue;
        }

        /* Teksten i tabellen skal finde raekken selv. */
        if (zs_maerke_find(m->maerke) != m) { ikke_sig_selv++; }

        /* Og den skal findes uanset hvordan inverteren skriver den. Vi
         * laver kopier med alt smaat og alt stort og leder efter dem. */
        char smaat[64], stort[64], laengere[96];
        size_t n = strlen(m->maerke);
        if (n >= sizeof(smaat)) { n = sizeof(smaat) - 1; }
        for (size_t k = 0; k < n; k++) {
            char c = m->maerke[k];
            smaat[k] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            stort[k] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        }
        smaat[n] = '\0';
        stort[n] = '\0';

        if (zs_maerke_find(smaat) != m) { ikke_smaat++; }
        if (zs_maerke_find(stort) != m) { ikke_stort++; }

        /* Som en del af et laengere felt, for det er saadan de skriver. */
        snprintf(laengere, sizeof(laengere), "  %s International GmbH", smaat);
        if (zs_maerke_find(laengere) != m) { ikke_del++; }
    }

    CHECK_INT("ingen række har tom tekst", tom, 0);
    CHECK_INT("hver række finder sig selv", ikke_sig_selv, 0);
    CHECK_INT("og findes skrevet med småt", ikke_smaat, 0);
    CHECK_INT("og skrevet med STORT", ikke_stort, 0);
    CHECK_INT("og som en del af et længere felt", ikke_del, 0);
}
