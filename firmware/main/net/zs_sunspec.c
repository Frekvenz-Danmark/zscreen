/*
 * zScreen - SunSpec-lag. Se zs_sunspec.h for registerkortet.
 */

#include "zs_sunspec.h"
#include "../zs_log.h"

#include <string.h>
#include <math.h>
#include <ctype.h>

static const char *TAG = "sunspec";

/* "SunS" som to registre, big-endian ASCII: 'S''u' og 'n''S'. */
#define SUNS_HI   0x5375u
#define SUNS_LO   0x6E53u

/* SunSpec-basen ligger paa 40001 i dokumentationen. Om det svarer til
 * ledningsadresse 40000 eller 40001 afhaenger af om producenten taeller
 * fra 0 eller 1. Fronius er ikke konsekvent paa tvaers af firmware, saa
 * vi proever begge. Det koster én ekstra laesning ved opstart. */
static const uint16_t SUNS_TRY[] = { 40000u, 40001u };

/* ------------------------------------------------------------------ */
/* Skalafaktorer                                                       */
/* ------------------------------------------------------------------ */

/*
 * 10 oploeftet i sf, slaaet op i en tabel i stedet for at kalde powf().
 *
 * To grunde: powf() traekker hele matematikbiblioteket med ind i
 * binaeren, og den kan give 99,99999 hvor vi vil have 100. En skaerm
 * der viser 4,19 kW fordi en potensfunktion rundede skaevt, er en
 * fejl ingen gider fejlsoege.
 *
 * Indeks er sf + 10, saa sf = -10 ligger paa plads 0.
 */
static const float POW10[21] = {
    1e-10f, 1e-9f, 1e-8f, 1e-7f, 1e-6f, 1e-5f, 1e-4f, 1e-3f, 1e-2f, 1e-1f,
    1.0f,
    1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f
};

int16_t zs_ss_i16(uint16_t raw)
{
    /* Bevidst ikke en cast. Implementation-defined opfoersel ved cast fra
     * uint16 til int16 er ikke noget vi vil have i en energimaaling. */
    return (raw > 32767u) ? (int16_t)((int32_t)raw - 65536) : (int16_t)raw;
}

bool zs_ss_sf_valid(int16_t sf)
{
    /* 0x8000 er SunSpec' "ikke implementeret". Alt uden for -10..10 er
     * uden for standarden og betyder i praksis at vi laeser i et register
     * der ikke er en skalafaktor. */
    return sf != ZS_SS_NA_SUNSSF && sf >= -10 && sf <= 10;
}

float zs_ss_apply_sf(float raw, int16_t sf)
{
    if (!zs_ss_sf_valid(sf)) {
        return 0.0f;
    }
    return raw * POW10[sf + 10];
}

/* ------------------------------------------------------------------ */
/* Afkodning af enkeltvaerdier                                         */
/* ------------------------------------------------------------------ */

zs_val_t zs_ss_dec_i16_sf(const uint16_t *regs, size_t n, size_t off, size_t sf_off)
{
    if (regs == NULL || off >= n || sf_off >= n) {
        return ZS_VAL_NONE;
    }
    int16_t raw = zs_ss_i16(regs[off]);
    int16_t sf  = zs_ss_i16(regs[sf_off]);
    if (raw == ZS_SS_NA_INT16) {
        return ZS_VAL_NONE;
    }
    if (!zs_ss_sf_valid(sf)) {
        return ZS_VAL_NONE;
    }
    return zs_val((float)raw * POW10[sf + 10]);
}

zs_val_t zs_ss_dec_u16_sf(const uint16_t *regs, size_t n, size_t off, size_t sf_off)
{
    if (regs == NULL || off >= n || sf_off >= n) {
        return ZS_VAL_NONE;
    }
    uint16_t raw = regs[off];
    int16_t  sf  = zs_ss_i16(regs[sf_off]);
    if (raw == ZS_SS_NA_UINT16) {
        return ZS_VAL_NONE;
    }
    if (!zs_ss_sf_valid(sf)) {
        return ZS_VAL_NONE;
    }
    return zs_val((float)raw * POW10[sf + 10]);
}

