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
