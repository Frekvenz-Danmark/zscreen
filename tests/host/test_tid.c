/*
 * localtime_r, tzset og setenv er POSIX, ikke C-standard. Uden det her er
 * de skjult paa glibc naar der oversaettes med -std=c11, og saa bygger
 * filen paa Mac men ikke paa Linux. Samme linje som i zs_price_now.c, og
 * den skal staa FOER enhver include.
 */
#define _POSIX_C_SOURCE 200809L

#include "zs_test.h"
#include "../../firmware/kerne/util/zs_tid.h"
#include "../../firmware/kerne/net/zs_energi.h"
#include "../../firmware/kerne/net/zs_kvarter.h"
#include "../../firmware/kerne/zs_config.h"

#include <stdlib.h>
#include <time.h>

/* Laver en struct tm af en dato. Kun felterne zs_tid_dagnr bruger. */
static struct tm dato(int aar, int maaned, int dag)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = aar - 1900;
    t.tm_mon  = maaned - 1;
    t.tm_mday = dag;
    return t;
}

/*
 * Testene her fodrer modulerne PRÆCIS som zs_app.c gør: klokken fra
 * localtime_r, dagnummeret fra zs_tid_dagnr, i firmwarens egen tidszone.
 *
 * Det er hele pointen. De gamle tests gav modulerne et bøttenummer vi
 * selv havde regnet ud, og derfor kunne de ikke se at firmwaren gav dem
 * to tal fra hver sit ur. Time 23 og time 00/01 blev aldrig sendt, og
 * 1258 grønne tjek sagde ingenting om det.
 */

