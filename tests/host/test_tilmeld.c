#include "zs_test.h"
#include "../../firmware/main/net/zs_tilmeld.h"

/* Levetiden i millisekunder. Ét sted, saa proeverne ikke gentager
 * regnestykket og kan komme ud af trit med headeren. */
static int64_t levetid_min_ms(void)
{
    return (int64_t)ZS_TILMELD_LEVETID_MIN * 60 * 1000;
}

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

    ZS_SUITE("Tilmelding: vejen til serveren, alle kombinationer");

    /*
     * DET HER ER FEJLEN FRA 11. OKTOBER 2026, HOLDT FAST.
     *
     * Skærmen stod på wifi med IP og sagde "Skærmen er ikke på nettet
     * endnu", fordi der kun var ét bool at sige det med. Her køres
     * ALLE kombinationer af de to tilstande igennem, så den
     * oversættelse ikke kan skride igen uden at nogen ser det.
     */
    CHECK("uden net er svaret intet net, uanset hvor flåden er",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTING, ZS_FLEET_READY)
              == ZS_TILMELD_VEJ_INTET_NET);
    CHECK("på nettet og indmeldt: hele vejen frem",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_READY)
              == ZS_TILMELD_VEJ_FLAADE);
    CHECK("på nettet, på vej op: vi venter",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_CONNECTING)
              == ZS_TILMELD_VEJ_VENTER);
    CHECK("på nettet, midt i indmeldelsen: vi venter",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_ENROLLING)
              == ZS_TILMELD_VEJ_VENTER);
    /*
     * ERROR er "på vej", ikke "gik i stå": modulet prøver igen af sig
     * selv. Målt på jern: første forsøg faldt kl. 17,5 sekunder, nummer
     * to kom ind fjorten sekunder senere uden at nogen rørte skærmen.
     * Sagde vi "ring til Frekvenz" her, ville vi sende montøren til
     * telefonen fjorten sekunder før koden kom af sig selv.
     */
    CHECK("på nettet, en netværksfejl undervejs: vi venter stadig",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_ERROR)
              == ZS_TILMELD_VEJ_VENTER);
    /* De to der IKKE går væk af sig selv. En montør der venter på dem,
     * venter for evigt. */
    CHECK("afvist certifikat: serveren siger nej",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_REJECTED)
              == ZS_TILMELD_VEJ_NEJ);
    CHECK("intet certifikat på enheden: serveren siger nej",
          zs_tilmeld_vejen(ZS_WIFI_CONNECTED, ZS_FLEET_OFF)
              == ZS_TILMELD_VEJ_NEJ);

    /*
     * Og så hele tabellen. Fem netværkstilstande gange seks
     * flådetilstande, plus tal uden for begge enums: ingen kombination
     * må give FLAADE uden at begge faktisk er fremme, for FLAADE er det
     * ene svar der laver en kode.
     */
    const zs_wifi_state_t net_alle[] = {
        ZS_WIFI_OFF, ZS_WIFI_SCANNING, ZS_WIFI_CONNECTING,
        ZS_WIFI_CONNECTED, ZS_WIFI_FAILED, (zs_wifi_state_t)42,
    };
    const zs_fleet_state_t fl_alle[] = {
        ZS_FLEET_OFF, ZS_FLEET_CONNECTING, ZS_FLEET_ENROLLING,
        ZS_FLEET_READY, ZS_FLEET_REJECTED, ZS_FLEET_ERROR,
        (zs_fleet_state_t)42,
    };
    int flaade_ja = 0, uden_for = 0, ingen_net = 0;
    for (size_t i = 0; i < sizeof(net_alle) / sizeof(net_alle[0]); i++) {
        for (size_t j = 0; j < sizeof(fl_alle) / sizeof(fl_alle[0]); j++) {
            zs_tilmeld_vej_t v = zs_tilmeld_vejen(net_alle[i], fl_alle[j]);
            if (v != ZS_TILMELD_VEJ_INTET_NET && v != ZS_TILMELD_VEJ_VENTER
                && v != ZS_TILMELD_VEJ_NEJ && v != ZS_TILMELD_VEJ_FLAADE) {
                uden_for++;
            }
            if (v == ZS_TILMELD_VEJ_FLAADE) {
                flaade_ja++;
                /* Det ene svar der laver en kode. Det må kun komme når
                 * BEGGE er fremme. */
                if (net_alle[i] != ZS_WIFI_CONNECTED
                    || fl_alle[j] != ZS_FLEET_READY) {
                    flaade_ja += 1000;
                }
            }
            if (v == ZS_TILMELD_VEJ_INTET_NET) {
                ingen_net++;
                if (net_alle[i] == ZS_WIFI_CONNECTED) {
                    ingen_net += 1000;   /* "intet net" om et net der er */
                }
            }
        }
    }
    CHECK_INT("hver kombination giver en vej vi kender", uden_for, 0);
    CHECK_INT("kun ÉN af 42 kombinationer laver en kode", flaade_ja, 1);
    /*
     * De syv flådetilstande - seks plus tallet uden for enummet - gange
     * de fem netværkstilstande der ikke er CONNECTED. 7 * 5 = 35, og
     * ingen af dem må sige "intet net" om et net der ER der. DET var
     * fejlen.
     */
    CHECK_INT("og \"intet net\" siges kun når der ikke er et net",
              ingen_net, 35);

    ZS_SUITE("Tilmelding: hvornår der er en kode at vise");

    zs_tilmeld_t s;
    memset(&s, 0, sizeof(s));
    fire(t, 0x12345678u);

    /* Uden inverter er der INGEN kode, og det er en beslutning, ikke en
     * fejl: et anlæg uden inverter er ikke et anlæg. */
    CHECK("uden inverter: mangler inverter",
          zs_tilmeld_stand(&s, false, ZS_TILMELD_VEJ_FLAADE, 0) == ZS_TILMELD_MANGLER_INVERTER);
    CHECK("og der laves ingen kode",
          zs_tilmeld_bed_om(&s, false, ZS_TILMELD_VEJ_FLAADE, 0, t, sizeof(t)) == false);
    CHECK("så der er heller ingen at sende",
          strcmp(zs_tilmeld_kode_nu(&s, false, ZS_TILMELD_VEJ_FLAADE, 0), "") == 0);

    /*
     * UDEN FLÅDEN ER DER TRE FORSKELLIGE GRUNDE, OG DE SKAL SIGES HVER
     * FOR SIG.
     *
     * Her stod der før ét bool, og så var der kun én besked: "Skærmen er
     * ikke på nettet endnu". Den blev vist på en skærm der var på nettet
     * med IP, målt på jern 11. oktober 2026, fordi indmeldelsen spredes
     * over op til et minut. Montøren blev sendt ud at tjekke et netværk
     * der virkede fint.
     */
    CHECK("intet net: sig at der ikke er noget net",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_INTET_NET, 0)
              == ZS_TILMELD_MANGLER_NET);
    CHECK("på nettet, på vej ind: sig at vi venter, ikke at nettet mangler",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_VENTER, 0)
              == ZS_TILMELD_VENTER_PAA_SERVER);
    CHECK("serveren siger nej: sig DET, så ingen venter forgæves",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_NEJ, 0)
              == ZS_TILMELD_SERVER_NEJ);

    /* Og ingen af de tre må lave en kode: serveren ville ikke kende den,
     * og montøren ville prøve den forgæves. */
    CHECK("intet net: ingen kode",
          zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_INTET_NET, 0,
                            t, sizeof(t)) == false);
    CHECK("på vej ind: ingen kode endnu",
          zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_VENTER, 0,
                            t, sizeof(t)) == false);
    CHECK("serveren siger nej: ingen kode",
          zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_NEJ, 0,
                            t, sizeof(t)) == false);
    CHECK("og ingen af dem har lagt en kode i strukturen",
          s.har_kode == false && s.kode[0] == '\0');

    /*
     * EN VEJ VI IKKE KENDER MÅ ALDRIG GIVE EN KODE.
     *
     * Et tal uden for enummet kan komme af en fejl i kalderen. Svaret
     * skal være det sikre: ingen kode, og sig at serveren ikke vil.
     * Lavede den en kode, ville en fejl ét sted blive en kode montøren
     * står med og som serveren ikke kender.
     */
    const zs_tilmeld_vej_t ukendt = (zs_tilmeld_vej_t)99;
    CHECK("ukendt vej: ingen kode",
          zs_tilmeld_bed_om(&s, true, ukendt, 0, t, sizeof(t)) == false);
    CHECK("ukendt vej: og den lover ingenting",
          zs_tilmeld_stand(&s, true, ukendt, 0) == ZS_TILMELD_SERVER_NEJ);

    /* Inverteren vejer tungest: mangler den, er det DEN der skal stå,
     * uanset hvor langt vi er nået mod serveren. */
    CHECK("uden inverter OG uden net: inverteren står først",
          zs_tilmeld_stand(&s, false, ZS_TILMELD_VEJ_INTET_NET, 0)
              == ZS_TILMELD_MANGLER_INVERTER);

    /* Med begge: der er stadig ingen før nogen beder om en. */
    CHECK("med begge, men uden at bede: ingen kode endnu",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_FLAADE, 0) == ZS_TILMELD_UDLOEBET);

    CHECK("der bedes om en, og den kommer",
          zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000, t, sizeof(t)) == true);
    CHECK("nu er den klar",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000) == ZS_TILMELD_KLAR);
    CHECK_STR("og den kan sendes",
              zs_tilmeld_kode_nu(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000), s.kode);

    /* Koden må IKKE skifte mens den er gyldig: montøren står og skriver
     * den af, og skærmen tegner sig selv om mange gange undervejs. */
    char foerste[ZS_TILMELD_KODE_LEN + 1];
    snprintf(foerste, sizeof(foerste), "%s", s.kode);
    uint8_t t2[ZS_TILMELD_TILF_BYTES];
    fire(t2, 0x87654321u);
    (void)zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_FLAADE, 2000, t2, sizeof(t2));
    CHECK_STR("den samme kode bliver stående mens den er gyldig",
              s.kode, foerste);

    ZS_SUITE("Tilmelding: udløb, engangsbrug og oprydning");

    const int64_t levetid = (int64_t)ZS_TILMELD_LEVETID_MIN * 60 * 1000;

    CHECK("et millisekund før udløb er den stadig klar",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000 + levetid - 1)
              == ZS_TILMELD_KLAR);
    CHECK("præcis ved udløb er den udløbet",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000 + levetid)
              == ZS_TILMELD_UDLOEBET);
    CHECK("og så er der ingenting at sende",
          strcmp(zs_tilmeld_kode_nu(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000 + levetid), "") == 0);

    /* Efter udløb laves der en NY, ikke den gamle igen. */
    CHECK("efter udløb kommer der en ny",
          zs_tilmeld_bed_om(&s, true, ZS_TILMELD_VEJ_FLAADE, 1000 + levetid, t2, sizeof(t2)));
    CHECK("og den er en anden end den gamle", strcmp(s.kode, foerste) != 0);

    /* At rydde er regel 2: holder skærmen op med at vise den, holder den
     * op med at virke. */
    zs_tilmeld_ryd(&s);
    CHECK("ryddet: der er ingen kode",
          zs_tilmeld_stand(&s, true, ZS_TILMELD_VEJ_FLAADE, 5000) == ZS_TILMELD_UDLOEBET);
    CHECK("og bufferen er tom, så koden ikke ligger i RAM",
          s.kode[0] == '\0');

    ZS_SUITE("Tilmelding: hvor længe koden gælder endnu");

    zs_tilmeld_t m;
    memset(&m, 0, sizeof(m));
    CHECK_INT("uden kode er der nul minutter igen", zs_tilmeld_min_igen(&m, 0), 0);
    CHECK_INT("NULL giver nul", zs_tilmeld_min_igen(NULL, 0), 0);

    (void)zs_tilmeld_bed_om(&m, true, ZS_TILMELD_VEJ_FLAADE, 0, t, sizeof(t));
    CHECK_INT("en ny kode gælder hele levetiden",
              zs_tilmeld_min_igen(&m, 0), ZS_TILMELD_LEVETID_MIN);
    /*
     * Rundet OP. Står der "0 minutter" på en kode der stadig virker,
     * tror montøren den er død og laver en ny uden grund.
     */
    CHECK_INT("ét millisekund igen er stadig ét minut",
              zs_tilmeld_min_igen(&m, levetid_min_ms() - 1), 1);
    CHECK_INT("61 sekunder igen er to minutter",
              zs_tilmeld_min_igen(&m, levetid_min_ms() - 61000), 2);
    CHECK_INT("præcis 60 sekunder igen er ét minut",
              zs_tilmeld_min_igen(&m, levetid_min_ms() - 60000), 1);
    CHECK_INT("ved udløb er der nul igen",
              zs_tilmeld_min_igen(&m, levetid_min_ms()), 0);
    CHECK_INT("og efter udløb også",
              zs_tilmeld_min_igen(&m, levetid_min_ms() + 99999), 0);

    ZS_SUITE("Tilmelding: hvornår koden sendes til serveren");

    zs_tilmeld_sendt_t sendt;
    memset(&sendt, 0, sizeof(sendt));

    CHECK("første gang sendes der, også selv om der ikke er nogen kode",
          zs_tilmeld_skal_sende(&sendt, "") == true);
    CHECK("og så tier den om det samme",
          zs_tilmeld_skal_sende(&sendt, "") == false);
    CHECK("en ny kode sendes", zs_tilmeld_skal_sende(&sendt, "ABC123") == true);
    CHECK("og den tier om den samme kode igen",
          zs_tilmeld_skal_sende(&sendt, "ABC123") == false);
    bool tav = true;
    for (int i = 0; i < 100; i++) {
        if (zs_tilmeld_skal_sende(&sendt, "ABC123")) { tav = false; }
    }
    CHECK("hundrede uændrede runder giver ikke én afsendelse mere", tav);
    CHECK("en anden kode sendes", zs_tilmeld_skal_sende(&sendt, "ZZZ999") == true);

    /*
     * At koden bliver TOM skal også sendes. Ellers bliver den ved med at
     * virke på serveren efter skærmen er holdt op med at vise den, og det
     * er regel 2 der falder.
     */
    CHECK("at koden er ryddet, sendes også",
          zs_tilmeld_skal_sende(&sendt, "") == true);
    CHECK("men kun én gang", zs_tilmeld_skal_sende(&sendt, "") == false);

    /*
     * Efter en NY indmeldelse står feltet tomt på serveren, mens vi tror
     * vi har sagt det. Så skal det siges forfra - ellers ser montøren en
     * kode skærmen viser, som serveren ikke kender.
     */
    (void)zs_tilmeld_skal_sende(&sendt, "KODE42");
    CHECK("og den tier bagefter", zs_tilmeld_skal_sende(&sendt, "KODE42") == false);
    zs_tilmeld_sendt_nulstil(&sendt);
    CHECK("efter en ny indmeldelse siges det forfra",
          zs_tilmeld_skal_sende(&sendt, "KODE42") == true);

    /*
     * GRAENSEN, lige på tegnet. Bufferen er seks tegn og en nulbyte.
     *
     * Mutationstesten fandt den: ">= sizeof" byttet til "> sizeof"
     * overlevede, fordi ingen prøve ramte PRÆCIS syv tegn. Og med den
     * mutant ville syv tegn slippe igennem og kopiere otte bytes ind i en
     * buffer på syv. Det er ikke en afrunding, det er en overskrivning.
     */
    zs_tilmeld_sendt_nulstil(&sendt);
    CHECK("seks tegn er det længste der kan være, og det sendes",
          zs_tilmeld_skal_sende(&sendt, "ABCDEF") == true);
    CHECK("syv tegn afvises, og bufferen bliver ikke overskrevet",
          zs_tilmeld_skal_sende(&sendt, "ABCDEFG") == false);
    CHECK_STR("og den gamle kode står urørt", sendt.sidst, "ABCDEF");

    /* En kode der er for lang, er ikke en vi har lavet. */
    CHECK("en for lang kode sendes ikke",
          zs_tilmeld_skal_sende(&sendt, "FOR-LANG-KODE") == false);
    CHECK("NULL-kode sendes ikke", zs_tilmeld_skal_sende(&sendt, NULL) == false);
    CHECK("NULL-tilstand sender ikke",
          zs_tilmeld_skal_sende(NULL, "ABC123") == false);
    zs_tilmeld_sendt_nulstil(NULL);
    CHECK("og NULL i nulstil går ikke ned", true);

    /* NULL må ikke gå ned. */
    CHECK("NULL-tilstand giver mangler inverter",
          zs_tilmeld_stand(NULL, true, ZS_TILMELD_VEJ_FLAADE, 0) == ZS_TILMELD_MANGLER_INVERTER);
    CHECK("NULL kan ikke bede om en kode",
          zs_tilmeld_bed_om(NULL, true, ZS_TILMELD_VEJ_FLAADE, 0, t, sizeof(t)) == false);
    CHECK_STR("NULL giver en tom kode",
              zs_tilmeld_kode_nu(NULL, true, ZS_TILMELD_VEJ_FLAADE, 0), "");
    zs_tilmeld_ryd(NULL);
    CHECK("og NULL i ryd går ikke ned", true);
}
