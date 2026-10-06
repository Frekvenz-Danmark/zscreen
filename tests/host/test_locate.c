/*
 * Test af hvem der er VORES inverter.
 *
 * Det er den farligste beslutning i hele firmwaren. Vaelger den forkert,
 * ser alt ud som om det virker: skaermen er forbundet, tallene opdaterer,
 * kurverne ser rigtige ud. De er bare naboens. En kunde kan se paa
 * saadan en skaerm i maaneder uden at opdage noget.
 *
 * Derfor staar beslutningen for sig selv uden netvaerk, og derfor er den
 * proevet af fra alle de vinkler vi kunne komme paa.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_locate.h"

#include <stdbool.h>
#include <string.h>

/*
 * Den rigtige zs_discovery.c bygges med nu, saa der er ingen stubbe
 * laengere. Den blev taget med fordi beslutningen om HVILKE raekker der
 * skal gennemsoeges hoerer til her, og den skal kunne proeves af.
 *
 * zs_locate_find kaldes stadig ikke herfra: den vil ud paa netvaerket.
 */

/* Laver et fund med en adresse og et serienummer. */
static zs_found_t fund(const char *ip, const char *serial)
{
    zs_found_t f;
    memset(&f, 0, sizeof(f));
    snprintf(f.ip, sizeof(f.ip), "%s", ip);
    snprintf(f.info.serial, sizeof(f.info.serial), "%s", serial);
    snprintf(f.info.manufacturer, sizeof(f.info.manufacturer), "Fronius");
    return f;
}

