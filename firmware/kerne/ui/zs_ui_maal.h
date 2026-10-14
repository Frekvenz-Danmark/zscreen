/*
 * zScreen - maaler fladen paa den rigtige skaerm.
 *
 * HVORFOR DEN FINDES.
 *
 * Fejl i en flade ser man. Men den der skriver koden sidder ikke altid
 * foran skaermen, og en fejl der kun KAN ses, bliver fundet af en kunde.
 *
 * 14. oktober 2026 blev fladen gjort skalerbar. Bagefter stod der paa
 * ZDisplay: maerket "Forbundet" oven i Z-logoet, wifi-ikonet 6 px for
 * lavt, tandhjulet 19 px ud over hoejre kant. Alle tre var regnefejl der
 * KUNNE vaere fanget med et maalebaand - men der var ikke noget
 * maalebaand.
 *
 * Det her er maalebaandet. Den gaar hele objekttraeet igennem paa hver
 * side og skriver i loggen hvad der er galt:
 *
 *   * noget der ligger uden for sin foraelder
 *   * noget man kan trykke paa, der er mindre end en finger
 *   * noget uden stoerrelse, altsaa usynligt
 *
 * Den dOEmmer ikke om det er pænt. Den siger hvad der er MAALT, og den
 * er tavs naar der ikke er noget at sige.
 */

#ifndef ZS_UI_MAAL_H
#define ZS_UI_MAAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maaler alle sider og skriver fundene i loggen. Giver antallet af fund,
 * saa en proeve kan kraeve nul.
 *
 * SKAL kaldes med LVGL-laasen taget, og efter at siderne er bygget.
 */
int zs_ui_maal_graenser(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_UI_MAAL_H */
