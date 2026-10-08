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
#define FC_WRITE_MULTI      0x10    /* skriv flere registre        */
/* Serveren saetter hoejeste bit naar den svarer med en fejl. */
#define FC_EXCEPTION_BIT    0x80

/* MBAP-headerens faste felter. */
#define MBAP_LEN            6   /* tid(2) + pid(2) + len(2)                */
#define PDU_MAX             253 /* unit(1) + fc(1) + bytecount(1) + 250    */

/*
 * DE TO TAL SKAL PASSE SAMMEN, og det skal holdes fast her.
 *
 * Vi laeser MBAP_LEN bytes, ser hvad laengden siger, og laeser saa den
 * laengde mere ind i SAMME buffer. Er PDU_MAX stoerre end der er plads
 * til, skriver vi uden for bufferen, og det er praecis det vagten paa
 * laengden skal forhindre. Saettes ZS_MB_MAX_REGS ned en dag, eller
 * PDU_MAX op, falder oversaettelsen her i stedet for at nogen opdager det
 * paa en vaeg.
 */
_Static_assert(MBAP_LEN + PDU_MAX <= ZS_MB_MAX_FRAME,
               "en ramme med den stoerst tillade laengde kan ikke vaere i "
               "frame[ZS_MB_MAX_FRAME]. Se zs_mb_pdu_laengde_ok.");

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
    case ZS_MB_ERR_NOT_WRITTEN:
        return "Inverteren tog ikke imod ændringen. "
               "Er \"Inverter control via Modbus\" slået til på den?";
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

size_t zs_mb_build_write_request(uint8_t *buf, size_t buf_len, uint16_t tid,
                                 uint8_t unit_id, uint16_t address,
                                 const uint16_t *values, uint16_t count)
{
    if (buf == NULL || values == NULL || count == 0) {
        return 0;
    }
    /*
     * FC16 kan hoejst 123 registre, ikke 125 som FC3. Grunden er at
     * rammen ogsaa skal rumme adresse, antal og en byte-taeller. Tallet
     * staar i Modbus-standarden, og det er ikke det samme som for
     * laesning, hvilket er en nem fejl at lave.
     */
    if (count > ZS_MB_MAX_WRITE_REGS) {
        return 0;
    }
    if ((uint32_t)address + (uint32_t)count > 0x10000u) {
        return 0;
    }
    size_t brug = 13 + (size_t)count * 2;
    if (buf_len < brug) {
        return 0;
    }

    wr_u16(buf + 0, tid);
    wr_u16(buf + 2, 0);
    /* Laengden daekker alt efter de foerste 6 bytes: unit, fc, adresse,
     * antal, byte-taeller og selve dataene. */
    wr_u16(buf + 4, (uint16_t)(7 + count * 2));
    buf[6] = unit_id;
    buf[7] = FC_WRITE_MULTI;
    wr_u16(buf + 8, address);
    wr_u16(buf + 10, count);
    buf[12] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++) {
        wr_u16(buf + 13 + i * 2, values[i]);
    }
    return brug;
}