void test_locate(void)
{
    ZS_SUITE("Hvilken inverter er vores");

    const char *VORES = "30430123";
    const char *NABOEN = "30499999";

    {
        /* Det almindelige: den sidder hvor den plejer. Kalderen har lagt
         * den gemte adresse foerst, og den har vores serienummer. */
        zs_found_t f[] = { fund("192.168.1.50", "30430123") };
        size_t v = 99;
        CHECK("samme adresse, samme serienummer: fundet",
              zs_locate_pick(f, 1, VORES, &v) == ZS_PICK_SERIAL);
        CHECK_INT("og det er den foerste", (int)v, 0);
    }

    {
        /* Det der var i stykker foer: inverteren har faaet en ny adresse.
         * Serienummeret er det samme, saa det ER vores. */
        zs_found_t f[] = {
            fund("192.168.1.77", NABOEN),
            fund("192.168.1.50", "30430123"),
        };
        size_t v = 99;
        CHECK("ny adresse, samme serienummer: fundet",
              zs_locate_pick(f, 2, VORES, &v) == ZS_PICK_SERIAL);
        CHECK_INT("og det er den rigtige af de to", (int)v, 1);
    }

    {
        /*
         * DET VIGTIGSTE TJEK.
         *
         * Vi kender vores inverter, og der er praecis én paa nettet, men
         * det er ikke vores. Saa tager vi den IKKE. I en lejlighed eller
         * et raekkehus kan naboens inverter sagtens svare.
         */
        zs_found_t f[] = { fund("192.168.1.50", NABOEN) };
        CHECK("én fundet, men det er naboens: tag den ikke",
              zs_locate_pick(f, 1, VORES, NULL) == ZS_PICK_INGEN);
    }

    {
        /* Flere paa nettet, ingen af dem er vores. Heller ikke her
         * gaetter vi. */
        zs_found_t f[] = {
            fund("192.168.1.50", NABOEN),
            fund("192.168.1.51", "30488888"),
        };
        CHECK("flere, ingen er vores: sig fra",
              zs_locate_pick(f, 2, VORES, NULL) == ZS_PICK_FLERE);
    }

    {
        /* Flere paa nettet, og én af dem er vores. Den tages uden at
         * kunden skal spoerges. */
        zs_found_t f[] = {
            fund("192.168.1.50", NABOEN),
            fund("192.168.1.51", "30430123"),
            fund("192.168.1.52", "30477777"),
        };
        size_t v = 99;
        CHECK("flere, én er vores: tag vores",
              zs_locate_pick(f, 3, VORES, &v) == ZS_PICK_SERIAL);
        CHECK_INT("og ikke en af de andre", (int)v, 1);
    }

    {
        /*
         * Ny skaerm, eller en der er opdateret fra en udgave der ikke
         * gemte serienummeret. Er der praecis én inverter, er det den.
         */
        zs_found_t f[] = { fund("192.168.1.50", "30430123") };
        size_t v = 99;
        CHECK("intet gemt serienummer, én fundet: tag den",
              zs_locate_pick(f, 1, "", &v) == ZS_PICK_ENESTE);
        CHECK_INT("og det er den", (int)v, 0);
        CHECK("NULL virker som tom streng",
              zs_locate_pick(f, 1, NULL, &v) == ZS_PICK_ENESTE);
    }

    {
        /* Intet gemt serienummer og flere invertere: saa er der ikke
         * noget at gaa efter, og kunden maa vaelge. */
        zs_found_t f[] = {
            fund("192.168.1.50", "30430123"),
            fund("192.168.1.51", NABOEN),
        };
        CHECK("intet gemt serienummer, flere fundet: kunden vælger",
              zs_locate_pick(f, 2, "", NULL) == ZS_PICK_FLERE);
    }

    {
        /* Ingenting fundet. */
        zs_found_t f[] = { fund("192.168.1.50", "30430123") };
        CHECK("ingen fundet: ingen", zs_locate_pick(f, 0, VORES, NULL) == ZS_PICK_INGEN);
        CHECK("NULL-liste: ingen", zs_locate_pick(NULL, 3, VORES, NULL) == ZS_PICK_INGEN);
    }

    {
        /*
         * En inverter der ikke udfylder sit serienummer kan ikke
         * genkendes. Den maa ikke komme til at passe paa "vores".
         */
        zs_found_t f[] = { fund("192.168.1.50", "") };
        CHECK("fundet uden serienummer passer ikke på vores",
              zs_locate_pick(f, 1, VORES, NULL) == ZS_PICK_INGEN);
    }

    {
        /*
         * Sammenligningen er noejagtig, ikke et praefiks. To Fronius fra
         * samme serie kan have numre der starter ens, og "30430123" maa
         * ikke passe paa "304301234".
         */
        zs_found_t f[] = { fund("192.168.1.50", "304301234") };
        CHECK("et længere nummer der starter ens: passer ikke",
              zs_locate_pick(f, 1, VORES, NULL) == ZS_PICK_INGEN);

        zs_found_t k[] = { fund("192.168.1.50", "3043012") };
        CHECK("et kortere nummer der starter ens: passer ikke",
              zs_locate_pick(k, 1, VORES, NULL) == ZS_PICK_INGEN);
    }

    {
        /*
         * Samme inverter svarer paa to adresser, fx fordi den baade har
         * kabel og wifi. Vi vaelger den FOERSTE, og kalderen har lagt den
         * gemte adresse foerst. Saa skifter skaermen ikke frem og tilbage
         * mellem to adresser der begge virker.
         */
        zs_found_t f[] = {
            fund("192.168.1.50", "30430123"),
            fund("192.168.1.90", "30430123"),
        };
        size_t v = 99;
        CHECK("samme inverter to steder: vælg den første",
              zs_locate_pick(f, 2, VORES, &v) == ZS_PICK_SERIAL);
        CHECK_INT("altså den gemte adresse", (int)v, 0);
    }

    {
        /* Teksterne skal findes til alle udfald. En tom streng paa
         * skaermen er en fejl man ikke kan fejlsoege. */
        const zs_loc_t alle[] = { ZS_LOC_SAMME, ZS_LOC_NY, ZS_LOC_TAGET,
                                  ZS_LOC_INGEN, ZS_LOC_FLERE,
                                  ZS_LOC_INTET_NET, ZS_LOC_AFBRUDT };
        bool alle_ok = true;
        for (size_t i = 0; i < sizeof(alle)/sizeof(alle[0]); i++) {
            const char *t = zs_locate_text(alle[i]);
            if (t == NULL || t[0] == '\0') { alle_ok = false; }
        }
        CHECK("alle udfald har en dansk tekst", alle_ok);
    }
}

/*
 * Hvilke raekker skal gennemsoeges?
 *
 * DET HER VAR EN RIGTIG FEJL, og den sad paa vores egen maskine.
 *
 * Foer tog scanningen et NETVAERK, fx "10.1.0.0", og gennemsoegte de tre
 * foerste tal plus 1 til 254. Paa et almindeligt /24 er det rigtigt. Men
 * vores eget net er et /20: netvaerket hedder 10.1.0.0 mens enhederne
 * sidder paa 10.1.4.x. Vi ledte altsaa i raekken 10.1.0.x, hvor der ikke
 * var nogen, og skaermen meldte at der ingen inverter var. Den kunne
 * aldrig findes.
 */
