#include "zs_test.h"
#include "../../firmware/main/net/zs_tilmeld.h"

/* Fire bytes som ét tal, saa proeverne kan skrives laeseligt. */
static void fire(uint8_t *ud, uint32_t v)
{
    ud[0] = (uint8_t)(v >> 24); ud[1] = (uint8_t)(v >> 16);
    ud[2] = (uint8_t)(v >> 8);  ud[3] = (uint8_t)v;
}

void test_tilmeld(void)
{
    ZS_SUITE("Tilmelding: koden ud af tilfældige bytes");

    char k[ZS_TILMELD_KODE_LEN + 1];
    uint8_t t[ZS_TILMELD_TILF_BYTES];

    fire(t, 0x00000000u);
    CHECK("nul giver det første tegn hele vejen",
          zs_tilmeld_kode(k, sizeof(k), t, sizeof(t)));
    CHECK_STR("og det er 000000", k, "000000");

    fire(t, 0xFFFFFFFFu);
    (void)zs_tilmeld_kode(k, sizeof(k), t, sizeof(t));
    CHECK_STR("alle bit sat giver det sidste tegn hele vejen", k, "ZZZZZZ");

    /* Koden er altid præcis seks tegn. */
    bool altid_seks = true;
    for (uint32_t i = 0; i < 5000; i++) {
        fire(t, i * 2654435761u);
        if (!zs_tilmeld_kode(k, sizeof(k), t, sizeof(t))
            || strlen(k) != ZS_TILMELD_KODE_LEN) { altid_seks = false; break; }
    }
    CHECK("5000 koder er alle præcis seks tegn", altid_seks);

    /*
     * ALLE tegn skal komme af alfabetet, og I, L, O og U må ALDRIG
     * optræde: de forveksles med 1, 1, 0 og V når et menneske læser
     * koden på skærmen og skriver den et andet sted.
     */
    const char *lovligt = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    bool kun_lovlige = true;
    bool set_tegn[32];
    for (int i = 0; i < 32; i++) { set_tegn[i] = false; }
    for (uint32_t i = 0; i < 20000; i++) {
        fire(t, i * 2654435761u + i);
        (void)zs_tilmeld_kode(k, sizeof(k), t, sizeof(t));
        for (int j = 0; j < ZS_TILMELD_KODE_LEN; j++) {
            const char *p = strchr(lovligt, k[j]);
            if (p == NULL) { kun_lovlige = false; break; }
            set_tegn[p - lovligt] = true;
        }
        if (!kun_lovlige) { break; }
    }
    CHECK("hvert tegn er fra alfabetet, og I, L, O og U er ikke med",
          kun_lovlige);
    bool alle_naas = true;
    for (int i = 0; i < 32; i++) { if (!set_tegn[i]) { alle_naas = false; } }
    CHECK("og alle 32 tegn kan faktisk komme ud", alle_naas);

    /*
     * De to nederste bit bruges IKKE. Seks tegn er 30 bit, og de sidste
     * to ville give et skævt tegn. To tal der kun er forskellige der,
     * skal give den SAMME kode.
     */
    char a[ZS_TILMELD_KODE_LEN + 1], b[ZS_TILMELD_KODE_LEN + 1];
    fire(t, 0xAAAAAAA8u); (void)zs_tilmeld_kode(a, sizeof(a), t, sizeof(t));
    fire(t, 0xAAAAAAABu); (void)zs_tilmeld_kode(b, sizeof(b), t, sizeof(t));
    CHECK_STR("de to nederste bit er ubrugte, som de skal være", a, b);

    /* Og de øvre bit betyder noget: et skift i toppen ændrer koden. */
    fire(t, 0x2AAAAAA8u); (void)zs_tilmeld_kode(b, sizeof(b), t, sizeof(t));
    CHECK("men de øvre bit ændrer koden", strcmp(a, b) != 0);

    CHECK("NULL ud afvises", zs_tilmeld_kode(NULL, 8, t, sizeof(t)) == false);
    CHECK("for lille buffer afvises",
          zs_tilmeld_kode(k, ZS_TILMELD_KODE_LEN, t, sizeof(t)) == false);
    CHECK("NULL tilfældighed afvises",
          zs_tilmeld_kode(k, sizeof(k), NULL, 4) == false);
    CHECK("for få tilfældige bytes afvises",
          zs_tilmeld_kode(k, sizeof(k), t, ZS_TILMELD_TILF_BYTES - 1) == false);

    ZS_SUITE("Tilmelding: hvornår der er en kode at vise");

    zs_tilmeld_t s;
    memset(&s, 0, sizeof(s));
    fire(t, 0x12345678u);

    /* Uden inverter er der INGEN kode, og det er en beslutning, ikke en
     * fejl: et anlæg uden inverter er ikke et anlæg. */
    CHECK("uden inverter: mangler inverter",
          zs_tilmeld_stand(&s, false, true, 0) == ZS_TILMELD_MANGLER_INVERTER);
    CHECK("og der laves ingen kode",
          zs_tilmeld_bed_om(&s, false, true, 0, t, sizeof(t)) == false);
    CHECK("så der er heller ingen at sende",
          strcmp(zs_tilmeld_kode_nu(&s, false, true, 0), "") == 0);

    /* Uden flåden ville serveren ikke kende koden, så den laves ikke. */
    CHECK("uden flåde: mangler flåde",
          zs_tilmeld_stand(&s, true, false, 0) == ZS_TILMELD_MANGLER_FLAADE);
    CHECK("og der laves ingen kode",
          zs_tilmeld_bed_om(&s, true, false, 0, t, sizeof(t)) == false);

    /* Med begge: der er stadig ingen før nogen beder om en. */
    CHECK("med begge, men uden at bede: ingen kode endnu",
          zs_tilmeld_stand(&s, true, true, 0) == ZS_TILMELD_UDLOEBET);

    CHECK("der bedes om en, og den kommer",
          zs_tilmeld_bed_om(&s, true, true, 1000, t, sizeof(t)) == true);
    CHECK("nu er den klar",
          zs_tilmeld_stand(&s, true, true, 1000) == ZS_TILMELD_KLAR);
    CHECK_STR("og den kan sendes",
              zs_tilmeld_kode_nu(&s, true, true, 1000), s.kode);

    /* Koden må IKKE skifte mens den er gyldig: montøren står og skriver
     * den af, og skærmen tegner sig selv om mange gange undervejs. */
    char foerste[ZS_TILMELD_KODE_LEN + 1];
    snprintf(foerste, sizeof(foerste), "%s", s.kode);
    uint8_t t2[ZS_TILMELD_TILF_BYTES];
    fire(t2, 0x87654321u);
    (void)zs_tilmeld_bed_om(&s, true, true, 2000, t2, sizeof(t2));
    CHECK_STR("den samme kode bliver stående mens den er gyldig",
              s.kode, foerste);

    ZS_SUITE("Tilmelding: udløb, engangsbrug og oprydning");

    const int64_t levetid = (int64_t)ZS_TILMELD_LEVETID_MIN * 60 * 1000;

    CHECK("et millisekund før udløb er den stadig klar",
          zs_tilmeld_stand(&s, true, true, 1000 + levetid - 1)
              == ZS_TILMELD_KLAR);
    CHECK("præcis ved udløb er den udløbet",
          zs_tilmeld_stand(&s, true, true, 1000 + levetid)
              == ZS_TILMELD_UDLOEBET);
    CHECK("og så er der ingenting at sende",
          strcmp(zs_tilmeld_kode_nu(&s, true, true, 1000 + levetid), "") == 0);

    /* Efter udløb laves der en NY, ikke den gamle igen. */
    CHECK("efter udløb kommer der en ny",
          zs_tilmeld_bed_om(&s, true, true, 1000 + levetid, t2, sizeof(t2)));
    CHECK("og den er en anden end den gamle", strcmp(s.kode, foerste) != 0);

    /* At rydde er regel 2: holder skærmen op med at vise den, holder den
     * op med at virke. */
    zs_tilmeld_ryd(&s);
    CHECK("ryddet: der er ingen kode",
          zs_tilmeld_stand(&s, true, true, 5000) == ZS_TILMELD_UDLOEBET);
    CHECK("og bufferen er tom, så koden ikke ligger i RAM",
          s.kode[0] == '\0');

    /* Tilmeldt: opbrugt for altid. */
    (void)zs_tilmeld_bed_om(&s, true, true, 6000, t, sizeof(t));
    zs_tilmeld_tilmeldt(&s);
    CHECK("tilmeldt: der er ikke mere at vise",
          zs_tilmeld_stand(&s, true, true, 6000) == ZS_TILMELD_TILMELDT);
    CHECK("og koden er ryddet", s.kode[0] == '\0');
    CHECK("der kan ikke bedes om en ny",
          zs_tilmeld_bed_om(&s, true, true, 7000, t, sizeof(t)) == false);
    /*
     * Og det gælder OGSÅ hvis inverteren forsvinder bagefter. En skærm
     * der mister inverteren i fem minutter, må ikke begynde at tilbyde
     * en ny tilmelding af en kunde der allerede findes.
     */
    CHECK("tilmeldt vejer tungere end en forsvunden inverter",
          zs_tilmeld_stand(&s, false, false, 8000) == ZS_TILMELD_TILMELDT);

    /* NULL må ikke gå ned. */
    CHECK("NULL-tilstand giver mangler inverter",
          zs_tilmeld_stand(NULL, true, true, 0) == ZS_TILMELD_MANGLER_INVERTER);
    CHECK("NULL kan ikke bede om en kode",
          zs_tilmeld_bed_om(NULL, true, true, 0, t, sizeof(t)) == false);
    CHECK_STR("NULL giver en tom kode",
              zs_tilmeld_kode_nu(NULL, true, true, 0), "");
    zs_tilmeld_ryd(NULL);
    zs_tilmeld_tilmeldt(NULL);
    CHECK("og NULL i ryd og tilmeldt går ikke ned", true);
}
