/*
 * Test af versionssammenligning.
 *
 * Den her funktion afgoer om en skaerm paa en vaeg henter ny firmware.
 * To fejl er mulige, og begge er slemme:
 *
 *   for streng   skaermen opdaterer aldrig, og en fejlrettelse naar
 *                aldrig ud til kunden
 *   for slap     skaermen henter den samme udgave igen og igen, eller
 *                ruller tilbage til noget aeldre
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_version.h"

void test_version(void)
{
    ZS_SUITE("Versionsnumre: kan de laeses");

    {
        unsigned v[3];
        struct { const char *s; bool ok; unsigned a, b, c; const char *hvad; } t[] = {
            { "1.2.3",     true,  1, 2, 3,  "almindelig" },
            { "v1.2.3",    true,  1, 2, 3,  "med v foran" },
            { "V1.2.3",    true,  1, 2, 3,  "med stort V" },
            { "0.0.0",     true,  0, 0, 0,  "nuller" },
            { "0.10.0",    true,  0, 10, 0, "to cifre i midten" },
            { "10.20.30",  true,  10, 20, 30, "to cifre alle steder" },

            /* De her SKAL afvises. Teksten kommer fra et maerke paa
             * GitHub, altsaa udefra. */
            { "1.2",       false, 0, 0, 0,  "kun to led" },
            { "1.2.3.4",   false, 0, 0, 0,  "fire led" },
            { "1.2.3-rc1", false, 0, 0, 0,  "forhaandsudgave" },
            { "1.2.x",     false, 0, 0, 0,  "bogstav i et led" },
            { " 1.2.3",    false, 0, 0, 0,  "mellemrum foran" },
            { "1.2.3 ",    false, 0, 0, 0,  "mellemrum bagved" },
            { "-1.0.0",    false, 0, 0, 0,  "minus foran" },
            { "+1.0.0",    false, 0, 0, 0,  "plus foran" },
            { "1..3",      false, 0, 0, 0,  "tomt led" },
            { "..",        false, 0, 0, 0,  "kun punktummer" },
            { "",          false, 0, 0, 0,  "tom tekst" },
            { "v",         false, 0, 0, 0,  "kun et v" },
            { "999999.0.0",false, 0, 0, 0,  "urimeligt stort tal" },
        };
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            bool ok = zs_version_parse(t[i].s, v);
            CHECK(t[i].hvad, ok == t[i].ok);
            if (t[i].ok && ok) {
                CHECK(t[i].hvad,
                      v[0] == t[i].a && v[1] == t[i].b && v[2] == t[i].c);
            }
        }
        CHECK("NULL er ikke en version", !zs_version_parse(NULL, v));
    }

    ZS_SUITE("Versionsnumre: hvad er nyest");

    {
        struct { const char *a, *b; int vent; const char *hvad; } t[] = {
            { "0.2.0", "0.1.0",  1, "hoejere rettelsestal er nyere" },
            { "0.1.0", "0.2.0", -1, "og omvendt" },
            { "0.1.0", "0.1.0",  0, "ens er ens" },
            { "v0.2.0", "0.2.0", 0, "v foran aendrer ingenting" },

            /*
             * Den vigtigste i hele filen.
             *
             * 0.10.0 er nyere end 0.9.0, men staar FOER den
             * alfabetisk. Sammenlignede vi teksterne, ville hver eneste
             * skaerm i marken staa paa 0.9.0 for altid, og ingen ville
             * opdage det foer nogen kiggede paa en af dem.
             */
            { "0.10.0", "0.9.0",  1, "0.10.0 er nyere end 0.9.0" },
            { "0.9.0",  "0.10.0", -1, "og 0.9.0 er aeldre end 0.10.0" },
            { "1.0.0",  "0.99.99", 1, "stoerste led vejer tungest" },
            { "0.1.10", "0.1.9",   1, "ogsaa i sidste led" },
            { "0.10.0", "0.10.0", 0, "lige store giver nul" },
            { "v0.10.0", "0.9.0", 1, "et v foran aendrer ingenting" },

            /* Kan én af dem ikke laeses, opdaterer vi ikke. */
            { "hvadsomhelst", "0.1.0", -1, "ulaeselig ny version" },
            { "0.2.0", "hvadsomhelst", -1, "ulaeselig koerende version" },
            { "-1.0.0", "0.1.0",  -1, "minus regnes ikke som kaempestort" },
        };
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            CHECK(t[i].hvad, zs_version_cmp(t[i].a, t[i].b) == t[i].vent);
        }
        CHECK("NULL opdaterer ikke", zs_version_cmp(NULL, "0.1.0") == -1);
        CHECK("NULL begge veje",     zs_version_cmp("0.1.0", NULL) == -1);
    }

    ZS_SUITE("Versionsnumre: v foran");

    {
        CHECK_STR("v fjernes", zs_version_strip_v("v1.2.3"), "1.2.3");
        CHECK_STR("stort V ogsaa", zs_version_strip_v("V1.2.3"), "1.2.3");
        CHECK_STR("uden v roeres den ikke", zs_version_strip_v("1.2.3"), "1.2.3");
    }
}

