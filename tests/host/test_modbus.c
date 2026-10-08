/*
 * Test af Modbus TCP-rammer.
 *
 * Vi banker paa ramme-haandteringen direkte, uden socket. Meningen er
 * at et forvansket eller fjendtligt svar aldrig maa give hverken et
 * forkert tal eller en laesning uden for bufferen.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_modbus_tcp.h"

#include <errno.h>
#include <string.h>

/* Bygger et gyldigt FC3-svar med de opgivne registre. */
static size_t make_response(uint8_t *buf, uint16_t tid, uint8_t unit,
                            const uint16_t *regs, uint8_t nregs)
{
    uint8_t bc = (uint8_t)(nregs * 2);
    buf[0] = (uint8_t)(tid >> 8);
    buf[1] = (uint8_t)(tid & 0xFF);
    buf[2] = 0; buf[3] = 0;                 /* protokol-ID */
    uint16_t len = (uint16_t)(3 + bc);      /* unit + fc + bc + data */
    buf[4] = (uint8_t)(len >> 8);
    buf[5] = (uint8_t)(len & 0xFF);
    buf[6] = unit;
    buf[7] = 0x03;
    buf[8] = bc;
    for (uint8_t i = 0; i < nregs; i++) {
        buf[9 + i * 2]     = (uint8_t)(regs[i] >> 8);
        buf[9 + i * 2 + 1] = (uint8_t)(regs[i] & 0xFF);
    }
    return (size_t)(9 + bc);
}

