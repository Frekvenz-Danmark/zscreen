/*
 * Test af Modbus TCP-rammer.
 *
 * Vi banker paa ramme-haandteringen direkte, uden socket. Meningen er
 * at et forvansket eller fjendtligt svar aldrig maa give hverken et
 * forkert tal eller en laesning uden for bufferen.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_modbus_tcp.h"

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

    CHECK_STR("fejltekst er dansk og ikke tom",
              zs_mb_strerror(ZS_MB_ERR_TIMEOUT), "timeout");
}
