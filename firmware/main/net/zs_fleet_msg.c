#include "zs_fleet_msg.h"

#include <stdio.h>
#include <string.h>

size_t zs_fleet_msg_enroll_size(const char *cert_pem)
{
    if (cert_pem == NULL) {
        return 0;
    }
    /* Vaerst tilfaelde er at hvert tegn skal undslippes til to, plus
     * den faste indpakning og afslutningen. */
    return strlen(cert_pem) * 2 + 32;
}

size_t zs_fleet_msg_enroll(char *ud, size_t ud_len, const char *cert_pem)
{
    static const char FOER[] = "{\"type\":\"x509\",\"cert\":\"";
    static const char EFTER[] = "\"}";

    if (ud == NULL || ud_len == 0) {
        return 0;
    }
    ud[0] = '\0';
    if (cert_pem == NULL || cert_pem[0] == '\0') {
        return 0;
    }

    const size_t f = sizeof(FOER) - 1;
    const size_t e = sizeof(EFTER) - 1;
    /* Der skal vaere plads til indpakningen og afslutningen foer vi
     * begynder. Ellers kan vi ende med en halv besked. */
    if (ud_len < f + e + 1) {
        return 0;
    }

    memcpy(ud, FOER, f);
    size_t o = f;
    const size_t plads = ud_len - e - 1;   /* gem plads til "} og nul */

    for (const char *p = cert_pem; *p != '\0'; p++) {
        /* Hvert tegn kan blive to. Er der ikke plads til begge, stopper
         * vi helt i stedet for at sende et afkortet certifikat: det
         * ville serveren afvise, og fejlen ville vaere svaer at se. */
        if (o + 2 > plads) {
            ud[0] = '\0';
            return 0;
        }
        switch (*p) {
        case '\n': ud[o++] = '\\'; ud[o++] = 'n';  break;
        case '\r':                                 break;  /* smides vaek */
        case '"':  ud[o++] = '\\'; ud[o++] = '"';  break;
        case '\\': ud[o++] = '\\'; ud[o++] = '\\'; break;
        default:   ud[o++] = *p;                   break;
        }
    }
    memcpy(ud + o, EFTER, e);
    o += e;
    ud[o] = '\0';
    return o;
}

/*
 * Begge emner bygges her, med samme tjek. Forskellen er ét ord:
 *
 *   writeattributevalue   vi skriver op til serveren
 *   attributevalue        vi lytter efter noget den sender ned
 *
 * De to stod foer kun ét sted hver, og det ene blev lavet ved at klippe
 * "write" ud af det andet med memmove. Det virkede, men det var ikke til
 * at laese, og en aendring i formen ville kun blive rettet det ene sted.
 */
static size_t byg_emne(char *ud, size_t ud_len, const char *verbum,
                       const char *realm, const char *unik, const char *felt,
                       const char *asset_id)
{
    if (ud == NULL || ud_len == 0) {
        return 0;
    }
    ud[0] = '\0';
    if (realm == NULL || unik == NULL || felt == NULL || asset_id == NULL
        || realm[0] == '\0' || unik[0] == '\0' || felt[0] == '\0'
        || asset_id[0] == '\0') {
        return 0;
    }
    int n = snprintf(ud, ud_len, "%s/%s/%s/%s/%s",
                     realm, unik, verbum, felt, asset_id);
    if (n < 0 || (size_t)n >= ud_len) {
        /* Et afkortet emne ville skrive i et andet felt eller i en
         * anden enhed. Hellere ingenting. */
        ud[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

size_t zs_fleet_msg_topic(char *ud, size_t ud_len, const char *realm,
                          const char *unik, const char *felt,
                          const char *asset_id)
{
    return byg_emne(ud, ud_len, "writeattributevalue", realm, unik, felt, asset_id);
}

size_t zs_fleet_msg_lyt_topic(char *ud, size_t ud_len, const char *realm,
                              const char *unik, const char *felt,
                              const char *asset_id)
{
    return byg_emne(ud, ud_len, "attributevalue", realm, unik, felt, asset_id);
}

bool zs_fleet_enroll_due(bool abonneret, bool har_asset,
                         int64_t naeste_forsoeg_ms, int64_t nu_ms)
{
    if (!abonneret || har_asset) {
        return false;
    }
    if (naeste_forsoeg_ms <= 0) {
        return false;
    }
    return nu_ms >= naeste_forsoeg_ms;
}
