/*
 * zScreen - Fronius-lag. Se zs_fronius.h.
 */

#include "zs_fronius.h"
#include "../zs_log.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "fronius";

/* Timeouts. Fronius' egen manual anbefaler mindst 1 sekund under drift.
 * Under scanning vil vi hellere give hurtigt op og proeve naeste adresse. */
#define TMO_NORMAL_MS   1500
#define TMO_PROBE_MS     600

/* Prioriteret raekkefoelge. Heltal med skalafaktor foerst, fordi det er
 * Fronius' standardindstilling og fordi de modeller er kortere at laese. */
static const uint16_t INV_MODELS[]   = { 103, 101, 102, 113, 111, 112 };
static const uint16_t METER_MODELS[] = { 203, 201, 202, 204, 213, 211, 212, 214 };

/*
 * Bruger modellen 32-bit flydende tal, eller heltal med skalafaktor?
 *
 * SunSpec nummererer sine modeller saadan her:
 *
 *     101 102 103 104     inverter, heltal + skalafaktor
 *     111 112 113 114     inverter, flydende tal
 *     201 202 203 204     elmaaler, heltal + skalafaktor
 *     211 212 213 214     elmaaler, flydende tal
 *
 * De to slags har HELT forskellige registerkort. Laeser man et
 * heltalskort som om det var flydende tal, faar man ikke en fejl.
 * Man faar et tal. Bare et forkert et.
 *
 * Foerste udgave af denne funktion stod der "id >= 111", hvilket er
 * rigtigt for inverteren og forkert for alle fire elmaaler-modeller,
 * fordi 201 til 204 ogsaa er stoerre end 111. Resultatet var at
 * NETTET-kortet stod paa 0 W mens elmaaleren i virkeligheden meldte
 * 5 kW eksport, og at FORBRUG dermed ogsaa var forkert. Fejlen blev
 * fanget af zs-probe mod simulatoren, ikke af enhedstestene, fordi
 * testene var skrevet med den samme forkerte antagelse.
 *
 * Derfor staar de otte modelnumre nu skrevet ud. Der er ingen regning
 * at regne forkert.
 */
static bool model_is_float(uint16_t id)
{
    switch (id) {
    case 111: case 112: case 113: case 114:
    case 211: case 212: case 213: case 214:
        return true;
    default:
        return false;
    }
}

/* ------------------------------------------------------------------ */
/* Modbus-adapter til SunSpec-vandringen                               */
/* ------------------------------------------------------------------ */

typedef struct {
    zs_mb_t *mb;
    uint32_t timeout_ms;
} walk_ctx_t;

static bool walk_read(void *ctx, uint8_t unit, uint16_t addr, uint16_t count, uint16_t *out)
{
    walk_ctx_t *w = (walk_ctx_t *)ctx;
    return zs_mb_read_holding(w->mb, unit, addr, count, out, w->timeout_ms) == ZS_MB_OK;
}

/*
 * Laeser en hel models dataregistre ind i buf.
 *
 * FC3 kan hoejst hente 125 registre ad gangen, og model 160 med mange
 * kanaler er laengere. Derfor deler vi op. Det er stadig langt faerre
 * kald end at hente ét register ad gangen: en model 160 med 4 kanaler
 * bliver til ét kald i stedet for omkring 25.
 *
 * Returnerer antal laeste registre, 0 ved fejl.
 */
static uint16_t read_model(zs_mb_t *mb, uint8_t unit, const zs_ss_model_t *m,
                           uint16_t *buf, uint16_t buf_len, uint32_t timeout_ms)
{
    if (m == NULL || buf == NULL) {
        return 0;
    }
    uint16_t want = m->len;
    if (want > buf_len) {
        want = buf_len;
    }
    uint16_t done = 0;
    while (done < want) {
        uint16_t chunk = want - done;
        if (chunk > ZS_MB_MAX_REGS) {
            chunk = ZS_MB_MAX_REGS;
        }
        zs_mb_err_t err = zs_mb_read_holding(mb, unit, (uint16_t)(m->addr + done),
                                             chunk, buf + done, timeout_ms);
        if (err != ZS_MB_OK) {
            ZS_LOGD(TAG, "model %u: laesning fra %u faejlede: %s",
                    m->id, (unsigned)(m->addr + done), zs_mb_strerror(err));
            return done;   /* delvis laesning, kalderen tjekker laengden */
        }
        done += chunk;
    }
    return done;
}

