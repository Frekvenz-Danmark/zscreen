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
#include "../../firmware/main/app/zs_status.h"
#include "../../firmware/main/net/zs_locate.h"
/* Selv om zs_discovery.h ogsaa traekker den ind, henter vi den her:
 * vi bruger ZS_SCAN_SUNSPEC_TIMEOUT_MS direkte, og saa skal det staa. */
#include "../../firmware/main/zs_config.h"

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

/*
 * Skriver "4,2 kW" eller "-" hoejrestillet i en kolonne paa KOL tegn.
 *
 * Baade bredde og praecision er sat til KOL. Bredden alene er kun et
 * mindstemaal, saa en lang vaerdi ville skubbe resten af raekken ud af
 * flugt uden at nogen opdagede det. Med praecisionen med bliver feltet
 * praecis lige bredt hver gang.
 */
#define KOL     10

static void put_power(char *dst, size_t n, zs_val_t v)
{
    if (!v.ok) {
        snprintf(dst, n, "%*.*s", KOL, KOL, "-");
        return;
    }
    zs_num_t f;
    zs_fmt_power(v.v, &f);
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%s %s", f.value, f.unit);
    snprintf(dst, n, "%*.*s", KOL, KOL, tmp);
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
        printf("    %-*s %-*s %-*s %-*s\n",
               KOL, "SOLCELLER", KOL, "FORBRUG", KOL, "BATTERI", KOL, "NETTET");
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

    printf("    %s %s %s %s\n", sol, hus, bat, net);
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

static void scan_frem(void *ctx, int done, int total, int found)
{
    (void) ctx;
    /* Samme linje hver gang, saa der ikke ruller 254 linjer forbi. */
    printf("\r    %d af %d adresser, %d fundet   ", done, total, found);
    fflush(stdout);
}

static int do_scan(const char *subnet_base, uint16_t port)
{
    /*
     * subnet_base er fx "192.168.1.0". Vi proever .1 til .254.
     *
     * Argumentet kommer fra kommandolinjen, saa det kan vaere hvad som
     * helst. Vi afviser det der ikke ligner en IPv4-adresse i stedet
     * for at klippe det af i stilhed: en afklippet adresse ville faa
     * vaerktoejet til at scanne et helt andet netvaerk end det brugeren
     * skrev, og det er svaert at gennemskue bagefter.
     */
    char base[16];      /* "255.255.255.255" plus afslutning */
    if (subnet_base == NULL || strlen(subnet_base) >= sizeof(base)) {
        fprintf(stderr, "Det ligner ikke en adresse. Skriv fx: "
                        "--scan 192.168.1.0\n");
        return 1;
    }
    for (const char *c = subnet_base; *c != '\0'; c++) {
        if ((*c < '0' || *c > '9') && *c != '.') {
            fprintf(stderr, "Adressen må kun indeholde tal og punktummer. "
                            "Skriv fx: --scan 192.168.1.0\n");
            return 1;
        }
    }
    snprintf(base, sizeof(base), "%s", subnet_base);
    char *last = strrchr(base, '.');
    if (last == NULL) {
        fprintf(stderr, "Skriv fx: --scan 192.168.1.0\n");
        return 1;
    }
    *last = '\0';

    /*
     * HER KOERER FIRMWARENS EGEN MOTOR, og det er hele pointen.
     *
     * Foer stod der en loekke herinde som proevede én adresse ad gangen
     * med sin egen taalmodighed paa 200 ms. Den lignede firmwarens
     * soegning, men var den ikke: firmwaren aabner tolv sockets ad
     * gangen, venter 250 ms, og proever hver adresse to gange.
     *
     * Det er den slags forskel der gaar ud over en kunde. Melder
     * skaermen at der ikke er nogen inverter, og finder vaerktoejet den
     * saa alligevel, har vi ikke fundet fejlen, vi har bare maalt to
     * forskellige ting. Derfor kalder vi nu zs_discovery_scan, saa
     * vaerktoejet og skaermen soeger ens, ogsaa naar tallene bliver
     * rettet i zs_config.h.
     *
     * Vi giver den ".1" som vores egen adresse og et /24, saa den
     * gennemgaar praecis den raekke brugeren skrev, og ikke mere.
     */
    char egen_ip[20];
    snprintf(egen_ip, sizeof(egen_ip), "%s.1", base);

    /* Nul betyder standarden, praecis som i firmwaren. --port findes, og
     * uden den her ledning blev den ignoreret ved en soegning: saa kunne
     * en inverter paa en anden port ikke findes med vaerktoejet, selv om
     * skaermen kan. */
    if (port == 0) {
        port = ZS_MB_DEFAULT_PORT;
    }

    printf("\n  Scanner %s.1 til %s.254 paa port %u ...\n\n",
           base, base, (unsigned)port);

    zs_found_t fundet[ZS_DISCOVERY_MAX];
    int n = zs_discovery_scan(egen_ip, 24, NULL, port,
                              fundet, ZS_DISCOVERY_MAX, scan_frem, NULL);
    printf("\r%60s\r", "");     /* ryd fremgangslinjen */

    if (n < 0) {
        fprintf(stderr, "  Søgningen kunne ikke starte.\n\n");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        printf("    %-16s %s %s%s%s\n", fundet[i].ip,
               fundet[i].info.manufacturer[0] ? fundet[i].info.manufacturer
                                              : "(ukendt)",
               fundet[i].info.model,
               fundet[i].info.serial[0] ? "  serienr " : "",
               fundet[i].info.serial);
    }
    printf("\n  %d inverter%s fundet.\n", n, n == 1 ? "" : "e");
    if (n == 0) {
        /* Den gamle udgave skrev en linje per vaert der havde port 502
         * aaben uden at tale SunSpec. Den slags hoerer til paa én
         * adresse ad gangen, hvor der er plads til at vise hvorfor:
         * koer "zs-probe <adresse>" paa den du har mistanke til. */
        printf("  Har du en mistanke om en bestemt adresse, så kør\n"
               "  zs-probe <adresse> og se hvad den svarer.\n");
    }
    printf("\n");
    return n > 0 ? 0 : 1;
}

/*
 * Proever genfindingsmotoren mod et rigtigt net, med firmwarens EGEN kode.
 *
 *   --genfind <undernet> [serienummer] [sidste-ip]
 *
 * Uden serienummer svarer det til en ny skaerm. Med et serienummer der
 * IKKE findes paa nettet svarer det til at vores inverter er vaek og
 * naboens svarer: saa skal den sige fra og ikke tage den der var.
 *
 * Exitkoden er udfaldet som tal, saa en test kan laese det uden at skulle
 * lede i teksten.
 */
static int do_genfind(const char *egen_ip, uint8_t praefiks,
                      const char *serial, const char *sidste, uint16_t port)
{
    char ip[16] = {0};
    char sn[33] = {0};

    printf("\n  Leder fra %s/%u", egen_ip, (unsigned)praefiks);
    if (serial != NULL && serial[0] != '\0') {
        printf(", efter serienummer %s", serial);
    } else {
        printf(", uden at kende et serienummer");
    }
    if (sidste != NULL && sidste[0] != '\0') {
        printf(", sidst set paa %s", sidste);
    }
    printf("\n");

    zs_loc_t r = zs_locate_find(egen_ip, praefiks, serial, sidste,
                                port, 1,
                                ip, sizeof(ip), sn, sizeof(sn), NULL, NULL);
    printf("  Resultat: %s\n", zs_locate_text(r));
    if (ip[0] != '\0') {
        printf("  Adresse:  %s\n", ip);
        printf("  Serienr:  %s\n", sn[0] ? sn : "(tomt)");
    }
    printf("\n");
    return (int) r;
}

/*
 * Skriver ét register og laeser tilbage. Kun til afproevning mod
 * simulatoren.
 *
 *   --skriv <adresse> <vaerdi>
 *
 * Den findes fordi Fronius IKKE melder fejl naar en skrivning bliver
 * afvist. Et paent svar beviser ingenting, saa hele pointen er at se at
 * tilbagelaesningen fanger det.
 */
static int do_skriv(const char *host, uint16_t port, uint8_t unit,
                    uint16_t adresse, uint16_t vaerdi)
{
    static zs_mb_t mb;
    zs_mb_init(&mb);
    if (zs_mb_connect(&mb, host, port, 2000) != ZS_MB_OK) {
        fprintf(stderr, "\n  Kunne ikke forbinde til %s:%u\n\n", host, port);
        return 1;
    }

    uint16_t foer = 0;
    bool har_foer = (zs_mb_read_holding(&mb, unit, adresse, 1, &foer, 2000) == ZS_MB_OK);

    printf("\n  Skriver %u i register %u paa %s:%u unit %u\n",
           (unsigned)vaerdi, (unsigned)adresse, host, port, unit);
    if (har_foer) {
        printf("  Foer:     %u\n", (unsigned)foer);
    }

    zs_mb_err_t err = zs_mb_write_verified(&mb, unit, adresse, &vaerdi, 1, 2000);
    uint16_t efter = 0;
    if (zs_mb_read_holding(&mb, unit, adresse, 1, &efter, 2000) == ZS_MB_OK) {
        printf("  Efter:    %u\n", (unsigned)efter);
    }
    printf("  Resultat: %s\n\n", err == ZS_MB_OK ? "SKREVET OG BEKRAEFTET"
                                                  : zs_mb_strerror(err));
    zs_mb_close(&mb);
    return err == ZS_MB_OK ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *host = NULL;
    uint16_t port = ZS_MB_DEFAULT_PORT;
    uint8_t unit = 1;
    bool watch = false;
    const char *scan = NULL;
    const char *genfind = NULL;
    long skriv_adr = -1, skriv_val = -1;
    /* Paa en almindelig maskine kender vi ikke netmasken, saa den gives
     * med. 24 er det almindelige, og det er hvad testene bruger. */
    unsigned praefiks = 24;
    const char *g_serial = NULL;
    const char *g_sidste = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--watch") == 0) {
            watch = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            zs_log_verbose = 1;
        } else if (strcmp(argv[i], "--scan") == 0 && i + 1 < argc) {
            scan = argv[++i];
        } else if (strcmp(argv[i], "--genfind") == 0 && i + 1 < argc) {
            /* Eksplicitte flag til resten. Foer tog --genfind to frie
             * argumenter, og saa afhang det af raekkefoelgen om et
             * serienummer blev opsamlet eller forvekslet med en vaert.
             * Et flag kan ikke staa forkert. */
            genfind = argv[++i];
        } else if (strcmp(argv[i], "--serienr") == 0 && i + 1 < argc) {
            g_serial = argv[++i];
        } else if (strcmp(argv[i], "--sidste") == 0 && i + 1 < argc) {
            g_sidste = argv[++i];
        } else if (strcmp(argv[i], "--praefiks") == 0 && i + 1 < argc) {
            praefiks = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--skriv") == 0 && i + 2 < argc) {
            skriv_adr = atol(argv[++i]);
            skriv_val = atol(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            /* Eksplicit, fordi --genfind tager to frie argumenter og
             * ellers sluger et portnummer som om det var et serienummer. */
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            unit = (uint8_t)atoi(argv[++i]);
        } else if (host == NULL) {
            host = argv[i];
        } else {
            port = (uint16_t)atoi(argv[i]);
        }
    }

    if (genfind != NULL) {
        return do_genfind(genfind, praefiks, g_serial, g_sidste, port);
    }
    if (skriv_adr >= 0) {
        if (skriv_adr > 65535 || skriv_val < 0 || skriv_val > 65535) {
            fprintf(stderr, "\n  Adresse og vaerdi skal vaere 0 til 65535\n\n");
            return 2;
        }
        return do_skriv(host != NULL ? host : "127.0.0.1", port, unit,
                        (uint16_t)skriv_adr, (uint16_t)skriv_val);
    }
    if (scan != NULL) {
        return do_scan(scan, port);
    }
    if (host == NULL) {
        fprintf(stderr,
            "\nBrug:\n"
            "  zs-probe <ip> [port] [--unit N] [--watch] [-v]\n"
            "  zs-probe --scan 192.168.1.0\n"
            "  zs-probe --genfind <egen-ip> [--praefiks N] [--serienr S]\n"
            "                     [--sidste IP] [--port N]\n"
            "  zs-probe <ip> [port] --skriv <adresse> <vaerdi>\n\n"
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

            /* Inverterens tilstand og fejl, som side 3 ville vise dem. */
            zs_status_list_t st;
            zs_status_build(&st, &lv);
            printf("\n  Tilstand og fejl:\n");
            printf("    %-22s %s\n", "Sammenfatning:",
                   zs_status_summary(&st, &lv));
            printf("    %-22s %s\n", "Inverteren:",
                   zs_status_state_text(lv.inverter_state));
            if (!st.har_svar) {
                printf("    (inverteren udfylder ikke statusfelterne)\n");
            } else if (st.antal == 0) {
                printf("    Ingen meldinger.\n");
            } else {
                for (uint8_t i = 0; i < st.antal; i++) {
                    static const char *sev[] = { "ok", "info", "advarsel", "FEJL" };
                    printf("    [%-8s] %-40s %s\n",
                           sev[st.poster[i].sev],
                           st.poster[i].tekst, st.poster[i].detalje);
                }
                /*
                 * Der var flere end der er plads til.
                 *
                 * Flaget blev sat af zs_status_build men aldrig laest, saa
                 * en inverter med mange samtidige fejl viste de foerste
                 * fjorten og tav om resten. Det er praecis den fejl modulets
                 * egen header advarer imod: at tie om noget fordi der ikke
                 * var plads. En montoer kunne rette de fjorten og gaa hjem
                 * mens aarsagen stod paa plads femten.
                 */
                if (st.afkortet) {
                    printf("    ... og FLERE end der var plads til "
                           "(hoejst %d vises)\n", ZS_STATUS_MAX);
                }
            }
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