zs_val_t zs_ss_dec_f32(const uint16_t *regs, size_t n, size_t off)
{
    if (regs == NULL || off + 1 >= n) {
        return ZS_VAL_NONE;
    }
    /* SunSpec-floats er big-endian med hoejeste halvord foerst.
     * Vi samler dem via en union i stedet for at pege en float* paa
     * bufferen: det sidste ville vaere en aligneringsfejl paa ESP32
     * naar registerbufferen ikke ligger paa en 4-byte-graense. */
    union { uint32_t u; float f; } conv;
    conv.u = ((uint32_t)regs[off] << 16) | (uint32_t)regs[off + 1];
    if (isnan(conv.f)) {
        return ZS_VAL_NONE;   /* SunSpec' "ikke implementeret" for float */
    }
    if (isinf(conv.f)) {
        return ZS_VAL_NONE;   /* ikke i standarden, men klart noget vaas */
    }
    return zs_val(conv.f);
}

bool zs_ss_dec_acc32(const uint16_t *regs, size_t n, size_t off, uint32_t *out)
{
    if (regs == NULL || out == NULL || off + 1 >= n) {
        return false;
    }
    uint32_t v = ((uint32_t)regs[off] << 16) | (uint32_t)regs[off + 1];
    if (v == ZS_SS_NA_ACC32) {
        return false;
    }
    *out = v;
    return true;
}

bool zs_ss_dec_bitfield32(const uint16_t *regs, size_t n, size_t off, uint32_t *out)
{
    if (regs == NULL || out == NULL || off + 1 >= n) {
        return false;
    }
    *out = ((uint32_t)regs[off] << 16) | (uint32_t)regs[off + 1];
    return true;
}

int32_t zs_ss_dec_enum16(const uint16_t *regs, size_t n, size_t off)
{
    if (regs == NULL || off >= n) {
        return -1;
    }
    if (regs[off] == ZS_SS_NA_ENUM16) {
        return -1;
    }
    return (int32_t)regs[off];
}

size_t zs_ss_dec_string(const uint16_t *regs, size_t n, size_t off,
                        size_t n_regs, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return 0;
    }
    out[0] = '\0';
    if (regs == NULL || n_regs == 0 || off + n_regs > n) {
        return 0;
    }

    /* Pak ud til en arbejdsbuffer. To tegn pr. register, hoejeste byte
     * foerst. Vi stopper ved foerste nul-byte, som SunSpec bruger til at
     * afslutte strenge der er kortere end feltet. */
    char tmp[2 * 32 + 1];   /* stoerste streng i vores modeller er 16 reg */
    size_t max_regs = (sizeof(tmp) - 1) / 2;
    if (n_regs > max_regs) {
        n_regs = max_regs;
    }

    size_t len = 0;
    for (size_t i = 0; i < n_regs; i++) {
        uint16_t r = regs[off + i];
        char hi = (char)((r >> 8) & 0xFF);
        char lo = (char)(r & 0xFF);
        if (hi == '\0') { break; }
        tmp[len++] = hi;
        if (lo == '\0') { break; }
        tmp[len++] = lo;
    }
    tmp[len] = '\0';

    /* Erstat alt der ikke er printbart ASCII med mellemrum. Vi kan ikke
     * regne med at inverteren sender ren UTF-8, og en enkelt raa byte i
     * et serienummer maa ikke kunne oedelaegge tegnsaetningen paa
     * skaermen eller loebe ud i en LVGL-label. */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)tmp[i];
        if (c < 0x20 || c > 0x7E) {
            tmp[i] = ' ';
        }
    }

    /* Klip mellemrum af i begge ender. Fronius polstrer med mellemrum. */
    size_t start = 0;
    while (start < len && tmp[start] == ' ') { start++; }
    size_t end = len;
    while (end > start && tmp[end - 1] == ' ') { end--; }

    size_t copy = end - start;
    if (copy > out_len - 1) {
        copy = out_len - 1;
    }
    memcpy(out, tmp + start, copy);
    out[copy] = '\0';
    return copy;
}

