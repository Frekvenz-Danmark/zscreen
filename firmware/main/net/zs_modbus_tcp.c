/*
 * zScreen - Modbus TCP klient, kun laesning. Se zs_modbus_tcp.h.
 */

#include "zs_modbus_tcp.h"
#include "../zs_log.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static const char *TAG = "modbus";

/* Funktionskode 3, den eneste vi bruger. */
#define FC_READ_HOLDING     0x03
/* Serveren saetter hoejeste bit naar den svarer med en fejl. */
#define FC_EXCEPTION_BIT    0x80

/* MBAP-headerens faste felter. */
#define MBAP_LEN            6   /* tid(2) + pid(2) + len(2)                */
#define PDU_MAX             253 /* unit(1) + fc(1) + bytecount(1) + 250    */

/* ------------------------------------------------------------------ */
/* Hjaelpere                                                           */
/* ------------------------------------------------------------------ */

static inline uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

static inline void wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

const char *zs_mb_strerror(zs_mb_err_t err)
{
    switch (err) {
    case ZS_MB_OK:            return "OK";
    case ZS_MB_ERR_ARG:       return "Ugyldig forespørgsel";
    case ZS_MB_ERR_NOT_OPEN:  return "Ingen forbindelse";
    case ZS_MB_ERR_CONNECT:   return "Kunne ikke forbinde";
    case ZS_MB_ERR_SEND:      return "Kunne ikke sende";
    case ZS_MB_ERR_TIMEOUT:   return "Inverteren svarede ikke i tide";
    case ZS_MB_ERR_CLOSED:    return "Forbindelsen blev lukket";
    case ZS_MB_ERR_FRAME:     return "Ugyldigt svar fra inverteren";
    case ZS_MB_ERR_EXCEPTION: return "Inverteren afviste forespørgslen";
    }
    return "Ukendt fejl";
}

/* ------------------------------------------------------------------ */
/* Rammer. Ingen sockets her, saa enhedstestene kan naa dem direkte.   */
/* ------------------------------------------------------------------ */

size_t zs_mb_build_read_request(uint8_t *buf, size_t buf_len, uint16_t tid,
                                uint8_t unit_id, uint16_t address, uint16_t count)
{
    if (buf == NULL || buf_len < 12) {
        return 0;
    }
    if (count == 0 || count > ZS_MB_MAX_REGS) {
        return 0;
    }
    /* Adresserummet er 16 bit. En laesning maa ikke loebe ud over kanten.
     * Uden dette tjek ville fx address=65535 count=10 blive sendt afsted
     * og give et svar vi ikke kan stole paa. */
    if ((uint32_t)address + (uint32_t)count > 0x10000u) {
        return 0;
    }

    wr_u16(buf + 0, tid);       /* transaktions-ID                */
    wr_u16(buf + 2, 0);         /* protokol-ID, altid 0 for Modbus */
    wr_u16(buf + 4, 6);         /* laengde af resten: unit + fc + addr + count */
    buf[6] = unit_id;
    buf[7] = FC_READ_HOLDING;
    wr_u16(buf + 8, address);
    wr_u16(buf + 10, count);
    return 12;
}

