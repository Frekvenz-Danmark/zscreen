/* Laver QR'en med VORES egen kode og skriver den som BMP. */
#include "zs_qr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void u16(FILE *f, unsigned v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void u32(FILE *f, unsigned long v)
{ for (int i = 0; i < 4; i++) { fputc((int)((v >> (8 * i)) & 0xFF), f); } }

int main(int argc, char **argv)
{
    const char *tekst = (argc > 1) ? argv[1] : "ABCDEF";
    const char *sti = (argc > 2) ? argv[2] : "qr.bmp";
    int skala = (argc > 3) ? atoi(argv[3]) : 8;

    static uint8_t buf[4096];
    int moduler = 0;
    if (!zs_qr_lav(tekst, buf, sizeof(buf), &moduler)) {
        printf("zs_qr_lav sagde NEJ til %zu tegn\n", strlen(tekst));
        return 1;
    }
    int kant = ZS_QR_KANT_MODULER;
    int side = (moduler + 2 * kant) * skala;
    /* BMP vil have raekker paa 4 bytes. 24-bit. */
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

    /* BMP er nederst-foerst. */
    for (int py = side - 1; py >= 0; py--) {
        for (int px = 0; px < side; px++) {
            int mx = px / skala - kant;
            int my = py / skala - kant;
            int sort = 0;
            if (mx >= 0 && mx < moduler && my >= 0 && my < moduler) {
                sort = zs_qr_modul(buf, mx, my) ? 1 : 0;
            }
            int v = sort ? 0 : 255;
            fputc(v, f); fputc(v, f); fputc(v, f);
        }
        for (int i = 0; i < pad; i++) { fputc(0, f); }
    }
    fclose(f);
    printf("%d moduler, %dx%d px, kant %d moduler -> %s\n",
           moduler, side, side, kant, sti);
    return 0;
}
