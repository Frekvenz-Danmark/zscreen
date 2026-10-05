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
 * Scanningen og den korte afproevning hoerer til netvaerkslaget og er
 * ikke med her. zs_locate_find kaldes derfor ikke fra disse tests, og de
 * her to findes kun for at kunne linke.
 */
int zs_discovery_scan(const char *subnet, const char *prefer,
                      zs_found_t *out, size_t max,
                      zs_discovery_progress_fn progress, void *ctx)
{
    (void)subnet; (void)prefer; (void)out; (void)max; (void)progress; (void)ctx;
    return -1;
}
bool zs_discovery_was_aborted(void) { return false; }

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
