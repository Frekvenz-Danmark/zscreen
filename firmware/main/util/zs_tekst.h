/*
 * zScreen - de smaa tekstgreb, samlet ét sted.
 *
 * HVORFOR DEN FINDES.
 *
 * To slags kode stod skrevet forfra hver gang den blev brugt, og begge
 * slags er af den type der ikke fejler i dag, men i morgen.
 *
 * DEN FOERSTE er at kopiere en streng ud til en kalder der gerne maa
 * sige NULL. Vagten "ud != NULL && len > 0" stod fire steder. Hver gang
 * den skrives forfra er der en chance for at && bliver skrevet som ||,
 * og saa skriver vi i NULL. En mutationstest viste praecis det: paa
 * zs_locate.c kunne && laves til || uden at en eneste af 848 proever
 * sagde fra, fordi ingen af dem gav NULL med. Nu staar vagten ét sted,
 * og det sted er proevet af med NULL, med laengde nul og med en streng
 * der er for lang.
 *
 * DEN ANDEN er at sammenligne tekst uden at skele til store og smaa
 * bogstaver. Det bruges til at genkende inverterens fabrikat, se
 * zs_maerker.h. Her fandt mutationstesten en rigtig fejl der bare ikke
 * var sprunget endnu: koden lavede store bogstaver om med
 *
 *      if (c >= 'A' && c <= 'Z')
 *
 * og den oevre graense var aldrig blevet proevet af, fordi det eneste
 * maerke i tabellen hedder "Fronius" og ikke indeholder et Z. Den dag
 * nogen skriver Zeversolar eller ZCS i tabellen, ville maerket ikke
 * kunne findes, og vi ville lede efter fejlen i afkodningen. Nu sker
 * ombytningen ét sted, og alle 256 byte-vaerdier er proevet af.
 *
 * Vi bruger IKKE tolower() fra ctype.h. Den ser paa maskinens sprogvalg,
 * og den vil have et unsigned char ind: giver man den et almindeligt
 * char med et tegn over 127, er opfoerslen udefineret. Modbus giver os
 * raa bytes fra en inverter vi ikke bestemmer over, saa vi holder os til
 * de 26 bogstaver vi kender.
 *
 * Ingenting herinde kommer fra ESP-IDF, saa den koerer ogsaa i
 * tests/host paa en almindelig maskine.
 */

#ifndef ZS_TEKST_H
#define ZS_TEKST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Goer ét ASCII-bogstav lille. Alt andet, ogsaa bytes over 127, kommer
 * uaendret tilbage. Intet sprogvalg, ingen udefineret opfoersel.
 */
char zs_tekst_smaa(char c);

/*
 * Goer ét ASCII-bogstav stort. Som zs_tekst_smaa, bare den anden vej.
 *
 * Den findes fordi zs_sunspec.c normaliserede kanalnavne med toupper()
 * fra ctype.h. Det virkede, men det ser paa maskinens sprogvalg, og vi
 * har allerede den samme ombytning den anden vej herinde. To steder at
 * tage fejl er ét for mange, saa nu ligger begge veje her og begge er
 * proevet af for alle 256 byte-vaerdier.
 */
char zs_tekst_stort(char c);

/*
 * Kopierer fra ud i ud, og afslutter ALTID med nul saa laenge der er
 * plads til mindst ét tegn.
 *
 *   ud == NULL eller ud_len == 0   der sker ingenting
 *   fra == NULL                    ud bliver en tom streng
 *   fra laengere end der er plads  den bliver klippet, stadig afsluttet
 *
 * EN KLIPNING LANDER MELLEM TO TEGN. Teksten er UTF-8, og æ, ø og å
 * fylder to bytes; en klipning midt i et par kan ikke tegnes. Derfor kan
 * en klippet streng vaere ÉN byte kortere end der var plads til. Se
 * noten i .c-filen.
 *
 * Returnerer true hvis hele strengen kom med, saa en kalder der ikke maa
 * taale en klipning kan sige fra. De fleste kaldere er ligeglade.
 */
bool zs_tekst_kopi(char *ud, size_t ud_len, const char *fra);

/*
 * Er naal at finde inde i hoejstak, uden at skele til store og smaa
 * bogstaver? NULL eller tom naal giver false: "alt indeholder ingenting"
 * er et rigtigt svar i teorien, men det vil her altid vaere en fejl i
 * kalderen, og at finde et maerke fordi tabellen havde en tom tekst er
 * vaerre end ikke at finde det.
 */
bool zs_tekst_indeholder(const char *hoejstak, const char *naal);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TEKST_H */