/* ------------------------------------------------------------------ */
/* Identitet                                                           */
/* ------------------------------------------------------------------ */

static void read_identity(zs_mb_t *mb, uint8_t unit, const zs_ss_map_t *map,
                          zs_fr_info_t *info, uint16_t *buf, uint16_t buf_len,
                          uint32_t timeout_ms)
{
    const zs_ss_model_t *m = zs_ss_find(map, ZS_SS_COMMON);
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(mb, unit, m, buf, buf_len, timeout_ms);
    if (n < ZS_M1_MIN_LEN) {
        ZS_LOGW(TAG, "Common-model for kort: %u registre", n);
        return;
    }
    zs_ss_dec_string(buf, n, ZS_M1_MN, ZS_M1_MN_LEN, info->manufacturer, sizeof(info->manufacturer));
    zs_ss_dec_string(buf, n, ZS_M1_MD, ZS_M1_MD_LEN, info->model,        sizeof(info->model));
    zs_ss_dec_string(buf, n, ZS_M1_VR, ZS_M1_VR_LEN, info->version,      sizeof(info->version));
    zs_ss_dec_string(buf, n, ZS_M1_SN, ZS_M1_SN_LEN, info->serial,       sizeof(info->serial));
    ZS_LOGI(TAG, "identitet: %s %s, firmware %s, serienr %s",
            info->manufacturer, info->model, info->version, info->serial);
}

static void read_nameplate(zs_mb_t *mb, uint8_t unit, const zs_ss_map_t *map,
                           zs_fr_info_t *info, uint16_t *buf, uint16_t buf_len,
                           uint32_t timeout_ms)
{
    const zs_ss_model_t *m = zs_ss_find(map, ZS_SS_NAMEPLATE);
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(mb, unit, m, buf, buf_len, timeout_ms);
    if (n < ZS_M120_MIN_LEN) {
        return;
    }
    /* WRtg er inverterens AC-maerkeeffekt, ikke batteriets DC-rate.
     * Blander man de to sammen, kommer et Gen24 med BYD-batteri til at
     * paastaa 53 kW, hvilket er batteriets kortvarige DC-maks. */
    zs_val_t wrtg = zs_ss_dec_u16_sf(buf, n, ZS_M120_WRTG, ZS_M120_WRTG_SF);
    if (wrtg.ok && wrtg.v > 0.0f) {
        info->inverter_rated_kw = wrtg.v / 1000.0f;
    }
    zs_val_t whrtg = zs_ss_dec_u16_sf(buf, n, ZS_M120_WHRTG, ZS_M120_WHRTG_SF);
    if (whrtg.ok && whrtg.v > 0.0f) {
        info->battery_capacity_kwh = whrtg.v / 1000.0f;
    }
}

/* ------------------------------------------------------------------ */
/* Elmaaler                                                            */
/* ------------------------------------------------------------------ */

static bool map_has_meter(const zs_ss_map_t *map, uint16_t *out_id)
{
    const zs_ss_model_t *m = zs_ss_find_any(map, METER_MODELS,
                                            sizeof(METER_MODELS) / sizeof(METER_MODELS[0]));
    if (m == NULL) {
        return false;
    }
    if (out_id) {
        *out_id = m->id;
    }
    return true;
}

/*
 * Finder elmaaleren.
 *
 * Foerst kigger vi i inverterens egen model-kaede: paa mange Gen24 er
 * maaleren broet ind der, og saa er der ingen grund til at scanne.
 * Ellers proever vi kandidat-unit'erne én for én paa den forbindelse
 * vi allerede har.
 */
