#include "zs_version.h"

#include <string.h>

#include <stddef.h>

/* Hoejeste tal vi accepterer i et led. Et versionsnummer der er stoerre
 * end det er ikke et versionsnummer, og graensen holder os langt fra at
 * loebe over i en unsigned. */
#define LED_MAKS  99999u

const char *zs_version_strip_v(const char *s)
{
    return (s != NULL && (s[0] == 'v' || s[0] == 'V')) ? s + 1 : s;
}

bool zs_version_parse(const char *s, unsigned ud[3])
{
    if (s == NULL) {
        return false;
    }
    const char *p = zs_version_strip_v(s);
    unsigned tal[3] = { 0, 0, 0 };

    for (int led = 0; led < 3; led++) {
        if (led > 0) {
            if (*p != '.') {
                return false;
            }
            p++;
        }
        /*
         * Mindst ét ciffer, og KUN cifre.
         *
         * sscanf med %u ville have taget baade mellemrum foran og et
         * fortegn. Maerket "v-1.0.0" ville saa blive laest som
         * 4294967295 og se nyere ud end alt andet.
         */
        if (*p < '0' || *p > '9') {
            return false;
        }
        unsigned v = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10u + (unsigned)(*p - '0');
            if (v > LED_MAKS) {
                return false;
            }
            p++;
        }
        tal[led] = v;
    }

    /* Intet maa staa efter det tredje led. "1.2.3-rc1" er ikke en
     * udgivelse vi laegger paa kundernes skaerme. */
    if (*p != '\0') {
        return false;
    }

    if (ud != NULL) {
        ud[0] = tal[0];
        ud[1] = tal[1];
        ud[2] = tal[2];
    }
    return true;
}

int zs_version_cmp(const char *a, const char *b)
{
    unsigned av[3], bv[3];
    if (!zs_version_parse(a, av) || !zs_version_parse(b, bv)) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        if (av[i] > bv[i]) { return 1; }
        if (av[i] < bv[i]) { return -1; }
    }
    return 0;
}

bool zs_version_tag_ok(const char *version)
{
    if (version == NULL) {
        return false;
    }
    /* Et valgfrit v foran, som i maerket v0.9.0. */
    if (*version == 'v' || *version == 'V') {
        version++;
    }
    size_t n = strlen(version);
    if (n == 0 || n >= ZS_VERSION_MAX) {
        return false;
    }

    /*
     * Praecis tre tal med punktum imellem. Vi er strenge med vilje:
     * teksten kommer fra serveren og ender i en URL, saa en skraastreg
     * eller et punktum-punktum maa aldrig slippe igennem. Alt der ikke
     * er noejagtig tal.tal.tal bliver forkastet.
     */
    int tal = 0, cifre = 0;
    for (size_t i = 0; i < n; i++) {
        char c = version[i];
        if (c >= '0' && c <= '9') {
            if (++cifre > 5) { return false; }
        } else if (c == '.') {
            if (cifre == 0) { return false; }   /* ".." eller ".1" */
            cifre = 0;
            if (++tal > 2) { return false; }    /* hoejst to punktummer */
        } else {
            return false;
        }
    }
    return tal == 2 && cifre > 0;
}
