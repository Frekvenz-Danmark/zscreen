/*
 * Test af SunSpec-laget.
 *
 * Her ligger de fejl der goer mest ondt i praksis, fordi de ikke ser
 * forkerte ud: en skalafaktor der er 100 gange ved siden af, en
 * "ikke implementeret"-vaerdi der bliver vist som en maaling, eller en
 * afladekanal der bliver taget for en ladekanal saa batteriet ser ud
 * til at goere det modsatte af hvad det goer.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_sunspec.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Et lille simuleret registerkort til vandringen                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint16_t base;
    uint16_t regs[512];
    size_t   n;
    int      fail_after;   /* -1 = svar altid, ellers fejl efter N kald */
    int      calls;
} fake_dev_t;

static bool fake_read(void *ctx, uint8_t unit, uint16_t addr, uint16_t count, uint16_t *out)
{
    fake_dev_t *d = (fake_dev_t *)ctx;
    (void)unit;
    d->calls++;
    if (d->fail_after >= 0 && d->calls > d->fail_after) {
        return false;
    }
    if (addr < d->base) {
        return false;
    }
    size_t off = (size_t)(addr - d->base);
    if (off + count > d->n) {
        return false;
    }
    for (uint16_t i = 0; i < count; i++) {
        out[i] = d->regs[off + i];
    }
    return true;
}

/* Skriver en model ind i kortet paa position pos. Returnerer ny position. */
static size_t put_model(fake_dev_t *d, size_t pos, uint16_t id, uint16_t len)
{
    d->regs[pos++] = id;
    d->regs[pos++] = len;
    for (uint16_t i = 0; i < len; i++) {
        d->regs[pos++] = 0;
    }
    return pos;
}

static void build_device(fake_dev_t *d, uint16_t base)
{
    memset(d, 0, sizeof(*d));
    d->base = base;
    d->fail_after = -1;
    size_t pos = 0;
    d->regs[pos++] = 0x5375;   /* "Su" */
    d->regs[pos++] = 0x6E53;   /* "nS" */
    pos = put_model(d, pos, 1,   66);
    pos = put_model(d, pos, 103, 50);
    pos = put_model(d, pos, 120, 26);
    pos = put_model(d, pos, 124, 24);
    pos = put_model(d, pos, 160, 88);   /* 8 + 4 kanaler a 20 */
    /* Slutblokken er ID 0xFFFF fulgt af laengde 0, som i standarden. */
    d->regs[pos++] = 0xFFFF;
    d->regs[pos++] = 0x0000;
    d->n = pos;
}

/* ------------------------------------------------------------------ */

