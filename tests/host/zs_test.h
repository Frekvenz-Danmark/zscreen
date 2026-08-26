/*
 * zScreen - lillebitte testramme.
 *
 * Bevidst uden bibliotek. Testene skal kunne koeres med én cc-kommando
 * paa en ny maskine uden at installere noget, ellers bliver de ikke
 * koert. Det eneste de gaar op i er: virkede det, og hvis ikke, hvad
 * stod der saa i stedet.
 */

#ifndef ZS_TEST_H
#define ZS_TEST_H

#include <stdio.h>
#include <string.h>
#include <math.h>

extern int zs_tests_run;
extern int zs_tests_failed;
extern const char *zs_current_suite;

#define ZS_SUITE(name) \
    do { zs_current_suite = (name); printf("\n\033[1m%s\033[0m\n", (name)); } while (0)

#define ZS_FAIL(fmt, ...) \
    do { \
        zs_tests_failed++; \
        printf("  \033[1;31mFEJL\033[0m %s:%d  " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define ZS_PASS(what) \
    do { printf("  \033[1;32m ok \033[0m %s\n", (what)); } while (0)

#define CHECK(what, cond) \
    do { \
        zs_tests_run++; \
        if (cond) { ZS_PASS(what); } else { ZS_FAIL("%s", what); } \
    } while (0)

#define CHECK_INT(what, got, want) \
    do { \
        zs_tests_run++; \
        long long _g = (long long)(got), _w = (long long)(want); \
        if (_g == _w) { ZS_PASS(what); } \
        else { ZS_FAIL("%s: fik %lld, forventede %lld", what, _g, _w); } \
    } while (0)

#define CHECK_STR(what, got, want) \
    do { \
        zs_tests_run++; \
        const char *_g = (got), *_w = (want); \
        if (_g != NULL && strcmp(_g, _w) == 0) { ZS_PASS(what); } \
        else { ZS_FAIL("%s: fik \"%s\", forventede \"%s\"", what, _g ? _g : "(null)", _w); } \
    } while (0)

/* Flydende tal sammenlignes med tolerance. Direkte == paa float er
 * en fejlkilde, ikke en test. */
#define CHECK_F(what, got, want, tol) \
    do { \
        zs_tests_run++; \
        double _g = (double)(got), _w = (double)(want); \
        if (fabs(_g - _w) <= (tol)) { ZS_PASS(what); } \
        else { ZS_FAIL("%s: fik %.6f, forventede %.6f", what, _g, _w); } \
    } while (0)

/* Testsuiterne. Prototyperne staar her saa hver .c-fil ser dem, og
 * -Wmissing-prototypes dermed er tilfreds. Den advarsel er slaaet til
 * med vilje: den fanger funktioner der burde have vaeret static. */
void test_modbus(void);
void test_sunspec(void);
void test_format(void);
void test_fronius(void);
void test_version(void);
void test_tilegrid(void);
void test_demo(void);

#endif /* ZS_TEST_H */
