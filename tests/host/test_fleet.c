/*
 * Test af beskederne til floedestyringen.
 *
 * Indmeldelsen staar og falder paa at et certifikat paa halvanden
 * kilobyte bliver undsluppet rigtigt til JSON. Er ét linjeskift
 * forkert, afviser serveren os, og det ville vi foerst opdage ude hos
 * en kunde. Derfor testes den med et RIGTIGT certifikat og ikke med en
 * opdigtet streng.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_fleet_msg.h"
#include "../../firmware/main/zs_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Et rigtigt certifikat, forkortet men med den rigtige form: linjeskift
 * mellem hver linje, og de to markoerer. */
static const char CERT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDazCCAlOgAwIBAgIUXQ7dKqRqF0dYlZ8rN3mW1jTcLKEwDQYJKoZIhvcNAQEL\n"
    "BQAwRTELMAkGA1UEBhMCREsxGTAXBgNVBAoMEEZyZWt2ZW56IERhbm1hcmsxGzAZ\n"
    "-----END CERTIFICATE-----\n";

void test_fleet(void)
{
    ZS_SUITE("Floedestyring: indmeldelsen");

    {
        size_t n = zs_fleet_msg_enroll_size(CERT);
        char *buf = malloc(n);
        CHECK("der kan regnes en stoerrelse ud", n > strlen(CERT));

        size_t len = zs_fleet_msg_enroll(buf, n, CERT);
        CHECK("beskeden blev bygget", len > 0);

        /* Formen skal vaere praecis den serveren vil have. */
        CHECK("begynder rigtigt",
              strncmp(buf, "{\"type\":\"x509\",\"cert\":\"", 23) == 0);
        CHECK("slutter rigtigt",
              len >= 2 && strcmp(buf + len - 2, "\"}") == 0);

        /* Det vigtigste: ingen RAA linjeskift inde i JSON-strengen. Et
         * raat linjeskift goer hele beskeden ulaeselig for serveren. */
        bool raa_linjeskift = false;
        for (size_t i = 0; i < len; i++) {
            if (buf[i] == '\n' || buf[i] == '\r') { raa_linjeskift = true; }
        }
        CHECK("ingen raa linjeskift i beskeden", !raa_linjeskift);

        /* Og de skal vaere der som to tegn i stedet. */
        CHECK("linjeskiftene er undsluppet", strstr(buf, "\\n") != NULL);

        /* Begge markoerer skal vaere med, ellers er certifikatet
         * afkortet og serveren afviser det. */
        CHECK("BEGIN er med", strstr(buf, "BEGIN CERTIFICATE") != NULL);
        CHECK("END er med",   strstr(buf, "END CERTIFICATE") != NULL);

        /* Hvert linjeskift i kilden skal give praecis ét \n. */
        size_t kilde_ns = 0, ud_ns = 0;
        for (const char *p = CERT; *p; p++) { if (*p == '\n') { kilde_ns++; } }
        for (size_t i = 0; i + 1 < len; i++) {
            if (buf[i] == '\\' && buf[i+1] == 'n') { ud_ns++; i++; }
        }
        CHECK("lige saa mange linjeskift ud som ind", kilde_ns == ud_ns);
        free(buf);
    }

    ZS_SUITE("Floedestyring: en for lille buffer sender ingenting");

    {
        /*
         * En afkortet besked ville serveren afvise, og fejlen ville
         * vaere svaer at se. Derfor skal den sige nej helt.
         */
        char lille[40];
        CHECK("for lille buffer giver nul",
              zs_fleet_msg_enroll(lille, sizeof(lille), CERT) == 0);
        CHECK("og efterlader en tom streng", lille[0] == '\0');

        char vinkel[8];
        CHECK("latterligt lille buffer giver ogsaa nul",
              zs_fleet_msg_enroll(vinkel, sizeof(vinkel), CERT) == 0);

        char ok[64];
        CHECK("NULL-certifikat giver nul",
              zs_fleet_msg_enroll(ok, sizeof(ok), NULL) == 0);
        CHECK("tomt certifikat giver nul",
              zs_fleet_msg_enroll(ok, sizeof(ok), "") == 0);
    }

    ZS_SUITE("Floedestyring: undslipning i sig selv");

    {
        char b[128];
        zs_fleet_msg_enroll(b, sizeof(b), "a\nb");
        CHECK_STR("linjeskift", b, "{\"type\":\"x509\",\"cert\":\"a\\nb\"}");

        zs_fleet_msg_enroll(b, sizeof(b), "a\"b");
        CHECK_STR("gaasefod", b, "{\"type\":\"x509\",\"cert\":\"a\\\"b\"}");

        zs_fleet_msg_enroll(b, sizeof(b), "a\\b");
        CHECK_STR("bagstreg", b, "{\"type\":\"x509\",\"cert\":\"a\\\\b\"}");

        zs_fleet_msg_enroll(b, sizeof(b), "a\r\nb");
        CHECK_STR("vognretur smides vaek", b, "{\"type\":\"x509\",\"cert\":\"a\\nb\"}");
    }

    ZS_SUITE("Floedestyring: emnet der skrives til");

    {
        char e[160];
        size_t n = zs_fleet_msg_topic(e, sizeof(e), "master",
                                      "zscreen-2884858b4bb0", "solarPower",
                                      "2jbChXAO768eclHSf5BpVZ");
        CHECK("emnet blev bygget", n > 0);
        CHECK_STR("og det er praecis det serveren vil have", e,
                  "master/zscreen-2884858b4bb0/writeattributevalue/solarPower/2jbChXAO768eclHSf5BpVZ");

        /* Det laengste vi kan komme ud for: laengste feltnavn og et
         * fuldt enheds-id. Skal passe i 160. */
        n = zs_fleet_msg_topic(e, sizeof(e), "master",
                               "zscreen-ffffffffffff", "batteryCapacity",
                               "123456789012345678901234567890");
        CHECK("det laengste emne passer stadig", n > 0 && n < sizeof(e));

        /*
         * Et AFKORTET emne ville skrive i et andet felt eller i en
         * anden enhed. Det maa aldrig ske.
         */
        char kort[30];
        CHECK("for lille buffer giver nul",
              zs_fleet_msg_topic(kort, sizeof(kort), "master",
                                 "zscreen-2884858b4bb0", "solarPower",
                                 "2jbChXAO768eclHSf5BpVZ") == 0);
        CHECK("og en tom streng", kort[0] == '\0');

        CHECK("manglende enheds-id giver nul",
              zs_fleet_msg_topic(e, sizeof(e), "master", "x", "y", "") == 0);
        CHECK("NULL giver nul",
              zs_fleet_msg_topic(e, sizeof(e), "master", "x", "y", NULL) == 0);
    }
}

