#include "zs_demo.h"
#include "zs_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * Et doegn paa tre minutter. Hurtigt nok til at man ser hele forloebet
 * mens man staar og kigger, langsomt nok til at tallene ikke flakker.
 */
#define DAY_SECONDS      86400.0f
#define DEMO_SPEED       (DAY_SECONDS / 180.0f)   /* 480 gange normal tid */

#define PV_PEAK_W        7000.0f
#define BATTERY_WH       10240.0f
#define BATTERY_MAX_W    5000.0f
#define MIN_RESERVE_PCT  5.0f
/* Batteriet rammer ikke sit maal oejeblikkeligt. Uden den forsinkelse
 * ville nettet staa paa praecis nul hele natten, og saa fik man aldrig
 * set hverken koeb eller salg paa skaermen. */
#define BATTERY_TAU_S    90.0f

static float s_day_s      = 13.0f * 3600.0f;   /* start midt paa dagen */
static float s_soc        = 48.0f;
static float s_battery_w  = 0.0f;
static float s_drift      = 0.0f;
static uint32_t s_rand    = 12345u;
static char  s_clock[8]   = "13:00";

/* Lille generator med fast startvaerdi. Vi vil have noget der ligner
 * stoej, ikke rigtig tilfaeldighed, og den skal opfoere sig ens hver
 * gang saa en fejl kan gentages. */
static float noise(float amplitude)
{
    s_rand = s_rand * 1664525u + 1013904223u;
    float u = (float)((s_rand >> 8) & 0xFFFF) / 65535.0f;   /* 0 til 1 */
    return (u * 2.0f - 1.0f) * amplitude;
}

void zs_demo_reset(void)
{
    s_day_s = 13.0f * 3600.0f;
    s_soc = 48.0f;
    s_battery_w = 0.0f;
    s_drift = 0.0f;
    s_rand = 12345u;
    snprintf(s_clock, sizeof(s_clock), "13:00");
}

/* Solkurve. Nul foer klokken 5 og efter 21, top ved middag. */
static float solar_at(float hour)
{
    if (hour < 5.0f || hour > 21.0f) {
        return 0.0f;
    }
    float x = (hour - 5.0f) / 16.0f;
    float base = powf(sinf((float)M_PI * x), 1.6f);
    return base > 0.0f ? PV_PEAK_W * base : 0.0f;
}

/* Grundforbrug plus morgen- og aftenspids. */
static float house_at(float hour)
{
    float w = 280.0f;
    float m = hour - 7.5f;
    float a = hour - 18.0f;
    w += 1400.0f * expf(-(m * m) / 2.0f);
    w += 2100.0f * expf(-(a * a) / 3.0f);
    return w;
}

void zs_demo_step(zs_fr_live_t *live, uint32_t dt_ms)
{
    if (live == NULL) {
        return;
    }
    memset(live, 0, sizeof(*live));
    live->charge_status = -1;

    float dt = ((float)dt_ms / 1000.0f) * DEMO_SPEED;
    if (dt <= 0.0f) {
        dt = 1.0f;
    }
    s_day_s += dt;
    while (s_day_s >= DAY_SECONDS) {
        s_day_s -= DAY_SECONDS;
    }
    float hour = s_day_s / 3600.0f;

    int hh = (int)hour;
    int mm = (int)((hour - (float)hh) * 60.0f);
    snprintf(s_clock, sizeof(s_clock), "%02d:%02d", hh, mm);

    /* ── sol, fordelt paa to strenge ── */
    float sun = solar_at(hour);
    float pv1 = sun * 0.55f;
    float pv2 = sun * 0.45f;
    float solar = pv1 + pv2;

    /* ── forbrug ── */
    s_drift += noise(30.0f);
    if (s_drift < -250.0f) { s_drift = -250.0f; }
    if (s_drift >  250.0f) { s_drift =  250.0f; }
    float house = house_at(hour) + s_drift + noise(60.0f);
    if (house < 80.0f) {
        house = 80.0f;
    }

    /* ── batteri, med forsinkelse ── */
    float surplus = solar - house;
    float target = 0.0f;
    if (surplus > 50.0f && s_soc < 99.9f) {
        target = -(surplus < BATTERY_MAX_W ? surplus : BATTERY_MAX_W);
    } else if (surplus < -50.0f && s_soc > MIN_RESERVE_PCT) {
        float need = -surplus;
        target = (need < BATTERY_MAX_W ? need : BATTERY_MAX_W);
    }
    float alpha = 1.0f - expf(-dt / BATTERY_TAU_S);
    s_battery_w += (target - s_battery_w) * alpha;

    /* Ladetilstanden integreres, 95 procents virkningsgrad hver vej. */
    float wh = s_battery_w * (dt / 3600.0f);
    wh = (s_battery_w < 0.0f) ? wh * 0.95f : wh / 0.95f;
    s_soc -= (wh / BATTERY_WH) * 100.0f;
    if (s_soc < 0.0f)   { s_soc = 0.0f; }
    if (s_soc > 100.0f) { s_soc = 100.0f; }

    if (s_soc <= MIN_RESERVE_PCT && s_battery_w > 0.0f) { s_battery_w = 0.0f; }
    if (s_soc >= 100.0f && s_battery_w < 0.0f)          { s_battery_w = 0.0f; }

    /* ── resten foelger af de tre ovenfor ── */
    float inverter_ac = solar + s_battery_w;
    float grid = house - solar - s_battery_w;

    live->solar_w       = zs_val(solar);
    live->house_w       = zs_val(house);
    live->battery_w     = zs_val(s_battery_w);
    live->soc_pct       = zs_val(s_soc);
    live->grid_w        = zs_val(grid);
    live->inverter_ac_w = zs_val(inverter_ac);
    live->grid_hz       = zs_val(50.0f);

    /*
     * Tilstand og fejl, saa side 3 ogsaa kan ses uden et anlaeg.
     *
     * Om eftermiddagen mellem 13 og 15 melder demoen at effekten er
     * saenket paa grund af varme. Det er ikke en opfundet fejl: det er
     * EvtVnd2 bit 7 i Fronius' egen liste, og det er praecis den
     * melding et anlaeg giver paa en varm sommerdag. Resten af doegnet
     * er der ingen meldinger, saa begge tilstande kan ses.
     */
    live->status_ok = true;
    live->inverter_state = (solar > 50.0f) ? 4 : 2;   /* producerer / sover */
    live->vendor_state = 0;
    live->evt1 = live->evt2 = 0;
    live->evtvnd1 = live->evtvnd2 = live->evtvnd3 = live->evtvnd4 = 0;

    if (hour >= 13.0f && hour < 15.0f) {
        live->evtvnd2 |= (1u << 7);      /* effekt saenket paa grund af varme */
        live->inverter_state = 5;        /* begraenset */
        live->vendor_state = 307;        /* Fronius' egen kode i det omraade */
    }

    if (s_soc >= 99.9f)                  { live->charge_status = ZS_CHAST_FULL; }
    else if (s_soc <= MIN_RESERVE_PCT)   { live->charge_status = ZS_CHAST_EMPTY; }
    else if (s_battery_w < -20.0f)       { live->charge_status = ZS_CHAST_CHARGING; }
    else if (s_battery_w >  20.0f)       { live->charge_status = ZS_CHAST_DISCHARGING; }
    else                                 { live->charge_status = ZS_CHAST_HOLDING; }

    /* Kanalerne, saa Detaljer-siden ogsaa har noget at vise. */
    struct { const char *navn; float w; } ch[4] = {
        { "MPPT 1",   pv1 },
        { "MPPT 2",   pv2 },
        { "STCHA",    s_battery_w < 0.0f ? -s_battery_w : 0.0f },
        { "STDISCHA", s_battery_w > 0.0f ?  s_battery_w : 0.0f },
    };
    for (int i = 0; i < 4; i++) {
        snprintf(live->channels[i].label, sizeof(live->channels[i].label),
                 "%s", ch[i].navn);
        live->channels[i].role   = zs_ss_classify_channel(ch[i].navn);
        live->channels[i].dcw    = zs_val(ch[i].w);
        live->channels[i].dcst   = (ch[i].w > 5.0f) ? ZS_DCST_MPPT : ZS_DCST_SLEEPING;
        live->channels[i].active = (ch[i].w > 5.0f);
    }
    live->channel_count = 4;
}