void test_modbus(void)
{
    ZS_SUITE("Modbus TCP-rammer");

    /* ---------- forespoergsel ---------- */
    {
        uint8_t req[16];
        size_t n = zs_mb_build_read_request(req, sizeof(req), 0x1234, 1, 40000, 2);
        CHECK_INT("forespoergsel er 12 bytes", n, 12);
        CHECK_INT("transaktions-ID hoej byte", req[0], 0x12);
        CHECK_INT("transaktions-ID lav byte",  req[1], 0x34);
        CHECK_INT("protokol-ID er 0",          (req[2] << 8) | req[3], 0);
        CHECK_INT("laengdefelt er 6",          (req[4] << 8) | req[5], 6);
        CHECK_INT("unit-ID",                   req[6], 1);
        CHECK_INT("funktionskode er 3",        req[7], 3);
        CHECK_INT("adresse",                   (req[8] << 8) | req[9], 40000);
        CHECK_INT("antal registre",            (req[10] << 8) | req[11], 2);
    }

    /* Graenser der skal afvises. */
    {
        uint8_t req[16];
        CHECK_INT("0 registre afvises",
                  zs_mb_build_read_request(req, sizeof(req), 1, 1, 100, 0), 0);
        CHECK_INT("126 registre afvises (FC3 kan hoejst 125)",
                  zs_mb_build_read_request(req, sizeof(req), 1, 1, 100, 126), 0);
        CHECK_INT("125 registre er tilladt",
                  zs_mb_build_read_request(req, sizeof(req), 1, 1, 100, 125), 12);
        CHECK_INT("laesning der loeber ud over 16-bit adresserummet afvises",
                  zs_mb_build_read_request(req, sizeof(req), 1, 1, 65530, 10), 0);
        CHECK_INT("laesning der slutter praecis paa kanten er tilladt",
                  zs_mb_build_read_request(req, sizeof(req), 1, 1, 65526, 10), 12);
        CHECK_INT("for lille buffer afvises",
                  zs_mb_build_read_request(req, 8, 1, 1, 100, 2), 0);
        CHECK_INT("NULL-buffer afvises",
                  zs_mb_build_read_request(NULL, 16, 1, 1, 100, 2), 0);
    }

    /* ---------- gyldigt svar ---------- */
    {
        uint16_t src[3] = { 0x5375, 0x6E53, 0xABCD };
        uint8_t frame[64];
        size_t n = make_response(frame, 0x1234, 1, src, 3);

        uint16_t out[3] = {0};
        uint8_t exc = 0xFF;
        zs_mb_err_t e = zs_mb_parse_read_response(frame, n, 0x1234, 1, 3, out, &exc);
        CHECK_INT("gyldigt svar accepteres", e, ZS_MB_OK);
        CHECK_INT("register 0", out[0], 0x5375);
        CHECK_INT("register 1", out[1], 0x6E53);
        CHECK_INT("register 2", out[2], 0xABCD);
        CHECK_INT("ingen exception-kode sat", exc, 0);
    }

    /* ---------- svar der skal afvises ---------- */
    {
        uint16_t src[2] = { 0x1111, 0x2222 };
        uint8_t frame[64];
        uint16_t out[2];
        uint8_t exc;
        size_t n;

        n = make_response(frame, 0x1234, 1, src, 2);
        CHECK_INT("forkert transaktions-ID afvises",
                  zs_mb_parse_read_response(frame, n, 0x9999, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        n = make_response(frame, 0x1234, 1, src, 2);
        CHECK_INT("forkert unit-ID afvises",
                  zs_mb_parse_read_response(frame, n, 0x1234, 200, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        n = make_response(frame, 0x1234, 1, src, 2);
        frame[2] = 0x00; frame[3] = 0x01;   /* protokol-ID 1 findes ikke */
        CHECK_INT("forkert protokol-ID afvises",
                  zs_mb_parse_read_response(frame, n, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        n = make_response(frame, 0x1234, 1, src, 2);
        frame[7] = 0x04;                    /* svar paa en anden funktionskode */
        CHECK_INT("forkert funktionskode afvises",
                  zs_mb_parse_read_response(frame, n, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        /* Byte-taelleren lyver om hvor meget data der foelger. Uden
         * tjekket ville vi laese to registre fra en ramme der kun
         * indeholder ét, altsaa uden for bufferen. */
        n = make_response(frame, 0x1234, 1, src, 2);
        frame[8] = 2;                       /* paastaar 1 register */
        CHECK_INT("byte-taeller der ikke passer med antal registre afvises",
                  zs_mb_parse_read_response(frame, n, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        /* Laengdefeltet passer ikke med hvor mange bytes vi faktisk fik. */
        n = make_response(frame, 0x1234, 1, src, 2);
        CHECK_INT("for kort ramme i forhold til laengdefeltet afvises",
                  zs_mb_parse_read_response(frame, n - 2, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);

        CHECK_INT("ramme under 9 bytes afvises",
                  zs_mb_parse_read_response(frame, 8, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);
        CHECK_INT("tom ramme afvises",
                  zs_mb_parse_read_response(frame, 0, 0x1234, 1, 2, out, &exc),
                  ZS_MB_ERR_FRAME);
    }

    /* ---------- exception ---------- */
    {
        uint8_t frame[9];
        frame[0] = 0x00; frame[1] = 0x07;
        frame[2] = 0x00; frame[3] = 0x00;
        frame[4] = 0x00; frame[5] = 0x03;
        frame[6] = 0x01;
        frame[7] = 0x83;   /* 0x03 med fejlbit */
        frame[8] = 0x02;   /* ulovlig dataadresse */

        uint16_t out[2];
        uint8_t exc = 0;
        zs_mb_err_t e = zs_mb_parse_read_response(frame, sizeof(frame), 7, 1, 2, out, &exc);
        CHECK_INT("exception genkendes", e, ZS_MB_ERR_EXCEPTION);
        CHECK_INT("exception-kode laeses", exc, 2);
    }

    /* ---------- fuld stoerrelse ---------- */
    {
        uint16_t src[125];
        for (int i = 0; i < 125; i++) { src[i] = (uint16_t)(i * 517); }
        uint8_t frame[ZS_MB_MAX_FRAME];
        size_t n = make_response(frame, 0x0001, 1, src, 125);
        CHECK_INT("125-register-svar fylder praecis ZS_MB_MAX_FRAME", n, ZS_MB_MAX_FRAME);

        uint16_t out[125];
        uint8_t exc;
        CHECK_INT("125 registre pakkes ud",
                  zs_mb_parse_read_response(frame, n, 0x0001, 1, 125, out, &exc), ZS_MB_OK);
        int all_ok = 1;
        for (int i = 0; i < 125; i++) { if (out[i] != src[i]) { all_ok = 0; break; } }
        CHECK("alle 125 registre er korrekte", all_ok);
    }

    /* NULL-argumenter maa ikke give et nedbrud. */
    {
        uint8_t frame[16] = {0};
        uint16_t out[2];
        CHECK_INT("NULL-ramme afvises",
                  zs_mb_parse_read_response(NULL, 16, 1, 1, 2, out, NULL), ZS_MB_ERR_ARG);
        CHECK_INT("NULL-output afvises",
                  zs_mb_parse_read_response(frame, 16, 1, 1, 2, NULL, NULL), ZS_MB_ERR_ARG);
        CHECK_INT("0 forventede registre afvises",
                  zs_mb_parse_read_response(frame, 16, 1, 1, 0, out, NULL), ZS_MB_ERR_ARG);
    }

    /*
     * Fejlteksterne staar paa Detaljer-siden og bliver laest af den
     * der skal finde ud af hvorfor en skaerm ikke virker. De skal
     * derfor vaere paa dansk, begynde med stort, og aldrig vaere
     * tomme. Vi tjekker det for HVER kode, ogsaa en der ikke findes,
     * saa en ny fejltype ikke kan slippe igennem uden tekst.
     */
    {
        const zs_mb_err_t alle[] = {
            ZS_MB_OK, ZS_MB_ERR_ARG, ZS_MB_ERR_NOT_OPEN, ZS_MB_ERR_CONNECT,
            ZS_MB_ERR_SEND, ZS_MB_ERR_TIMEOUT, ZS_MB_ERR_CLOSED,
            ZS_MB_ERR_FRAME, ZS_MB_ERR_EXCEPTION, (zs_mb_err_t)99,
        };
        int alle_ok = 1;
        for (size_t i = 0; i < sizeof(alle) / sizeof(alle[0]); i++) {
            const char *t = zs_mb_strerror(alle[i]);
            if (t == NULL || t[0] == '\0') { alle_ok = 0; break; }
            /* Stort begyndelsesbogstav. Teksten staar som en linje for
             * sig selv paa skaermen, ikke midt i en saetning. */
            if (t[0] >= 'a' && t[0] <= 'z') { alle_ok = 0; break; }
        }
        CHECK("hver fejlkode har en tekst der begynder med stort", alle_ok);
        CHECK_STR("timeout siger hvad der skete, ikke hvad koden hedder",
                  zs_mb_strerror(ZS_MB_ERR_TIMEOUT),
                  "Inverteren svarede ikke i tide");
    }
}

/*
 * Rammerne til at SKRIVE.
 *
 * Vi har kun laest indtil nu. At skrive paa en kundes inverter er en
 * anden slags ansvar, og rammen skal vaere rigtig foer noget som helst
 * andet. Fronius' egen manual siger at en afvist skrivning IKKE giver en
 * exception, saa et paent svar beviser ingenting. Derfor er ekko-tjekket
 * her, og derfor laeses der tilbage bagefter i zs_mb_write_verified.
 */
void test_modbus_skriv(void)
{
    ZS_SUITE("Modbus-rammer til at skrive");

    uint8_t buf[64];
    const uint16_t to[2] = { 0x1234, 0xABCD };

    {
        size_t n = zs_mb_build_write_request(buf, sizeof(buf), 0x0007, 1,
                                             40000, to, 2);
        CHECK_INT("forespørgsel med to registre er 17 bytes", (int)n, 17);
        CHECK_INT("transaktions-ID", (int)((buf[0] << 8) | buf[1]), 7);
        CHECK_INT("protokol-ID er nul", (int)((buf[2] << 8) | buf[3]), 0);
        CHECK_INT("længdefelt dækker resten", (int)((buf[4] << 8) | buf[5]), 11);
        CHECK_INT("unit", (int)buf[6], 1);
        CHECK_INT("funktionskode er 16", (int)buf[7], 0x10);
        CHECK_INT("adresse", (int)((buf[8] << 8) | buf[9]), 40000);
        CHECK_INT("antal", (int)((buf[10] << 8) | buf[11]), 2);
        CHECK_INT("byte-tæller er dobbelt så stor", (int)buf[12], 4);
        CHECK_INT("første værdi", (int)((buf[13] << 8) | buf[14]), 0x1234);
        CHECK_INT("anden værdi", (int)((buf[15] << 8) | buf[16]), 0xABCD);
    }

    {
        /* Graenserne. FC16 kan hoejst 123, ikke 125 som laesning, og det
         * er en nem fejl at lave. */
        uint16_t mange[130];
        for (int i = 0; i < 130; i++) { mange[i] = (uint16_t)i; }
        uint8_t stor[13 + 123 * 2];
        CHECK_INT("123 registre giver den rigtige længde",
                  (int)zs_mb_build_write_request(stor, sizeof(stor), 1, 1, 0, mange, 123),
                  13 + 123 * 2);
        CHECK_INT("124 registre afvises",
                  (int)zs_mb_build_write_request(stor, sizeof(stor), 1, 1, 0, mange, 124), 0);
        CHECK_INT("0 registre afvises",
                  (int)zs_mb_build_write_request(buf, sizeof(buf), 1, 1, 0, to, 0), 0);
        CHECK_INT("NULL-buffer afvises",
                  (int)zs_mb_build_write_request(NULL, 64, 1, 1, 0, to, 2), 0);
        CHECK_INT("NULL-værdier afvises",
                  (int)zs_mb_build_write_request(buf, sizeof(buf), 1, 1, 0, NULL, 2), 0);
        CHECK_INT("for lille buffer afvises",
                  (int)zs_mb_build_write_request(buf, 16, 1, 1, 0, to, 2), 0);
        CHECK_INT("hen over kanten af adresserummet afvises",
                  (int)zs_mb_build_write_request(buf, sizeof(buf), 1, 1, 65535, to, 2), 0);
    }

    {
        /* Svaret er et ekko af adresse og antal, ikke data. */
        uint8_t svar[12] = { 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x01, 0x10,
                             0x9C, 0x40, 0x00, 0x02 };
        uint8_t exc = 0xFF;
        CHECK_INT("gyldigt ekko accepteres",
                  zs_mb_parse_write_response(svar, sizeof(svar), 7, 1, 40000, 2, &exc),
                  ZS_MB_OK);
        CHECK_INT("og der er ingen exception", (int)exc, 0);

        /* DET VIGTIGSTE: et ekko der ikke passer. Saa har inverteren
         * skrevet et andet sted end vi bad om. */
        CHECK("forkert adresse i ekkoet afvises",
              zs_mb_parse_write_response(svar, sizeof(svar), 7, 1, 40002, 2, &exc)
              != ZS_MB_OK);
        CHECK("forkert antal i ekkoet afvises",
              zs_mb_parse_write_response(svar, sizeof(svar), 7, 1, 40000, 3, &exc)
              != ZS_MB_OK);
        CHECK("forkert transaktions-ID afvises",
              zs_mb_parse_write_response(svar, sizeof(svar), 8, 1, 40000, 2, &exc)
              != ZS_MB_OK);
        CHECK("forkert unit afvises",
              zs_mb_parse_write_response(svar, sizeof(svar), 7, 2, 40000, 2, &exc)
              != ZS_MB_OK);

        uint8_t kort[11];
        memcpy(kort, svar, sizeof(kort));
        CHECK("for kort svar afvises",
              zs_mb_parse_write_response(kort, sizeof(kort), 7, 1, 40000, 2, &exc)
              != ZS_MB_OK);
    }

    {
        /* En rigtig exception, fx "det register maa du ikke skrive i". */
        uint8_t exc_svar[9] = { 0x00, 0x07, 0x00, 0x00, 0x00, 0x03,
                                0x01, 0x90, 0x02 };
        uint8_t exc = 0;
        CHECK_INT("exception genkendes",
                  zs_mb_parse_write_response(exc_svar, sizeof(exc_svar), 7, 1,
                                             40000, 2, &exc),
                  ZS_MB_ERR_EXCEPTION);
        CHECK_INT("og koden læses", (int)exc, 2);
    }
}

/*
 * De to steder i socket-laget der kan proeves af uden et netvaerk.
 *
 * Begge kom frem af en mutationstest, og begge var usynlige:
 *
 *   1. "port == 0" i zs_mb_probe_port kunne laves om til "port != 0" uden
 *      at nogen sagde fra. Port nul findes ikke, og et forsoeg paa at
 *      forbinde dertil opfoerer sig forskelligt fra maskine til maskine.
 *      Vagten skal vaere der, og den skal vaere proevet af.
 *
 *   2. errno-vurderingen i send og modtag stod skrevet forfra to steder,
 *      midt i en loekke, og kunne derfor ikke proeves. "errno == EAGAIN"
 *      kunne laves til "errno != EAGAIN", og saa ville hver eneste fejl,
 *      ogsaa en forbindelse der blev lukket, blive meldt til kunden som
 *      "svarede ikke i tide". Vi har én gang foer brugt en halv dag paa
 *      en misvisende fejlbesked, saa den skal holdes paa plads.
 */
void test_modbus_sokkel(void)
{
    ZS_SUITE("Modbus: hvad en fejl fra soklen betyder");

    /* Afbrudt af et signal er ikke en fejl. Der skal proeves igen med det
     * samme, ellers taber vi en aflaesning hver gang systemet bliver
     * forstyrret. */
    CHECK("EINTR: prøv igen", zs_mb_errno_art(EINTR) == ZS_SOK_IGEN);

    /* Tiden loeb ud. Det er den ENE ting der maa hedde "svarede ikke i
     * tide" over for kunden. */
    CHECK("EAGAIN: tiden løb ud", zs_mb_errno_art(EAGAIN) == ZS_SOK_TIMEOUT);
    CHECK("EWOULDBLOCK: det samme", zs_mb_errno_art(EWOULDBLOCK) == ZS_SOK_TIMEOUT);

    /* Alt andet er en forbindelse der ikke duer. Den skal IKKE kaldes en
     * timeout, for saa leder man efter et langsomt netvaerk i stedet for
     * efter en inverter der har lukket. */
    CHECK("ECONNRESET: forbindelsen duer ikke",
          zs_mb_errno_art(ECONNRESET) == ZS_SOK_DOED);
    CHECK("EPIPE: heller ikke", zs_mb_errno_art(EPIPE) == ZS_SOK_DOED);
    CHECK("EHOSTUNREACH: heller ikke",
          zs_mb_errno_art(EHOSTUNREACH) == ZS_SOK_DOED);
    CHECK("ENOTCONN: heller ikke", zs_mb_errno_art(ENOTCONN) == ZS_SOK_DOED);
    CHECK("EBADF: heller ikke", zs_mb_errno_art(EBADF) == ZS_SOK_DOED);
    /* Nul betyder at der ikke er sat noget errno. Det er ikke en timeout. */
    CHECK("nul er ikke en timeout", zs_mb_errno_art(0) == ZS_SOK_DOED);

    /*
     * Og hele talomraadet: kun de tre vi kender maa give noget andet end
     * DOED. Ellers kan en graense der er sat ved siden af lade et
     * vilkaarligt errno slippe igennem som en timeout.
     */
    int forkert = 0;
    for (int e = 0; e < 256; e++) {
        zs_sok_t a = zs_mb_errno_art(e);
        bool vented_andet = (e == EINTR || e == EAGAIN || e == EWOULDBLOCK);
        if (!vented_andet && a != ZS_SOK_DOED) { forkert++; }
    }
    CHECK_INT("kun de tre kendte slipper forbi DØD", forkert, 0);

    ZS_SUITE("Modbus: en frisk forbindelse starter lukket");

    /*
     * HVORFOR DET SKAL PROEVES AF.
     *
     * zs_mb_init og zs_mb_is_open var slet ikke naevnt i nogen proeve. En
     * mutationstest kunne lave "if (mb == NULL) return;" om til "if (mb !=
     * NULL) return;", saa zs_mb_init gjorde INGENTING for en gyldig
     * pointer, og ikke én af de mange proever sagde fra. Saa staar fd paa
     * hvad der nu laa i hukommelsen, og en tilfaeldig vaerdi over nul
     * betyder "forbindelsen er aaben". Derfra kan vi lukke en sokkel der
     * hoerer til noget helt andet.
     */
    zs_mb_t mb;
    memset(&mb, 0xFF, sizeof(mb));   /* skrald, saa init SKAL rydde op */
    zs_mb_init(&mb);
    CHECK_INT("fd står på -1 bagefter", mb.fd, -1);
    CHECK("og forbindelsen regnes som lukket", !zs_mb_is_open(&mb));
    CHECK_INT("og der er en svartid at regne med", (int)mb.timeout_ms, 1500);
    CHECK_INT("og ingen exception-kode", mb.last_exception, 0);
    CHECK_INT("og transaktions-ID starter på nul", mb.next_tid, 0);
    CHECK_STR("og ingen vært", mb.host, "");

    /* NULL ind maa ikke give et nedbrud. */
    zs_mb_init(NULL);
    CHECK("init(NULL) går ikke ned", true);
    CHECK("is_open(NULL) er falsk", !zs_mb_is_open(NULL));

    /*
     * Og graensen paa fd. NUL er et gyldigt filhaandtag: koden siger
     * "fd >= 0 betyder aaben", og det skal den blive ved med. Bliver det
     * til "fd > 0", melder vi en aaben forbindelse som lukket, og saa
     * laekker soklen i stedet for at blive lukket.
     */
    mb.fd = 0;
    CHECK("fd nul regnes som åben, for nul er et gyldigt håndtag",
          zs_mb_is_open(&mb));
    mb.fd = 1;
    CHECK("fd ét er også åben", zs_mb_is_open(&mb));
    mb.fd = -1;
    CHECK("fd minus ét er lukket", !zs_mb_is_open(&mb));

    ZS_SUITE("Modbus: vagterne på at forbinde, uden at røre nettet");

    /*
     * De her to vagter afviser FOER der bliver aabnet en socket, saa de kan
     * proeves paa en almindelig maskine. Begge var uproevede.
     *
     * Den foerste: "mb == NULL || !zs_mb_maal_ok(...)". Blev || lavet til
     * &&, slipper en NULL-pointer igennem naar vaert og port er i orden, og
     * saa skrives der i NULL lige efter.
     */
    CHECK("NULL som forbindelse afvises",
          zs_mb_connect(NULL, "192.168.1.50", 502, 100) == ZS_MB_ERR_ARG);

    zs_mb_t c;
    zs_mb_init(&c);
    CHECK("NULL som vært afvises",
          zs_mb_connect(&c, NULL, 502, 100) == ZS_MB_ERR_ARG);
    CHECK("tom vært afvises",
          zs_mb_connect(&c, "", 502, 100) == ZS_MB_ERR_ARG);
    CHECK("port nul afvises",
          zs_mb_connect(&c, "192.168.1.50", 0, 100) == ZS_MB_ERR_ARG);

    /*
     * Den anden: "strlen(host) >= sizeof(mb->host)". Blev >= lavet til >,
     * slipper en vaert der er PRAECIS saa lang som feltet igennem, og saa
     * er der ikke plads til nulbyten. En IPv6-adresse kan godt fylde det
     * meste af de 46 tegn, saa det er ikke en teoretisk graense.
     */
    /* Hver laengde bygges FORFRA.
     *
     * Foerste forsoeg satte bare en nulbyte laengere ude i den samme
     * buffer. Men den foerste nulbyte stod stadig der, saa strengen var
     * uaendret kort, og de to proever maalte det samme. De var groenne og
     * sagde ingenting. Det er samme slags fejl som koden selv skal fange,
     * og den slap ind i proeven. */
    char lang[64];
    size_t plads = sizeof(c.host);

    memset(lang, 'a', plads - 1);
    lang[plads - 1] = '\0';            /* ét under: der er plads */
    CHECK_INT("længden er som vi tror", (int)strlen(lang), (int)plads - 1);

    memset(lang, 'a', plads);
    lang[plads] = '\0';                /* praecis saa lang som feltet */
    CHECK_INT("og nu præcis så lang som feltet", (int)strlen(lang),
              (int)plads);
    CHECK("en vært præcis så lang som feltet afvises",
          zs_mb_connect(&c, lang, 502, 100) == ZS_MB_ERR_ARG);

    memset(lang, 'a', plads + 1);
    lang[plads + 1] = '\0';            /* ét over */
    CHECK("og én længere afvises også",
          zs_mb_connect(&c, lang, 502, 100) == ZS_MB_ERR_ARG);

    CHECK("og ingen af dem åbnede en forbindelse", !zs_mb_is_open(&c));

    ZS_SUITE("Modbus: at lukke må aldrig kunne gå ned");

    /*
     * zs_mb_close havde vagten "mb == NULL || mb->fd < 0". Blev || lavet
     * om til &&, bliver mb->fd laest selv om mb er NULL, og saa gaar
     * skaermen ned. Vi lukker ved ENHVER fejl, saa det er den sti der
     * bliver gaaet oftest af alle naar noget er i vejen. Ingen proeve
     * naevnte den.
     */
    zs_mb_close(NULL);
    CHECK("close(NULL) går ikke ned", true);

    zs_mb_t luk;
    zs_mb_init(&luk);
    zs_mb_close(&luk);                 /* aldrig aabnet */
    CHECK_INT("at lukke en der aldrig var åben: fd er stadig -1", luk.fd, -1);
    zs_mb_close(&luk);                 /* og to gange */
    CHECK_INT("og to gange i træk gør ingen skade", luk.fd, -1);
    CHECK("og den regnes stadig som lukket", !zs_mb_is_open(&luk));

    ZS_SUITE("Modbus: hvad select sagde da vi ventede på forbindelsen");

    /*
     * NUL er det farlige. Nul betyder at tiden loeb ud, altsaa INGEN
     * forbindelse. Blev "rc <= 0" lavet om til "rc < 0", ville en timeout
     * blive regnet som en forbindelse der kom op, og en scanning af 254
     * adresser ville melde en inverter paa hver af dem.
     */
    CHECK("nul er en timeout, ikke en forbindelse",
          zs_mb_select_art(0) == ZS_CON_TIMEOUT);
    CHECK("under nul er en fejl", zs_mb_select_art(-1) == ZS_CON_FEJL);
    CHECK("og enhver anden negativ", zs_mb_select_art(-99) == ZS_CON_FEJL);
    CHECK("én er klar", zs_mb_select_art(1) == ZS_CON_KLAR);
    CHECK("og flere end én er også klar", zs_mb_select_art(2) == ZS_CON_KLAR);

    /* Hele området, så en grænse der er sat ved siden af bliver fanget. */
    int galt = 0;
    for (int rc = -50; rc <= 50; rc++) {
        zs_con_t a = zs_mb_select_art(rc);
        zs_con_t vented = (rc < 0) ? ZS_CON_FEJL
                        : (rc == 0) ? ZS_CON_TIMEOUT : ZS_CON_KLAR;
        if (a != vented) { galt++; }
    }
    CHECK_INT("hele området fra -50 til 50 opfører sig som aftalt", galt, 0);

    ZS_SUITE("Modbus: er værten og porten noget vi kan forsøge");

    /*
     * Vagten proeves DIREKTE og ikke gennem zs_mb_probe_port.
     *
     * Grunden: gennem probe_port kan man ikke se forskel. Fjerner man
     * vagten, fejler maskinens eget socket-lag alligevel, og svaret
     * bliver det samme. En mutationstest siger saa god for at vagten
     * ryger ud, og saa staar vi og stoler paa hvad lwIP paa en ESP32
     * tilfaeldigvis goer med port nul og en tom vaert. Det er ikke en
     * beslutning vi har taget, det er noget vi haaber paa.
     */
    CHECK("almindelig vært og port er i orden",
          zs_mb_maal_ok("192.168.1.50", 502));
    CHECK("port nul er ikke en port", !zs_mb_maal_ok("127.0.0.1", 0));
    CHECK("NULL er ikke en vært", !zs_mb_maal_ok(NULL, 502));
    /* En tom tekst er IKKE en vært. Nogle maskiners navneopslag svarer
     * "det er dig selv", og saa ville vi melde inverteren fundet fordi
     * der stod noget paa vores egen port. */
    CHECK("tom tekst er ikke en vært", !zs_mb_maal_ok("", 502));
    CHECK("og begge dele galt på én gang", !zs_mb_maal_ok(NULL, 0));
    CHECK("tom tekst og port nul", !zs_mb_maal_ok("", 0));
    /* Højeste port skal være lovlig: en vagt der er sat ved siden af
     * ville afvise den, og 65535 er en gyldig port. */
    CHECK("højeste port er lovlig", zs_mb_maal_ok("127.0.0.1", 65535));

    /* Og vejen igennem skal give det samme, saa vagten faktisk ER koblet
     * paa. Ingen af dem aabner en socket. */
    CHECK("probe_port falder på den samme vagt",
          zs_mb_probe_port("127.0.0.1", 0, 50) == false
          && zs_mb_probe_port(NULL, 502, 50) == false
          && zs_mb_probe_port("", 502, 50) == false);
}
