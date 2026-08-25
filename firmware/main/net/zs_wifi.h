/*
 * zScreen - wifi.
 *
 * Skaermen bruger kun wifi som klient. Den laver ikke sit eget
 * netvaerk, lytter ikke paa nogen port, og har ingen tjeneste koerende.
 * Hele opsaetningen sker paa selve touchskaermen.
 *
 * Kaldene her er blokerende og skal derfor koeres fra opgaven i
 * zs_app.c, aldrig fra LVGL's egen opgave. En scanning tager et par
 * sekunder, og skaermen skal reagere paa tryk imens.
 */

#ifndef ZS_WIFI_H
#define ZS_WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "zs_nvs.h"   /* ZS_SSID_MAX, ZS_PASS_MAX */

#ifdef __cplusplus
extern "C" {
#endif

/* Hoejst saa mange netvaerk vises. Flere end det kan ingen overskue at
 * rulle igennem paa en 4 tommer skaerm, og listen er sorteret efter
 * signalstyrke, saa det man leder efter staar i toppen. */
#define ZS_WIFI_MAX_APS   20

typedef struct {
    char    ssid[ZS_SSID_MAX];
    int8_t  rssi;       /* dBm, tyettere paa 0 er bedre     */
    bool    secured;    /* kraever kodeord                  */
} zs_ap_t;

typedef enum {
    ZS_WIFI_OFF = 0,
    ZS_WIFI_SCANNING,
    ZS_WIFI_CONNECTING,
    ZS_WIFI_CONNECTED,
    ZS_WIFI_FAILED,
} zs_wifi_state_t;

/* Saetter netvaerksstakken op. Kaldes én gang, foer alt andet her. */
bool zs_wifi_init(void);

/*
 * Scanner efter netvaerk. Blokerer i to til fire sekunder.
 *
 * Listen sorteres efter signalstyrke, og netvaerk der optraeder flere
 * gange (en mesh med flere adgangspunkter) staar kun én gang, med det
 * staerkeste signal.
 *
 * Returnerer antallet der blev fundet, eller -1 ved fejl.
 */
int zs_wifi_scan(zs_ap_t *out, size_t max);

/*
 * Forbinder. Blokerer indtil der er en IP-adresse eller det er givet op.
 *
 * pass maa vaere NULL eller tom for et aabent netvaerk.
 * Returnerer false ved fejl, og zs_wifi_last_error() siger hvorfor,
 * paa dansk og formuleret til brugeren, ikke til en udvikler.
 */
bool zs_wifi_connect(const char *ssid, const char *pass, uint32_t timeout_ms);

void zs_wifi_disconnect(void);

zs_wifi_state_t zs_wifi_state(void);

/* Signalstyrke i dBm. 0 hvis vi ikke er forbundet. */
int zs_wifi_rssi(void);

/* Vores egen IP som tekst. Tom streng hvis vi ikke har en. */
bool zs_wifi_get_ip(char *buf, size_t len);

/* Vores eget undernet, fx 192.168.1.0, brugt af inverter-scanningen. */
bool zs_wifi_get_subnet(char *buf, size_t len, uint8_t *prefix_bits);

/* Sidste fejl paa dansk. Aldrig NULL. */
const char *zs_wifi_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_WIFI_H */
