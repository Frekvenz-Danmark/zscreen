/*
 * zScreen - genfinder inverteren naar den har skiftet adresse.
 *
 * HVORFOR DEN FINDES.
 *
 * En IP-adresse er ikke en identitet. Routeren uddeler dem paa laan, og
 * en inverter der har vaeret slukket laenge nok, eller som bliver
 * genstartet samtidig med at routeren bliver det, kan komme tilbage paa
 * en anden adresse. Foer det her modul stod skaermen og bankede paa den
 * gamle adresse for evigt, med voksende pause, og kunden skulle selv ind
 * i indstillingerne og scanne forfra.
 *
 * Og den anden halvdel, som er vaerre: adressen kan imens vaere givet til
 * en ANDEN enhed. Er det ogsaa en inverter, svarede den, skaermen
 * forbandt, og kunden saa en fremmed inverters tal som om det var
 * anlaegget paa taget. Derfor er det ikke nok at lede efter en ny
 * adresse, vi skal ogsaa kunne afvise en der svarer.
 *
 * IDENTITETEN er serienummeret fra SunSpec model 1 felt SN. Det sidder i
 * hardwaren og skifter ikke. Det laeses alligevel ved hver forbindelse,
 * saa det koster ingen ekstra kald. Afkodningen er deterministisk, se
 * zs_ss_dec_string: samme registre giver altid samme streng, saa en
 * sammenligning tegn for tegn er til at stole paa.
 *
 * REDUNDANS, i den raekkefoelge det koster:
 *   1. proev den gemte adresse, det tager under et sekund
 *   2. scan hele undernettet, med den gemte adresse foerst
 *   3. vaelg efter serienummer, ikke efter hvad der tilfaeldigvis svarer
 *
 * VI GAETTER IKKE. Kender vi et serienummer og finder vi det ikke, saa
 * siger vi fra i stedet for at tage den eneste inverter der var. I en
 * lejlighed kan naboens inverter sagtens svare, og at vise naboens tal
 * er vaerre end at sige at vi ikke kan finde anlaegget.
 */

#ifndef ZS_LOCATE_H
#define ZS_LOCATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zs_discovery.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hvad valget endte med. */
typedef enum {
    ZS_PICK_SERIAL = 0,   /* en af dem har vores serienummer          */
    ZS_PICK_ENESTE,       /* vi kender intet serienummer, der var én   */
    ZS_PICK_INGEN,        /* ingen passer, eller der var ingen         */
    ZS_PICK_FLERE         /* flere, og vi kan ikke afgoere hvilken     */
} zs_pick_t;

/*
 * Vaelger hvilken af de fundne der er VORES. Ren beslutning, ingen
 * netvaerk, saa den kan proeves af paa en almindelig maskine.
 *
 *   fundet/n       hvad scanningen gav. Den gemte adresse ligger foerst
 *                  hvis den svarede, saa "foerste traeffer vinder"
 *                  foretraekker den af sig selv.
 *   gemt_serial    vores inverters serienummer. Tom streng eller NULL
 *                  betyder at vi ikke kender det endnu.
 *   valgt          index i fundet. Skrives kun ved SERIAL og ENESTE.
 *
 * Reglerne, i raekkefoelge:
 *   kender vi serienummeret   -> kun en noejagtig traeffer taeller
 *   kender vi det ikke        -> én fundet er vores, flere maa kunden
 *                                vaelge imellem
 */
zs_pick_t zs_locate_pick(const zs_found_t *fundet, size_t n,
                         const char *gemt_serial, size_t *valgt);

/* Hvordan det gik med at lede. */
typedef enum {
    ZS_LOC_SAMME = 0,     /* den sad hvor den plejer                  */
    ZS_LOC_NY,            /* samme inverter, ny adresse               */
    ZS_LOC_TAGET,         /* vi kendte intet serienummer, tog den ene  */
    ZS_LOC_INGEN,         /* ikke fundet paa nettet                   */
    ZS_LOC_FLERE,         /* flere invertere, ingen af dem er vores   */
    ZS_LOC_INTET_NET,     /* vi er ikke paa et net, kan ikke lede     */
    ZS_LOC_AFBRUDT        /* brugeren gik videre imens                */
} zs_loc_t;

/*
 * Leder efter inverteren. BLOKERER i op til omkring tyve sekunder, saa
 * den skal kaldes fra aflaesningsopgaven og aldrig fra skaermens.
 *
 *   ip_ud          den adresse der skal bruges. Skrives ved SAMME, NY
 *                  og TAGET.
 *   serial_ud      inverterens serienummer som vi laeste det. Skrives de
 *                  samme tre steder, saa kalderen kan gemme det foerste
 *                  gang. Maa vaere NULL.
 *   subnet         fx "192.168.1.0". Gives UDEFRA og hentes ikke herinde,
 *                  saa modulet ikke afhaenger af wifi-laget og kan
 *                  proeves af paa en almindelig maskine. Tom eller NULL
 *                  giver INTET_NET.
 *   frem/ctx       kaldes undervejs saa skaermen kan vise at der sker
 *                  noget. Maa vaere NULL.
 */
zs_loc_t zs_locate_find(const char *subnet,
                        const char *gemt_serial, const char *sidste_ip,
                        uint16_t port, uint8_t unit,
                        char *ip_ud, size_t ip_len,
                        char *serial_ud, size_t serial_len,
                        zs_discovery_progress_fn frem, void *ctx);

/* Kort tekst paa dansk til Detaljer-siden. Aldrig NULL. */
const char *zs_locate_text(zs_loc_t r);

#ifdef __cplusplus
}
#endif

#endif /* ZS_LOCATE_H */
