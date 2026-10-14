/*
 * Pakken der bliver et bip.
 *
 * Lyden laves af den ANDEN chip paa boardet, saa et bip er fem bytes
 * over en seriel ledning. Er indpakningen forkert med én byte, sker der
 * ingenting - der kommer ingen fejl, ingen log, ingenting. Det er
 * praecis den slags der ellers kun kan proeves med en logikanalysator
 * paa to ben, og derfor aldrig bliver proevet.
 *
 * Her maales den to gange: mod pakker der er regnet i haanden, og ved
 * at pakke den ud igen med en UAFHAENGIG afkoder. Den uafhaengige del
 * er vigtig: var begge skrevet af den samme misforstaaelse, ville de
 * passe perfekt sammen og stadig vaere forkerte paa ledningen.
 *
 * zs_lyd.c inkluderes i stedet for at linkes, saa indpakkeren kan naas
 * selvom den er static. Samme greb som i test_fronius.c, og derfor
 * staar zs_lyd.c ikke i SRC i run.sh.
 */

#include "zs_test.h"
#include "../../firmware/main/ui/zs_lyd.c"

/*
 * Afkoderen. Skrevet efter beskrivelsen af COBS og ikke efter
 * indpakkeren ovenfor: hver laengde-byte siger hvor mange bytes der er
 * til det naeste nul, og nullet selv staar ikke paa ledningen.
 *
 * Nyttelasten gives UDEN skilletegnet til sidst.
 */
static int cobs_udpak(uint8_t *ud, size_t plads, const uint8_t *ind, size_t n)
{
    size_t skriv = 0, laes = 0;

    while (laes < n) {
        uint8_t kode = ind[laes++];
        if (kode == 0) {
            return -1;              /* nul maa ikke staa i en pakke */
        }
        for (uint8_t i = 1; i < kode; i++) {
            if (laes >= n || skriv >= plads) {
                return -1;
            }
            ud[skriv++] = ind[laes++];
        }
        /* Et stykke der ikke var fyldt helt op, sluttede ved et nul. */
        if (kode < 0xFF && laes < n) {
            if (skriv >= plads) {
                return -1;
            }
            ud[skriv++] = 0;
        }
    }
    return (int)skriv;
}

static bool lig_med(const uint8_t *a, const uint8_t *b, size_t n)
{
    return memcmp(a, b, n) == 0;
}

