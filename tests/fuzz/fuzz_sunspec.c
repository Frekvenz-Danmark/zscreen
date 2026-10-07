/*
 * Fuzzer VANDRINGEN gennem SunSpec-kaeden, med en GYLDIG enhed som frø.
 *
 * Foerste udgave svarede med rent skrald og naaede aldrig ind: nul af
 * 200.000 runder gav et kort. En groen fuzzer der ikke naar koden er
 * vaerre end ingen fuzzer, for den ligner et bevis.
 *
 * Nu bygges en rigtig enhed med markoer og modeller, praecis som
 * enhedstestene gOEr det, og saa oedelaegges et par registre ad gangen.
 * Saa kommer vi dybt ind i kaeden med data der naesten er rigtige, og det
 * er dér fejlene bor.
 */
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

#define N_REGS 1200
typedef struct { uint16_t base; uint16_t regs[N_REGS]; size_t n; } enhed_t;

static bool laes(void *ctx, uint8_t unit, uint16_t addr, uint16_t count,
                 uint16_t *out)
{
    enhed_t *d = (enhed_t *)ctx;
    (void)unit;
    if (out == NULL || count == 0 || addr < d->base) { return false; }
    size_t off = (size_t)(addr - d->base);
    if (off + count > d->n) { return false; }
    for (uint16_t i = 0; i < count; i++) { out[i] = d->regs[off + i]; }
    return true;
}

static void byg(enhed_t *d)
{
    memset(d, 0, sizeof(*d));
    d->base = 40000;
    d->n = N_REGS;
    size_t p = 0;
    d->regs[p++] = 0x5375;          /* "Su" */
    d->regs[p++] = 0x6E53;          /* "nS" */
    /* Et par modeller med rigtige laengder. */
    const uint16_t id[]  = { 1, 103, 124, 160, 203 };
    const uint16_t len[] = { 66, 50, 24, 48, 105 };
    for (size_t m = 0; m < 5 && p + 2 + len[m] < N_REGS; m++) {
        d->regs[p++] = id[m];
        d->regs[p++] = len[m];
        for (uint16_t i = 0; i < len[m]; i++) { d->regs[p++] = (uint16_t)rnd(); }
    }
    d->regs[p++] = 0xFFFF;          /* slut paa kaeden */
    d->regs[p++] = 0;
}

int main(int argc, char **argv)
{
    unsigned long runder = (argc > 1) ? strtoul(argv[1], NULL, 10) : 50000;
    if (argc > 2) { frø = strtoul(argv[2], NULL, 10); }

    enhed_t d;
    unsigned long fandt = 0, afkortet = 0;
    for (unsigned long i = 0; i < runder; i++) {
        byg(&d);
        /* Oedelaeg én til otte registre. Nogle gange i laengdefelterne,
         * som er dem der kan faa en vandring til at loebe loebsk. */
        int antal = 1 + (int)(rnd() % 8);
        for (int k = 0; k < antal; k++) {
            size_t pos = rnd() % N_REGS;
            switch (rnd() % 5) {
            case 0: d.regs[pos] = 0xFFFF; break;
            case 1: d.regs[pos] = 0;      break;
            case 2: d.regs[pos] = 65535;  break;
            default: d.regs[pos] = (uint16_t)rnd(); break;
            }
        }
        /* Og af og til en kortere enhed, saa laesninger loeber tør. */
        if (rnd() % 4 == 0) { d.n = 4 + rnd() % N_REGS; }

        zs_ss_map_t kort;
        memset(&kort, 0xAA, sizeof(kort));
        if (zs_ss_walk(laes, &d, (uint8_t)(rnd() % 256), &kort)) {
            fandt++;
            if (kort.count > ZS_SS_MAX_MODELS) {
                printf("  FEJL: kortet siger %u modeller, plads til %d\n",
                       kort.count, ZS_SS_MAX_MODELS); return 1;
            }
            if (kort.truncated) { afkortet++; }
            for (uint8_t m = 0; m < kort.count; m++) {
                const zs_ss_model_t *mm = &kort.models[m];
                (void)zs_ss_find(&kort, mm->id);
                /* Laes i modellen, med offsets ogsaa UDENFOR dens laengde. */
                uint16_t buf[80];
                uint16_t c = (uint16_t)(1 + rnd() % 80);
                if (laes(&d, 1, mm->addr, c, buf)) {
                    size_t off = rnd() % 100;
                    (void)zs_ss_dec_i16_sf(buf, c, off, rnd() % 100);
                    (void)zs_ss_dec_f32(buf, c, off);
                    char t[48];
                    (void)zs_ss_dec_string(buf, c, off, rnd() % 20,
                                           t, 1 + rnd() % sizeof(t));
                }
            }
        }
    }
    printf("  %lu vandringer: %lu gav et kort, heraf %lu afkortede\n",
           runder, fandt, afkortet);
    if (fandt == 0) {
        printf("  FEJL: fuzzeren naaede ALDRIG ind i kaeden, den tester intet\n");
        return 1;
    }
    printf("  ingen nedbrud, ingen laesning udenfor, intet kort over graensen\n");
    return 0;
}