static void discover_meter(zs_fr_t *fr)
{
    fr->info.has_meter = false;
    fr->info.meter_unit = 0;
    fr->info.meter_model_id = 0;
    fr->meter_in_inverter_chain = false;
    memset(&fr->meter_map, 0, sizeof(fr->meter_map));

    uint16_t id = 0;
    if (map_has_meter(&fr->inv_map, &id)) {
        fr->info.has_meter = true;
        fr->info.meter_unit = fr->inverter_unit;
        fr->info.meter_model_id = id;
        fr->meter_in_inverter_chain = true;
        ZS_LOGI(TAG, "elmaaler ligger i inverterens egen kaede, model %u", id);
        return;
    }

    static const uint8_t candidates[] = ZS_FR_METER_CANDIDATES;
    walk_ctx_t wc = { .mb = &fr->mb, .timeout_ms = TMO_PROBE_MS };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        uint8_t unit = candidates[i];
        if (unit == fr->inverter_unit) {
            continue;   /* allerede kigget */
        }
        zs_ss_map_t m;
        if (!zs_ss_walk(walk_read, &wc, unit, &m)) {
            /* Forbindelsen kan vaere lukket af en protokolfejl. Er den
             * det, giver vi op med det samme i stedet for at brase
             * videre gennem resten af listen paa en doed socket. */
            if (!zs_mb_is_open(&fr->mb)) {
                ZS_LOGW(TAG, "forbindelsen lukkede under maaler-soegning");
                return;
            }
            continue;
        }
        if (map_has_meter(&m, &id)) {
            fr->meter_map = m;
            fr->info.has_meter = true;
            fr->info.meter_unit = unit;
            fr->info.meter_model_id = id;
            ZS_LOGI(TAG, "elmaaler fundet paa unit %u, model %u", unit, id);
            return;
        }
    }
    ZS_LOGI(TAG, "ingen elmaaler fundet. Forbrug og net kan ikke vises.");
}

/* ------------------------------------------------------------------ */
/* Opsaetning og nedlukning                                            */
/* ------------------------------------------------------------------ */

void zs_fr_init(zs_fr_t *fr)
{
    if (fr == NULL) {
        return;
    }
    memset(fr, 0, sizeof(*fr));
    zs_mb_init(&fr->mb);
    fr->inverter_unit = 1;
    fr->port = ZS_MB_DEFAULT_PORT;
    fr->meter_import_positive = true;
}

void zs_fr_disconnect(zs_fr_t *fr)
{
    if (fr == NULL) {
        return;
    }
    fr->connected = false;
    zs_mb_close(&fr->mb);
}

bool zs_fr_is_connected(const zs_fr_t *fr)
{
    return fr != NULL && fr->connected && zs_mb_is_open(&fr->mb);
}

bool zs_fr_connect(zs_fr_t *fr, const char *host, uint16_t port, uint8_t unit)
{
    if (fr == NULL || host == NULL) {
        return false;
    }
    /* Flaget ned foerst. Fra nu af og indtil alt er paa plads maa ingen
     * anden traad tro at der er brugbare data. */
    fr->connected = false;

    zs_mb_close(&fr->mb);
    if (port == 0) {
        port = ZS_MB_DEFAULT_PORT;
    }
    if (unit == 0) {
        unit = 1;
    }

    zs_mb_err_t err = zs_mb_connect(&fr->mb, host, port, TMO_NORMAL_MS);
    if (err != ZS_MB_OK) {
        ZS_LOGW(TAG, "kunne ikke forbinde til %s:%u: %s", host, port, zs_mb_strerror(err));
        return false;
    }

    snprintf(fr->host, sizeof(fr->host), "%s", host);
    fr->port = port;
    fr->inverter_unit = unit;

    /* Nulstil alt der beskriver anlaegget. Skifter kunden inverter,
     * maa gammel identitet ikke blive haengende paa Detaljer-siden. */
    memset(&fr->info, 0, sizeof(fr->info));

    walk_ctx_t wc = { .mb = &fr->mb, .timeout_ms = TMO_NORMAL_MS };
    if (!zs_ss_walk(walk_read, &wc, unit, &fr->inv_map)) {
        ZS_LOGW(TAG, "%s:%u unit %u taler ikke SunSpec", host, port, unit);
        zs_mb_close(&fr->mb);
        return false;
    }

    read_identity(&fr->mb, unit, &fr->inv_map, &fr->info,
                  fr->block, ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);
    read_nameplate(&fr->mb, unit, &fr->inv_map, &fr->info,
                   fr->block, ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);

    const zs_ss_model_t *inv = zs_ss_find_any(&fr->inv_map, INV_MODELS,
                                              sizeof(INV_MODELS) / sizeof(INV_MODELS[0]));
    fr->info.has_inverter = (inv != NULL);
    fr->info.inverter_model_id = inv ? inv->id : 0;

    fr->info.has_battery = (zs_ss_find(&fr->inv_map, ZS_SS_STORAGE) != NULL);

    const zs_ss_model_t *mppt = zs_ss_find(&fr->inv_map, ZS_SS_MPPT);
    fr->info.has_mppt = (mppt != NULL);

    discover_meter(fr);

    if (!zs_mb_is_open(&fr->mb)) {
        ZS_LOGW(TAG, "forbindelsen gik tabt under opstart");
        return false;
    }
    if (!fr->info.has_inverter) {
        ZS_LOGW(TAG, "fandt SunSpec, men ingen invertermodel. Er det en elmaaler?");
        zs_mb_close(&fr->mb);
        return false;
    }

    /* Foerst her er alt paa plads. */
    fr->connected = true;
    ZS_LOGI(TAG, "klar: %s %s paa %s:%u (inverter unit %u, maaler unit %u)",
            fr->info.manufacturer, fr->info.model, host, port, unit, fr->info.meter_unit);
    return true;
}