/*
 * Hvornaar melder vi ind igen?
 *
 * Den her regel var EN GANG forkert, og det er grunden til at den nu
 * ligger for sig selv. Indmeldelsen blev kun forsoegt naar abonnementet
 * var nyt, altsaa én gang per forbindelse. Og maalt mod en rigtig
 * OpenRemote holder serveren forbindelsen AABEN efter en afvisning, den
 * svarer {"type":"error","error":"UNAUTHORIZED"} og lader den ligge. Saa
 * kom der aldrig et nyt abonnement, og skaermen var afvist for evigt,
 * ogsaa efter at certifikatet var rettet paa serveren, indtil nogen tog
 * stroemmen.
 */
void test_fleet_igen(void)
{
    ZS_SUITE("Melder vi ind igen");

    const int64_t NU = 1000000;

    /* Det der gik galt: afvist, pausen er gaaet, forbindelsen er stadig
     * aaben, og der kommer aldrig et nyt abonnement. */
    CHECK("afvist og pausen gaaet: proev igen",
          zs_fleet_enroll_due(true, false, NU - 1, NU) == true);
    CHECK("praecis naar uret er gaaet: proev igen",
          zs_fleet_enroll_due(true, false, NU, NU) == true);
    CHECK("pausen er ikke gaaet endnu: vent",
          zs_fleet_enroll_due(true, false, NU + 1, NU) == false);

    /* Er vi inde, er der intet at proeve. Ellers ville en indmeldt
     * skaerm melde ind oven i sig selv og faa forbindelsen lukket. */
    CHECK("allerede indmeldt: lad vaere",
          zs_fleet_enroll_due(true, true, NU - 1, NU) == false);

    /* Uden abonnement kommer svaret ingen steder. Melder vi ind her,
     * sender vi certifikatet ud i det blaa. */
    CHECK("ikke abonneret: lad vaere",
          zs_fleet_enroll_due(false, false, NU - 1, NU) == false);

    /*
     * Nul betyder "ingen plan", ikke "med det samme". Betoed det med det
     * samme, ville en nystartet skaerm melde ind fra den rene tilstand.
     */
    CHECK("nul er ingen plan, ikke nu",
          zs_fleet_enroll_due(true, false, 0, NU) == false);
    CHECK("negativ er ogsaa ingen plan",
          zs_fleet_enroll_due(true, false, -5, NU) == false);

    /* Lige efter opstart er uret smaat. Der maa ikke vaere et hul hvor
     * vi melder ind foer der er noget at melde ind paa. */
    CHECK("nul i baade ur og tid: lad vaere",
          zs_fleet_enroll_due(true, false, 0, 0) == false);
}

