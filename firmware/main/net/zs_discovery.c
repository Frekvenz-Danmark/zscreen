/*
 * Pusten mellem to connect-kald laves med select, ikke med usleep.
 *
 * Tre steder skal kunne oversaette den her fil, og de tre er uenige:
 *
 *   usleep       blev FJERNET af POSIX i 2008 og er skjult paa glibc med
 *                -std=c11. Byggede paa Mac og paa ESP32, ikke paa Linux
 *   nanosleep    kraever _POSIX_C_SOURCE for at vaere erklaeret, OG den
 *                findes slet ikke i ESP-IDF's newlib. Byggede paa Linux,
 *                ikke paa ESP32
 *   select       er der alle tre steder, bruges allerede i den her fil,
 *                og kraever ingen feature-makro overhovedet
 *
 * Alle tre udfald er maalt, ikke gaettet: i en gcc-beholder og med en
 * rigtig firmware-oversaettelse.
 */
#include "zs_discovery.h"
#include "zs_modbus_tcp.h"
#include "zs_config.h"
#include "../zs_log.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *TAG = "discovery";

static volatile bool s_abort = false;
static volatile bool s_was_aborted = false;

void zs_discovery_abort(void)
{
    s_abort = true;
}

bool zs_discovery_was_aborted(void)
{
    return s_was_aborted;
}

/*
 * Proever mange adresser paa én gang.
 *
 * En efter en ville tage 254 gange den tid vi venter paa hver. Med et
 * kvart sekunds taalmodighed er det over et minut, og saa staar
 * kunden og kigger paa en bjaelke der ikke rykker sig.
 *
 * I stedet aabner vi et bundt sockets uden at vente paa hver enkelt,
 * og spoerger med select() hvem der er kommet igennem. Hele
 * undernettet er saa klaret paa omkring fem sekunder.
 *
 * Antallet er sat efter hvor mange sockets lwIP har (16 i vores
 * opsaetning, se sdkconfig.defaults). Vi bruger ikke dem alle: der
 * skal vaere plads til den forbindelse der allerede laeser fra
 * inverteren, hvis der er én.
 */
static int probe_batch(const char base[static 12], int first, int count,
                       uint16_t port,
                       bool *alive)
{
    /*
     * Begge felter nulstilles FOERST. Loekken nedenfor kan stoppe
     * tidligt, og loekken laengere nede laeser dem alligevel.
     */
    int fds[ZS_SCAN_PARALLEL];
    for (int i = 0; i < ZS_SCAN_PARALLEL; i++) {
        fds[i] = -1;
        alive[i] = false;
    }

    /*
     * Hver adresse proeves op til ZS_SCAN_TRIES gange, og anden runde
     * roerer kun dem der ikke svarede. Se noten ved ZS_SCAN_TRIES om
     * hvorfor det, og ikke bare en laengere pause, er rettelsen.
     */
    for (int runde = 0; runde < ZS_SCAN_TRIES; runde++) {
        int n = 0;
        for (int i = 0; i < count && n < ZS_SCAN_PARALLEL; i++) {
            if (alive[i]) {
                continue;   /* svarede allerede */
            }
            char ip[16];
            /*
             * %.11s og ikke %s.
             *
             * base er en peger nu, ikke et array, saa oversaetteren kan
             * ikke laengere SE at raekken hoejst er elleve tegn
             * ("255.255.255"). Uden graensen her antager den 191 og
             * standser byggeriet med en advarsel om afkortning, og den
             * har ret i at den ikke kan vide det. Elleve plus punktum
             * plus tre cifre plus afslutning er praecis 16.
             */
            snprintf(ip, sizeof(ip), "%.11s.%d", base, first + i);

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
                continue;
            }

            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (fd < 0) {
                /* Uden den her linje ligner en soegning der ikke fandt
                 * noget, en soegning der gik godt. */
                ZS_LOGW(TAG, "ingen ledig socket til %s", ip);
                continue;
            }
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            if (rc == 0) {
                alive[i] = true;      /* kom igennem med det samme */
                close(fd);
            } else if (errno == EINPROGRESS) {
                fds[i] = fd;
                n++;
            } else {
                close(fd);
            }

            /* Pust mellem hvert kald, se ZS_SCAN_CONNECT_GAP_MS. */
            if (i + 1 < count) {
                struct timeval pust = {
                    .tv_sec  = ZS_SCAN_CONNECT_GAP_MS / 1000,
                    .tv_usec = (ZS_SCAN_CONNECT_GAP_MS % 1000) * 1000,
                };
                select(0, NULL, NULL, NULL, &pust);
            }
        }

        if (n == 0) {
            break;   /* intet at vente paa */
        }

        fd_set wset;
        FD_ZERO(&wset);
        int maxfd = -1;
        for (int i = 0; i < count; i++) {
            if (fds[i] >= 0) {
                FD_SET(fds[i], &wset);
                if (fds[i] > maxfd) { maxfd = fds[i]; }
            }
        }
        struct timeval tv = {
            .tv_sec  = ZS_SCAN_PORT_TIMEOUT_MS / 1000,
            .tv_usec = (ZS_SCAN_PORT_TIMEOUT_MS % 1000) * 1000,
        };
        select(maxfd + 1, NULL, &wset, NULL, &tv);

        for (int i = 0; i < count; i++) {
            if (fds[i] < 0) {
                continue;
            }
            if (FD_ISSET(fds[i], &wset)) {
                /* select siger skrivbar, men det goer den ogsaa naar
                 * forbindelsen blev afvist. Det rigtige svar ligger i
                 * SO_ERROR. Springer man det over, tror man at alle
                 * 254 adresser har en inverter. */
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                if (getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &soerr, &sl) == 0
                    && soerr == 0) {
                    alive[i] = true;
                }
            }
            close(fds[i]);
            fds[i] = -1;
        }
    }

    int hits = 0;
    for (int i = 0; i < count; i++) {
        if (alive[i]) { hits++; }
    }
    return hits;
}

