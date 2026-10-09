#include "zs_test.h"
#include "../../firmware/main/ui/zs_qr.h"

/*
 * Søgemønstret: hvert af de tre hjørner i en QR har et 7x7 mærke der
 * ser sådan ud, og det er DET en læser finder koden med.
 *
 *     # # # # # # #      ydre ring mørk
 *     #           #
 *     #   # # #   #      indre 3x3 mørk
 *     #   # # #   #
 *     #   # # #   #
 *     #           #
 *     # # # # # # #
 *
 * Er det forkert, bliver koden ikke læst, og den ser helt almindelig ud
 * på skærmen. Derfor er det her prøven og ikke et checksum: et checksum
 * ville også passe på et mønster der er rigtigt forkert.
 */
static bool soegemoenster_ok(const uint8_t *qr, int ox, int oy)
{
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 7; x++) {
            bool kant  = (x == 0 || x == 6 || y == 0 || y == 6);
            bool midte = (x >= 2 && x <= 4 && y >= 2 && y <= 4);
            bool vil   = kant || midte;
            if (zs_qr_modul(qr, ox + x, oy + y) != vil) {
                return false;
            }
        }
    }
    return true;
}

void test_qr(void)
{
    ZS_SUITE("QR: geometrien, som kan regnes");

    /* 21 moduler plus fire moduls luft på hver side er 29 i alt. */
    CHECK_INT("21 moduler i 148 pixels giver 5 pixels per modul",
              zs_qr_skala(21, 148), 5);
    CHECK_INT("i 100 pixels giver det 3", zs_qr_skala(21, 100), 3);
    CHECK_INT("og i 29 pixels giver det præcis 1", zs_qr_skala(21, 29), 1);
    /*
     * Under det kan det IKKE gå op, og så skal den sige nej i stedet for
     * at tegne noget der ikke kan læses.
     */
    CHECK_INT("i 28 pixels kan det ikke gå op", zs_qr_skala(21, 28), 0);
    CHECK_INT("nul moduler giver nul", zs_qr_skala(0, 148), 0);
    CHECK_INT("negativt giver nul", zs_qr_skala(-1, 148), 0);
    CHECK_INT("nul pixels giver nul", zs_qr_skala(21, 0), 0);

    CHECK_INT("29 moduler gange 5 er 145 pixels", zs_qr_px(21, 5), 145);
    CHECK_INT("luften er fire moduler, altså 20 pixels", zs_qr_kant_px(5), 20);
    CHECK_INT("uden skala er der ingen bredde", zs_qr_px(21, 0), 0);
    CHECK_INT("og ingen luft", zs_qr_kant_px(0), 0);

    /*
     * DEN VIGTIGSTE REGEL: luften skal være der. Det tegnede minus
     * luften på begge sider skal være præcis modulerne. Går den i stykker,
     * fylder koden hele pladsen og bliver ikke læst af nogen telefon.
     */
    bool luft_passer = true;
    for (int m = 21; m <= 29; m += 4) {
        for (int px = 40; px <= 400; px++) {
            int s = zs_qr_skala(m, px);
            if (s == 0) { continue; }
            if (zs_qr_px(m, s) - 2 * zs_qr_kant_px(s) != m * s) {
                luft_passer = false;
            }
            /* Og det tegnede må ALDRIG være bredere end pladsen. */
            if (zs_qr_px(m, s) > px) { luft_passer = false; }
        }
    }
    CHECK("luften er lige stor på alle sider, og koden går aldrig ud over "
          "pladsen", luft_passer);

    ZS_SUITE("QR: koden der faktisk kommer ud");

    uint8_t qr[ZS_QR_BUF];
    int moduler = 0;

    CHECK("seks tegn bliver en kode", zs_qr_lav("ABCDEF", qr, sizeof(qr),
                                                &moduler));
    CHECK_INT("og den er den mindste udgave, 21 moduler", moduler, 21);

    CHECK("søgemønstret øverst til venstre er rigtigt",
          soegemoenster_ok(qr, 0, 0));
    CHECK("øverst til højre",  soegemoenster_ok(qr, moduler - 7, 0));
    CHECK("og nederst til venstre", soegemoenster_ok(qr, 0, moduler - 7));

    /* Den samme tekst skal give den samme kode. Ellers er der noget der
     * ikke er nulstillet mellem to kald. */
    uint8_t qr2[ZS_QR_BUF];
    int m2 = 0;
    (void)zs_qr_lav("ABCDEF", qr2, sizeof(qr2), &m2);
    CHECK_INT("samme tekst giver samme bredde", m2, moduler);
    bool ens = true;
    for (int y = 0; y < moduler; y++) {
        for (int x = 0; x < moduler; x++) {
            if (zs_qr_modul(qr, x, y) != zs_qr_modul(qr2, x, y)) { ens = false; }
        }
    }
    CHECK("og præcis samme moduler", ens);

    /* En anden tekst skal give en anden kode. */
    uint8_t qr3[ZS_QR_BUF];
    int m3 = 0;
    (void)zs_qr_lav("ZZZZZZ", qr3, sizeof(qr3), &m3);
    bool forskel = false;
    for (int y = 0; y < moduler && !forskel; y++) {
        for (int x = 0; x < moduler; x++) {
            if (zs_qr_modul(qr, x, y) != zs_qr_modul(qr3, x, y)) {
                forskel = true;
                break;
            }
        }
    }
    CHECK("en anden tekst giver en anden kode", forskel);

    /* Udenfor koden er der ingen moduler. qrcodegen svarer falsk, og det
     * er det der gør at luften bliver lys. */
    CHECK("uden for koden er der ingen moduler",
          zs_qr_modul(qr, -1, 0) == false
          && zs_qr_modul(qr, moduler, 0) == false
          && zs_qr_modul(qr, 0, moduler) == false);

    ZS_SUITE("QR: når det ikke kan gå");

    /* Mere end der kan vaere i den stoerste udgave vi tillader. */
    char lang[512];
    memset(lang, 'A', sizeof(lang) - 1);
    lang[sizeof(lang) - 1] = '\0';
    CHECK("en tekst der er for lang bliver sagt nej til",
          zs_qr_lav(lang, qr, sizeof(qr), &moduler) == false);

    CHECK("NULL-tekst", zs_qr_lav(NULL, qr, sizeof(qr), &moduler) == false);
    CHECK("NULL-buffer", zs_qr_lav("ABCDEF", NULL, sizeof(qr), &moduler) == false);
    CHECK("for lille buffer",
          zs_qr_lav("ABCDEF", qr, ZS_QR_BUF - 1, &moduler) == false);
    CHECK("NULL til bredden",
          zs_qr_lav("ABCDEF", qr, sizeof(qr), NULL) == false);
    CHECK("NULL-kode har ingen moduler", zs_qr_modul(NULL, 0, 0) == false);
}