void test_lyd(void)
{
    ZS_SUITE("Lyden: pakken ud paa ledningen");

    uint8_t p[32];
    int n;

    /* ── den korte: kun kommandoen, ingen nyttelast ── */
    n = zs_lyd_pakke(p, sizeof(p), ZS_RP_BEEP_ON, 0);
    {
        /* 0xA1 har ingen nul i sig, saa stykket er ét byte langt:
         * laengde 2, data 0xA1, og nul til sidst. */
        const uint8_t vent[] = { 0x02, 0xA1, 0x00 };
        CHECK_INT("bip uden laengde bliver tre bytes", n, 3);
        CHECK("og de tre bytes er de rigtige",
              n == 3 && lig_med(p, vent, 3));
    }

    /* ── den rigtige: kommandoen og tonens laengde ── */
    n = zs_lyd_pakke(p, sizeof(p), ZS_RP_BEEP_ON, 180);
    {
        /*
         * Raa: A1 B4 00 00 00   (180 = 0xB4, mindste byte foerst)
         * De tre nuller bliver tre laengder, og forsvinder:
         *   03 A1 B4   stykke med to bytes, saa et nul
         *   01         tomt stykke, saa et nul
         *   01         tomt stykke, saa et nul
         *   01         tomt stykke, slut
         *   00         skilletegn
         */
        const uint8_t vent[] = { 0x03, 0xA1, 0xB4, 0x01, 0x01, 0x01, 0x00 };
        CHECK_INT("bip med laengde bliver syv bytes", n, 7);
        CHECK("og de syv bytes er de rigtige",
              n == 7 && lig_med(p, vent, 7));
    }

    /*
     * ── DET VIGTIGSTE: der er praecis ÉT nul, og det staar til sidst ──
     *
     * Det er hele grunden til at der pakkes ind. Staar der et nul inde i
     * pakken, laeser RP2040'eren den som to halve pakker og kaster dem
     * begge vaek.
     */
    for (uint32_t ms = 0; ms <= 2000; ms += 7) {
        n = zs_lyd_pakke(p, sizeof(p), ZS_RP_BEEP_ON, ms);
        if (n < 2) {
            ZS_FAIL("pakken kunne ikke bygges for ms=%u", (unsigned)ms);
            break;
        }
        bool kun_til_sidst = (p[n - 1] == 0);
        for (int i = 0; i < n - 1; i++) {
            if (p[i] == 0) { kun_til_sidst = false; }
        }
        if (!kun_til_sidst) {
            ZS_FAIL("nul inde i pakken ved ms=%u", (unsigned)ms);
            break;
        }
    }
    CHECK("intet nul inde i pakken, uanset tonens laengde", true);

    /* ── og den kan pakkes ud igen ── */
    {
        bool alle_ens = true;
        for (uint32_t ms = 0; ms <= 100000; ms += 977) {
            n = zs_lyd_pakke(p, sizeof(p), ZS_RP_BEEP_ON, ms);
            uint8_t tilbage[16];
            int m = cobs_udpak(tilbage, sizeof(tilbage), p, (size_t)n - 1);
            uint8_t vent[5];
            size_t vn = 0;
            vent[vn++] = ZS_RP_BEEP_ON;
            if (ms > 0) {
                vent[vn++] = (uint8_t)(ms & 0xFF);
                vent[vn++] = (uint8_t)((ms >> 8) & 0xFF);
                vent[vn++] = (uint8_t)((ms >> 16) & 0xFF);
                vent[vn++] = (uint8_t)((ms >> 24) & 0xFF);
            }
            if (m != (int)vn || !lig_med(tilbage, vent, vn)) {
                ZS_FAIL("ms=%u kom ikke rigtigt tilbage (fik %d bytes, "
                        "ventede %d)", (unsigned)ms, m, (int)vn);
                alle_ens = false;
                break;
            }
        }
        CHECK("en uafhaengig afkoder faar praecis det samme tilbage",
              alle_ens);
    }

    ZS_SUITE("Lyden: naar der ikke er plads");

    CHECK_INT("NULL giver -1", zs_lyd_pakke(NULL, 8, ZS_RP_BEEP_ON, 180), -1);
    /* Den lange pakke er syv bytes. Seks maa afvises, syv skal passe. */
    CHECK_INT("seks bytes er for lidt til en pakke paa syv",
              zs_lyd_pakke(p, 6, ZS_RP_BEEP_ON, 180), -1);
    CHECK_INT("syv bytes er praecis nok",
              zs_lyd_pakke(p, 7, ZS_RP_BEEP_ON, 180), 7);
    CHECK_INT("nul plads giver -1", zs_lyd_pakke(p, 0, ZS_RP_BEEP_ON, 0), -1);
    CHECK_INT("én byte er for lidt", zs_lyd_pakke(p, 1, ZS_RP_BEEP_ON, 0), -1);
    /*
     * Bufferen i send_tone er otte bytes. Den skal kunne rumme den
     * laengste pakke modulet nogensinde bygger, og det er den med en
     * tonelaengde: syv bytes. Gaar nogen en dag op i tonelaengde saa
     * den fylder mere, skal den her linje fange det.
     */
    CHECK("de otte bytes i send_tone er nok til den laengste pakke",
          zs_lyd_pakke(p, 8, ZS_RP_BEEP_ON, 0xFFFFFFFFu) > 0
          && zs_lyd_pakke(p, 8, ZS_RP_BEEP_ON, 0xFFFFFFFFu) <= 8);

    ZS_SUITE("Lyden: COBS over 254 bytes");

    /*
     * Den gren bliver ALDRIG brugt af modulet selv - vores pakker er fem
     * bytes - men en indpakker der er forkert over 254 er en fejl der
     * ligger og venter paa den dag nogen sender noget laengere. Derfor
     * naas den static-funktionen direkte.
     */
    {
        static uint8_t lang[600];
        static uint8_t pakket[700];
        static uint8_t tilbage[600];

        /* 300 bytes uden et eneste nul: tvinger et stykke til at blive
         * lukket ved 254 og et nyt til at begynde. */
        for (int i = 0; i < 300; i++) {
            lang[i] = (uint8_t)(1 + (i % 255));
        }
        int pn = cobs_indpak(pakket, sizeof(pakket), lang, 300);
        CHECK("300 bytes uden nul kan pakkes ind", pn > 300);
        int tn = cobs_udpak(tilbage, sizeof(tilbage), pakket, (size_t)pn);
        CHECK_INT("og kommer tilbage med samme laengde", tn, 300);
        CHECK("og med samme indhold", tn == 300 && lig_med(tilbage, lang, 300));

        /* Og det samme med nuller spredt ud. */
        for (int i = 0; i < 500; i++) {
            lang[i] = (uint8_t)((i % 7 == 0) ? 0 : (i % 256));
        }
        pn = cobs_indpak(pakket, sizeof(pakket), lang, 500);
        CHECK("500 bytes med nuller i kan pakkes ind", pn > 0);
        tn = cobs_udpak(tilbage, sizeof(tilbage), pakket, (size_t)pn);
        CHECK_INT("og kommer tilbage med samme laengde", tn, 500);
        CHECK("og med samme indhold", tn == 500 && lig_med(tilbage, lang, 500));

        /*
         * ── GRAENSEN VED 254, ÉN BYTE AD GANGEN ──
         *
         * 254 er der hvor et stykke bliver fyldt helt op. Praecis dér
         * gav indpakkeren foer en overfloedig 0x01 til sidst - 256 bytes
         * i stedet for 255 - fordi den altid aabnede et nyt stykke, ogsaa
         * naar der ikke kom mere. Dataene kom rigtigt tilbage, saa
         * proeverne paa 300 og 500 bytes saa ingenting.
         *
         * Nu maales BAADE laengden og indholdet, én byte ad gangen hen
         * over graensen. Den kanoniske laengde er n + ét stykke pr. 254
         * bytes.
         */
        bool kanonisk = true, alle_ens = true;
        for (int gn = 250; gn <= 260; gn++) {
            for (int gi = 0; gi < gn; gi++) {
                lang[gi] = (uint8_t)(1 + (gi % 254));   /* ingen nuller */
            }
            int gpak = cobs_indpak(pakket, sizeof(pakket), lang, (size_t)gn);
            int vent = gn + (gn + 253) / 254;
            if (gpak != vent) {
                ZS_FAIL("%d bytes gav %d pakket, kanonisk er %d",
                        gn, gpak, vent);
                kanonisk = false;
            }
            int gud = cobs_udpak(tilbage, sizeof(tilbage), pakket,
                                 (size_t)gpak);
            if (gud != gn || !lig_med(tilbage, lang, (size_t)gn)) {
                ZS_FAIL("%d bytes kom ikke rigtigt tilbage (fik %d)",
                        gn, gud);
                alle_ens = false;
            }
        }
        CHECK("laengden er kanonisk fra 250 til 260 bytes", kanonisk);
        CHECK("og indholdet kommer rigtigt tilbage hele vejen", alle_ens);

        /* En buffer der er én byte for lille skal sige fra, ikke skrive
         * uden for. Sanitizeren i run.sh fanger det hvis den ikke gOEr. */
        CHECK_INT("for lille buffer afvises",
                  cobs_indpak(pakket, 10, lang, 500), -1);
    }

    ZS_SUITE("Lyden: tallene i zs_config.h");

    CHECK("der er mindst én tone i et bip", ZS_LYD_TONER >= 1);
    CHECK("en tone er lang nok til at hoeres", ZS_LYD_TONE_MS >= 50);
    /*
     * Og kort nok. Et bip er ét bip og ikke en alarmklokke: fire bip
     * med ZS_LYD_TONER toner skal kunne vaere ovre paa under to
     * sekunder, ellers er det ikke laengere et bip.
     */
    CHECK("og et helt bip er under to sekunder",
          ZS_LYD_TONER * ZS_LYD_TONE_MS
          + (ZS_LYD_TONER - 1) * ZS_LYD_TONE_PAUSE_MS < 2000);
    /*
     * Frekvensen proeves IKKE her. Den staar ikke i zs_config.h
     * laengere: tonen laves af RP2040'eren, og ESP32'en sender kun en
     * laengde. Se TONE_HZ i tools/rp2040/zscreen_rp2040.ino.
     */

    /* Lyden skal vaere slaaet fra som standard: fabrikkens RP2040 svarer
     * ikke paa bip, saa en byggning med lyd til og uden den anden chip
     * flashet er tavs uden at sige hvorfor. */
    CHECK("lyden er slaaet fra som standard", ZS_LYD_ENABLED == 0);
    CHECK("og paa en Mac findes den aldrig", !zs_lyd_findes());
}