size_t zs_discovery_blokke(const char *egen_ip, uint8_t praefiks,
                           char ud[][12], size_t maks)
{
    if (egen_ip == NULL || ud == NULL || maks == 0) {
        return 0;
    }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(egen_ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return 0;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return 0;
    }
    /* Et praefiks vi ikke forstaar behandles som et almindeligt /24.
     * Hellere ét hug det rigtige sted end mange de forkerte. */
    if (praefiks == 0 || praefiks > 32) {
        praefiks = 24;
    }

    /* Vores EGEN raekke foerst. Der er inverteren naesten altid, og en
     * soegning der finder den paa de foerste sekunder er en anden
     * oplevelse end en der finder den efter fire minutter. */
    snprintf(ud[0], 12, "%u.%u.%u", a, b, c);
    size_t n = 1;

    if (praefiks >= 24 || maks == 1) {
        return n;                 /* et almindeligt hjemmenet */
    }

    /*
     * Resten af nettet, raekke for raekke.
     *
     * Antallet af raekker i et net er 2 opløftet i (24 minus praefiks):
     * et /23 har to, et /20 har seksten, et /16 har 256. Vi tager dem
     * fra nettets foerste og opad og springer vores egen over, saa der
     * ikke scannes dobbelt.
     */
    unsigned raekker = 1u << (24 - praefiks);
    unsigned maske_c = (unsigned)(~(raekker - 1)) & 0xFFu;
    unsigned foerste_c = c & maske_c;

    for (unsigned i = 0; i < raekker && n < maks; i++) {
        unsigned denne = foerste_c + i;
        if (denne > 255) {
            break;
        }
        if (denne == c) {
            continue;             /* vores egen, og den er allerede med */
        }
        snprintf(ud[n], 12, "%u.%u.%u", a, b, denne);
        n++;
    }
    return n;
}

static bool already_found(const zs_found_t *out, size_t n, const char *ip)
{
    for (size_t i = 0; i < n; i++) {
        if (strcmp(out[i].ip, ip) == 0) {
            return true;
        }
    }
    return false;
}