/* ------------------------------------------------------------------ */
/* Aflaesning                                                          */
/* ------------------------------------------------------------------ */

/* Kanaler i model 160 der er slukket, sover eller er i fejl. Deres DCW
 * kan staa med en gammel vaerdi som ikke maa taelle med. */
static bool channel_is_dead(int32_t dcst)
{
    return dcst == ZS_DCST_OFF || dcst == ZS_DCST_SLEEPING ||
           dcst == ZS_DCST_FAULT || dcst == ZS_DCST_SHUTTING_DOWN;
}

/*
 * Laeser hele model 160 i ét hug og fylder kanallisten.
 *
 * Zbox-koden laeser label, effekt og tilstand hver for sig, hvilket
 * bliver til omkring 25 Modbus-kald for fire kanaler. Her henter vi
 * hele modellen i ét kald og pakker ud lokalt.
 */
static void read_channels(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->channel_count = 0;

    const zs_ss_model_t *m = zs_ss_find(&fr->inv_map, ZS_SS_MPPT);
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(&fr->mb, fr->inverter_unit, m, fr->block,
                            ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);
    if (n <= ZS_M160_CH_BASE) {
        return;   /* ikke engang den faste del af modellen kom hjem */
    }

    /*
     * Antal kanaler staar paa offset 6.
     *
     * Evt lige foer er en bitfield32 og fylder BAADE offset 4 og 5.
     * Laeser man N paa offset 5, faar man den nederste halvdel af Evt,
     * som naesten altid er 0. Resultatet er "nul kanaler", og saa
     * forsvinder hele kanalgenkendelsen lydloest uden en eneste fejl
     * i loggen. Det er praecis den fejl der staar i Zbox-koden i dag.
     */
    uint16_t nch = fr->block[ZS_M160_N];
    if (nch == ZS_SS_NA_UINT16 || nch == 0 || nch > ZS_M160_MAX_CH) {
        /* Nogle firmwareversioner udfylder ikke N. Saa regner vi den ud
         * af modellens laengde i stedet, hvilket er lige saa paalideligt. */
        if (m->len > ZS_M160_CH_BASE) {
            nch = (uint16_t)((m->len - ZS_M160_CH_BASE) / ZS_M160_CH_SIZE);
        } else {
            nch = 0;
        }
        if (nch > ZS_M160_MAX_CH) {
            nch = ZS_M160_MAX_CH;
        }
    }
    if (nch == 0) {
        return;
    }

    int16_t dcw_sf = zs_ss_i16(fr->block[ZS_M160_DCW_SF]);
    bool sf_ok = zs_ss_sf_valid(dcw_sf);
    if (!sf_ok) {
        ZS_LOGW(TAG, "model 160 har ubrugelig skalafaktor for DCW (%d)", dcw_sf);
    }

    uint8_t count = 0;
    for (uint16_t ch = 0; ch < nch; ch++) {
        size_t base = (size_t)ZS_M160_CH_BASE + (size_t)ch * ZS_M160_CH_SIZE;
        if (base + ZS_M160_CH_SIZE > n) {
            break;   /* blokken blev ikke laest helt igennem */
        }
        zs_fr_channel_t *c = &live->channels[count];
        memset(c, 0, sizeof(*c));

        zs_ss_dec_string(fr->block, n, base + ZS_M160_CH_IDSTR,
                         ZS_M160_CH_IDSTR_LEN, c->label, sizeof(c->label));
        c->role = zs_ss_classify_channel(c->label);

        /* DCW er uint16 i SunSpec, ikke int16. Retningen ligger i hvilken
         * kanal det er, ikke i fortegnet. */
        uint16_t raw = fr->block[base + ZS_M160_CH_DCW];
        if (sf_ok && raw != ZS_SS_NA_UINT16) {
            c->dcw = zs_val(zs_ss_apply_sf((float)raw, dcw_sf));
        } else {
            c->dcw = ZS_VAL_NONE;
        }

        c->dcst   = zs_ss_dec_enum16(fr->block, n, base + ZS_M160_CH_DCST);
        c->active = zs_ss_channel_active(c->dcst);
        (void)zs_ss_dec_bitfield32(fr->block, n, base + ZS_M160_CH_DCEVT, &c->dcevt);
        count++;
    }
    live->channel_count = count;
}

