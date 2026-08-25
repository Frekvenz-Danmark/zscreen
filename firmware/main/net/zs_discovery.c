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
                       bool *alive)
{
    /*
     * Begge felter nulstilles FOERST.
     *
     * Loekken nedenfor har to betingelser, og hvis den anden stopper
     * den tidligt, ville de resterende pladser aldrig blive udfyldt.
     * Loekken laengere nede laeser dem alligevel. I dag kan det ikke
     * ske, fordi count aldrig er stoerre end ZS_SCAN_PARALLEL, men det
     * er en fejl der venter paa at nogen aendrer den ene af de to.
     */
    int fds[ZS_SCAN_PARALLEL];
    for (int i = 0; i < ZS_SCAN_PARALLEL; i++) {
        fds[i] = -1;
        alive[i] = false;
    }
    int n = 0;

    for (int i = 0; i < count && n < ZS_SCAN_PARALLEL; i++) {
        char ip[16];
        snprintf(ip, sizeof(ip), "%s.%d", base, first + i);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ZS_MB_DEFAULT_PORT);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            fds[i] = -1;
            continue;
        }

        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0) {
            fds[i] = -1;
            continue;
        }
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == 0) {
            alive[i] = true;      /* kom igennem med det samme */
            close(fd);
            fds[i] = -1;
        } else if (errno == EINPROGRESS) {
            fds[i] = fd;
            n++;
        } else {
            close(fd);
            fds[i] = -1;
        }
    }

    if (n > 0) {
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

/*
 * Klipper "192.168.1.0" ned til "192.168.1".
 *
 * Resultatet er hoejst 11 tegn ("255.255.255"), og derfor er base
 * netop 12 bytes. Det er ikke smaaligt: naar der bagefter skrives
 * "%s.%d" ind i en buffer paa 16, skal oversaetteren kunne SE at det
 * ikke kan loebe over. Var base 16 bytes, kunne den ikke, og saa
 * standser den byggeriet med en advarsel om afkortning.
 *     11 + 1 + 3 + afslutning = 16. Det passer praecis.
 */
static bool split_subnet(const char *subnet, char *base, size_t len)
{
    if (subnet == NULL || base == NULL || len == 0) {
        return false;
    }
    const char *dot = strrchr(subnet, '.');
    if (dot == NULL) {
        return false;
    }
    size_t n = (size_t)(dot - subnet);
    if (n == 0 || n >= len) {
        return false;
    }
    memcpy(base, subnet, n);
    base[n] = '\0';
    return true;
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

int zs_discovery_scan(const char *subnet, const char *prefer,
                      zs_found_t *out, size_t max,
                      zs_discovery_progress_fn progress, void *ctx)
{
    if (out == NULL || max == 0) {
        return -1;
    }
    char base[12];
    if (!split_subnet(subnet, base, sizeof(base))) {
        ZS_LOGE(TAG, "ugyldigt undernet: %s", subnet ? subnet : "(ingen)");
        return -1;
    }

    s_abort = false;
    s_was_aborted = false;
    size_t found = 0;
    const int total = 254;
    int done = 0;

    /* Trin 1: den adresse vi kender i forvejen. */
    if (prefer != NULL && prefer[0] != '\0') {
        ZS_LOGI(TAG, "prøver den kendte adresse %s først", prefer);
        if (zs_mb_probe_port(prefer, ZS_MB_DEFAULT_PORT, ZS_SCAN_PORT_TIMEOUT_MS)) {
            zs_fr_info_t info;
            if (zs_fr_probe(prefer, ZS_MB_DEFAULT_PORT,
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
        probe_batch(base, first, count, alive);

        for (int i = 0; i < count && found < max; i++) {
            if (!alive[i]) {
                continue;
            }
            char ip[16];
            snprintf(ip, sizeof(ip), "%s.%d", base, first + i);
            if (already_found(out, found, ip)) {
                continue;
            }

            /* Noget lytter paa Modbus-porten. Er det en inverter? */
            zs_fr_info_t info;
            if (zs_fr_probe(ip, ZS_MB_DEFAULT_PORT,
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

    if (progress != NULL) {
        progress(ctx, total, total, (int)found);
    }
    ZS_LOGI(TAG, "søgning færdig: %u inverter%s fundet",
            (unsigned)found, found == 1 ? "" : "e");
    return (int)found;
}
