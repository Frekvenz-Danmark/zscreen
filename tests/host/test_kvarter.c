#include "zs_test.h"
#include "../../firmware/kerne/net/zs_kvarter.h"

void test_kvarter(void)
{
    ZS_SUITE("Kvartersgennemsnit: nummeret og bøtten");

    CHECK("midnat er kvarter 0", zs_kvarter_nummer(0, 0) == 0);
    CHECK("00:14 er stadig kvarter 0", zs_kvarter_nummer(0, 14) == 0);
    CHECK("00:15 er kvarter 1", zs_kvarter_nummer(0, 15) == 1);
    CHECK("01:00 er kvarter 4", zs_kvarter_nummer(1, 0) == 4);
    CHECK("23:45 er kvarter 95", zs_kvarter_nummer(23, 45) == 95);
    CHECK("23:59 er stadig 95", zs_kvarter_nummer(23, 59) == 95);
    CHECK("der er 96 i et døgn", zs_kvarter_nummer(23, 59) + 1 == ZS_KVARTER_PR_DOEGN);

    /* Alle 1440 minutter i et døgn skal give et kvarter mellem 0 og 95, og
     * hvert kvarter skal ramme præcis 15 minutter. Regnet igennem i stedet
     * for stikprøver. */
    int taelling[ZS_KVARTER_PR_DOEGN];
    for (int i = 0; i < ZS_KVARTER_PR_DOEGN; i++) { taelling[i] = 0; }
    bool alle_i_omraadet = true;
    for (int t = 0; t < 24; t++) {
        for (int m = 0; m < 60; m++) {
            int k = zs_kvarter_nummer(t, m);
            if (k < 0 || k >= ZS_KVARTER_PR_DOEGN) { alle_i_omraadet = false; break; }
            taelling[k]++;
        }
    }
    CHECK("alle 1440 minutter giver et kvarter i området", alle_i_omraadet);
    bool femten_hver = true;
    for (int i = 0; i < ZS_KVARTER_PR_DOEGN; i++) {
        if (taelling[i] != 15) { femten_hver = false; break; }
    }
    CHECK("og hvert kvarter dækker præcis 15 minutter", femten_hver);

    CHECK("umulig time afvises", zs_kvarter_nummer(24, 0) == -1);
    CHECK("negativ time afvises", zs_kvarter_nummer(-1, 0) == -1);
    CHECK("umuligt minut afvises", zs_kvarter_nummer(0, 60) == -1);
    CHECK("negativt minut afvises", zs_kvarter_nummer(0, -1) == -1);

    /* Bøtten skal tælle opad hen over et døgnskifte, ellers virker reglen
     * om "lige før" ikke ved midnat. */
    CHECK("bøtten tæller opad over midnat",
          zs_kvarter_boette(10, 0) == zs_kvarter_boette(9, 95) + 1);

    ZS_SUITE("Kvartersgennemsnit: at samle og sende");

    zs_kvarter_t s;
    memset(&s, 0, sizeof(s));
    const bool alle[ZS_KV_ANTAL] = { true, true, true, true, true };
    float v[ZS_KV_ANTAL];

    /* Første gang er der intet at sende. Vi må IKKE sende et gennemsnit af
     * det kvarter vi tilfældigvis lander midt i: starter skærmen 14
     * minutter inde, dækker det ene minut ikke kvarteret. */
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 100.0f; }
    zs_kvarter_ud_t u = zs_kvarter_tik(&s, v, alle, 1000);
    CHECK("første gang sendes der ingenting", u.klar == false);

    /* Tre tal i samme kvarter: stadig ingenting, men de bliver samlet. */
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 200.0f; }
    u = zs_kvarter_tik(&s, v, alle, 1000);
    CHECK("andet tal i samme kvarter: stadig ingenting", u.klar == false);
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 300.0f; }
    u = zs_kvarter_tik(&s, v, alle, 1000);
    CHECK("tredje tal: stadig ingenting", u.klar == false);

    /* Og så skifter kvarteret. 100, 200, 300 giver 200. */
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 999.0f; }
    u = zs_kvarter_tik(&s, v, alle, 1001);
    CHECK("ved kvarterskift sendes der", u.klar == true);
    CHECK("og det er gennemsnittet, ikke det sidste tal",
          u.vaerdi[ZS_KV_SOL] > 199.9f && u.vaerdi[ZS_KV_SOL] < 200.1f);
    CHECK("alle fem felter er med",
          u.har[ZS_KV_SOL] && u.har[ZS_KV_FORBRUG] && u.har[ZS_KV_BATTERI] &&
          u.har[ZS_KV_NET] && u.har[ZS_KV_SOC]);
    CHECK("og der står hvilken bøtte det dækker", u.boette == 1000);

    /* Det næste kvarter må IKKE have det gamle med. 999 alene giver 999. */
    u = zs_kvarter_tik(&s, v, alle, 1002);
    CHECK("næste kvarter starter forfra", u.klar == true);
    CHECK("og det gamle er ikke blandet ind",
          u.vaerdi[ZS_KV_SOL] > 998.9f && u.vaerdi[ZS_KV_SOL] < 999.1f);

    ZS_SUITE("Kvartersgennemsnit: huller og felter der mangler");

    /*
     * Et HUL i bøtterne. Det sker hvis skærmen har været slukket, hvis uret
     * er rettet, eller ved årsskiftet hvor dagnummeret går tilbage til nul.
     *
     * Så smider vi det væk. Et gennemsnit fra et kvarter vi ikke kan
     * placere rigtigt, ville ligge på tidslinjen som om det var nyt, og så
     * regner en prognose på et tal fra i går.
     */
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 500.0f; }
    (void)zs_kvarter_tik(&s, v, alle, 2000);
    u = zs_kvarter_tik(&s, v, alle, 2005);
    CHECK("et hul på fem kvarterer: der sendes INTET", u.klar == false);
    /* Men den er begyndt forfra, så det næste virker. */
    u = zs_kvarter_tik(&s, v, alle, 2006);
    CHECK("og bagefter virker den igen", u.klar == true);

    /* Uret stillet TILBAGE. Samme regel. */
    memset(&s, 0, sizeof(s));
    (void)zs_kvarter_tik(&s, v, alle, 3000);
    u = zs_kvarter_tik(&s, v, alle, 2999);
    CHECK("uret stillet tilbage: der sendes INTET", u.klar == false);

    /* Et anlæg uden batteri og uden elmåler. De felter må IKKE komme med
     * som nul: et nul for "købt fra nettet" ser ud som et anlæg der aldrig
     * køber strøm, og det er en løgn. */
    memset(&s, 0, sizeof(s));
    const bool kun_sol[ZS_KV_ANTAL] = { true, false, false, false, false };
    for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = 700.0f; }
    (void)zs_kvarter_tik(&s, v, kun_sol, 4000);
    u = zs_kvarter_tik(&s, v, kun_sol, 4001);
    CHECK("solen alene: der sendes", u.klar == true);
    CHECK("og solen er med", u.har[ZS_KV_SOL] == true);
    CHECK("men batteriet er IKKE med", u.har[ZS_KV_BATTERI] == false);
    CHECK("og forbruget er IKKE med", u.har[ZS_KV_FORBRUG] == false);
    CHECK("de felter der ikke er med, står på nul",
          u.vaerdi[ZS_KV_BATTERI] == 0.0f);

    /* Og hvis INTET felt kom ind i et helt kvarter, er der ingenting at
     * sende. En besked med nul felter i er bare støj. */
    memset(&s, 0, sizeof(s));
    const bool ingen[ZS_KV_ANTAL] = { false, false, false, false, false };
    (void)zs_kvarter_tik(&s, v, ingen, 5000);
    u = zs_kvarter_tik(&s, v, ingen, 5001);
    CHECK("et helt kvarter uden tal: der sendes intet", u.klar == false);

    /* NULL må ikke gå ned. */
    u = zs_kvarter_tik(NULL, v, alle, 6000);
    CHECK("NULL-tilstand sender ikke", u.klar == false);
    u = zs_kvarter_tik(&s, NULL, alle, 6000);
    CHECK("NULL-værdier sender ikke", u.klar == false);
    u = zs_kvarter_tik(&s, v, NULL, 6000);
    CHECK("NULL-har sender ikke", u.klar == false);

    ZS_SUITE("Kvartersgennemsnit: et helt døgn igennem");

    /*
     * 96 kvarterer med 450 aflæsninger i hvert, altså et helt døgn i
     * firmwarens egen takt. Der skal komme PRÆCIS 96 gennemsnit ud, ikke
     * 95 og ikke 97.
     */
    memset(&s, 0, sizeof(s));
    int sendt = 0;
    bool alle_rigtige = true;
    for (int k = 0; k < 97; k++) {
        int32_t b = zs_kvarter_boette(200, 0) + k;
        for (int i = 0; i < ZS_KV_ANTAL; i++) { v[i] = (float)(k * 10); }
        for (int n = 0; n < 450; n++) {
            u = zs_kvarter_tik(&s, v, alle, b);
            if (u.klar) {
                sendt++;
                /* Gennemsnittet skal være det FORRIGE kvarters tal. */
                float vil = (float)((k - 1) * 10);
                if (u.vaerdi[ZS_KV_SOL] < vil - 0.1f ||
                    u.vaerdi[ZS_KV_SOL] > vil + 0.1f) { alle_rigtige = false; }
            }
        }
    }
    CHECK("97 kvarterer giver 96 gennemsnit", sendt == 96);
    CHECK("og hvert gennemsnit er det forrige kvarters tal", alle_rigtige);
}