/*
 * Regner solproduktion og batterieffekt ud af kanallisten.
 *
 * Beslutningen tages i denne raekkefoelge:
 *
 *  1. Navngiver inverteren sine kanaler, bruger vi navnene. Det er
 *     producent-uafhaengigt og virker ogsaa hvis nogen tilfoejer en
 *     tredje solstreng i morgen.
 *
 *  2. Ellers, hvis det er en Fronius med fire kanaler og batteri,
 *     bruger vi Fronius' egen dokumenterede opdeling: kanal 1 og 2 er
 *     solstrenge, kanal 3 er batteriets ladeside og kanal 4 dets
 *     afladeside.
 *
 *  3. Ellers, hvis der slet ikke er noget batteri, er alle kanaler sol.
 *
 *  4. Ellers giver vi op og siger "ingen data".
 *
 * Punkt 4 er vigtigt. Den gamle Zbox-formel "batteri = AC minus
 * solstreng 1" gaettede, og gaettede forkert to steder i marken: paa et
 * anlaeg uden solceller viste den 824 W spoegelses-sol, og paa et
 * anlaeg med to strenge tilskrev den streng 2's produktion til
 * batteriet. Vi viser hellere en aerlig streg end et forkert tal.
 */
static void compute_dc(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->solar_w   = ZS_VAL_NONE;
    live->battery_w = ZS_VAL_NONE;

    if (live->channel_count == 0) {
        return;
    }

    float pv = 0.0f,  chg = 0.0f, dis = 0.0f;
    bool  pv_seen = false, chg_seen = false, dis_seen = false;
    bool  any_role = false;

    for (uint8_t i = 0; i < live->channel_count; i++) {
        const zs_fr_channel_t *c = &live->channels[i];
        if (c->role != ZS_CH_UNKNOWN) {
            any_role = true;
        }
        if (!c->dcw.ok) {
            continue;
        }
        switch (c->role) {
        case ZS_CH_PV:
            pv_seen = true;
            /* Kun kanaler der faktisk sporer maksimalpunkt taeller med.
             * En frakoblet streng kan sagtens staa med en gammel vaerdi. */
            if (c->active) {
                pv += c->dcw.v;
            }
            break;
        case ZS_CH_BATTERY_CHARGE:
            chg_seen = true;
            if (!channel_is_dead(c->dcst)) {
                chg += c->dcw.v;
            }
            break;
        case ZS_CH_BATTERY_DISCHARGE:
            dis_seen = true;
            if (!channel_is_dead(c->dcst)) {
                dis += c->dcw.v;
            }
            break;
        default:
            break;
        }
    }

    fr->info.labels_usable = any_role;

    if (any_role) {
        if (pv_seen) {
            live->solar_w = zs_val(pv);
        }
        if (chg_seen || dis_seen) {
            /* Plus betyder aflader, minus betyder lader. Samme fortegn
             * som resten af huset ser det: batteriet leverer stroem. */
            live->battery_w = zs_val(dis - chg);
        }
        return;
    }

    /* Ingen brugbare navne. Er det en Fronius med fire kanaler og et
     * batteri, kender vi opdelingen fra Fronius' Modbus-manual
     * (42,0410,2649): "For devices with a storage solution, there are
     * two additional blocks (charging (MPP3) and discharging (MPP4))". */
    bool is_fronius = (strstr(fr->info.manufacturer, "Fronius") != NULL) ||
                      (strstr(fr->info.manufacturer, "FRONIUS") != NULL);

    /*
     * De TO SIDSTE kanaler er batteriets, resten er solstrenge.
     *
     * Fronius skriver det direkte i deres Modbus-manual (42,0410,2649):
     * "For devices with a storage solution, there are two additional
     * blocks (charging (MPP3) and discharging (MPP4))." De to blokke
     * laegges altsaa i enden, efter solstrengene.
     *
     * Vi taeller bagfra og ikke forfra. Et anlaeg med to strenge har
     * kanal 3 og 4 til batteriet, men et med ÉN streng har kanal 2 og 3.
     * Taeller man forfra og gaar ud fra fire kanaler, laeser man
     * solstreng nummer to som batteriets ladeside paa et anlaeg med
     * én streng.
     */
    if (is_fronius && fr->info.has_battery && live->channel_count >= 3) {
        uint8_t n_pv = (uint8_t)(live->channel_count - 2);

        float p = 0.0f;
        for (uint8_t i = 0; i < n_pv; i++) {
            const zs_fr_channel_t *c = &live->channels[i];
            if (c->dcw.ok && c->active) {
                p += c->dcw.v;
            }
        }
        live->solar_w = zs_val(p);

        const zs_fr_channel_t *c_chg = &live->channels[n_pv];
        const zs_fr_channel_t *c_dis = &live->channels[n_pv + 1];
        float g = 0.0f;
        bool  got = false;
        if (c_dis->dcw.ok && !channel_is_dead(c_dis->dcst)) { g += c_dis->dcw.v; got = true; }
        if (c_chg->dcw.ok && !channel_is_dead(c_chg->dcst)) { g -= c_chg->dcw.v; got = true; }
        if (got) {
            live->battery_w = zs_val(g);
        }
        return;
    }

    if (!fr->info.has_battery) {
        /* Ren solcelleinverter. Alle aktive kanaler er solstrenge. */
        float p = 0.0f;
        bool  got = false;
        for (uint8_t i = 0; i < live->channel_count; i++) {
            const zs_fr_channel_t *c = &live->channels[i];
            if (c->dcw.ok) {
                got = true;
                if (c->active) {
                    p += c->dcw.v;
                }
            }
        }
        if (got) {
            live->solar_w = zs_val(p);
        }
        return;
    }

    /* Der er et batteri, men vi kan ikke se hvilke kanaler der er hvad.
     * Her stopper vi hellere end at gaette. */
    ZS_LOGW(TAG, "batteri til stede, men kanalerne er unavngivne og passer "
                 "ikke paa Fronius-moenstret. Sol og batteri vises som ukendt.");
}