/*
 * Maalversionen kommer fra SERVEREN og ender inde i en URL.
 *
 * Det gOEr den til det eneste sted hvor en tekst udefra bliver til en
 * adresse skaermen henter firmware fra. Slipper en skraastreg eller et
 * punktum-punktum igennem, kan den peges et andet sted hen. Underskriften
 * ville stadig redde os, for en fremmed firmware bliver afvist, men en
 * skaerm der henter fra et forkert sted igen og igen er ogsaa en fejl.
 *
 * Derfor er den streng: noejagtig tal.tal.tal, med et valgfrit v foran.
 */
void test_version_tag(void)
{
    ZS_SUITE("Målversionen fra serveren");

    CHECK("almindelig version", zs_version_tag_ok("0.9.0") == true);
    CHECK("med v foran, som mærket på GitHub", zs_version_tag_ok("v0.9.0") == true);
    CHECK("stort V virker også", zs_version_tag_ok("V1.2.3") == true);
    CHECK("flercifret", zs_version_tag_ok("10.20.30") == true);
    CHECK("nuller", zs_version_tag_ok("0.0.0") == true);

    CHECK("tom afvises", zs_version_tag_ok("") == false);
    CHECK("NULL afvises", zs_version_tag_ok(NULL) == false);
    CHECK("kun v afvises", zs_version_tag_ok("v") == false);
    CHECK("to tal er ikke nok", zs_version_tag_ok("1.2") == false);
    CHECK("fire tal er for mange", zs_version_tag_ok("1.2.3.4") == false);
    CHECK("bogstaver afvises", zs_version_tag_ok("1.2.3a") == false);
    CHECK("mellemrum afvises", zs_version_tag_ok("1.2.3 ") == false);

    /* Det der kunne pille ved adressen. */
    CHECK("skråstreg afvises", zs_version_tag_ok("1.2.3/evil") == false);
    CHECK("punktum-punktum afvises", zs_version_tag_ok("../1.2.3") == false);
    CHECK("to punktummer i træk afvises", zs_version_tag_ok("1..3") == false);
    CHECK("punktum forrest afvises", zs_version_tag_ok(".1.2") == false);
    CHECK("punktum bagerst afvises", zs_version_tag_ok("1.2.") == false);
    CHECK("spørgsmålstegn afvises", zs_version_tag_ok("1.2.3?a=b") == false);
    CHECK("kolon afvises", zs_version_tag_ok("1.2.3:80") == false);
    CHECK("procent afvises", zs_version_tag_ok("1.2.%2e") == false);

    /* En uendelig lang streng maa ikke kunne fylde feltet op. */
    char lang[200];
    memset(lang, '1', sizeof(lang) - 1);
    lang[sizeof(lang) - 1] = '\0';
    CHECK("alt for lang afvises", zs_version_tag_ok(lang) == false);
    CHECK("for mange cifre i ét tal afvises", zs_version_tag_ok("1234567.1.1") == false);

    /*
     * De angreb teksten kunne baere, hvis nogen fik fat i dashboardet.
     *
     * Maalversionen kommer fra serveren og ender i en URL, saa den her
     * funktion ER graensen. Vognretur og linjeskift staar foerst, fordi
     * det er den der kunne lave to HTTP-headere ud af én linje.
     */
    CHECK("vognretur og linjeskift afvises",
          zs_version_tag_ok("1.2.3\r\nHost: evil") == false);
    CHECK("linjeskift alene afvises", zs_version_tag_ok("1.2.3\nX") == false);
    CHECK("bagstreg afvises", zs_version_tag_ok("1.2.3\\evil") == false);
    CHECK("en hel URL afvises", zs_version_tag_ok("http://evil/x.bin") == false);
    CHECK("tabulator afvises", zs_version_tag_ok("1.2.3\t") == false);
    CHECK("et tegn over 127 afvises", zs_version_tag_ok("1.2.3\xff") == false);
    CHECK("semikolon afvises", zs_version_tag_ok("1.2.3;rm -rf") == false);
    CHECK("havelaage afvises", zs_version_tag_ok("1.2.3#frag") == false);
    CHECK("minus foran afvises", zs_version_tag_ok("-1.2.3") == false);
    CHECK("plus foran afvises", zs_version_tag_ok("+1.2.3") == false);
    CHECK("to v foran afvises", zs_version_tag_ok("vv1.2.3") == false);

    /* Og graensen den anden vej: fem cifre i hvert tal er lovligt. */
    CHECK("fem cifre i hvert tal er lovligt",
          zs_version_tag_ok("99999.99999.99999") == true);
    CHECK("seks cifre er ikke", zs_version_tag_ok("999999.1.1") == false);

    /*
     * Den laengst mulige version, og at den kan GEMMES hel.
     *
     * Her laa foer en laengdevagt i koden, som ikke kunne proeves fordi
     * formatet aldrig kan give saa lang en streng. Nu staar sammenhaengen
     * som et tjek ved bygningen, og den proeves her: det laengste formatet
     * siger ja til, skal kunne staa i feltet UDEN at blive klippet.
     *
     * Bliver cifferloftet sat op uden at feltet foelger med, falder
     * bygningen. Bliver formatet laempet paa en anden maade, falder den
     * her.
     */
    char laengst[ZS_VERSION_MAX * 2];
    int skrevet = snprintf(laengst, sizeof(laengst), "%s", "99999.99999.99999");
    CHECK("den laengste version er praecis saa lang som regnet ud",
          skrevet == ZS_VERSION_LAENGST);
    CHECK("og formatet siger ja til den", zs_version_tag_ok(laengst) == true);

    zs_maal_t m = zs_version_vaelg_maal("", laengst);
    CHECK("den kan gemmes som maal", m.gyldig == true);
    CHECK("og den staar hel, ikke klippet", strcmp(m.maal, laengst) == 0);

    /* Med v foran er den ét tegn laengere ind, men v'et bliver skrællet
     * af, saa der skal staa det samme i feltet.
     *
     * Feltet er ét tegn stoerre end laengst, netop fordi v'et skal med.
     * Stod der samme maal som laengst, kunne gcc ikke se at strengen er
     * kort nok, og -Wformat-truncation faldt over det paa Linux mens
     * clang lod det ligge. Maalet siger nu hvad det skal rumme. */
    char med_v[sizeof(laengst) + 1];
    snprintf(med_v, sizeof(med_v), "v%s", laengst);
    m = zs_version_vaelg_maal("", med_v);
    CHECK("med v foran er den stadig gyldig", m.gyldig == true);
    CHECK("og der staar stadig det samme uden v", strcmp(m.maal, laengst) == 0);
}
/*
 * Teksten naar GitHub svarer 404.
 *
 * Der stod ÉN tekst: "Der er ingen udgivelser endnu". Den er rigtig naar vi
 * bad om den nyeste og der ikke er nogen. Men bad vi om en BESTEMT udgave,
 * fordi nogen skrev et maal i dashboardet, saa er 404 noget andet: der ER
 * udgivelser, den ene vi bad om findes bare ikke. Med den gamle tekst gaar
 * man og leder efter et problem med udgivelser i det hele taget i stedet
 * for at se paa det nummer man selv skrev.
 */
