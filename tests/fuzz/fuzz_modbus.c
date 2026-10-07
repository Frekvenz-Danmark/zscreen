/*
 * Kaster tilfaeldige og MUTEREDE rammer ind i parserne.
 *
 * Modbus- og SunSpec-koden laeser data direkte fra en enhed vi ikke
 * styrer. En inverter med en fejl i firmwaren, eller noget helt andet
 * der svarer paa port 502, kan sende hvad som helst. Det er praecis den
 * slags en fuzzer er til.
 *
 * To slags input, for de naar hver sit sted hen:
 *   RENT TILFAELDIGT   bliver for det meste afvist i headeren
 *   MUTERET GYLDIGT    kommer dybt ind, hvor de interessante fejl er
 */
#include "zs_modbus_tcp.h"
#include "zs_sunspec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int zs_log_verbose = 0;

static unsigned long frø = 1;
static unsigned rnd(void)
{
    frø = frø * 6364136223846793005UL + 1442695040888963407UL;
    return (unsigned)(frø >> 33);
}

int main(int argc, char **argv)
{
    unsigned long runder = (argc > 1) ? strtoul(argv[1], NULL, 10) : 200000;
    if (argc > 2) { frø = strtoul(argv[2], NULL, 10); }

    uint8_t ramme[600];
    uint16_t ud[ZS_MB_MAX_REGS];
    uint16_t regs[200];
    char tekst[64];
    unsigned long muteret = 0, rent = 0;

    for (unsigned long i = 0; i < runder; i++) {
        /* --- Modbus --------------------------------------------- */
        size_t n;
        if (i % 2 == 0) {
            /* Byg en GYLDIG ramme og oedelaeg et tilfaeldigt sted i den. */
            uint16_t antal = 1 + rnd() % 60;
            n = zs_mb_build_read_request(ramme, sizeof(ramme),
                                         (uint16_t)rnd(), 1,
                                         (uint16_t)rnd(), antal);
            if (n == 0) { continue; }
            /* Lav den om til et svar: fc, bytetaeller og data. */
            size_t svar = 9 + (size_t)antal * 2;
            if (svar > sizeof(ramme)) { continue; }
            ramme[7] = 3;
            ramme[8] = (uint8_t)(antal * 2);
            for (size_t k = 9; k < svar; k++) { ramme[k] = (uint8_t)rnd(); }
            ramme[4] = (uint8_t)((svar - 6) >> 8);
            ramme[5] = (uint8_t)(svar - 6);
            n = svar;
            /* og saa én til tre mutationer */
            for (int m = 0, mm = 1 + (int)(rnd() % 3); m < mm; m++) {
                ramme[rnd() % n] = (uint8_t)rnd();
            }
            muteret++;
        } else {
            n = rnd() % (sizeof(ramme) + 1);
            for (size_t k = 0; k < n; k++) { ramme[k] = (uint8_t)rnd(); }
            rent++;
        }

        uint8_t exc = 0;
        (void)zs_mb_parse_read_response(ramme, n, (uint16_t)rnd(),
                                        (uint8_t)rnd(), (uint16_t)(rnd() % 130),
                                        ud, &exc);
        (void)zs_mb_parse_write_response(ramme, n, (uint16_t)rnd(),
                                         (uint8_t)rnd(), (uint16_t)rnd(),
                                         (uint16_t)(rnd() % 130), &exc);

        /* --- SunSpec-afkoderne ---------------------------------- */
        size_t nr = rnd() % (sizeof(regs) / sizeof(regs[0]) + 1);
        for (size_t k = 0; k < nr; k++) { regs[k] = (uint16_t)rnd(); }
        size_t off = rnd() % 220;          /* med vilje ogsaa UDENFOR */
        size_t sf  = rnd() % 220;
        (void)zs_ss_dec_i16_sf(regs, nr, off, sf);
        (void)zs_ss_dec_u16_sf(regs, nr, off, sf);
        (void)zs_ss_dec_f32(regs, nr, off);
        uint32_t u32;
        (void)zs_ss_dec_acc32(regs, nr, off, &u32);
        (void)zs_ss_dec_bitfield32(regs, nr, off, &u32);
        (void)zs_ss_dec_enum16(regs, nr, off);
        (void)zs_ss_dec_string(regs, nr, off, rnd() % 40,
                               tekst, 1 + rnd() % sizeof(tekst));
    }
    printf("  %lu runder: %lu muterede gyldige, %lu rent tilfaeldige\n",
           runder, muteret, rent);
    printf("  ingen nedbrud, ingen laesning udenfor\n");
    return 0;
}