/* ------------------------------------------------------------------ */
/* Kanal-roller                                                        */
/* ------------------------------------------------------------------ */

zs_ch_role_t zs_ss_classify_channel(const char *idstr)
{
    if (idstr == NULL || idstr[0] == '\0') {
        return ZS_CH_UNKNOWN;
    }

    /* Normaliser: store bogstaver, og fjern mellemrum, bindestreg og
     * understreg. Firmwareversioner skriver det samme paa mange maader,
     * fx "STCHA", "ST CHA", "St_Cha" og "  STCHA  ". */
    char norm[33];
    size_t k = 0;
    for (const char *p = idstr; *p != '\0' && k < sizeof(norm) - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '_' || c == '-' || c == '.') {
            continue;
        }
        norm[k++] = (char)toupper(c);
    }
    norm[k] = '\0';

    /*
     * DISCHA skal tjekkes FOER CHA.
     *
     * "STCHA" er en delstreng af "STDISCHA". Tjekker man lade-moenstret
     * foerst, bliver batteriets afladekanal klassificeret som ladekanal.
     * Resultatet er at batteri-effekten faar omvendt fortegn: skaermen
     * siger "lader 3 kW" mens batteriet i virkeligheden aflader. Det er
     * praecis den slags fejl der ikke ser forkert ud foer man staar med
     * en kunde der undrer sig.
     */
    if (strstr(norm, "DISCHA") != NULL) {
        return ZS_CH_BATTERY_DISCHARGE;
    }
    if (strstr(norm, "STCHA") != NULL ||
        strstr(norm, "STORAGECHA") != NULL ||
        strstr(norm, "BATCHA") != NULL ||
        strstr(norm, "BATTERYCHA") != NULL) {
        return ZS_CH_BATTERY_CHARGE;
    }
    /* Solstrenge. Fronius skriver "MPPT 1", Kostal "DC_STRING 1",
     * andre bare "String 1" eller "PV1". */
    if (strstr(norm, "MPPT") != NULL ||
        strstr(norm, "DCSTR") != NULL ||
        strstr(norm, "STRING") != NULL ||
        strstr(norm, "SOLAR") != NULL ||
        strstr(norm, "PV") != NULL) {
        return ZS_CH_PV;
    }
    return ZS_CH_UNKNOWN;
}

bool zs_ss_channel_active(int32_t dcst)
{
    /* Kun MPPT (4) og THROTTLING (5) betyder at kanalen leverer.
     * STARTING (3) er en overgang paa vej op hvor DCW endnu ikke er
     * troværdig, og OFF/SLEEPING/FAULT kan sagtens staa med en gammel
     * vaerdi i DCW som saa ville blive lagt til soltallet. */
    return dcst == ZS_DCST_MPPT || dcst == ZS_DCST_THROTTLING;
}

/* ------------------------------------------------------------------ */
/* Vandring gennem model-kaeden                                        */
/* ------------------------------------------------------------------ */

const zs_ss_model_t *zs_ss_find(const zs_ss_map_t *map, uint16_t id)
{
    if (map == NULL) {
        return NULL;
    }
    for (uint8_t i = 0; i < map->count; i++) {
        if (map->models[i].id == id) {
            return &map->models[i];
        }
    }
    return NULL;
}

const zs_ss_model_t *zs_ss_find_any(const zs_ss_map_t *map,
                                    const uint16_t *ids, size_t n_ids)
{
    if (map == NULL || ids == NULL) {
        return NULL;
    }
    /* Bevidst ydre loekke over ids, ikke over modeller: rækkefølgen i
     * ids er en prioritet. Vil kalderen helst have int+SF frem for
     * float, skriver den bare 103 foer 113. */
    for (size_t j = 0; j < n_ids; j++) {
        const zs_ss_model_t *m = zs_ss_find(map, ids[j]);
        if (m != NULL) {
            return m;
        }
    }
    return NULL;
}