void test_version_404(void)
{
    ZS_SUITE("Udgivelser: teksten når GitHub svarer 404");

    char t[96];

    zs_version_tekst_404(NULL, t, sizeof(t));
    CHECK_STR("uden mål: der er ingen udgivelser endnu", t,
              "Der er ingen udgivelser endnu");

    zs_version_tekst_404("", t, sizeof(t));
    CHECK_STR("tom tekst er det samme som intet mål", t,
              "Der er ingen udgivelser endnu");

    zs_version_tekst_404("0.24.3", t, sizeof(t));
    CHECK_STR("med mål: nummeret står i teksten", t,
              "Der findes ingen udgivelse 0.24.3");
    CHECK("og den siger IKKE at der ingen udgivelser er",
          strstr(t, "ingen udgivelser endnu") == NULL);

    /* Kanterne paa udgangen. Teksten skrives til en buffer kalderen ejer,
     * og en for lille buffer maa klippe og ikke skrive udenfor. */
    char lille[10];
    memset(lille, 'Z', sizeof(lille));
    zs_version_tekst_404("1.2.3", lille, sizeof(lille));
    CHECK("for lille buffer er stadig afsluttet",
          lille[sizeof(lille) - 1] == '\0');
    CHECK("og der står noget i den", lille[0] != 'Z');

    /* NULL og nul laengde maa ikke give et nedbrud. */
    zs_version_tekst_404("1.2.3", NULL, 10);
    zs_version_tekst_404("1.2.3", t, 0);
    CHECK("NULL og længde nul går ikke ned", true);
}

