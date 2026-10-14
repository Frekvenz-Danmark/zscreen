/*
 * zScreen - log-shim.
 *
 * Praecis samme kildekode skal kunne oversaettes to steder:
 *   1. paa ESP32-S3 med ESP-IDF, hvor vi vil have esp_log
 *   2. paa en Mac som almindeligt C-program, saa vi kan enhedsteste
 *      Modbus- og SunSpec-logikken uden hardware
 *
 * Derfor gaar alle log-kald gennem disse makroer i stedet for direkte
 * ESP_LOGx. Naar ZS_HOST_BUILD er defineret bliver det til fprintf.
 */

#ifndef ZS_LOG_H
#define ZS_LOG_H

#ifdef ZS_HOST_BUILD

#include <stdio.h>

/* Paa host skriver vi til stderr saa testens egne printf paa stdout
 * ikke bliver blandet sammen med log-stoej. */
#define ZS_LOGE(tag, fmt, ...) fprintf(stderr, "E %-10s " fmt "\n", tag, ##__VA_ARGS__)
#define ZS_LOGW(tag, fmt, ...) fprintf(stderr, "W %-10s " fmt "\n", tag, ##__VA_ARGS__)
#define ZS_LOGI(tag, fmt, ...) fprintf(stderr, "I %-10s " fmt "\n", tag, ##__VA_ARGS__)
#define ZS_LOGD(tag, fmt, ...) \
    do { if (zs_log_verbose) fprintf(stderr, "D %-10s " fmt "\n", tag, ##__VA_ARGS__); } while (0)

/* Saettes af testrunneren med -v. Standard er stille. */
extern int zs_log_verbose;

#else /* ESP-IDF */

#include "esp_log.h"

#define ZS_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define ZS_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define ZS_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define ZS_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)

#endif /* ZS_HOST_BUILD */

#endif /* ZS_LOG_H */