int zs_discovery_scan(const char *egen_ip, uint8_t praefiks,
                      const char *prefer, uint16_t port,
                      zs_found_t *out, size_t max,
                      zs_discovery_progress_fn progress, void *ctx)
{
    /* Nul betyder standarden. Saa behoever kalderen ikke kende tallet,
     * og en gemt indstilling der aldrig blev sat virker alligevel. */
    if (port == 0) {
        port = ZS_MB_DEFAULT_PORT;
    }
    if (out == NULL || max == 0) {
        return -1;
    }

    char blokke[ZS_SCAN_MAX_BLOKKE][12];
    size_t n_blokke = zs_discovery_blokke(egen_ip, praefiks, blokke,
                                          ZS_SCAN_MAX_BLOKKE);
    if (n_blokke == 0) {
        ZS_LOGE(TAG, "kan ikke laese vores egen adresse: %s",
                egen_ip ? egen_ip : "(ingen)");
        return -1;
    }

    s_abort = false;
    s_was_aborted = false;
    size_t found = 0;
    const int total = (int)(254 * n_blokke);
    int done = 0;

    /* Trin 1: den adresse vi kender i forvejen. */
    if (prefer != NULL && prefer[0] != '\0') {
        ZS_LOGI(TAG, "prøver den kendte adresse %s først", prefer);
        if (zs_mb_probe_port(prefer, port, ZS_SCAN_PORT_TIMEOUT_MS)) {
            zs_fr_info_t info;
            if (zs_fr_probe(prefer, port,
                            ZS_SCAN_SUNSPEC_TIMEOUT_MS, &info)) {
                snprintf(out[found].ip, sizeof(out[found].ip), "%s", prefer);
                out[found].info = info;
                found++;
                ZS_LOGI(TAG, "fandt %s %s paa den kendte adresse",
                        info.manufacturer, info.model);
            }
        }
    }

    /* Trin 2 og 3: gennemgaa undernettet. */

    /*
     * Raekke for raekke. Vores egen foerst, se zs_discovery_blokke.
     *
     * Paa et almindeligt /24 er der kun én, og saa er det her praecis som
     * foer. Paa et /20 er der seksten, og saa leder vi videre i stedet
     * for at melde at der ingen inverter var.
     */
    for (size_t blok = 0; blok < n_blokke && found < max && !s_abort; blok++) {
        const char *base = blokke[blok];
        if (blok > 0) {
            ZS_LOGI(TAG, "videre til raekken %s.x", base);
        }

        for (int first = 1; first <= 254 && found < max; first += ZS_SCAN_PARALLEL) {
            if (s_abort) {
                ZS_LOGI(TAG, "søgningen blev afbrudt");
                s_was_aborted = true;
                break;
            }
            int count = ZS_SCAN_PARALLEL;
            if (first + count - 1 > 254) {
                count = 254 - first + 1;
            }

            bool alive[ZS_SCAN_PARALLEL];
            probe_batch(base, first, count, port, alive);

            for (int i = 0; i < count && found < max; i++) {
                if (!alive[i]) {
                    continue;
                }
                char ip[16];
                /*
             * %.11s og ikke %s.
             *
             * base er en peger nu, ikke et array, saa oversaetteren kan
             * ikke laengere SE at raekken hoejst er elleve tegn
             * ("255.255.255"). Uden graensen her antager den 191 og
             * standser byggeriet med en advarsel om afkortning, og den
             * har ret i at den ikke kan vide det. Elleve plus punktum
             * plus tre cifre plus afslutning er praecis 16.
             */
            snprintf(ip, sizeof(ip), "%.11s.%d", base, first + i);
                if (already_found(out, found, ip)) {
                    continue;
                }

                /* Noget lytter paa Modbus-porten. Er det en inverter? */
                zs_fr_info_t info;
                if (zs_fr_probe(ip, port,
                                ZS_SCAN_SUNSPEC_TIMEOUT_MS, &info)) {
                    snprintf(out[found].ip, sizeof(out[found].ip), "%s", ip);
                    out[found].info = info;
                    found++;
                    ZS_LOGI(TAG, "fandt %s %s paa %s",
                            info.manufacturer, info.model, ip);
                } else {
                    ZS_LOGD(TAG, "%s lytter paa 502, men taler ikke SunSpec", ip);
                }
            }

            done += count;
            if (progress != NULL) {
                progress(ctx, done, total, (int)found);
            }
        }
    }

    if (progress != NULL) {
        progress(ctx, total, total, (int)found);
    }
    ZS_LOGI(TAG, "søgning færdig: %u inverter%s fundet",
            (unsigned)found, found == 1 ? "" : "e");
    return (int)found;
}