zs_mb_err_t zs_mb_parse_read_response(const uint8_t *frame, size_t frame_len,
                                      uint16_t expect_tid, uint8_t expect_unit,
                                      uint16_t expect_count, uint16_t *out,
                                      uint8_t *out_exception)
{
    if (out_exception) {
        *out_exception = 0;
    }
    if (frame == NULL || out == NULL) {
        return ZS_MB_ERR_ARG;
    }
    if (expect_count == 0 || expect_count > ZS_MB_MAX_REGS) {
        return ZS_MB_ERR_ARG;
    }
    /* Korteste lovlige svar er en exception: MBAP(7) + fc(1) + kode(1). */
    if (frame_len < 9) {
        ZS_LOGW(TAG, "svar for kort: %u bytes", (unsigned)frame_len);
        return ZS_MB_ERR_FRAME;
    }

    uint16_t tid = rd_u16(frame + 0);
    uint16_t pid = rd_u16(frame + 2);
    uint16_t len = rd_u16(frame + 4);
    uint8_t  unit = frame[6];
    uint8_t  fc  = frame[7];

    if (pid != 0) {
        ZS_LOGW(TAG, "protokol-ID var %u, forventede 0", pid);
        return ZS_MB_ERR_FRAME;
    }
    /* Laengdefeltet daekker alt efter de foerste 6 bytes. Passer det ikke
     * med hvad vi rent faktisk modtog, er stroemmen ude af trit. */
    if ((size_t)len + MBAP_LEN != frame_len) {
        ZS_LOGW(TAG, "laengdefelt %u passer ikke med %u modtagne bytes",
                len, (unsigned)frame_len);
        return ZS_MB_ERR_FRAME;
    }
    /* Transaktions-ID skal matche. Fordi vi lukker forbindelsen ved enhver
     * timeout, kan der aldrig ligge et gammelt svar og vente i roeret. Et
     * forkert ID er derfor en aegte fejl, ikke bare et efternoeler. */
    if (tid != expect_tid) {
        ZS_LOGW(TAG, "transaktions-ID %u, forventede %u", tid, expect_tid);
        return ZS_MB_ERR_FRAME;
    }
    if (unit != expect_unit) {
        ZS_LOGW(TAG, "unit-ID %u, forventede %u", unit, expect_unit);
        return ZS_MB_ERR_FRAME;
    }

    if (fc == (FC_READ_HOLDING | FC_EXCEPTION_BIT)) {
        uint8_t code = frame[8];
        if (out_exception) {
            *out_exception = code;
        }
        /* Exception 2 (ulovlig adresse) er helt normal under
         * SunSpec-vandring og under scanning. Derfor kun debug. */
        ZS_LOGD(TAG, "exception-kode %u", code);
        return ZS_MB_ERR_EXCEPTION;
    }
    if (fc != FC_READ_HOLDING) {
        ZS_LOGW(TAG, "funktionskode 0x%02X, forventede 0x03", fc);
        return ZS_MB_ERR_FRAME;
    }

    uint8_t bc = frame[8];
    if (bc != (uint8_t)(expect_count * 2)) {
        ZS_LOGW(TAG, "byte-taeller %u, forventede %u", bc, expect_count * 2);
        return ZS_MB_ERR_FRAME;
    }
    /* Baelte og seler: bc er allerede bundet af laengdetjekket ovenfor,
     * men vi laeser aldrig ud over bufferen uden at have set efter. */
    if (frame_len < (size_t)9 + bc) {
        return ZS_MB_ERR_FRAME;
    }

    for (uint16_t i = 0; i < expect_count; i++) {
        out[i] = rd_u16(frame + 9 + i * 2);
    }
    return ZS_MB_OK;
}

/* ------------------------------------------------------------------ */
/* Socket-lag                                                          */
/* ------------------------------------------------------------------ */

void zs_mb_init(zs_mb_t *mb)
{
    if (mb == NULL) {
        return;
    }
    memset(mb, 0, sizeof(*mb));
    mb->fd = -1;
    mb->timeout_ms = 1500;
}

bool zs_mb_is_open(const zs_mb_t *mb)
{
    return mb != NULL && mb->fd >= 0;
}

void zs_mb_close(zs_mb_t *mb)
{
    if (mb == NULL || mb->fd < 0) {
        return;
    }
    close(mb->fd);
    mb->fd = -1;
}

/* Slaar de socket-indstillinger til vi har brug for.
 * Fejl her er ikke fatale: mangler en platform en af dem, koerer vi
 * videre med lidt daarligere opfoersel frem for slet ingen forbindelse. */
static void apply_socket_options(int fd, uint32_t timeout_ms)
{
    /* Modbus-forespoergsler er 12 bytes. Uden TCP_NODELAY samler Nagle
     * dem op og venter, hvilket giver 40 ms ekstra pr. kald. */
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    /* Keepalive saa vi opdager en doed inverter i stedet for at haenge
     * i OS'ets standard paa to timer. */
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
#ifdef TCP_KEEPIDLE
    int idle = 30, intvl = 5, cnt = 3;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif

    /* SO_LINGER med tid 0 goer at close() sender RST i stedet for FIN.
     * Det er ikke paent, men det er noedvendigt: en Fronius Gen24 holder
     * ellers sin side af forbindelsen aaben i 5 til 15 minutter efter en
     * uventet genstart, og afviser imens nye forbindelser. Det er
     * dokumenteret i Zbox-koden efter at have ramt det i marken. */
    struct linger lg = { .l_onoff = 1, .l_linger = 0 };
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));

