/*
 * zScreen - zs-probe.
 *
 * Koerer PRAECIS den samme C-kode som firmwaren (Modbus, SunSpec,
 * Fronius-beregningen, dansk talformatering), men paa en Mac i stedet
 * for paa skaermen. Den findes af to grunde:
 *
 *   1. Vi kan proeve hele datavejen af mod simulatoren uden at have
 *      hardware fremme, og se de fire tal skaermen ville have vist.
 *   2. Naar der en dag staar et rigtigt anlaeg, kan man pege den paa
 *      inverteren og se hvad zScreen laeser, uden at flashe noget.
 *
 * Kun laesning. Der findes ingen skrivekode i noget af det her.
 *
 *     ./zs-probe 192.168.1.50
 *     ./zs-probe 192.168.1.50 502 --watch
 *     ./zs-probe --scan 192.168.1.0
 */

#include "../../firmware/main/net/zs_fronius.h"
#include "../../firmware/main/app/zs_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int zs_log_verbose = 0;

static const char *role_name(zs_ch_role_t r)
{
    switch (r) {
    case ZS_CH_PV:                return "solstreng";
    case ZS_CH_BATTERY_CHARGE:    return "batteri lade";
    case ZS_CH_BATTERY_DISCHARGE: return "batteri aflade";
    default:                      return "ukendt";
    }
}

static const char *dcst_name(int32_t s)
{
    switch (s) {
    case 1: return "slukket";
    case 2: return "sover";
    case 3: return "starter";
    case 4: return "sporer";
    case 5: return "begraenset";
    case 6: return "lukker ned";
    case 7: return "fejl";
    case 8: return "standby";
    case 9: return "test";
    default: return "ukendt";
    }
}

/* Skriver "4,2 kW" eller "-" i et fast bredt felt. */
static void put_power(char *dst, size_t n, zs_val_t v)
{
    if (!v.ok) {
        snprintf(dst, n, "%9s", "-");
        return;
    }
    zs_num_t f;
    zs_fmt_power(v.v, &f);
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%s %s", f.value, f.unit);
    snprintf(dst, n, "%9s", tmp);
}

static void print_info(const zs_fr_t *fr)
{
    printf("\n");
    printf("  %s %s\n", fr->info.manufacturer[0] ? fr->info.manufacturer : "(ukendt)",
           fr->info.model);
    printf("  ----------------------------------------------------------\n");
    printf("  Adresse         %s:%u, unit %u\n", fr->host, fr->port, fr->inverter_unit);
    printf("  Firmware        %s\n", fr->info.version);
    printf("  Serienummer     %s\n", fr->info.serial);
    printf("  Invertermodel   %u%s\n", fr->info.inverter_model_id,
           fr->info.inverter_model_id >= 111 ? " (float)" : " (heltal + skalafaktor)");
    if (fr->info.has_meter) {
        printf("  Elmaaler        model %u paa unit %u%s\n",
               fr->info.meter_model_id, fr->info.meter_unit,
               fr->meter_in_inverter_chain ? " (i inverterens egen kaede)" : "");
    } else {
        printf("  Elmaaler        ingen fundet\n");
    }
    printf("  Batteri         %s", fr->info.has_battery ? "ja" : "nej");
    if (fr->info.battery_capacity_kwh > 0.0f) {
        printf(", %.2f kWh", (double)fr->info.battery_capacity_kwh);
    }
    printf("\n");
    if (fr->info.inverter_rated_kw > 0.0f) {
        printf("  Maerkeeffekt    %.1f kW\n", (double)fr->info.inverter_rated_kw);
    }
    printf("  SunSpec         base %u, %u modeller:", fr->inv_map.base, fr->inv_map.count);
    for (uint8_t i = 0; i < fr->inv_map.count; i++) {
        printf(" %u", fr->inv_map.models[i].id);
    }
    printf("%s\n", fr->inv_map.truncated ? " (ufuldstaendig)" : "");
    printf("  ----------------------------------------------------------\n");
}

static void print_live(const zs_fr_t *fr, const zs_fr_live_t *lv, bool header)
{
    if (header) {
        printf("\n  Det skaermen ville vise:\n\n");
        printf("    %-10s %-10s %-10s %-10s\n", "SOLCELLER", "FORBRUG", "BATTERI", "NETTET");
    }
    char sol[16], hus[16], bat[16], net[16];
    put_power(sol, sizeof(sol), lv->solar_w);
    put_power(hus, sizeof(hus), lv->house_w);
    put_power(bat, sizeof(bat), lv->battery_w);
    put_power(net, sizeof(net), lv->grid_w);

    char soc[16] = "        -";
    if (lv->soc_pct.ok) {
        zs_num_t f;
        zs_fmt_percent(lv->soc_pct.v, &f);
        snprintf(soc, sizeof(soc), "%8s%s", f.value, f.unit);
    }

    const char *bdir = "";
    if (lv->battery_w.ok) {
        bdir = (lv->battery_w.v < -20.0f) ? "lader"
             : (lv->battery_w.v >  20.0f) ? "aflader" : "hviler";
    }
    const char *gdir = "";
    if (lv->grid_w.ok) {
        gdir = (lv->grid_w.v < -20.0f) ? "saelger"
             : (lv->grid_w.v >  20.0f) ? "koeber" : "i balance";
    }

    printf("    %s  %s  %s  %s\n", sol, hus, bat, net);
    printf("    %-10s %-10s %-9s%-2s %s\n", "", "", soc, "", "");
    printf("    %-10s %-10s %-11s %s\n", "", "", bdir, gdir);
}

