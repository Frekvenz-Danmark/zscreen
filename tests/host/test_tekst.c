/*
 * De smaa tekstgreb.
 *
 * Den her fil findes fordi en mutationstest fandt to huller der begge
 * handlede om det samme: kode der stod skrevet forfra hver gang, og hvor
 * den ene halvdel aldrig var blevet proevet af.
 *
 *   1. vagten "ud != NULL && len > 0" stod fire steder, og ingen proeve
 *      gav NULL med. && kunne laves til || uden at nogen sagde fra.
 *   2. ombytningen af store bogstaver havde en oevre graense der aldrig
 *      blev ramt, fordi det eneste maerke i tabellen hedder "Fronius".
 *
 * Derfor proever vi her hele byte-omraadet og alle kanterne, ikke bare
 * de tilfaelde vi lige kan komme i tanke om.
 */

#include "zs_test.h"
#include "../../firmware/main/util/zs_tekst.h"

#include <stdlib.h>
#include <string.h>

void test_tekst(void)
{
    ZS_SUITE("Tekst: store bogstaver bliver smaa, HELE vejen");

    /*
     * Alle 26. Ikke tre stikproever: det var netop Z, altsaa den sidste,
     * der var gaaet fri, og med kun Fronius i tabellen var der ingen der
     * opdagede det.
     */
    int forkert = 0;
    for (char c = 'A'; c <= 'Z'; c++) {
        if (zs_tekst_smaa(c) != (char)(c - 'A' + 'a')) { forkert++; }
    }
    CHECK_INT("alle 26 store bogstaver bliver smaa", forkert, 0);
    CHECK("A, den foerste", zs_tekst_smaa('A') == 'a');
    CHECK("Z, den sidste, og den der var forkert", zs_tekst_smaa('Z') == 'z');

    /* Og lige udenfor. '@' ligger lige foer 'A' og '[' lige efter 'Z' i
     * ASCII, saa de to fanger en graense der er sat ét ved siden af. */
    CHECK("@ lige foer A roeres ikke", zs_tekst_smaa('@') == '@');
    CHECK("[ lige efter Z roeres ikke", zs_tekst_smaa('[') == '[');

    /*
     * Hele byte-omraadet. Alt andet end de 26 skal komme uaendret ud,
     * ogsaa tegn over 127. Inverteren sender raa bytes vi ikke bestemmer
     * over, og et serienummer med skrald i maa ikke blive lavet om.
     */
    forkert = 0;
    for (int i = -128; i <= 127; i++) {
        char c = (char)i;
        char ud = zs_tekst_smaa(c);
        bool stort = (c >= 'A' && c <= 'Z');
        if (stort ? (ud != (char)(c - 'A' + 'a')) : (ud != c)) { forkert++; }
    }
    CHECK_INT("alle 256 byte-vaerdier opfoerer sig som aftalt", forkert, 0);

    ZS_SUITE("Tekst: kopi der taaler NULL");

    /*
     * DET VIGTIGSTE HER. Kalderne har lov at sige NULL, se zs_locate.h,
     * og hvis vagten er forkert skriver vi i NULL. Det giver ikke en
     * advarsel, det giver et nedbrud paa en vaeg hos en kunde.
     */
    CHECK("NULL ud goer ingenting", zs_tekst_kopi(NULL, 10, "abc") == false);
    char et[4] = { 'x', 0, 0, 0 };
    CHECK("laengde nul goer ingenting", zs_tekst_kopi(et, 0, "abc") == false);
    CHECK("og roerte ikke bufferen", et[0] == 'x');

    char ud[8];
    memset(ud, 'Z', sizeof(ud));
    CHECK("NULL fra giver tom streng", zs_tekst_kopi(ud, sizeof(ud), NULL) == false);
    CHECK_STR("og den ER tom", ud, "");

    CHECK("almindelig kopi lykkes", zs_tekst_kopi(ud, sizeof(ud), "Fronius"));
    CHECK_STR("og staar der", ud, "Fronius");

    /* Praecis saa lang at nulbyten er den sidste plads. Her falder en
     * off-by-one fra hinanden, og det er den typiske fejl. */
    char lige[8];
    CHECK("praecis fuld lykkes", zs_tekst_kopi(lige, sizeof(lige), "1234567"));
    CHECK_STR("og hele vejen med", lige, "1234567");

    char kort[4];
    CHECK("for lang bliver klippet", zs_tekst_kopi(kort, sizeof(kort), "123456") == false);
    CHECK_STR("og er stadig afsluttet", kort, "123");
    CHECK("ét tegns plads giver tom streng",
          zs_tekst_kopi(kort, 1, "abc") == false && kort[0] == '\0');

    /*
     * Praecis stor buffer paa heapen, saa ASAN ser det hvis vi skriver
     * én byte for meget. I en buffer paa stakken ligger der plads ved
     * siden af, og saa gaar fejlen ubemaerket forbi.
     */
    char *noejagtig = malloc(5);
    CHECK("heap-buffer", noejagtig != NULL);
    if (noejagtig != NULL) {
        CHECK("fylder en noejagtig buffer helt",
              zs_tekst_kopi(noejagtig, 5, "abcd"));
        CHECK_STR("uden at skrive udenfor", noejagtig, "abcd");
        CHECK("og klipper naar der er én for meget",
              zs_tekst_kopi(noejagtig, 5, "abcde") == false);
        CHECK_STR("stadig afsluttet", noejagtig, "abcd");
        free(noejagtig);
    }

    ZS_SUITE("Tekst: find uden at skele til store og smaa");

    CHECK("samme tekst", zs_tekst_indeholder("Fronius", "Fronius"));
    CHECK("alt med stort", zs_tekst_indeholder("FRONIUS", "Fronius"));
    CHECK("alt med smaat", zs_tekst_indeholder("fronius", "Fronius"));
    CHECK("som en del af en laengere", zs_tekst_indeholder("Fronius GmbH", "Fronius"));
    CHECK("og naar den staar bagerst",
          zs_tekst_indeholder("Leveret af Fronius", "Fronius"));

    /*
     * DET HER ER FEJLEN DER BLEV FUNDET.
     *
     * Naalen kommer fra tabellen og skrives med stort forbogstav, se
     * MAERKER i zs_maerker.c. Et maerke med Z i navnet kunne ikke findes,
     * fordi den oevre graense i ombytningen var sat ved siden af. Baade
     * Zeversolar og ZCS Azzurro er rigtige fabrikater, saa det var ikke
     * teori. Proeverne her skal blive staaende selv om vi ikke har dem i
     * tabellen endnu: de holder kanten paa plads.
     */
    CHECK("et maerke med Z, naal med stort",
          zs_tekst_indeholder("zeversolar tlc", "Zeversolar"));
    CHECK("et maerke der kun er store bogstaver",
          zs_tekst_indeholder("zcs azzurro", "ZCS"));
    CHECK("og den anden vej", zs_tekst_indeholder("ZCS AZZURRO", "zcs"));

    /* Kanterne. */
    CHECK("naalen staar lige i enden", zs_tekst_indeholder("abcXYZ", "xyz"));
    CHECK("naalen staar lige i starten", zs_tekst_indeholder("XYZabc", "xyz"));
    CHECK("naal laengere end hoejstakken",
          zs_tekst_indeholder("Fron", "Fronius") == false);
    CHECK("naal der ikke er der", zs_tekst_indeholder("Huawei", "Fronius") == false);
    CHECK("naesten, men ét tegn galt",
          zs_tekst_indeholder("Froniux", "Fronius") == false);

    CHECK("NULL hoejstak", zs_tekst_indeholder(NULL, "Fronius") == false);
    CHECK("NULL naal", zs_tekst_indeholder("Fronius", NULL) == false);
    /* Tom naal siger false med vilje: et maerke fundet fordi tabellen
     * havde en tom tekst er vaerre end et maerke vi ikke fandt. */
    CHECK("tom naal finder ikke alt", zs_tekst_indeholder("Fronius", "") == false);
    CHECK("tom hoejstak", zs_tekst_indeholder("", "Fronius") == false);

    /*
     * Hoejstak uden nulbyte efter sig, praecis stor paa heapen. Laeser
     * soegningen én byte for langt, siger ASAN fra. Uden det kan en
     * "i + n <= h" der er blevet til "i + n <= h + 1" ligge og gemme sig.
     */
    const char *tekst = "Fronius";
    size_t len = strlen(tekst);
    char *h = malloc(len + 1);
    CHECK("heap-hoejstak", h != NULL);
    if (h != NULL) {
        memcpy(h, tekst, len + 1);
        CHECK("findes i en noejagtig buffer", zs_tekst_indeholder(h, "Fronius"));
        CHECK("og findes ikke naar der mangler ét tegn",
              zs_tekst_indeholder(h, "Froniusx") == false);
        free(h);
    }
}