static void read_inverter(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->inverter_ac_w = ZS_VAL_NONE;
    live->grid_hz       = ZS_VAL_NONE;
    live->status_ok     = false;
    live->inverter_state = -1;
    live->vendor_state   = -1;

    const zs_ss_model_t *m = zs_ss_find_any(&fr->inv_map, INV_MODELS,
                                            sizeof(INV_MODELS) / sizeof(INV_MODELS[0]));
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(&fr->mb, fr->inverter_unit, m, fr->block,
                            ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);

    size_t o_st, o_stvnd, o_evt1, o_evt2, o_vnd1;

    if (model_is_float(m->id)) {
        if (n < ZS_M113_MIN_LEN) { return; }
        live->inverter_ac_w = zs_ss_dec_f32(fr->block, n, ZS_M113_W);
        live->grid_hz       = zs_ss_dec_f32(fr->block, n, ZS_M113_HZ);
        o_st = ZS_M113_ST; o_stvnd = ZS_M113_STVND;
        o_evt1 = ZS_M113_EVT1; o_evt2 = ZS_M113_EVT2; o_vnd1 = ZS_M113_EVTVND1;
    } else {
        if (n < ZS_M103_MIN_LEN) { return; }
        live->inverter_ac_w = zs_ss_dec_i16_sf(fr->block, n, ZS_M103_W, ZS_M103_W_SF);
        live->grid_hz       = zs_ss_dec_u16_sf(fr->block, n, ZS_M103_HZ, ZS_M103_HZ_SF);
        o_st = ZS_M103_ST; o_stvnd = ZS_M103_STVND;
        o_evt1 = ZS_M103_EVT1; o_evt2 = ZS_M103_EVT2; o_vnd1 = ZS_M103_EVTVND1;
    }

    /*
     * Tilstand og fejlflag.
     *
     * Hvert felt tjekkes for sig. En inverter behoever ikke udfylde de
     * producentspecifikke felter, og saa skal vi vise "ingen
     * oplysninger" i stedet for "ingen fejl". De to ting ligner
     * hinanden paa en skaerm, men betyder noget helt forskelligt for
     * den der staar og fejlsoeger.
     */
    live->inverter_state = zs_ss_dec_enum16(fr->block, n, o_st);
    live->vendor_state   = zs_ss_dec_enum16(fr->block, n, o_stvnd);

    bool fik_noget = (live->inverter_state >= 0);
    fik_noget |= zs_ss_dec_bitfield32(fr->block, n, o_evt1,     &live->evt1);
    (void)      zs_ss_dec_bitfield32(fr->block, n, o_evt2,     &live->evt2);
    (void)      zs_ss_dec_bitfield32(fr->block, n, o_vnd1,     &live->evtvnd1);
    (void)      zs_ss_dec_bitfield32(fr->block, n, o_vnd1 + 2, &live->evtvnd2);
    (void)      zs_ss_dec_bitfield32(fr->block, n, o_vnd1 + 4, &live->evtvnd3);
    (void)      zs_ss_dec_bitfield32(fr->block, n, o_vnd1 + 6, &live->evtvnd4);
    live->status_ok = fik_noget;
}