/*
 * Emnet vi LYTTER paa, altsaa den vej serveren sender ned til skaermen.
 *
 * Det er den eneste vej udefra og ind i enheden, saa det skal pege
 * praecis paa vores egen enhed og ikke paa en andens. Et afkortet emne
 * ville i vaerste fald lytte paa en nabo.
 */
void test_fleet_lyt(void)
{
    ZS_SUITE("Emnet vi lytter på");

    char e[160];
    size_t n = zs_fleet_msg_lyt_topic(e, sizeof(e), "master",
                                      "zscreen-2884858b4bb0", "targetVersion",
                                      "2jbChXAO768eclHSf5BpVZ");
    CHECK("der kommer et emne", n > 0);
    CHECK("og det er det rigtige",
          strcmp(e, "master/zscreen-2884858b4bb0/attributevalue/"
                    "targetVersion/2jbChXAO768eclHSf5BpVZ") == 0);
    CHECK("der står IKKE write i det", strstr(e, "write") == NULL);

    /* Skrive-emnet til samme felt skal stadig have write. De to maa
     * ikke kunne forveksles. */
    char w[160];
    zs_fleet_msg_topic(w, sizeof(w), "master", "zscreen-2884858b4bb0",
                       "targetVersion", "2jbChXAO768eclHSf5BpVZ");
    CHECK("skrive-emnet har write", strstr(w, "/writeattributevalue/") != NULL);
    CHECK("de to er ikke ens", strcmp(e, w) != 0);

    /* Samme strenghed som skrive-emnet: hellere ingenting end et halvt. */
    char kort[30];
    CHECK("for lille buffer giver nul",
          zs_fleet_msg_lyt_topic(kort, sizeof(kort), "master",
                                 "zscreen-2884858b4bb0", "targetVersion",
                                 "2jbChXAO768eclHSf5BpVZ") == 0);
    CHECK("og en tom streng", kort[0] == '\0');
    CHECK("manglende enheds-id giver nul",
          zs_fleet_msg_lyt_topic(e, sizeof(e), "master", "x", "y", "") == 0);
    CHECK("NULL giver nul",
          zs_fleet_msg_lyt_topic(e, sizeof(e), "master", "x", "y", NULL) == 0);
}

/*
 * Beskeder der kommer i stykker.
 *
 * Det her er den dyreste fejl vi har haft, saa proeven bruger de
 * RIGTIGE tal. Maalt mod vores egen OpenRemote: svaret paa en
 * indmeldelse er 2604 bytes, og esp-mqtt's modtagebuffer er 1024. Saa
 * kommer det i tre stykker, og kun det foerste har et emne paa sig.
 */