zs_mb_err_t zs_mb_parse_write_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t expect_tid, uint8_t expect_unit,
                                       uint16_t expect_address,
                                       uint16_t expect_count,
                                       uint8_t *out_exception)
{
    if (out_exception != NULL) {
        *out_exception = 0;
    }
    if (frame == NULL) {
        return ZS_MB_ERR_FRAME;
    }
    /* Korteste lovlige svar er en exception: MBAP(7) + fc(1) + kode(1). */
    if (frame_len < 9) {
        return ZS_MB_ERR_FRAME;
    }

    uint16_t tid = rd_u16(frame + 0);
    uint16_t pid = rd_u16(frame + 2);
    uint16_t len = rd_u16(frame + 4);
    uint8_t  unit = frame[6];
    uint8_t  fc  = frame[7];

    if (pid != 0) {
        return ZS_MB_ERR_FRAME;
    }
    if ((size_t)len + MBAP_LEN != frame_len) {
        return ZS_MB_ERR_FRAME;
    }
    if (tid != expect_tid) {
        return ZS_MB_ERR_FRAME;
    }
    if (unit != expect_unit) {
        return ZS_MB_ERR_FRAME;
    }
    if (fc == (FC_WRITE_MULTI | FC_EXCEPTION_BIT)) {
        uint8_t code = frame[8];
        if (out_exception != NULL) {
            *out_exception = code;
        }
        ZS_LOGD(TAG, "exception-kode %u ved skrivning", code);
        return ZS_MB_ERR_EXCEPTION;
    }
    if (fc != FC_WRITE_MULTI) {
        return ZS_MB_ERR_FRAME;
    }
    /* Et normalt svar er praecis MBAP + fc + adresse + antal. */
    if (frame_len != 12) {
        return ZS_MB_ERR_FRAME;
    }
    /*
     * Ekkoet skal passe. Svarer inverteren med en anden adresse eller et
     * andet antal, har den skrevet et andet sted end vi bad om, og saa
     * er et "ok" vaerre end en fejl.
     */
    if (rd_u16(frame + 8) != expect_address) {
        return ZS_MB_ERR_FRAME;
    }
    if (rd_u16(frame + 10) != expect_count) {
        return ZS_MB_ERR_FRAME;
    }
    return ZS_MB_OK;
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
        ZS_LOGW(TAG, "byte-taeller %u, forventede %u",
                 (unsigned)bc, (unsigned)(expect_count * 2));
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
    mb->timeout_ms = ZS_MB_TIMEOUT_STANDARD_MS;
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

/*
 * Lukker og HUSKER hvorfor.
 *
 * Vi lukker ved enhver fejl, for en Modbus TCP-forbindelse hvor et svar
 * udeblev er i ukendt tilstand: svaret kan komme bagefter og forskyde
 * alt det naeste. Men udefra ligner det at nettet faldt ud, ogsaa naar
 * sandheden er at inverteren holdt op med at svare.
 *
 * De to sender én ud at lede helt forskellige steder: den ene efter et
 * kabel eller en switch, den anden efter en inverter der har travlt.
 * Derfor staar grunden i mb->last_error bagefter, og lagene ovenpaa kan
 * sige hvad der faktisk skete.
 */
static zs_mb_err_t luk_med_grund(zs_mb_t *mb, zs_mb_err_t grund)
{
    mb->stat_errors++;
    mb->last_error = grund;
    zs_mb_close(mb);
    return grund;
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

    /* To uafhaengige tjek, skrevet hver for sig.
     *
     * Foer stod de som ÉN betingelse med ||. Det gav ingenting og var et
     * sted at tage fejl: blev || lavet til &&, slap en mislykket F_SETFL
     * igennem, og vi ville koere videre med en sokkel der BLOKERER. Saa
     * ville connect haenge i stedet for at give op, og timeouten her ville
     * ikke virke. Delt op findes den fejl ikke laengere. */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
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
        if (zs_mb_select_art(rc) != ZS_CON_KLAR) {
            /* Timeout og fejl: begge dele betyder giv op. Se
             * zs_mb_select_art for hvorfor det ligger som egen funktion. */
            close(fd);
            return -1;
        }
        /* select siger skrivbar, men det siger den ogsaa naar forbindelsen
         * blev afvist. Det rigtige svar ligger i SO_ERROR. Springer man
         * dette over, tror man man er forbundet til alle 254 adresser. */
        /* To uafhaengige ting, skrevet hver for sig. Samme grund som ved
         * fcntl ovenfor: ét || er ét sted at tage fejl uden gevinst. */
        int soerr = 0;
        socklen_t slen = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0) {
            close(fd);
            return -1;
        }
        if (soerr != 0) {
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
    if (mb == NULL || !zs_mb_maal_ok(host, port)) {
        return ZS_MB_ERR_ARG;
    }
    if (strlen(host) >= sizeof(mb->host)) {
        return ZS_MB_ERR_ARG;
    }
    if (connect_timeout_ms == 0) {
        connect_timeout_ms = ZS_MB_TIMEOUT_STANDARD_MS;
    }

    /* Luk foerst. En halvdoed socket maa ikke overleve et reconnect. */
    zs_mb_close(mb);

    int fd = connect_with_timeout(host, port, connect_timeout_ms);
    if (fd < 0) {
        return ZS_MB_ERR_CONNECT;
    }

    apply_socket_options(fd, mb->timeout_ms ? mb->timeout_ms
                                             : ZS_MB_TIMEOUT_STANDARD_MS);

    mb->fd = fd;
    snprintf(mb->host, sizeof(mb->host), "%s", host);
    mb->port = port;
    /* Transaktions-ID starter paa 1, se zs_mb_naeste_tid. */
    if (mb->next_tid == 0) {
        mb->next_tid = 1;
    }
    ZS_LOGI(TAG, "forbundet til %s:%u", host, port);
    return ZS_MB_OK;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
bool zs_mb_luk_efter(zs_mb_err_t err)
{
    return err != ZS_MB_OK && err != ZS_MB_ERR_EXCEPTION;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
bool zs_mb_pdu_laengde_ok(uint16_t len)
{
    return len >= 3 && len <= PDU_MAX;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
uint16_t zs_mb_naeste_tid(uint16_t *taeller)
{
    if (taeller == NULL) {
        return 1;
    }
    if (*taeller == 0) {
        *taeller = 1;       /* foerste gang, eller lige efter en ombrydning */
    }
    uint16_t tid = (*taeller)++;
    if (*taeller == 0) {
        *taeller = 1;       /* vi broed om: spring nul over */
    }
    return tid;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
zs_mb_timeout_t zs_mb_timeout_vaelg(uint32_t oenske, uint32_t nuvaerende)
{
    zs_mb_timeout_t r;
    if (oenske != 0) {
        r.ms = oenske;
    } else if (nuvaerende != 0) {
        r.ms = nuvaerende;
    } else {
        r.ms = ZS_MB_TIMEOUT_STANDARD_MS;
    }
    r.skal_saettes = (r.ms != nuvaerende);
    return r;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
zs_send_t zs_mb_send_art(long n, int fejlnr)
{
    if (n > 0) {
        return ZS_SEND_VIDERE;
    }
    if (n == 0) {
        /* send() tog ingenting. errno hoerer ikke til dette kald, saa der
         * bliver ikke set paa det. Se noten i headeren. */
        return ZS_SEND_FEJL;
    }
    switch (zs_mb_errno_art(fejlnr)) {
    case ZS_SOK_IGEN:    return ZS_SEND_IGEN;
    case ZS_SOK_TIMEOUT: return ZS_SEND_TIMEOUT;
    case ZS_SOK_DOED:    break;
    }
    return ZS_SEND_FEJL;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
zs_con_t zs_mb_select_art(int rc)
{
    if (rc < 0) {
        return ZS_CON_FEJL;
    }
    if (rc == 0) {
        return ZS_CON_TIMEOUT;
    }
    return ZS_CON_KLAR;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
bool zs_mb_maal_ok(const char *host, uint16_t port)
{
    return host != NULL && host[0] != '\0' && port != 0;
}

/* Se zs_modbus_tcp.h: ligger som egen funktion saa den kan proeves af. */
zs_sok_t zs_mb_errno_art(int fejlnr)
{
    if (fejlnr == EINTR) {
        return ZS_SOK_IGEN;
    }
    if (fejlnr == EAGAIN || fejlnr == EWOULDBLOCK) {
        return ZS_SOK_TIMEOUT;
    }
    return ZS_SOK_DOED;
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
        /* Hele beslutningen ligger i zs_mb_send_art, saa den kan proeves
         * af. Her staar kun hvad vi goer med svaret. */
        switch (zs_mb_send_art((long)n, errno)) {
        case ZS_SEND_VIDERE:  sent += (size_t)n; continue;
        case ZS_SEND_IGEN:    continue;
        case ZS_SEND_TIMEOUT: return ZS_MB_ERR_TIMEOUT;
        case ZS_SEND_FEJL:    break;
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
        switch (zs_mb_errno_art(errno)) {
        case ZS_SOK_IGEN:    continue;
        case ZS_SOK_TIMEOUT: return ZS_MB_ERR_TIMEOUT;
        case ZS_SOK_DOED:    break;
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

    /* Hele beslutningen ligger i zs_mb_timeout_vaelg saa den kan proeves.
     * Den stod foer skrevet forfra i baade laesningen og skrivningen. */
    zs_mb_timeout_t t = zs_mb_timeout_vaelg(timeout_ms, mb->timeout_ms);
    timeout_ms = t.ms;
    if (t.skal_saettes) {
        struct timeval tv;
        tv.tv_sec  = (long)(timeout_ms / 1000u);
        tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
        setsockopt(mb->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(mb->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        mb->timeout_ms = timeout_ms;
    }

    uint16_t tid = zs_mb_naeste_tid(&mb->next_tid);

    uint8_t req[12];
    size_t req_len = zs_mb_build_read_request(req, sizeof(req), tid, unit_id, address, count);
    if (req_len == 0) {
        return ZS_MB_ERR_ARG;
    }

    mb->stat_requests++;

    zs_mb_err_t err = send_all(mb->fd, req, req_len);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }

    /* Foerst de 6 bytes der fortaeller hvor langt resten er. */
    uint8_t frame[ZS_MB_MAX_FRAME];
    err = recv_exact(mb->fd, frame, MBAP_LEN);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }

    uint16_t len = rd_u16(frame + 4);
    /* Protokolreglen, se zs_mb_pdu_laengde_ok. Den haarde stopklods mod
     * et overloeb er den naeste if, paa bufferens stoerrelse. */
    if (!zs_mb_pdu_laengde_ok(len)) {
        ZS_LOGW(TAG, "urimeligt laengdefelt %u, lukker forbindelsen", len);
        return luk_med_grund(mb, ZS_MB_ERR_FRAME);
    }
    if (MBAP_LEN + (size_t)len > sizeof(frame)) {
        return luk_med_grund(mb, ZS_MB_ERR_FRAME);
    }

    err = recv_exact(mb->fd, frame + MBAP_LEN, len);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }

    err = zs_mb_parse_read_response(frame, MBAP_LEN + (size_t)len, tid, unit_id,
                                    count, out, &mb->last_exception);
    if (err != ZS_MB_OK) {
        /* Se zs_mb_luk_efter: en exception beholder forbindelsen, alt
         * andet lukker den. Reglen staar ÉT sted. */
        if (zs_mb_luk_efter(err)) {
            return luk_med_grund(mb, err);
        }
        mb->stat_errors++;
        mb->last_error = err;
        return err;
    }
    return ZS_MB_OK;
}

zs_mb_err_t zs_mb_write_verified(zs_mb_t *mb, uint8_t unit_id, uint16_t address,
                                 const uint16_t *values, uint16_t count,
                                 uint32_t timeout_ms)
{
    /* Fire ting, hver for sig. Fire led i ÉN betingelse er fire steder at
     * tage fejl og ingen af dem til at se hver for sig. */
    if (mb == NULL || values == NULL) {
        return ZS_MB_ERR_ARG;
    }
    if (count == 0 || count > ZS_MB_MAX_WRITE_REGS) {
        return ZS_MB_ERR_ARG;
    }
    if (mb->fd < 0) {
        return ZS_MB_ERR_NOT_OPEN;
    }

    /* Hele beslutningen ligger i zs_mb_timeout_vaelg saa den kan proeves.
     * Den stod foer skrevet forfra i baade laesningen og skrivningen. */
    zs_mb_timeout_t t = zs_mb_timeout_vaelg(timeout_ms, mb->timeout_ms);
    timeout_ms = t.ms;
    if (t.skal_saettes) {
        struct timeval tv;
        tv.tv_sec  = (long)(timeout_ms / 1000u);
        tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
        setsockopt(mb->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(mb->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        mb->timeout_ms = timeout_ms;
    }

    uint16_t tid = zs_mb_naeste_tid(&mb->next_tid);

    uint8_t req[13 + ZS_MB_MAX_WRITE_REGS * 2];
    size_t req_len = zs_mb_build_write_request(req, sizeof(req), tid, unit_id,
                                               address, values, count);
    if (req_len == 0) {
        return ZS_MB_ERR_ARG;
    }

    mb->stat_requests++;
    zs_mb_err_t err = send_all(mb->fd, req, req_len);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }

    uint8_t frame[ZS_MB_MAX_FRAME];
    err = recv_exact(mb->fd, frame, MBAP_LEN);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }
    uint16_t len = rd_u16(frame + 4);
    /*
     * SAMME regel som i laesningen, og det var den ikke foer.
     *
     * Her stod "len < 1 || MBAP_LEN + len > sizeof(frame)", altsaa to ting
     * i ÉN betingelse og med en anden nedre graense end laesningens. En
     * laengde paa 1 eller 2 blev taget imod her og afvist der, selv om det
     * er det samme felt i den samme protokol. Det korteste svar der
     * betyder noget er unit + funktionskode + fejlkode = 3 bytes.
     *
     * To steder der tjekker det samme med forskellige regler er ikke to
     * lag, det er ét sted hvor nogen senere retter det ene og tror begge
     * er rettet.
     */
    if (!zs_mb_pdu_laengde_ok(len)) {
        ZS_LOGW(TAG, "urimeligt laengdefelt %u i svaret paa en skrivning",
                len);
        return luk_med_grund(mb, ZS_MB_ERR_FRAME);
    }
    if (MBAP_LEN + (size_t)len > sizeof(frame)) {
        return luk_med_grund(mb, ZS_MB_ERR_FRAME);
    }
    err = recv_exact(mb->fd, frame + MBAP_LEN, len);
    if (err != ZS_MB_OK) {
        return luk_med_grund(mb, err);
    }

    err = zs_mb_parse_write_response(frame, MBAP_LEN + (size_t)len, tid, unit_id,
                                     address, count, &mb->last_exception);
    if (err != ZS_MB_OK) {
        /* Samme regel som i laesningen, se zs_mb_luk_efter. */
        if (zs_mb_luk_efter(err)) {
            return luk_med_grund(mb, err);
        }
        mb->stat_errors++;
        mb->last_error = err;
        return err;
    }

    /*
     * Og saa det vigtigste: LAES TILBAGE.
     *
     * Fronius skriver selv i sin manual at en afvist skrivning IKKE giver
     * en exception. Et paent svar ovenfor beviser altsaa kun at rammen kom
     * frem, ikke at inverteren gjorde noget. Afvisning sker fx naar
     * "Inverter control via Modbus" ikke er slaaet til paa inverterens
     * webside, eller naar en anden styring har forrang.
     *
     * Uden det her ville skaermen melde "sendt" mens inverteren gjorde som
     * den plejede, og vi ville lede efter fejlen alle de forkerte steder.
     */
    uint16_t tilbage[ZS_MB_MAX_WRITE_REGS];
    err = zs_mb_read_holding(mb, unit_id, address, count, tilbage, timeout_ms);
    if (err != ZS_MB_OK) {
        return err;
    }
    for (uint16_t i = 0; i < count; i++) {
        if (tilbage[i] != values[i]) {
            ZS_LOGW(TAG, "skrev %u i register %u, men der staar %u",
                    (unsigned)values[i], (unsigned)(address + i),
                    (unsigned)tilbage[i]);
            return ZS_MB_ERR_NOT_WRITTEN;
        }
    }
    return ZS_MB_OK;
}


bool zs_mb_probe_port(const char *host, uint16_t port, uint32_t timeout_ms)
{
    if (!zs_mb_maal_ok(host, port)) {
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
