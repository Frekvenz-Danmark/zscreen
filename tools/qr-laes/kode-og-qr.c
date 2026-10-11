/*
 * zScreen - skaermens EGEN vej: koden og QR'en, som montoeren ser dem.
 *
 * Den bruger zs_tilmeld til at lave koden og zs_qr til at tegne den,
 * praecis som zs_screen_tilmeld.c gor. Et script der laver sin egen kode
 * ville bevise ingenting om firmwaren - se memory/proev-med-firmwarens-
 * egen-vej.
 *
 * Skriver koden paa stdout og QR'en som en BMP, saa forloebet kan
 * fortsaette: telefonen laeser billedet, og Frekvenz kalder API'et med
 * det der blev laest.
 *
 *   kode-og-qr <bmp-sti> [skala]
 */
#include "zs_tilmeld.h"
#include "zs_qr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void u16(FILE *f, unsigned v) { fputc((int)(v & 0xFF), f); fputc((int)((v >> 8) & 0xFF), f); }
static void u32(FILE *f, unsigned long v)
{ for (int i = 0; i < 4; i++) { fputc((int)((v >> (8 * i)) & 0xFF), f); } }

int main(int argc, char **argv)
{
    const char *sti = (argc > 1) ? argv[1] : "qr.bmp";
    int skala_ind = (argc > 2) ? atoi(argv[2]) : 0;

    /*
     * Tilfaeldige bytes, som skaermen faar dem fra esp_fill_random.
     * Her fra uret: det er en proeve, ikke en kode der skal holde.
     */
    uint8_t tilf[ZS_TILMELD_TILF_BYTES];
    unsigned long froe = (unsigned long)time(NULL) ^ (unsigned long)clock();
    for (size_t i = 0; i < sizeof(tilf); i++) {
        froe = froe * 6364136223846793005UL + 1442695040888963407UL;
        tilf[i] = (uint8_t)(froe >> 33);
    }

    /* Skaermens egen tilstand: inverter fundet, paa flaaden. */
    zs_tilmeld_t s;
    memset(&s, 0, sizeof(s));
    if (!zs_tilmeld_bed_om(&s, true, true, 1000, tilf, sizeof(tilf))) {
        fprintf(stderr, "zs_tilmeld_bed_om sagde nej\n");
        return 1;
    }
    const char *kode = zs_tilmeld_kode_nu(&s, true, true, 1000);
    if (kode == NULL || kode[0] == '\0') {
        fprintf(stderr, "der kom ingen kode\n");
        return 1;
    }

    static uint8_t qr[4096];
    int moduler = 0;
    if (!zs_qr_lav(kode, qr, sizeof(qr), &moduler)) {
        fprintf(stderr, "zs_qr_lav sagde nej til \"%s\"\n", kode);
        return 1;
    }
    /* Skaermens egen skala, med mindre der staar andet. Tallet kommer
     * fra zs_qr.h, det samme sted siden tegner efter. */
    int skala = (skala_ind > 0) ? skala_ind
                                : zs_qr_skala(moduler, ZS_QR_SKAERM_MAAL_PX);
    if (skala <= 0) {
        fprintf(stderr, "skalaen blev nul\n");
        return 1;
    }
    int kant = ZS_QR_KANT_MODULER;
    int side = (moduler + 2 * kant) * skala;
    int raekke = side * 3;
    int pad = (4 - (raekke % 4)) % 4;
    unsigned long data = (unsigned long)(raekke + pad) * (unsigned long)side;

    FILE *f = fopen(sti, "wb");
    if (f == NULL) { perror("fopen"); return 1; }
    fputc('B', f); fputc('M', f);
    u32(f, 54 + data); u16(f, 0); u16(f, 0); u32(f, 54);
    u32(f, 40); u32(f, (unsigned long)side); u32(f, (unsigned long)side);
    u16(f, 1); u16(f, 24); u32(f, 0); u32(f, data);
    u32(f, 2835); u32(f, 2835); u32(f, 0); u32(f, 0);
    for (int py = side - 1; py >= 0; py--) {
        for (int px = 0; px < side; px++) {
            int mx = px / skala - kant;
            int my = py / skala - kant;
            int sort = (mx >= 0 && mx < moduler && my >= 0 && my < moduler)
                       ? (zs_qr_modul(qr, mx, my) ? 1 : 0) : 0;
            int v = sort ? 0 : 255;
            fputc(v, f); fputc(v, f); fputc(v, f);
        }
        for (int i = 0; i < pad; i++) { fputc(0, f); }
    }
    fclose(f);

    /* Kun koden paa stdout, saa den kan laeses af et script. */
    printf("%s\n", kode);
    fprintf(stderr, "  %d moduler, %d px/modul, %dx%d px -> %s\n",
            moduler, skala, side, side, sti);
    return 0;
}