static void read_storage(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->soc_pct       = ZS_VAL_NONE;
    live->charge_status = -1;

    const zs_ss_model_t *m = zs_ss_find(&fr->inv_map, ZS_SS_STORAGE);
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(&fr->mb, fr->inverter_unit, m, fr->block,
                            ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);
    if (n < ZS_M124_MIN_LEN) {
        return;
    }
    zs_val_t soc = zs_ss_dec_u16_sf(fr->block, n, ZS_M124_CHA_STATE, ZS_M124_CHA_STATE_SF);
    if (soc.ok) {
        /* Klip til 0-100. Et batteri kan ikke vaere 103 procent fuldt,
         * og en skalafaktor der er lidt ved siden af maa ikke kunne
         * skubbe soejlen ud over kanten af kortet. */
        if (soc.v < 0.0f)   { soc.v = 0.0f; }
        if (soc.v > 100.0f) { soc.v = 100.0f; }
    }
    live->soc_pct       = soc;
    live->charge_status = zs_ss_dec_enum16(fr->block, n, ZS_M124_CHA_ST);
}

static void read_meter(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->grid_w = ZS_VAL_NONE;

    if (!fr->info.has_meter || fr->info.meter_model_id == 0) {
        return;
    }
    const zs_ss_map_t *map = fr->meter_in_inverter_chain ? &fr->inv_map : &fr->meter_map;
    const zs_ss_model_t *m = zs_ss_find(map, fr->info.meter_model_id);
    if (m == NULL) {
        return;
    }
    uint16_t n = read_model(&fr->mb, fr->info.meter_unit, m, fr->block,
                            ZS_FR_MAX_MODEL_REGS, TMO_NORMAL_MS);

    zs_val_t w;
    if (model_is_float(m->id)) {
        if (n < ZS_M213_MIN_LEN) { return; }
        w = zs_ss_dec_f32(fr->block, n, ZS_M213_W);
    } else {
        if (n < ZS_M203_MIN_LEN) { return; }
        w = zs_ss_dec_i16_sf(fr->block, n, ZS_M203_W, ZS_M203_W_SF);
    }

    /* Vend fortegnet hvis stroemtangen sidder omvendt. Se noten i
     * zs_fronius.h om hvorfor det er en indstilling og ikke en konstant. */
    if (w.ok && !fr->meter_import_positive) {
        w.v = -w.v;
    }
    live->grid_w = w;
}

/*
 * Husets forbrug.
 *
 *     forbrug = inverterens AC-effekt + det der koebes fra nettet
 *
 * Kontrolregning med de tre situationer der daekker alt:
 *
 *   Sol producerer 4200 W, huset bruger 1800, 2400 saelges.
 *     4200 + (-2400) = 1800 W. Rigtigt.
 *
 *   Nat, batteriet leverer 1000 W, huset bruger 1500, 500 koebes.
 *     1000 + 500 = 1500 W. Rigtigt.
 *
 *   Batteriet lades fra nettet med 2000 W, huset bruger 500.
 *     Inverteren traekker 2000 W fra AC, altsaa -2000. Der koebes 2500.
 *     -2000 + 2500 = 500 W. Rigtigt.
 *
 * Formlen forudsaetter at elmaaleren sidder ved nettilslutningen, som
 * er den normale Fronius-montering. Sidder den et andet sted, giver
 * den forkerte tal, og saa vil taelleren nedenfor loebe op.
 */