void test_sunspec(void)
{
    ZS_SUITE("SunSpec: fortegn og skalafaktorer");

    CHECK_INT("0 er 0",                 zs_ss_i16(0), 0);
    CHECK_INT("32767 er 32767",         zs_ss_i16(32767), 32767);
    CHECK_INT("32768 er -32768",        zs_ss_i16(32768), -32768);
    CHECK_INT("65535 er -1",            zs_ss_i16(65535), -1);
    CHECK_INT("0xFFFE er -2",           zs_ss_i16(0xFFFE), -2);

    CHECK("skalafaktor 0 er gyldig",     zs_ss_sf_valid(0));
    CHECK("skalafaktor -2 er gyldig",    zs_ss_sf_valid(-2));
    CHECK("skalafaktor 10 er gyldig",    zs_ss_sf_valid(10));
    CHECK("skalafaktor -10 er gyldig",   zs_ss_sf_valid(-10));
    CHECK("skalafaktor 11 er ugyldig",  !zs_ss_sf_valid(11));
    CHECK("skalafaktor -11 er ugyldig", !zs_ss_sf_valid(-11));
    CHECK("0x8000 er 'ikke implementeret'", !zs_ss_sf_valid(ZS_SS_NA_SUNSSF));

    CHECK_F("42 med skalafaktor 0",   zs_ss_apply_sf(42.0f, 0),   42.0,   0.0001);
    CHECK_F("42 med skalafaktor 1",   zs_ss_apply_sf(42.0f, 1),   420.0,  0.0001);
    CHECK_F("4200 med skalafaktor -2", zs_ss_apply_sf(4200.0f, -2), 42.0, 0.0001);
    CHECK_F("1 med skalafaktor 3",    zs_ss_apply_sf(1.0f, 3),    1000.0, 0.0001);
    /* Praecision: en potensfunktion ville give 0,99999994 her. */
    CHECK_F("100 med skalafaktor -2 er praecis 1", zs_ss_apply_sf(100.0f, -2), 1.0, 1e-9);

    ZS_SUITE("SunSpec: afkodning af vaerdier");

    {
        /* index:      0      1     2       3       4       5 */
        uint16_t r[] = { 4200,  0xFFFE, 0x8000, 0xFFFF, 0x8000, 42 };
        /*               vaerdi  sf=-2   NA-i16  NA-u16  NA-sf   vaerdi */
        size_t n = sizeof(r) / sizeof(r[0]);

        zs_val_t v = zs_ss_dec_i16_sf(r, n, 0, 1);
        CHECK("int16 med skalafaktor giver et resultat", v.ok);
        CHECK_F("4200 x 10^-2 = 42 W", v.v, 42.0, 0.001);

        v = zs_ss_dec_i16_sf(r, n, 2, 1);
        CHECK("int16 0x8000 giver 'ingen data'", !v.ok);

        v = zs_ss_dec_u16_sf(r, n, 3, 1);
        CHECK("uint16 0xFFFF giver 'ingen data'", !v.ok);

        v = zs_ss_dec_i16_sf(r, n, 0, 4);
        CHECK("skalafaktor 0x8000 giver 'ingen data'", !v.ok);

        v = zs_ss_dec_i16_sf(r, n, 99, 1);
        CHECK("offset uden for blokken giver 'ingen data'", !v.ok);
        v = zs_ss_dec_i16_sf(r, n, 0, 99);
        CHECK("skalafaktor uden for blokken giver 'ingen data'", !v.ok);
        v = zs_ss_dec_i16_sf(NULL, n, 0, 1);
        CHECK("NULL-blok giver 'ingen data'", !v.ok);
    }

    {
        /* 1234,5 som IEEE 754: 0x449A5000. Big-endian, hoej halvdel foerst. */
        uint16_t r[] = { 0x449A, 0x5000, 0x7FC0, 0x0000, 0x0000 };
        size_t n = sizeof(r) / sizeof(r[0]);

        zs_val_t v = zs_ss_dec_f32(r, n, 0);
        CHECK("float32 laeses", v.ok);
        CHECK_F("float32 giver 1234,5", v.v, 1234.5, 0.001);

        v = zs_ss_dec_f32(r, n, 2);
        CHECK("float32 NaN giver 'ingen data'", !v.ok);

        v = zs_ss_dec_f32(r, n, 4);
        CHECK("float32 der raekker ud over blokken giver 'ingen data'", !v.ok);
    }

    {
        uint16_t r[] = { 0x0001, 0x0000, 0x0000, 0x0000 };
        uint32_t acc = 0;
        CHECK("acc32 laeses", zs_ss_dec_acc32(r, 4, 0, &acc));
        CHECK_INT("acc32 samles korrekt fra to registre", acc, 65536);
        CHECK("acc32 lig 0 betyder 'ikke implementeret'", !zs_ss_dec_acc32(r, 4, 2, &acc));
    }

    {
        uint16_t r[] = { 4, 0xFFFF };
        CHECK_INT("enum16 laeses", zs_ss_dec_enum16(r, 2, 0), 4);
        CHECK_INT("enum16 0xFFFF giver -1", zs_ss_dec_enum16(r, 2, 1), -1);
        CHECK_INT("enum16 uden for blokken giver -1", zs_ss_dec_enum16(r, 2, 5), -1);
    }

    ZS_SUITE("SunSpec: strenge");

    {
        char out[33];
        /* "Fronius" polstret med mellemrum, som Fronius faktisk goer. */
        uint16_t r[] = { 0x4672, 0x6F6E, 0x6975, 0x7320, 0x2020, 0x2020 };
        size_t len = zs_ss_dec_string(r, 6, 0, 6, out, sizeof(out));
        CHECK_STR("polstring klippes af", out, "Fronius");
        CHECK_INT("laengden passer", len, 7);
    }
    {
        char out[33];
        /* Nul-afsluttet midt i et register. */
        uint16_t r[] = { 0x4142, 0x4300, 0x5858 };
        zs_ss_dec_string(r, 3, 0, 3, out, sizeof(out));
        CHECK_STR("nul-byte afslutter strengen", out, "ABC");
    }
    {
        char out[33];
        uint16_t r[] = { 0x0000, 0x0000 };
        zs_ss_dec_string(r, 2, 0, 2, out, sizeof(out));
        CHECK_STR("tom streng giver tom streng", out, "");
    }
    {
        char out[33];
        /* Raa bytes der ikke er printbare maa ikke naa skaermen. */
        uint16_t r[] = { 0x4101, 0x42FF };
        zs_ss_dec_string(r, 2, 0, 2, out, sizeof(out));
        CHECK_STR("ikke-printbare bytes bliver til mellemrum", out, "A B");
    }
    {
        char out[4];
        uint16_t r[] = { 0x4142, 0x4344, 0x4546 };
        zs_ss_dec_string(r, 3, 0, 3, out, sizeof(out));
        CHECK_STR("for lille udbuffer klipper og nul-afslutter", out, "ABC");
    }
    {
        char out[33];
        uint16_t r[] = { 0x4142 };
        CHECK_INT("laesning uden for blokken giver 0",
                  zs_ss_dec_string(r, 1, 0, 8, out, sizeof(out)), 0);
        CHECK_STR("og skriver en tom streng", out, "");
    }

    ZS_SUITE("SunSpec: kanal-navne");

    /* Rækkefoelgen er det vigtige her. "STCHA" er en delstreng af
     * "STDISCHA", saa hvis lade tjekkes foerst, vender batteriet forkert. */
    CHECK_INT("STDISCHA er afladning",
              zs_ss_classify_channel("STDISCHA"), ZS_CH_BATTERY_DISCHARGE);
    CHECK_INT("ST DISCHA med mellemrum er afladning",
              zs_ss_classify_channel("ST DISCHA"), ZS_CH_BATTERY_DISCHARGE);
    CHECK_INT("Storage Discharge er afladning",
              zs_ss_classify_channel("Storage Discharge"), ZS_CH_BATTERY_DISCHARGE);
    CHECK_INT("STCHA er opladning",
              zs_ss_classify_channel("STCHA"), ZS_CH_BATTERY_CHARGE);
    CHECK_INT("  STCHA   med polstring er opladning",
              zs_ss_classify_channel("  STCHA   "), ZS_CH_BATTERY_CHARGE);
    CHECK_INT("Storage Charge er opladning",
              zs_ss_classify_channel("Storage Charge"), ZS_CH_BATTERY_CHARGE);
    CHECK_INT("St_Cha med understreg er opladning",
              zs_ss_classify_channel("St_Cha"), ZS_CH_BATTERY_CHARGE);
    CHECK_INT("MPPT 1 er solceller",
              zs_ss_classify_channel("MPPT 1"), ZS_CH_PV);
    CHECK_INT("DC_STRING 2 er solceller",
              zs_ss_classify_channel("DC_STRING 2"), ZS_CH_PV);
    CHECK_INT("String 1 er solceller",
              zs_ss_classify_channel("String 1"), ZS_CH_PV);
    CHECK_INT("PV1 er solceller",
              zs_ss_classify_channel("PV1"), ZS_CH_PV);
    CHECK_INT("tom streng er ukendt",
              zs_ss_classify_channel(""), ZS_CH_UNKNOWN);
    CHECK_INT("NULL er ukendt",
              zs_ss_classify_channel(NULL), ZS_CH_UNKNOWN);
    CHECK_INT("noget helt andet er ukendt",
              zs_ss_classify_channel("Aux Input"), ZS_CH_UNKNOWN);

    CHECK("MPPT (4) taeller som aktiv",       zs_ss_channel_active(ZS_DCST_MPPT));
    CHECK("THROTTLING (5) taeller som aktiv", zs_ss_channel_active(ZS_DCST_THROTTLING));
    CHECK("STARTING (3) taeller IKKE som aktiv", !zs_ss_channel_active(ZS_DCST_STARTING));
    CHECK("OFF (1) taeller ikke",             !zs_ss_channel_active(ZS_DCST_OFF));
    CHECK("SLEEPING (2) taeller ikke",        !zs_ss_channel_active(ZS_DCST_SLEEPING));
    CHECK("FAULT (7) taeller ikke",           !zs_ss_channel_active(ZS_DCST_FAULT));
    CHECK("ukendt (-1) taeller ikke",         !zs_ss_channel_active(-1));

    ZS_SUITE("SunSpec: vandring gennem model-kaeden");

    {
        fake_dev_t d;
        zs_ss_map_t map;

        build_device(&d, 40000);
        CHECK("kaeden findes paa 40000", zs_ss_walk(fake_read, &d, 1, &map));
        CHECK_INT("basen er 40000", map.base, 40000);
        CHECK_INT("fem modeller fundet", map.count, 5);
        CHECK("kortet er komplet", !map.truncated);

        const zs_ss_model_t *m = zs_ss_find(&map, 1);
        CHECK("Common-modellen findes", m != NULL);
        /* base 40000 + "SunS" (2) + ID og laengde (2) = 40004 */
        CHECK_INT("Common-data starter paa 40004", m ? m->addr : 0, 40004);
        CHECK_INT("Common er 66 registre", m ? m->len : 0, 66);

        m = zs_ss_find(&map, 160);
        CHECK("MPPT-modellen findes", m != NULL);
        /* 40004 + 66 + 2 = 40072 (103), + 50 + 2 = 40124 (120),
         * + 26 + 2 = 40152 (124), + 24 + 2 = 40178 (160) */
        CHECK_INT("MPPT-data starter paa 40178", m ? m->addr : 0, 40178);

        CHECK("model 999 findes ikke", zs_ss_find(&map, 999) == NULL);

        /* find_any respekterer prioriteten i listen. */
        uint16_t want[] = { 113, 103 };
        m = zs_ss_find_any(&map, want, 2);
        CHECK_INT("find_any tager foerste match i PRIORITETSraekkefoelge, ikke i kortets",
                  m ? m->id : 0, 103);
    }

    {
        fake_dev_t d;
        zs_ss_map_t map;
        build_device(&d, 40001);
        CHECK("kaeden findes ogsaa paa 40001", zs_ss_walk(fake_read, &d, 1, &map));
        CHECK_INT("basen er 40001", map.base, 40001);
    }

    {
        /* Ingen SunS-markoer: enheden taler ikke SunSpec. */
        fake_dev_t d;
        memset(&d, 0, sizeof(d));
        d.base = 40000;
        d.fail_after = -1;
        d.regs[0] = 0x1234;
        d.regs[1] = 0x5678;
        d.n = 2;
        zs_ss_map_t map;
        CHECK("uden SunS-markoer giver ingen vandring", !zs_ss_walk(fake_read, &d, 1, &map));
        CHECK_INT("og ingen modeller", map.count, 0);
    }

    {
        /*
         * En model der paastaar laengde 0.
         *
         * Uden et vaern staar adressen stille og loekken snurrer rundt
         * paa den samme model igen og igen. Vi vil have at den stopper
         * og markerer kortet som ufuldstaendigt.
         */
        fake_dev_t d;
        memset(&d, 0, sizeof(d));
        d.base = 40000;
        d.fail_after = -1;
        size_t pos = 0;
        d.regs[pos++] = 0x5375;
        d.regs[pos++] = 0x6E53;
        pos = put_model(&d, pos, 1, 66);
        d.regs[pos++] = 103;   /* model 103 ... */
        d.regs[pos++] = 0;     /* ... med laengde 0 */
        d.regs[pos++] = 0xFFFF;
        d.regs[pos++] = 0x0000;
        d.n = pos;

        zs_ss_map_t map;
        CHECK("laengde 0 stopper vandringen i stedet for at gaa i ring",
              zs_ss_walk(fake_read, &d, 1, &map));
        CHECK_INT("kun modellen foer den defekte er med", map.count, 1);
        CHECK("kortet markeres som ufuldstaendigt", map.truncated);
    }

    {
        /* Laesningen doer midtvejs. Det vi naaede skal beholdes, men
         * kortet skal markeres ufuldstaendigt saa ingen konkluderer
         * "her er ingen elmaaler". */
        fake_dev_t d;
        build_device(&d, 40000);
        d.fail_after = 3;   /* SunS-soegning + to model-headere */
        zs_ss_map_t map;
        zs_ss_walk(fake_read, &d, 1, &map);
        CHECK("afbrudt vandring markeres som ufuldstaendig", map.truncated);
        CHECK("men det fundne beholdes", map.count > 0);
    }

    {
        /*
         * Slut-markoeren ligger helt ude paa kanten af enhedens
         * registerplads: der er plads til 0xFFFF, men ikke til
         * laengdefeltet efter. Nogle invertere er sat op sadan, og
         * saa svarer de med "ulovlig adresse" paa en to-registers
         * laesning. Det skal stadig taelle som en pænt afsluttet
         * kaede, ikke som et ufuldstaendigt kort.
         */
        fake_dev_t d;
        memset(&d, 0, sizeof(d));
        d.base = 40000;
        d.fail_after = -1;
        size_t pos = 0;
        d.regs[pos++] = 0x5375;
        d.regs[pos++] = 0x6E53;
        pos = put_model(&d, pos, 1, 66);
        d.regs[pos++] = 0xFFFF;   /* kun ID'et, intet laengdefelt */
        d.n = pos;

        zs_ss_map_t map;
        CHECK("kaeden vandres", zs_ss_walk(fake_read, &d, 1, &map));
        CHECK_INT("modellen foer slut-markoeren er med", map.count, 1);
        CHECK("slut-markoer paa kanten taeller som pæn afslutning", !map.truncated);
    }

    {
        /* Flere modeller end vi har plads til. */
        fake_dev_t d;
        memset(&d, 0, sizeof(d));
        d.base = 40000;
        d.fail_after = -1;
        size_t pos = 0;
        d.regs[pos++] = 0x5375;
        d.regs[pos++] = 0x6E53;
        for (int i = 0; i < ZS_SS_MAX_MODELS + 5; i++) {
            pos = put_model(&d, pos, (uint16_t)(100 + i), 2);
        }
        d.regs[pos++] = 0xFFFF;
        d.regs[pos++] = 0x0000;
        d.n = pos;

        zs_ss_map_t map;
        zs_ss_walk(fake_read, &d, 1, &map);
        CHECK_INT("vi stopper ved graensen", map.count, ZS_SS_MAX_MODELS);
        CHECK("og siger det", map.truncated);
    }
}