#ifdef SO_NOSIGPIPE
    /* macOS: undgaa at processen doer af SIGPIPE naar modparten lukker. */
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

    struct timeval tv;
    tv.tv_sec  = (long)(timeout_ms / 1000u);
    tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/* Forbinder med en rigtig timeout. Standard-connect() paa en blokerende
 * socket kan haenge i over et minut, og det maa en scanning over 254
 * adresser ikke goere. Derfor: saet non-blocking, start connect, vent med
 * select, saet tilbage. */
static int connect_with_timeout(const char *host, uint16_t port, uint32_t timeout_ms)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    /* Bevidst kun IPv4-litteraler, ingen DNS. Skaermen kender kun
     * IP-adresser: enten fra netvaerksscanningen eller fra det
     * taltastatur brugeren skriver dem paa. At undgaa getaddrinfo
     * betyder ogsaa at ingen scanning kan haenge paa en DNS-server. */
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc != 0) {
        if (errno != EINPROGRESS) {
            close(fd);
            return -1;
        }
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv;
        tv.tv_sec  = (long)(timeout_ms / 1000u);
        tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);

        rc = select(fd + 1, NULL, &wset, NULL, &tv);
        if (rc <= 0) {
            /* 0 = timeout, under 0 = fejl. Begge dele: giv op. */
            close(fd);
            return -1;
        }
        /* select siger skrivbar, men det siger den ogsaa naar forbindelsen
         * blev afvist. Det rigtige svar ligger i SO_ERROR. Springer man
         * dette over, tror man man er forbundet til alle 254 adresser. */
        int soerr = 0;
        socklen_t slen = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0) {
            close(fd);
            return -1;
        }
    }

    if (fcntl(fd, F_SETFL, flags) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

zs_mb_err_t zs_mb_connect(zs_mb_t *mb, const char *host, uint16_t port,
                          uint32_t connect_timeout_ms)
{
    if (mb == NULL || host == NULL || host[0] == '\0' || port == 0) {
        return ZS_MB_ERR_ARG;
    }
    if (strlen(host) >= sizeof(mb->host)) {
        return ZS_MB_ERR_ARG;
    }
    if (connect_timeout_ms == 0) {
        connect_timeout_ms = 1500;
    }

    /* Luk foerst. En halvdoed socket maa ikke overleve et reconnect. */
    zs_mb_close(mb);

    int fd = connect_with_timeout(host, port, connect_timeout_ms);
    if (fd < 0) {
        return ZS_MB_ERR_CONNECT;
    }

    apply_socket_options(fd, mb->timeout_ms ? mb->timeout_ms : 1500);

    mb->fd = fd;
    snprintf(mb->host, sizeof(mb->host), "%s", host);
    mb->port = port;
    /* Transaktions-ID starter paa 1. Nul er lovligt, men et logfilsudtraek
     * hvor alt staar 0 er ubrugeligt til fejlsoegning. */
    if (mb->next_tid == 0) {
        mb->next_tid = 1;
    }
    ZS_LOGI(TAG, "forbundet til %s:%u", host, port);
    return ZS_MB_OK;
}

/* Sender hele bufferen. send() maa gerne tage mindre end vi bad om. */
static zs_mb_err_t send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, buf + sent, len - sent, 0);
#endif
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EINTR)) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return ZS_MB_ERR_TIMEOUT;
        }
        return ZS_MB_ERR_SEND;
    }
    return ZS_MB_OK;
}

/* Laeser praecis len bytes. TCP er en stroem uden rammer, saa et Modbus-svar
 * kan sagtens komme i to stumper. Uden denne loekke ville vi laese en halv
 * ramme og kalde den ugyldig. Det er en klassiker. */
static zs_mb_err_t recv_exact(int fd, uint8_t *buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, buf + got, len - got, 0);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if (n == 0) {
            return ZS_MB_ERR_CLOSED;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ZS_MB_ERR_TIMEOUT;
        }
        return ZS_MB_ERR_CLOSED;
    }
    return ZS_MB_OK;
}

