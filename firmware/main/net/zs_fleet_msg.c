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

/* ------------------------------------------------------------------ */
/* Beskeder der kommer i stykker. Se zs_fleet_msg.h for hvorfor.       */
/* ------------------------------------------------------------------ */

void zs_fleet_saml_init(zs_fleet_saml_t *s, char *buf, size_t buf_len)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->buf = buf;
    s->buf_len = buf_len;
}

zs_saml_t zs_fleet_saml_tag(zs_fleet_saml_t *s,
                            const char *emne, int emne_len,
                            const char *data, int data_len,
                            int offset, int total)
{
    if (s == NULL || s->buf == NULL || s->buf_len == 0) {
        return ZS_SAML_USAMMENHAENG;
    }
    if (data_len < 0 || offset < 0 || total < 0) {
        return ZS_SAML_USAMMENHAENG;
    }

    if (offset == 0) {
        /*
         * Et nyt stykke nummer ét. Vi starter forfra, OGSAA hvis der laa
         * en halv besked. En forbindelse der falder midt i en besked
         * efterlader netop det, og den halve maa ikke blandes ind i den
         * naeste.
         */
        s->har = 0;
        s->dropper = false;
        s->emne[0] = '\0';
        if (emne != NULL && emne_len > 0) {
            size_t n = (size_t)emne_len;
            if (n > sizeof(s->emne) - 1) {
                n = sizeof(s->emne) - 1;
            }
            memcpy(s->emne, emne, n);
            s->emne[n] = '\0';
        }
        /*
         * total er hele beskedens laengde, ogsaa naar den kommer i ét
         * stykke. Er den nul, men der ER data, saa stol paa data_len:
         * saadan opfoerer en enkeltstykket besked sig.
         */
        s->venter = (total > 0) ? (size_t)total : (size_t)data_len;
        if (s->venter > s->buf_len - 1) {
            /*
             * Den kan ikke vaere der. Vi siger det ÉN gang og smider
             * resten vaek i stilhed, i stedet for at skrive uden for
             * bufferen eller klage tre gange om den samme besked.
             */
            s->dropper = true;
            return ZS_SAML_FOR_STOR;
        }
    } else {
        if (s->dropper) {
            return ZS_SAML_FOR_STOR;    /* resten af en vi har opgivet */
        }
        /*
         * Stykkerne skal komme i raekkefoelge og haenge sammen. Gaar der
         * et tabt, er det vi har samlet ikke en besked, og saa skal det
         * smides vaek frem for at blive sendt videre som om det var hel.
         */
        if ((size_t)offset != s->har || s->venter == 0) {
            s->har = 0;
            s->venter = 0;
            s->emne[0] = '\0';
            return ZS_SAML_USAMMENHAENG;
        }
    }

    if (data_len > 0) {
        if (s->har + (size_t)data_len > s->venter ||
            s->har + (size_t)data_len > s->buf_len - 1) {
            /* Mere end der blev lovet. Vi gemmer ikke paa noget vi ikke
             * forstaar. */
            s->har = 0;
            s->venter = 0;
            s->emne[0] = '\0';
            return ZS_SAML_USAMMENHAENG;
        }
        if (data != NULL) {
            memcpy(s->buf + s->har, data, (size_t)data_len);
        }
        s->har += (size_t)data_len;
    }

    if (s->har >= s->venter) {
        s->buf[s->har] = '\0';
        return ZS_SAML_KLAR;
    }
    return ZS_SAML_VENTER;
}

bool zs_fleet_emne_har_led(const char *emne, const char *led)
{
    if (emne == NULL || led == NULL) {
        return false;
    }
    size_t nl = strlen(led);
    if (nl == 0) {
        return false;
    }
    size_t ne = strlen(emne);
    /*
     * Graensen er ikke pynt. Foerste udgave kaldte memcmp med laengden
     * paa ordet uden at se paa hvor meget der var TILBAGE af emnet, og
     * saa laeste den ud over strengens ende. Det faldt med det samme i
     * enhedstesten, og paa en skaerm ville det vaere en fejl der kommer
     * og gaar efter hvad der tilfaeldigvis ligger i hukommelsen bagefter.
     */
    for (size_t i = 0; i + nl <= ne; i++) {
        /* Starten af et led: begyndelsen, eller lige efter en skraastreg. */
        if (i != 0 && emne[i - 1] != '/') {
            continue;
        }
        if (memcmp(emne + i, led, nl) != 0) {
            continue;
        }
        /* Og det skal SLUTTE her, ellers er det et laengere ord. */
        if (emne[i + nl] == '\0' || emne[i + nl] == '/') {
            return true;
        }
    }
    return false;
}