bool zs_ss_walk(zs_ss_read_fn read, void *ctx, uint8_t unit, zs_ss_map_t *map)
{
    if (read == NULL || map == NULL) {
        return false;
    }
    memset(map, 0, sizeof(*map));

    /* Find "SunS"-markoeren. */
    uint16_t regs[2];
    bool found = false;
    for (size_t i = 0; i < sizeof(SUNS_TRY) / sizeof(SUNS_TRY[0]); i++) {
        if (!read(ctx, unit, SUNS_TRY[i], 2, regs)) {
            continue;
        }
        if (regs[0] == SUNS_HI && regs[1] == SUNS_LO) {
            map->base = SUNS_TRY[i];
            found = true;
            break;
        }
    }
    if (!found) {
        ZS_LOGD(TAG, "unit %u: ingen SunS-markoer", unit);
        return false;
    }

    /* Vandr kaeden. addr peger paa model-ID'et for den naeste model. */
    uint32_t addr = (uint32_t)map->base + 2u;

    /* Haard graense paa antal skridt. Bliver kaeden nogensinde
     * selvrefererende eller fyldt med skrald, skal vi stoppe og ikke
     * bruge resten af dagen paa at gaa i ring. */
    for (int step = 0; step < 64; step++) {
        if (addr + 2u > 0x10000u) {
            ZS_LOGW(TAG, "model-kaeden loeber ud over adresserummet ved %u",
                    (unsigned)addr);
            map->truncated = true;
            break;
        }
        if (!read(ctx, unit, (uint16_t)addr, 2, regs)) {
            /*
             * Kunne ikke laese to registre. Det behoever ikke vaere en
             * fejl: nogle enheder laegger slut-markoeren helt ude paa
             * kanten af deres registerplads, saa der er plads til
             * 0xFFFF men ikke til laengdefeltet efter. Modbus svarer
             * saa med "ulovlig adresse" paa hele laesningen.
             *
             * Vi proever derfor ét register. Staar der 0xFFFF, er
             * kaeden pænt afsluttet og kortet er komplet. Ellers er
             * det en aegte fejl, og saa markerer vi kortet som
             * ufuldstaendigt, saa ingen konkluderer "her er ingen
             * elmaaler" paa et halvt kort.
             */
            uint16_t one;
            if (read(ctx, unit, (uint16_t)addr, 1, &one) && one == ZS_SS_END) {
                break;
            }
            ZS_LOGW(TAG, "kunne ikke laese model-header paa %u", (unsigned)addr);
            map->truncated = true;
            break;
        }
        uint16_t id  = regs[0];
        uint16_t len = regs[1];

        if (id == ZS_SS_END) {
            break;      /* pænt afsluttet kæde */
        }

        /* En model med laengde 0 findes ikke i SunSpec. Ser vi den, er
         * kaeden i stykker, og saa kan vi ikke stole paa noget efter den.
         * Uden dette tjek ville addr staa stille og loekken snurre
         * 64 gange forgaeves. */
        if (len == 0) {
            ZS_LOGW(TAG, "model %u har laengde 0, stopper vandringen", id);
            map->truncated = true;
            break;
        }
        if (addr + 2u + (uint32_t)len > 0x10000u) {
            ZS_LOGW(TAG, "model %u paastaar laengde %u og loeber ud over kanten",
                    id, len);
            map->truncated = true;
            break;
        }

        if (map->count < ZS_SS_MAX_MODELS) {
            map->models[map->count].id   = id;
            map->models[map->count].addr = (uint16_t)(addr + 2u);
            map->models[map->count].len  = len;
            map->count++;
            ZS_LOGD(TAG, "model %-5u data paa %u, laengde %u",
                    id, (unsigned)(addr + 2u), len);
        } else {
            map->truncated = true;
            ZS_LOGW(TAG, "flere end %d modeller, resten springes over",
                    ZS_SS_MAX_MODELS);
            break;
        }

        addr += 2u + (uint32_t)len;
    }

    ZS_LOGI(TAG, "unit %u: base %u, %u modeller%s",
            unit, map->base, map->count, map->truncated ? " (ufuldstaendig)" : "");
    return map->count > 0;
}