static void compute_house(zs_fr_t *fr, zs_fr_live_t *live)
{
    live->house_w = ZS_VAL_NONE;

    if (!live->inverter_ac_w.ok || !live->grid_w.ok) {
        return;
    }
    float house = live->inverter_ac_w.v + live->grid_w.v;

    /* Et hus kan ikke bruge negativ stroem. Sker det alligevel, og bliver
     * ved med at ske, sidder maalerens fortegn eller placering forkert.
     * Vi taeller det, viser det paa Detaljer-siden, og retter os IKKE
     * selv: en skaerm der vender fortegn af sig selv midt i en maaling
     * er vaerre end en der tager fejl konsekvent. */
    if (house < -50.0f) {
        fr->negative_house_count++;
    }
    if (house < 0.0f) {
        house = 0.0f;
    }
    live->house_w = zs_val(house);
}

bool zs_fr_poll(zs_fr_t *fr, zs_fr_live_t *live)
{
    if (fr == NULL || live == NULL) {
        return false;
    }
    memset(live, 0, sizeof(*live));
    live->charge_status = -1;
    live->solar_w = live->house_w = live->battery_w = ZS_VAL_NONE;
    live->soc_pct = live->grid_w = live->inverter_ac_w = live->grid_hz = ZS_VAL_NONE;

    if (!zs_fr_is_connected(fr)) {
        return false;
    }
    fr->poll_count++;

    /* Sekventielt, aldrig parallelt. Fronius' manual siger det direkte,
     * og selv hvis den ikke gjorde, deler alle fire kald den samme
     * arbejdsbuffer. */
    read_inverter(fr, live);
    read_storage(fr, live);
    read_channels(fr, live);
    read_meter(fr, live);

    compute_dc(fr, live);
    compute_house(fr, live);

    if (!zs_mb_is_open(&fr->mb)) {
        fr->connected = false;
        fr->poll_error_count++;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Undersoegelse under netvaerksscanning                               */
/* ------------------------------------------------------------------ */

bool zs_fr_probe(const char *host, uint16_t port, uint32_t timeout_ms,
                 zs_fr_info_t *info)
{
    if (host == NULL || info == NULL) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    if (timeout_ms == 0) {
        timeout_ms = TMO_PROBE_MS;
    }
    if (port == 0) {
        port = ZS_MB_DEFAULT_PORT;
    }

    zs_mb_t mb;
    zs_mb_init(&mb);
    mb.timeout_ms = timeout_ms;
    if (zs_mb_connect(&mb, host, port, timeout_ms) != ZS_MB_OK) {
        return false;
    }

    bool ok = false;
    zs_ss_map_t map;
    walk_ctx_t wc = { .mb = &mb, .timeout_ms = timeout_ms };

    if (zs_ss_walk(walk_read, &wc, 1, &map)) {
        /* Kun identiteten. Resten venter til brugeren har valgt enheden,
         * saa en scanning over 254 adresser ikke tager evigheder. */
        uint16_t buf[ZS_M1_MIN_LEN + 2];
        read_identity(&mb, 1, &map, info, buf,
                      (uint16_t)(sizeof(buf) / sizeof(buf[0])), timeout_ms);

        const zs_ss_model_t *inv = zs_ss_find_any(&map, INV_MODELS,
                                                  sizeof(INV_MODELS) / sizeof(INV_MODELS[0]));
        info->has_inverter      = (inv != NULL);
        info->inverter_model_id = inv ? inv->id : 0;
        info->has_battery       = (zs_ss_find(&map, ZS_SS_STORAGE) != NULL);
        info->has_mppt          = (zs_ss_find(&map, ZS_SS_MPPT) != NULL);

        /* Vi vil kun vise invertere paa listen. En elmaaler der ligger
         * alene paa sin egen IP er ikke noget brugeren skal vaelge. */
        ok = info->has_inverter;
    }

    zs_mb_close(&mb);
    return ok;
}

const char *zs_fr_charge_status_text(int32_t chast)
{
    switch (chast) {
    case ZS_CHAST_OFF:          return "Slukket";
    case ZS_CHAST_EMPTY:        return "Tomt";
    case ZS_CHAST_DISCHARGING:  return "Aflader";
    case ZS_CHAST_CHARGING:     return "Lader";
    case ZS_CHAST_FULL:         return "Fuldt";
    case ZS_CHAST_HOLDING:      return "Holder";
    case ZS_CHAST_TESTING:      return "Tester";
    default:                    return "Ukendt";
    }
}