zs_mb_err_t zs_mb_read_holding(zs_mb_t *mb, uint8_t unit_id, uint16_t address,
                               uint16_t count, uint16_t *out, uint32_t timeout_ms)
{
    if (mb == NULL || out == NULL) {
        return ZS_MB_ERR_ARG;
    }
    if (count == 0 || count > ZS_MB_MAX_REGS) {
        return ZS_MB_ERR_ARG;
    }
    if (mb->fd < 0) {
        return ZS_MB_ERR_NOT_OPEN;
    }

    if (timeout_ms == 0) {
        timeout_ms = mb->timeout_ms ? mb->timeout_ms : 1500;
    }
    if (timeout_ms != mb->timeout_ms) {
        struct timeval tv;
        tv.tv_sec  = (long)(timeout_ms / 1000u);
        tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
        setsockopt(mb->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(mb->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        mb->timeout_ms = timeout_ms;
    }

    uint16_t tid = mb->next_tid++;
    if (mb->next_tid == 0) {
        mb->next_tid = 1;   /* spring 0 over, se zs_mb_connect */
    }

    uint8_t req[12];
    size_t req_len = zs_mb_build_read_request(req, sizeof(req), tid, unit_id, address, count);
    if (req_len == 0) {
        return ZS_MB_ERR_ARG;
    }

    mb->stat_requests++;

    zs_mb_err_t err = send_all(mb->fd, req, req_len);
    if (err != ZS_MB_OK) {
        mb->stat_errors++;
        zs_mb_close(mb);
        return err;
    }

    /* Foerst de 6 bytes der fortaeller hvor langt resten er. */
    uint8_t frame[ZS_MB_MAX_FRAME];
    err = recv_exact(mb->fd, frame, MBAP_LEN);
    if (err != ZS_MB_OK) {
        mb->stat_errors++;
        zs_mb_close(mb);
        return err;
    }

    uint16_t len = rd_u16(frame + 4);
    /* Mindst unit + fc + kode = 3, hoejst unit + fc + bc + 250 = 253.
     * Uden denne graense kunne et fjendtligt svar bede os laese
     * 65535 bytes ind i en buffer paa 259. */
    if (len < 3 || len > PDU_MAX) {
        ZS_LOGW(TAG, "urimeligt laengdefelt %u, lukker forbindelsen", len);
        mb->stat_errors++;
        zs_mb_close(mb);
        return ZS_MB_ERR_FRAME;
    }
    if (MBAP_LEN + (size_t)len > sizeof(frame)) {
        mb->stat_errors++;
        zs_mb_close(mb);
        return ZS_MB_ERR_FRAME;
    }

    err = recv_exact(mb->fd, frame + MBAP_LEN, len);
    if (err != ZS_MB_OK) {
        mb->stat_errors++;
        zs_mb_close(mb);
        return err;
    }

    err = zs_mb_parse_read_response(frame, MBAP_LEN + (size_t)len, tid, unit_id,
                                    count, out, &mb->last_exception);
    if (err != ZS_MB_OK) {
        mb->stat_errors++;
        /* En exception er serveren der siger "det register har jeg ikke".
         * Forbindelsen er stadig sund og stroemmen i trit, saa den beholder
         * vi. Alt andet betyder at vi ikke laengere kan stole paa hvad der
         * kommer ud af roeret, og saa lukker vi. */
        if (err != ZS_MB_ERR_EXCEPTION) {
            zs_mb_close(mb);
        }
        return err;
    }
    return ZS_MB_OK;
}

bool zs_mb_probe_port(const char *host, uint16_t port, uint32_t timeout_ms)
{
    if (host == NULL || port == 0) {
        return false;
    }
    int fd = connect_with_timeout(host, port, timeout_ms ? timeout_ms : 300);
    if (fd < 0) {
        return false;
    }
    /* Vi vil kun vide om der lytter noget. Luk med RST saa modparten ikke
     * bliver haengende med en halvaaben forbindelse efter en scanning
     * over 254 adresser. */
    struct linger lg = { .l_onoff = 1, .l_linger = 0 };
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    close(fd);
    return true;
}