void test_tid(void)
{
    ZS_SUITE("Tid: dagnummeret af den lokale dato");

    struct tm t;
    t = dato(1970, 1, 1);
    CHECK_INT("1. januar 1970 er dag 0", zs_tid_dagnr(&t), 0);
    t = dato(2026, 1, 1);
    CHECK_INT("1. januar 2026", zs_tid_dagnr(&t), 20454);
    t = dato(2026, 10, 9);
    CHECK_INT("9. oktober 2026", zs_tid_dagnr(&t), 20735);
    t = dato(2026, 3, 29);
    CHECK_INT("forårsskiftet 2026", zs_tid_dagnr(&t), 20541);
    t = dato(2026, 10, 25);
    CHECK_INT("efterårsskiftet 2026", zs_tid_dagnr(&t), 20751);
    CHECK_INT("NULL giver 0", zs_tid_dagnr(NULL), 0);
    t = dato(1969, 12, 31);
    CHECK_INT("før 1970 giver 0", zs_tid_dagnr(&t), 0);

    /* Skudaar: 29. februar skal findes, og 1. marts skal ligge dagen
     * efter. Uden undtagelsen for aarhundreder ville 2100 skride. */
    t = dato(2024, 2, 29);
    uint16_t skud = zs_tid_dagnr(&t);
    t = dato(2024, 3, 1);
    CHECK_INT("dagen efter 29. februar 2024", zs_tid_dagnr(&t), skud + 1);
    t = dato(2023, 2, 28);
    uint16_t ikke_skud = zs_tid_dagnr(&t);
    t = dato(2023, 3, 1);
    CHECK_INT("2023 har ingen 29. februar", zs_tid_dagnr(&t), ikke_skud + 1);

    /* Hver dag i tre aar skal give praecis ét mere end dagen foer. */
    setenv("TZ", ZS_TIMEZONE, 1);
    tzset();
    bool altid_en_mere = true;
    bool skifter_ved_midnat = true;
    uint16_t forrige = 0;
    int forrige_mday = -1;
    for (time_t s = 1767225600; s < 1767225600 + 1095L * 86400L; s += 600) {
        struct tm lt;
        localtime_r(&s, &lt);
        uint16_t d = zs_tid_dagnr(&lt);
        if (forrige_mday >= 0) {
            if (lt.tm_mday != forrige_mday) {
                /* Ny lokal dato: dagnummeret skal vaere gaaet ét op. */
                if (d != forrige + 1) { altid_en_mere = false; }
                /* Og det maa kun ske naar klokken er under 00:10. */
                if (lt.tm_hour != 0) { skifter_ved_midnat = false; }
            } else if (d != forrige) {
                altid_en_mere = false;
            }
        }
        forrige = d;
        forrige_mday = lt.tm_mday;
    }
    CHECK("tre år igennem: altid præcis én dag mere", altid_en_mere);
    CHECK("og dagen skifter ved lokal midnat", skifter_ved_midnat);

    ZS_SUITE("Tid: timeenergien som firmwaren kalder den");

    /*
     * Syv almindelige døgn. Hver af de 24 timer skal sendes syv gange.
     *
     * DET HER ER FEJLEN DER VAR: med dagnummeret af UTC og timen af
     * lokal tid blev time 23 og time 01 sendt NUL gange.
     */
    zs_energi_basis_t basis;
    memset(&basis, 0, sizeof(basis));
    const bool e_har[ZS_E_ANTAL] = { true, true, true, true, true };
    float staand[ZS_E_ANTAL], e_ud[ZS_E_ANTAL];
    bool e_ud_har[ZS_E_ANTAL];
    int8_t ud_time;
    int pr_time[24];
    for (int i = 0; i < 24; i++) { pr_time[i] = 0; }

    /*
     * Vinduet ligger paa LOKAL midnat og er syv doegn plus ét minut.
     * Det sidste minut er med med vilje: en time sendes foerst naar den
     * naeste tikker, saa uden det ville time 23 paa sidste dag mangle, og
     * testen ville raabe om en fejl der var dens egen.
     */
    time_t fra = 1791410400;            /* 8. okt 2026 00:00 CEST */
    for (time_t s = fra; s <= fra + 7L * 86400L; s += 60) {
        struct tm lt;
        localtime_r(&s, &lt);
        for (int i = 0; i < ZS_E_ANTAL; i++) {
            staand[i] = (float)((s - fra) / 36);
        }
        if (zs_energi_tik(&basis, staand, e_har, "SN1", lt.tm_hour,
                          zs_tid_dagnr(&lt), e_ud, e_ud_har,
                          &ud_time) == ZS_ENERGI_KLAR) {
            if (ud_time >= 0 && ud_time < 24) { pr_time[ud_time]++; }
        }
    }
    bool alle_timer_med = true;
    int mangler = -1;
    for (int h = 0; h < 24; h++) {
        if (pr_time[h] != 7) { alle_timer_med = false; mangler = h; }
    }
    CHECK("alle 24 timer sendes syv gange på syv døgn", alle_timer_med);
    if (!alle_timer_med) {
        ZS_FAIL("time %02d blev sendt %d gange, ikke 7", mangler,
                mangler >= 0 ? pr_time[mangler] : -1);
    }

    ZS_SUITE("Tid: kvartersgennemsnittet som firmwaren kalder det");

    /*
     * Tre almindelige døgn. Bøtten skal taelle opad HELE vejen, ogsaa
     * over midnat, ellers smider zs_kvarter_tik kvarteret vaek som et hul.
     */
    zs_kvarter_t s_kv;
    memset(&s_kv, 0, sizeof(s_kv));
    const bool kv_har[ZS_KV_ANTAL] = { true, true, true, true, true };
    float kv_v[ZS_KV_ANTAL];
    for (int i = 0; i < ZS_KV_ANTAL; i++) { kv_v[i] = 100.0f; }

    int sendt = 0;
    bool boette_voksede = true;
    int32_t forrige_boette = 0;
    bool har_forrige = false;
    for (time_t s = fra; s <= fra + 3L * 86400L; s += 60) {
        struct tm lt;
        localtime_r(&s, &lt);
        int k = zs_kvarter_nummer(lt.tm_hour, lt.tm_min);
        int32_t b = zs_kvarter_boette(zs_tid_dagnr(&lt), k);
        if (har_forrige && b != forrige_boette && b != forrige_boette + 1) {
            boette_voksede = false;
        }
        forrige_boette = b;
        har_forrige = true;
        if (zs_kvarter_tik(&s_kv, kv_v, kv_har, b).klar) { sendt++; }
    }
    CHECK("bøtten tæller opad hele vejen, også over midnat", boette_voksede);
    CHECK_INT("alle 96 kvarterer i hvert af tre døgn", sendt, 3 * 96);

    ZS_SUITE("Tid: hvad sommertidsskiftet koster");

    /*
     * Et helt aar igennem. Der maa vaere PRAECIS ét hul, og det skal
     * ligge paa foraarsskiftet hvor timen 02 ikke findes. Om efteraaret
     * kommer timen 02 to gange, og den anden lander i samme boette som
     * den foerste - intet tabt, men timen daekker to.
     *
     * Før rettelsen var der to huller om DØGNET, altså 730 om året.
     */
    memset(&basis, 0, sizeof(basis));
    int aars_huller = 0;
    int hul_maaned = -1;
    time_t nytaar = 1767225600 - 86400;   /* 31. dec 2025 UTC */
    for (time_t s = nytaar; s < nytaar + 367L * 86400L; s += 60) {
        struct tm lt;
        localtime_r(&s, &lt);
        for (int i = 0; i < ZS_E_ANTAL; i++) {
            staand[i] = (float)((s - nytaar) / 36);
        }
        if (zs_energi_tik(&basis, staand, e_har, "SN1", lt.tm_hour,
                          zs_tid_dagnr(&lt), e_ud, e_ud_har,
                          &ud_time) == ZS_ENERGI_HUL) {
            aars_huller++;
            hul_maaned = lt.tm_mon + 1;
        }
    }
    CHECK_INT("et helt år giver præcis ét hul", aars_huller, 1);
    CHECK_INT("og det ligger i marts, ved forårsskiftet", hul_maaned, 3);
}