static void print_channels(const zs_fr_live_t *lv)
{
    if (lv->channel_count == 0) {
        printf("\n  Ingen DC-kanaler.\n");
        return;
    }
    printf("\n  DC-kanaler (model 160):\n");
    printf("    %-3s %-18s %-16s %-12s %s\n", "nr", "navn fra inverter", "rolle", "effekt", "tilstand");
    for (uint8_t i = 0; i < lv->channel_count; i++) {
        const zs_fr_channel_t *c = &lv->channels[i];
        char w[16];
        put_power(w, sizeof(w), c->dcw);
        printf("    %-3u %-18s %-16s %-12s %s%s\n",
               i + 1,
               c->label[0] ? c->label : "(uden navn)",
               role_name(c->role),
               w,
               dcst_name(c->dcst),
               c->active ? "" : "  (taeller ikke med)");
    }
}

static int do_scan(const char *subnet_base)
{
    /* subnet_base er fx "192.168.1.0". Vi proever .1 til .254. */
    char base[46];
    snprintf(base, sizeof(base), "%s", subnet_base);
    char *last = strrchr(base, '.');
    if (last == NULL) {
        fprintf(stderr, "Skriv fx: --scan 192.168.1.0\n");
        return 1;
    }
    *last = '\0';

    printf("\n  Scanner %s.1 til %s.254 paa port 502 ...\n\n", base, base);
    int found = 0;
    for (int host = 1; host <= 254; host++) {
        char ip[46];
        snprintf(ip, sizeof(ip), "%s.%d", base, host);
        if (!zs_mb_probe_port(ip, ZS_MB_DEFAULT_PORT, 200)) {
            continue;
        }
        zs_fr_info_t info;
        if (zs_fr_probe(ip, ZS_MB_DEFAULT_PORT, 800, &info)) {
            printf("    %-16s %s %s%s%s\n", ip,
                   info.manufacturer[0] ? info.manufacturer : "(ukendt)",
                   info.model,
                   info.serial[0] ? "  serienr " : "",
                   info.serial);
            found++;
        } else {
            printf("    %-16s port 502 er aaben, men taler ikke SunSpec\n", ip);
        }
    }
    printf("\n  %d inverter%s fundet.\n\n", found, found == 1 ? "" : "e");
    return found > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *host = NULL;
    uint16_t port = ZS_MB_DEFAULT_PORT;
    uint8_t unit = 1;
    bool watch = false;
    const char *scan = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--watch") == 0) {
            watch = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            zs_log_verbose = 1;
        } else if (strcmp(argv[i], "--scan") == 0 && i + 1 < argc) {
            scan = argv[++i];
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            unit = (uint8_t)atoi(argv[++i]);
        } else if (host == NULL) {
            host = argv[i];
        } else {
            port = (uint16_t)atoi(argv[i]);
        }
    }

    if (scan != NULL) {
        return do_scan(scan);
    }
    if (host == NULL) {
        fprintf(stderr,
            "\nBrug:\n"
            "  zs-probe <ip> [port] [--unit N] [--watch] [-v]\n"
            "  zs-probe --scan 192.168.1.0\n\n"
            "Eksempler:\n"
            "  zs-probe 127.0.0.1 5020          laes én gang fra simulatoren\n"
            "  zs-probe 192.168.1.50 --watch    foelg et rigtigt anlaeg\n\n");
        return 2;
    }

    static zs_fr_t fr;   /* static: 400+ bytes, hoerer ikke hjemme paa stakken */
    zs_fr_init(&fr);

    printf("\n  Forbinder til %s:%u ...\n", host, port);
    if (!zs_fr_connect(&fr, host, port, unit)) {
        fprintf(stderr, "\n  Kunne ikke laese SunSpec fra %s:%u.\n"
                        "  Er Modbus TCP slaaet til paa inverteren?\n\n", host, port);
        return 1;
    }
    print_info(&fr);

    zs_fr_live_t lv;
    bool first = true;
    do {
        if (!zs_fr_poll(&fr, &lv)) {
            fprintf(stderr, "\n  Aflaesningen faejlede. Forbindelsen er lukket.\n\n");
            zs_fr_disconnect(&fr);
            return 1;
        }
        if (watch && !first) {
            printf("\n");
        }
        print_live(&fr, &lv, first || !watch);
        if (first) {
            print_channels(&lv);
            printf("\n  Batteriets tilstand: %s\n",
                   zs_fr_charge_status_text(lv.charge_status));
            if (lv.grid_hz.ok) {
                printf("  Netfrekvens:         %.2f Hz\n", (double)lv.grid_hz.v);
            }
            if (lv.inverter_ac_w.ok) {
                printf("  Inverterens AC:      %.0f W\n", (double)lv.inverter_ac_w.v);
            }
            printf("\n");
        }
        first = false;
        if (watch) {
            sleep(2);
        }
    } while (watch);

    if (fr.negative_house_count > 0) {
        printf("  ADVARSEL: forbruget blev udregnet negativt %u gange.\n"
               "  Elmaalerens fortegn eller placering er sandsynligvis omvendt.\n\n",
               fr.negative_house_count);
    }

    zs_fr_disconnect(&fr);
    return 0;
}