void test_locate_blokke(void)
{
    ZS_SUITE("Hvilke rækker skal gennemsøges");

    char b[ZS_SCAN_MAX_BLOKKE][12];

    {
        /* Det almindelige hjemmenet. Én raekke, og den er vores egen. */
        size_t n = zs_discovery_blokke("192.168.1.50", 24, b, ZS_SCAN_MAX_BLOKKE);
        CHECK_INT("et /24 giver én række", (int)n, 1);
        CHECK_STR("og det er vores egen", b[0], "192.168.1");
    }

    {
        /* DET DER VAR GALT. Vores eget net. */
        size_t n = zs_discovery_blokke("10.1.4.140", 20, b, ZS_SCAN_MAX_BLOKKE);
        CHECK_INT("et /20 giver seksten rækker", (int)n, 16);
        CHECK_STR("vores EGEN række kommer først", b[0], "10.1.4");

        /* Og alle seksten raekker i nettet skal vaere der, 10.1.0 til
         * 10.1.15, uden dubletter. */
        bool har_alle = true, dublet = false;
        for (int i = 0; i < 16; i++) {
            char vent[12];
            /* %u og en graense. Oversaetteren kan ikke se at i er
             * lille, og med %d regner den med et minustegn og ti cifre. */
            snprintf(vent, sizeof(vent), "10.1.%u", (unsigned)i % 1000u);
            int fundet = 0;
            for (size_t j = 0; j < n; j++) {
                if (strcmp(b[j], vent) == 0) { fundet++; }
            }
            if (fundet == 0) { har_alle = false; }
            if (fundet > 1)  { dublet = true; }
        }
        CHECK("alle seksten rækker i nettet er med", har_alle);
        CHECK("og ingen af dem står to gange", !dublet);
    }

    {
        /* Et /23 er to raekker, og de skal vaere de rigtige to. */
        size_t n = zs_discovery_blokke("192.168.5.10", 23, b, ZS_SCAN_MAX_BLOKKE);
        CHECK_INT("et /23 giver to rækker", (int)n, 2);
        CHECK_STR("vores egen først", b[0], "192.168.5");
        CHECK_STR("og naboen bagefter", b[1], "192.168.4");
    }

    {
        /* Et /16 har 256 raekker, men vi stopper ved graensen. */
        size_t n = zs_discovery_blokke("172.16.9.3", 16, b, ZS_SCAN_MAX_BLOKKE);
        CHECK_INT("et /16 stopper ved grænsen", (int)n, ZS_SCAN_MAX_BLOKKE);
        CHECK_STR("og vores egen er stadig først", b[0], "172.16.9");
    }

    {
        /* Et praefiks vi ikke forstaar skal ikke saette tusind raekker i
         * gang. Hellere ét hug det rigtige sted. */
        size_t n = zs_discovery_blokke("10.0.0.5", 0, b, ZS_SCAN_MAX_BLOKKE);
        CHECK_INT("praefiks nul regnes som /24", (int)n, 1);
        CHECK_STR("og det er vores egen", b[0], "10.0.0");
        CHECK_INT("praefiks 99 regnes også som /24",
                  (int)zs_discovery_blokke("10.0.0.5", 99, b, ZS_SCAN_MAX_BLOKKE), 1);
        CHECK_INT("et /32 er også bare vores egen",
                  (int)zs_discovery_blokke("10.0.0.5", 32, b, ZS_SCAN_MAX_BLOKKE), 1);
    }

    {
        /* Plads til én række skal give præcis vores egen, aldrig mere. */
        CHECK_INT("plads til én række giver én",
                  (int)zs_discovery_blokke("10.1.4.140", 20, b, 1), 1);
        CHECK_STR("og det er vores egen", b[0], "10.1.4");
    }

    {
        /* Det der ikke giver mening. */
        CHECK_INT("NULL-adresse giver nul",
                  (int)zs_discovery_blokke(NULL, 24, b, ZS_SCAN_MAX_BLOKKE), 0);
        CHECK_INT("skrald giver nul",
                  (int)zs_discovery_blokke("ikke en adresse", 24, b, ZS_SCAN_MAX_BLOKKE), 0);
        CHECK_INT("for store tal giver nul",
                  (int)zs_discovery_blokke("300.1.2.3", 24, b, ZS_SCAN_MAX_BLOKKE), 0);
        CHECK_INT("nul plads giver nul",
                  (int)zs_discovery_blokke("10.0.0.5", 24, b, 0), 0);
    }
}
