/*
 * zScreen - gennemsnit per kvarter.
 *
 * HVORFOR DEN FINDES.
 *
 * Skaermen laeser inverteren hvert andet sekund og sender tallene op. Det
 * er rigtigt: flaadestyringen skal vise live data, og en skaerm der er
 * holdt op med at sende er en skaerm man vil hoere om.
 *
 * Men de tal GEMMES ikke, og det er ogsaa rigtigt: fem felter gange 43.200
 * om dagen er tre millioner raekker om maaneden per skaerm.
 *
 * Og saa er der ingen historik. Det er et problem, for OpenRemotes
 * forbrugsprognose regner paa GEMT historik, og uden den planlaegger en
 * batterioptimering efter et hus der ikke bruger stroem.
 *
 * Derfor det her: vi lægger tallene sammen mens de kommer ind, og sender ÉT
 * gennemsnit per kvarter. 96 raekker per felt om dagen i stedet for 43.200.
 * Og et kvarter er ogsaa den takt OpenRemotes optimering regner i, saa vi
 * giver den praecis den oploesning den bruger.
 *
 * BOETTEN, OG HVORFOR DEN IKKE BARE ER ET KVARTERNUMMER.
 *
 * Et kvarternummer 0 til 95 gentager sig hver dag. Havde skaermen vaeret
 * slukket et doegn, kunne den vaagne i det SAMME kvarternummer og tro at
 * den stod midt i det den forlod. Saa ville et gennemsnit fra i gaar blive
 * sendt som om det var fra nu.
 *
 * Derfor regner kalderen én boette ud af baade dag og kvarter, og den
 * taeller opad. Og vi sender KUN et gennemsnit videre hvis den boette vi
 * har samlet paa, er den der ligger lige FOER den vi er i nu. Er der et
 * hul, smider vi det vaek. Et gennemsnit man ikke kan placere rigtigt paa
 * tidslinjen, er vaerre end ingenting.
 */

#ifndef ZS_KVARTER_H
#define ZS_KVARTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Kvarterer i et doegn. */
#define ZS_KVARTER_PR_DOEGN  96

/*
 * Felterne, i den raekkefoelge de staar i alle tre tabeller: her, i
 * firmwarens afsendelse og i serverens fordeling. Raekkefoelgen er ÉN
 * sandhed, og den maa ikke staa to steder.
 */
typedef enum {
    ZS_KV_SOL = 0,
    ZS_KV_FORBRUG,
    ZS_KV_BATTERI,
    ZS_KV_NET,
    ZS_KV_SOC,
    ZS_KV_ANTAL
} zs_kv_felt_t;

/* Det vi samler paa. En nulstillet struct betyder "ikke begyndt", saa den
 * kan laves med memset eller som en statisk variabel uden videre. */
typedef struct {
    double   sum[ZS_KV_ANTAL];
    uint32_t antal[ZS_KV_ANTAL];
    int32_t  boette;      /* den boette vi samler paa                    */
    bool     begyndt;     /* har vi samlet paa noget overhovedet         */
} zs_kvarter_t;

/* Det der kommer ud, naar et kvarter er gaaet. */
typedef struct {
    bool    klar;                 /* er der et gennemsnit at sende       */
    int32_t boette;               /* hvilken boette det daekker          */
    int8_t  kvarter;              /* 0 til 95, til at sende med          */
    float   vaerdi[ZS_KV_ANTAL];
    bool    har[ZS_KV_ANTAL];     /* for hvert felt: kom der noget ind   */
} zs_kvarter_ud_t;

/*
 * Boetten, regnet af dag og kvarter. Kalderen bruger den.
 *
 *   dagnr    dagnummer, som i zs_energi_tik
 *   kvarter  0 til 95
 */
int32_t zs_kvarter_boette(uint16_t dagnr, int kvarter);

/* Kvarternummeret ud af time og minut. Giver -1 hvis de ikke kan passe. */
int zs_kvarter_nummer(int time, int minut);

/*
 * Ét sæt tal ind. Giver et gennemsnit ud naar et kvarter er gaaet.
 *
 *   s           det vi samler paa
 *   vaerdi      ZS_KV_ANTAL tal
 *   har         for hvert: er det gyldigt. Et felt uden maaler eller uden
 *               batteri bliver IKKE regnet med, og kommer ikke med ud
 *   boette_nu   hvilken boette vi er i NU
 *
 * Kaldes ved hver aflaesning. Giver klar == false naesten altid.
 */
zs_kvarter_ud_t zs_kvarter_tik(zs_kvarter_t *s, const float *vaerdi,
                               const bool *har, int32_t boette_nu);

#ifdef __cplusplus
}
#endif

#endif /* ZS_KVARTER_H */
