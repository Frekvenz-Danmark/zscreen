/*
 * zScreen - sammenligning af versionsnumre.
 *
 * Ligger for sig selv og bruger intet fra ESP-IDF, saa den kan testes
 * paa en almindelig maskine. Det er med vilje: det er den her funktion
 * der afgoer om en skaerm paa en vaeg henter ny firmware, og en fejl
 * her betyder enten at den aldrig opdaterer, eller at den opdaterer i
 * ring til den samme udgave for evigt.
 */

#ifndef ZS_VERSION_H
#define ZS_VERSION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Laeser "1.2.3" eller "v1.2.3" til tre tal.
 *
 * Kun cifre og praecis to punktummer. Mellemrum, fortegn, bogstaver
 * eller et fjerde led giver false. Vi er strenge fordi teksten kommer
 * fra et versionsmaerke paa GitHub, altsaa udefra.
 *
 * ud maa vaere NULL hvis man kun vil vide om den kan laeses.
 */
bool zs_version_parse(const char *s, unsigned ud[3]);

/*
 * Sammenligner to versioner som tal, ikke som tekst.
 *
 * "0.10.0" er nyere end "0.9.0", men staar FOER den alfabetisk. Blev
 * strengene bare sammenlignet, ville en skaerm paa 0.9.0 aldrig komme
 * videre.
 *
 * Returnerer 1 hvis a er nyere end b, 0 hvis de er ens, og -1 hvis a er
 * aeldre ELLER hvis en af dem ikke kan laeses. Det sidste er med vilje:
 * vi opdaterer hellere ikke end at opdatere til noget vi ikke forstaar.
 */
int zs_version_cmp(const char *a, const char *b);

/* "v0.2.0" og "0.2.0" er det samme. Peger ind i s, kopierer ikke. */
const char *zs_version_strip_v(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* ZS_VERSION_H */
