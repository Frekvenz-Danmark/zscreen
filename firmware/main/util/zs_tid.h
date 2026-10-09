/*
 * zScreen - dagnummeret, regnet af den LOKALE dato.
 *
 * HVORFOR DEN FINDES.
 *
 * Baade timeenergien og kvartersgennemsnittet skal vide hvor laenge der
 * er gaaet siden sidst. De regner det af to tal: hvilken dag, og hvilken
 * time eller hvilket kvarter paa dagen.
 *
 *     gaaet = (dagnr - sidste_dagnr) * 24 + (time - sidste_time)
 *
 * Og er svaret ikke praecis 1, kaldes det et hul og tallet smides vaek.
 * Det er rigtigt: et gennemsnit vi ikke kan placere paa tidslinjen er
 * vaerre end ingenting.
 *
 * Men saa SKAL de to tal komme fra det SAMME ur. Foer regnede vi
 * dagnummeret af UTC-sekunder mens timen kom fra localtime_r, og saa
 * skiftede de ikke samtidig:
 *
 *   - ved lokal midnat wrappede timen 23 -> 0 mens UTC-dagnummeret stod
 *     stille, saa gaaet blev -23
 *   - ved UTC-midnat (lokal 01 om vinteren, 02 om sommeren) skiftede
 *     dagnummeret mens timen gik jaevnt videre, saa gaaet blev 25
 *
 * Begge blev kaldt huller. Maalt gennem modulernes egen kode over syv
 * doegn blev time 23 og time 00/01 ALDRIG sendt. To af 24 timer, og to af
 * 96 kvarterer, forsvandt hver eneste dag - og det er netop den historik
 * forbrugsprognosen regner paa.
 *
 * Derfor regnes dagnummeret her af den lokale dato. Saa skifter dag og
 * time samtidig, ved lokal midnat, og alle 24 timer kommer med.
 *
 * HVAD DET KOSTER VED SOMMERTIDSSKIFTET.
 *
 * Et lokalt doegn er 23 eller 25 timer to gange om aaret. Om foraaret
 * findes timen 02 ikke, saa der springes én time over - rigtigt, for den
 * time var der ikke. Om efteraaret kommer timen 02 to gange, og den
 * anden lander i samme boette som den foerste.
 *
 * To gange om AARET i stedet for to gange om DOEGNET.
 */

#ifndef ZS_TID_H
#define ZS_TID_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dage siden 1. januar 1970, regnet af en lokal dato.
 *
 *   lt   fra localtime_r. Kun tm_year, tm_mon og tm_mday bruges, saa
 *        tallet skifter praecis ved lokal midnat og ikke en time ved
 *        siden af.
 *
 * Giver 0 hvis lt er NULL eller datoen er foer 1970. Raekker til aar
 * 2149, og dagnummeret er altid voksende inden for det.
 */
uint16_t zs_tid_dagnr(const struct tm *lt);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TID_H */