void test_fleet_stykker(void)
{
    ZS_SUITE("Flådestyring: et svar der kommer i stykker");

    /* Samme tal som i virkeligheden. */
    const char *EMNE = "provisioning/zscreen-44b176b06c60/response";
    enum { MQTT_BUF = 1024, SVAR = 2604 };

    /* Et svar i den rigtige stoerrelse, og med gyldig JSON i enderne saa
     * vi kan se at det er HELE beskeden der kommer ud igen. */
    static char svar[SVAR + 1];
    int skrevet = snprintf(svar, sizeof(svar),
                           "{\"type\":\"success\",\"realm\":\"master\","
                           "\"asset\":{\"id\":\"59EUfmPaPsWslr9GpLVpMl\","
                           "\"fyld\":\"");
    while (skrevet < SVAR - 4) { svar[skrevet++] = 'x'; }
    svar[skrevet++] = '"';
    svar[skrevet++] = '}';
    svar[skrevet++] = '}';
    svar[skrevet] = '\0';
    CHECK("proevesvaret er lige saa stort som det rigtige", skrevet == SVAR - 1);

    static char plads[ZS_FLEET_SVAR_MAX];
    zs_fleet_saml_t s;
    zs_fleet_saml_init(&s, plads, sizeof(plads));

    /*
     * Stykke 1: har emnet. Resten har topic_len nul, praecis som
     * esp-mqtt leverer dem, se mqtt_client.c omkring post_data_event.
     */
    int n = (int)strlen(svar);
    int stk1 = MQTT_BUF - (int)strlen(EMNE) - 7;   /* emne og header tager plads */
    zs_saml_t r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE),
                                    svar, stk1, 0, n);
    CHECK("foerste stykke: der mangler mere", r == ZS_SAML_VENTER);
    CHECK("og emnet er husket", strcmp(s.emne, EMNE) == 0);

    int stk2 = MQTT_BUF;
    r = zs_fleet_saml_tag(&s, NULL, 0, svar + stk1, stk2, stk1, n);
    CHECK("andet stykke, uden emne: der mangler stadig", r == ZS_SAML_VENTER);
    CHECK("emnet staar endnu", strcmp(s.emne, EMNE) == 0);

    int stk3 = n - stk1 - stk2;
    r = zs_fleet_saml_tag(&s, NULL, 0, svar + stk1 + stk2, stk3, stk1 + stk2, n);
    CHECK("tredje stykke: nu er den hel", r == ZS_SAML_KLAR);
    CHECK("og hele beskeden er samlet, tegn for tegn", strcmp(plads, svar) == 0);
    CHECK("og den er lige saa lang som den vi sendte", strlen(plads) == (size_t)n);

    /* Og det vigtigste: nu KAN den laeses som JSON, hvilket ingen af de
     * tre stykker kunne alene. */
    CHECK("den samlede besked slutter rigtigt",
          plads[n - 1] == '}' && plads[n - 2] == '}');

    ZS_SUITE("Flådestyring: en besked i ét stykke");
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    const char *kort = "{\"type\":\"success\"}";
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE),
                          kort, (int)strlen(kort), 0, (int)strlen(kort));
    CHECK("en lille besked er hel med det samme", r == ZS_SAML_KLAR);
    CHECK("og staar der uroert", strcmp(plads, kort) == 0);

    /* esp-mqtt saetter total_data_len til beskedens laengde ogsaa naar der
     * kun er ét stykke. Men en server der sender nul skal ogsaa forstaas. */
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE),
                          kort, (int)strlen(kort), 0, 0);
    CHECK("total paa nul: vi stoler paa stykkets egen laengde",
          r == ZS_SAML_KLAR && strcmp(plads, kort) == 0);

    ZS_SUITE("Flådestyring: når stykkerne ikke hænger sammen");

    /* En forbindelse der falder midt i en besked efterlader en halv. Den
     * maa ALDRIG blandes ind i den naeste. */
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    (void)zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), "{\"halv\":", 8, 0, 100);
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE),
                          kort, (int)strlen(kort), 0, (int)strlen(kort));
    CHECK("en ny besked starter forfra og bliver hel", r == ZS_SAML_KLAR);
    CHECK("og den halve er vaek", strcmp(plads, kort) == 0);

    /* Et stykke der ikke passer hvor det siger. */
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    (void)zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), "abcde", 5, 0, 100);
    r = zs_fleet_saml_tag(&s, NULL, 0, "fghij", 5, 99, 100);
    CHECK("et stykke med forkert plads bliver afvist",
          r == ZS_SAML_USAMMENHAENG);
    r = zs_fleet_saml_tag(&s, NULL, 0, "fghij", 5, 5, 100);
    CHECK("og bagefter stoler vi ikke paa resten heller",
          r == ZS_SAML_USAMMENHAENG);

    /* Mere end der blev lovet. */
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    (void)zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), "abcde", 5, 0, 8);
    r = zs_fleet_saml_tag(&s, NULL, 0, "fghijklmn", 9, 5, 8);
    CHECK("flere bytes end lovet bliver afvist", r == ZS_SAML_USAMMENHAENG);

    ZS_SUITE("Flådestyring: en besked der er for stor");

    /* En lille samler, saa vi kan ramme loftet uden otte kilobyte. */
    static char lille[64];
    zs_fleet_saml_init(&s, lille, sizeof(lille));
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), "abc", 3, 0, 5000);
    CHECK("for stor besked siges der fra om", r == ZS_SAML_FOR_STOR);
    r = zs_fleet_saml_tag(&s, NULL, 0, "def", 3, 3, 5000);
    CHECK("og resten smides vaek uden at klage igen", r == ZS_SAML_FOR_STOR);
    /* Men den naeste besked skal virke. En stor besked maa ikke gOEre
     * samleren ubrugelig for altid. */
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), "{\"a\":1}", 7, 0, 7);
    CHECK("og den naeste besked kommer igennem",
          r == ZS_SAML_KLAR && strcmp(lille, "{\"a\":1}") == 0);

    /* Praecis paa kanten: der skal vaere plads til afslutningen. */
    zs_fleet_saml_init(&s, lille, sizeof(lille));
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), NULL, 0, 0,
                          (int)sizeof(lille) - 1);
    CHECK("en besked paa praecis bufferen minus ét er tilladt",
          r != ZS_SAML_FOR_STOR);
    zs_fleet_saml_init(&s, lille, sizeof(lille));
    r = zs_fleet_saml_tag(&s, EMNE, (int)strlen(EMNE), NULL, 0, 0,
                          (int)sizeof(lille));
    CHECK("én byte mere er for meget", r == ZS_SAML_FOR_STOR);

    ZS_SUITE("Flådestyring: et langt emne");

    /* Emnet klippes, men samleren vaelter ikke. */
    char langt[ZS_FLEET_EMNE_MAX + 50];
    memset(langt, 'a', sizeof(langt) - 1);
    langt[sizeof(langt) - 1] = '\0';
    zs_fleet_saml_init(&s, plads, sizeof(plads));
    r = zs_fleet_saml_tag(&s, langt, (int)strlen(langt), kort,
                          (int)strlen(kort), 0, (int)strlen(kort));
    CHECK("beskeden bliver hel alligevel", r == ZS_SAML_KLAR);
    CHECK("og emnet er klippet, ikke loebet over",
          strlen(s.emne) == ZS_FLEET_EMNE_MAX - 1);

    ZS_SUITE("Flådestyring: emnet kendes på et helt led");

    /* De to rigtige emner, som de ser ud paa traaden. */
    const char *E_SVAR = "provisioning/zscreen-44b176b06c60/response";
    const char *E_LYT  = "master/zscreen-44b176b06c60/attributevalue/"
                         "targetVersion/59EUfmPaPsWslr9GpLVpMl";

    CHECK("maalversionen genkendes midt i emnet",
          zs_fleet_emne_har_led(E_LYT, "targetVersion"));
    CHECK("svaret genkendes til sidst i emnet",
          zs_fleet_emne_har_led(E_SVAR, "response"));

    /*
     * De to maa ikke forveksles. Foer var reglen "alt der ikke
     * indeholder targetVersion er et svar", og saa ville en tredje slags
     * besked blive laest som et svar.
     */
    CHECK("svaret er ikke en maalversion",
          !zs_fleet_emne_har_led(E_SVAR, "targetVersion"));
    CHECK("maalversionen er ikke et svar",
          !zs_fleet_emne_har_led(E_LYT, "response"));

    /* Et led er et HELT led, ikke en del af et ord. */
    CHECK("et laengere ord taeller ikke",
          !zs_fleet_emne_har_led("master/x/attributevalue/targetVersionX/id",
                                 "targetVersion"));
    CHECK("en del af et ord taeller ikke",
          !zs_fleet_emne_har_led("master/x/attributevalue/mintargetVersion/id",
                                 "targetVersion"));
    CHECK("foerste led taeller med",
          zs_fleet_emne_har_led(E_SVAR, "provisioning"));
    CHECK("sidste led taeller med",
          zs_fleet_emne_har_led(E_LYT, "59EUfmPaPsWslr9GpLVpMl"));
    CHECK("tomt led taeller ikke",
          !zs_fleet_emne_har_led(E_SVAR, ""));
    CHECK("NULL taeller ikke",
          !zs_fleet_emne_har_led(NULL, "response") &&
          !zs_fleet_emne_har_led(E_SVAR, NULL));
    CHECK("et led der er laengere end emnet taeller ikke",
          !zs_fleet_emne_har_led("kort", "meget langt"));
}
