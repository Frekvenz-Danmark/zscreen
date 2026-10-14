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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Laengste versionsstreng vi tager imod, inklusive afslutning. */
#define ZS_VERSION_MAX  24

/*
 * Hvad formatet HOEJST kan sige ja til.
 *
 * zs_version_tag_ok godtager tre tal med hoejst ZS_VERSION_MAX_CIFRE
 * cifre hver og to punktummer imellem. Mere kan ikke komme igennem, saa
 * laengden er bundet af formatet og ikke af en laengdevagt.
 *
 * HVORFOR DET STAAR HER OG IKKE SOM ET TJEK I KODEN.
 *
 * Der stod foer "n == 0 || n >= ZS_VERSION_MAX" inde i funktionen. Begge
 * halvdele var doed kode: en tom streng falder paa den sidste linje
 * alligevel, og formatet kan hoejst give 17 tegn, saa graensen paa 24 blev
 * aldrig naaet. Derfor kunne en mutationstest vende begge halvdele uden at
 * en eneste proeve faldt, og en vagt man ikke kan proeve er ingen vagt.
 *
 * Nu staar sammenhaengen som et tjek ved bygningen i stedet. Saetter nogen
 * cifferloftet op, falder bygningen med det samme, i stedet for at der
 * opstaar en bufferfejl man skal vaere heldig at finde.
 */
#define ZS_VERSION_MAX_CIFRE  5
#define ZS_VERSION_LAENGST    (3 * ZS_VERSION_MAX_CIFRE + 2)
_Static_assert(ZS_VERSION_LAENGST + 1 <= ZS_VERSION_MAX,
               "formatet kan sige ja til mere end ZS_VERSION_MAX kan rumme");

/*
 * Er teksten et versionsnummer vi kan slaa op som et maerke paa GitHub?
 *
 * Godtager "0.9.0" og "v0.9.0". Forkaster tom, for lang, bogstaver,
 * skraastreger, punktum-punktum og alt andet.
 *
 * Den er STRENG med vilje: teksten kommer udefra, fra serveren, og
 * ender inde i en URL. En skraastreg eller et punktum-punktum maa
 * aldrig slippe igennem. Se ZS_FLEET_TARGET_FELT.
 */
bool zs_version_tag_ok(const char *version);

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

/*
 * Teksten naar GitHub svarer 404 paa en udgivelse.
 *
 * HVORFOR DEN FINDES.
 *
 * Der stod ÉN tekst: "Der er ingen udgivelser endnu". Den er rigtig naar vi
 * spurgte efter den NYESTE og der ikke er nogen. Men bad vi om en BESTEMT
 * udgave, fordi nogen skrev et maal i dashboardet, saa er 404 noget helt
 * andet: der ER udgivelser, den ene vi bad om findes bare ikke.
 *
 * Forskellen betyder noget for den der skal rette det. Med den gamle tekst
 * gaar man og leder efter et problem med udgivelser i det hele taget, i
 * stedet for at se paa det versionsnummer man selv har skrevet. Vi har
 * brugt en halv dag paa en misvisende fejlbesked én gang, og det skal vi
 * ikke igen.
 *
 * Ligger her og ikke i zs_ota.c fordi zs_ota.c ikke kan oversaettes paa en
 * almindelig maskine: den haenger paa esp_http_client. Her kan teksten
 * proeves af.
 *
 *   maal   det versionsnummer der blev bedt om, eller NULL eller tom hvis
 *          vi spurgte efter den nyeste.
 */
void zs_version_tekst_404(const char *maal, char *ud, size_t ud_len);

/*
 * Hvad skal maalversionen vaere, og er det en NY ordre?
 *
 * HVORFOR DEN LIGGER HER.
 *
 * Beslutningen laa inde i zs_ota_set_target, og den fil kan ikke
 * oversaettes paa en almindelig maskine: den haenger paa esp_https_ota og
 * paa FreeRTOS. Saa var reglerne uproevede, og det er fire regler der
 * hver kan vendes om:
 *
 *   tom eller NULL  -> intet maal, foelg nyeste. Og det ER en ordre, hvis
 *                      der var et maal foer.
 *   noget skrald    -> behold det gamle. At falde tilbage til "nyeste"
 *                      fordi nogen tastede forkert, ville vaere en
 *                      overraskelse.
 *   v foran         -> gem UDEN v. Maerket paa GitHub har v, og det
 *                      saettes paa igen naar vi slaar op, saa der kun er
 *                      ét sted der ved det.
 *   samme som foer  -> IKKE en ny ordre. Serveren sender vaerdien igen
 *                      ved hver indmeldelse, og en genforbindelse er
 *                      ikke en ordre. Uden den regel ville en skaerm paa
 *                      et daarligt net banke paa GitHub i stedet for at
 *                      vise tal.
 *
 * Den sidste er den der betyder mest. Maalt 8. oktober 2026: en ny
 * maalversion naar skaermen paa omkring 5 millisekunder, men blev foer
 * bare gemt og foerst brugt ved det periodiske tjek en halv time senere.
 * Nu udloeser "aendret" et tjek med det samme, og saa skal "aendret"
 * vaere rigtig.
 */
typedef struct {
    char maal[ZS_VERSION_MAX];  /* det der skal gemmes, uden v           */
    bool gyldig;                /* kunne oensket bruges                  */
    bool aendret;               /* er det et ANDET maal end foer         */
} zs_maal_t;

zs_maal_t zs_version_vaelg_maal(const char *nuvaerende, const char *oensket);

/*
 * Er den her fil i udgivelsen firmware til DET HER produkt?
 *
 * HVORFOR DET ER ET SPOERGSMAAL.
 *
 * zScreen og ZDisplay udgives fra samme repo med samme versionsmaerke,
 * saa hver udgivelse har to .bin-filer: "zscreen-v0.28.0.bin" og
 * "zdisplay-v0.28.0.bin". Koden tog foer den foerste der sluttede paa
 * ".bin", og hvilken af de to det var, afhang af hvilken raekkefoelge de
 * blev lagt op i. Se ZS_OTA_ASSET_PREFIX for hvad det kostede.
 *
 * LIGGER HER og ikke i zs_ota.c af samme grund som zs_version_tekst_404:
 * zs_ota.c kan ikke oversaettes paa en almindelig maskine, den haenger
 * paa esp_http_client. Her kan reglen proeves - ogsaa for det produkt
 * man IKKE bygger, fordi praefikset er en parameter og ikke en konstant
 * hentet inde i funktionen.
 *
 * Siger ja til: praefikset foerst, mindst ét tegn derefter, og ".bin"
 * til sidst. Altsaa ikke "zscreen-.bin", ikke "zdisplay-v1.bin" naar vi
 * er en zScreen, ikke "zscreen-v1.bin.sha256", og ikke
 * "min-zscreen-v1.bin".
 *
 * navn og praefiks maa vaere NULL; saa er svaret nej.
 */
bool zs_version_asset_passer(const char *navn, const char *praefiks);

#ifdef __cplusplus
}
#endif

#endif /* ZS_VERSION_H */