void zs_demo_info(zs_fr_info_t *info)
{
    if (info == NULL) {
        return;
    }
    memset(info, 0, sizeof(*info));
    snprintf(info->manufacturer, sizeof(info->manufacturer), "Fronius");
    snprintf(info->model,        sizeof(info->model),        "Symo GEN24 10.0");
    snprintf(info->version,      sizeof(info->version),      "demo");
    snprintf(info->serial,       sizeof(info->serial),       "DEMO");

    info->has_inverter        = true;
    info->has_meter           = true;
    info->has_battery         = true;
    info->has_mppt            = true;
    info->labels_usable       = true;
    info->inverter_model_id   = 103;
    info->meter_model_id      = 203;
    info->meter_unit          = 200;
    info->channel_count       = 4;
    info->battery_capacity_kwh = BATTERY_WH / 1000.0f;
    info->inverter_rated_kw   = 10.0f;
}

void zs_demo_price(zs_price_day_t *ud)
{
    if (ud == NULL) {
        return;
    }
    memset(ud, 0, sizeof(*ud));
    ud->ok = true;
    snprintf(ud->zone, sizeof(ud->zone), "DK2");
    snprintf(ud->dato, sizeof(ud->dato), "demo");
    ud->antal = 24;

    /*
     * Doegnkurven. To toppe, morgen og aften, og en bund midt paa
     * dagen hvor solen producerer mest. Tallene er i samme
     * stoerrelsesorden som en almindelig dag paa spotmarkedet.
     */
    float sum = 0.0f;
    for (uint8_t h = 0; h < 24; h++) {
        float x = (float)h;
        float m = x - 7.5f;
        float a = x - 19.0f;
        float d = x - 13.0f;
        float pris = 1.05f
                   + 0.45f * expf(-(m * m) / 6.0f)     /* morgenspids  */
                   + 0.60f * expf(-(a * a) / 5.0f)     /* aftenspids   */
                   - 0.50f * expf(-(d * d) / 14.0f);   /* midt paa dagen */
        if (pris < 0.05f) {
            pris = 0.05f;
        }
        ud->timer[h].hour = h;
        ud->timer[h].dkk = pris;
        sum += pris;
    }
    ud->gennemsnit = sum / 24.0f;
    ud->billigst = 0;
    ud->dyrest = 0;
    for (uint8_t i = 1; i < 24; i++) {
        if (ud->timer[i].dkk < ud->timer[ud->billigst].dkk) { ud->billigst = i; }
        if (ud->timer[i].dkk > ud->timer[ud->dyrest].dkk)   { ud->dyrest = i; }
    }
    /* Den fremhaevede time foelger demoens eget ur, ikke maskinens. */
    int h = (int)(s_day_s / 3600.0f);
    ud->nu = (int8_t)((h >= 0 && h < 24) ? h : 0);
}

const char *zs_demo_clock(void)
{
    return s_clock;
}