/*
 * Maalversionen: hvad skal den vaere, og er det en NY ordre?
 *
 * Beslutningen laa inde i zs_ota_set_target, og den fil kan ikke
 * oversaettes paa en almindelig maskine. Saa var fire regler uproevede,
 * og de blev skrevet om samme dag som de blev vigtige: et NYT maal
 * udloeser nu et tjek med det samme i stedet for om op til en halv time.
 *
 * Er "aendret" forkert, sker der én af to ting. Siger den for lidt, bliver
 * opdateringen liggende som foer. Siger den for meget, tjekker skaermen
 * ved hver genforbindelse, og en skaerm paa et daarligt net banker paa
 * GitHub i stedet for at vise tal.
 */
void test_version_maal(void)
{
    ZS_SUITE("Målversionen: hvad gemmes, og er det en ny ordre");

    zs_maal_t m;

    /* ---- fra intet til noget ---- */
    m = zs_version_vaelg_maal("", "0.24.3");
    CHECK("et maal kan saettes", m.gyldig);
    CHECK_STR("og det gemmes", m.maal, "0.24.3");
    CHECK("og det ER en ny ordre", m.aendret);

    /* ---- v foran skal VAEK ---- */
    m = zs_version_vaelg_maal("", "v0.24.3");
    CHECK_STR("v foran bliver fjernet", m.maal, "0.24.3");
    CHECK("og det er stadig en ordre", m.aendret);
    m = zs_version_vaelg_maal("", "V1.2.3");
    CHECK_STR("stort V også", m.maal, "1.2.3");

    /* ---- DEN VIGTIGSTE: samme maal igen er IKKE en ordre ---- */
    m = zs_version_vaelg_maal("0.24.3", "0.24.3");
    CHECK("samme maal igen er gyldigt", m.gyldig);
    CHECK_STR("og staar uaendret", m.maal, "0.24.3");
    CHECK("men det er IKKE en ny ordre", !m.aendret);

    /* Og med v foran skal den stadig se at det er det samme. Serveren
     * kan sende begge former, og en genforbindelse maa ikke blive en
     * ordre bare fordi formen er en anden. */
    m = zs_version_vaelg_maal("0.24.3", "v0.24.3");
    CHECK("v foran er stadig det samme maal", !m.aendret);
    CHECK_STR("og det gemmes uden v", m.maal, "0.24.3");

    /* ---- et ANDET maal er en ordre ---- */
    m = zs_version_vaelg_maal("0.24.3", "0.24.4");
    CHECK("et andet maal er en ordre", m.aendret);
    CHECK_STR("og det nye gemmes", m.maal, "0.24.4");

    /* ---- at RYDDE er ogsaa en ordre ---- */
    m = zs_version_vaelg_maal("0.24.3", "");
    CHECK("ryddet er gyldigt", m.gyldig);
    CHECK_STR("og maalet er tomt", m.maal, "");
    CHECK("og det ER en ordre: tilbage til nyeste", m.aendret);

    m = zs_version_vaelg_maal("0.24.3", NULL);
    CHECK("NULL er det samme som ryddet", m.gyldig && m.maal[0] == '\0');
    CHECK("og også en ordre", m.aendret);

    /* Men ryddet NAAR DER INTET VAR, er ingen ordre. Ellers ville hver
     * indmeldelse paa en skaerm uden maal udloese et tjek. */
    m = zs_version_vaelg_maal("", "");
    CHECK("ryddet naar der intet var, er ingen ordre", !m.aendret);
    m = zs_version_vaelg_maal(NULL, NULL);
    CHECK("NULL begge veje går ikke ned", m.gyldig && !m.aendret);

    /* ---- skrald beholder det gamle ---- */
    const char *skrald[] = { "0.24", "0.24.3.1", "abc", "0.24.3a",
                             "../0.24.3", "0.24.3/evil", " 0.24.3",
                             "0..3", "v", "." };
    int beholdt = 0, sagde_ordre = 0, sagde_gyldig = 0;
    for (size_t i = 0; i < sizeof skrald / sizeof *skrald; i++) {
        zs_maal_t r = zs_version_vaelg_maal("0.24.3", skrald[i]);
        if (strcmp(r.maal, "0.24.3") != 0) { beholdt++; }
        if (r.aendret) { sagde_ordre++; }
        if (r.gyldig)  { sagde_gyldig++; }
    }
    CHECK_INT("skrald beholder det gamle maal", beholdt, 0);
    CHECK_INT("og er ingen ordre", sagde_ordre, 0);
    CHECK_INT("og meldes som ugyldigt", sagde_gyldig, 0);

    /* Og skrald naar der INTET maal var, maa ikke lave et. */
    m = zs_version_vaelg_maal("", "abc");
    CHECK("skrald laver ikke et maal ud af ingenting", m.maal[0] == '\0');
    CHECK("og er ingen ordre", !m.aendret);

    /* ---- en for lang tekst maa ikke skrive uden for ---- */
    char lang[200];
    memset(lang, '1', sizeof(lang) - 1);
    lang[sizeof(lang) - 1] = '\0';
    m = zs_version_vaelg_maal("0.24.3", lang);
    CHECK("en meget lang tekst afvises", !m.gyldig);
    CHECK_STR("og det gamle staar endnu", m.maal, "0.24.3");

    /* Og at maalet ALTID er afsluttet, uanset hvad der kom ind. */
    int uafsluttet = 0;
    const char *alt[] = { "", "0.1.2", "v9.9.9", "abc", NULL, lang };
    for (size_t i = 0; i < sizeof alt / sizeof *alt; i++) {
        zs_maal_t r = zs_version_vaelg_maal("0.24.3", alt[i]);
        bool nul_fundet = false;
        for (size_t k = 0; k < sizeof r.maal; k++) {
            if (r.maal[k] == '\0') { nul_fundet = true; break; }
        }
        if (!nul_fundet) { uafsluttet++; }
    }
    CHECK_INT("målet er altid afsluttet med nul", uafsluttet, 0);
}



