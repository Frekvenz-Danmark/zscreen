/*
 * zScreen - selvtest. Se zs_selftest.c for hvad den goer og hvorfor.
 * Slaas til med ZS_SELFTEST i zs_config.h.
 */

#ifndef ZS_SELFTEST_H
#define ZS_SELFTEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bygger alle sider om mange gange i begge temaer og maaler
 * hukommelsen. Blokerer i nogle sekunder. */
void zs_selftest_run(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SELFTEST_H */
