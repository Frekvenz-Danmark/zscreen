/*
 * zScreen - koerer alle enhedstestene.
 *
 * De tester den del af koden der ikke roerer hardware: Modbus-rammer,
 * SunSpec-afkodning og tal-formatering. Det er praecis der de fejl
 * ligger der er svaerest at faa oeje paa naar man staar med skaermen i
 * haanden, fordi et forkert tal ser ud som et rigtigt tal.
 */

#include "zs_test.h"

#include <stdio.h>
#include <string.h>

int zs_tests_run = 0;
int zs_tests_failed = 0;
const char *zs_current_suite = "";
int zs_log_verbose = 0;

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            zs_log_verbose = 1;
        }
    }

    printf("\n\033[1mzScreen enhedstest\033[0m\n");

    test_modbus();
    test_sunspec();
    test_format();
    test_fronius();

    printf("\n────────────────────────────────────────\n");
    if (zs_tests_failed == 0) {
        printf("\033[1;32m%d tjek, alle bestaaet\033[0m\n\n", zs_tests_run);
        return 0;
    }
    printf("\033[1;31m%d tjek, %d fejlede\033[0m\n\n", zs_tests_run, zs_tests_failed);
    return 1;
}
