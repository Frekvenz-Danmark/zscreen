/*
 * zScreen - tal paa dansk.
 *
 * Skaermen skal kunne laeses paa afstand af folk der ikke arbejder med
 * el til daglig. Derfor:
 *   - komma som decimaltegn, ikke punktum
 *   - W under 1000, kW derover, saa tallet altid har faa cifre
 *   - vaerdi og enhed hver for sig, saa vi kan saette dem med
 *     forskellig skriftstoerrelse paa kortet
 *
 * Ingen af funktionerne allokerer, og de skriver altid en nul-afsluttet
 * streng. Ved daarligt input skriver de en tankestreg, aldrig skrald.
 */

#ifndef ZS_FORMAT_H
#define ZS_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char value[12];   /* fx "4,2" eller "850" eller "-"  */
    char unit[4];     /* fx "kW" eller "W" eller "%"     */
} zs_num_t;

/*
 * Effekt. Der formateres paa STOERRELSEN, ikke fortegnet: retningen
 * (koeber/saelger, lader/aflader) siges med ord under tallet i stedet,
 * fordi et minus foran et stort tal er nemt at overse paa afstand.
 *
 *      0 W      ->  "0"    "W"
 *    850 W      ->  "850"  "W"
 *   -850 W      ->  "850"  "W"
 *   1000 W      ->  "1,0"  "kW"
 *   4249 W      ->  "4,2"  "kW"
 *   4250 W      ->  "4,3"  "kW"
 *  99950 W      ->  "100"  "kW"
 */
void zs_fmt_power(float watts, zs_num_t *out);

/* Procent uden decimaler, klippet til 0-100. */
void zs_fmt_percent(float pct, zs_num_t *out);

/*
 * Kroner med to decimaler, fx "1,58" eller "-0,05".
 *
 * Fortegnet haandteres for sig. Regner man bare oere/100, forsvinder
 * minusset for alt mellem -1 og 0, fordi heltalsdivisionen giver nul,
 * og -0,05 kr bliver vist som "0,05". Negative timepriser er ikke
 * teoretiske: de forekommer flere gange om aaret naar det blaeser.
 *
 * Uden enhed. Kalderen saetter selv "kr" eller "kr/kWh" efter.
 */
void zs_fmt_kroner(float kr, char *ud, size_t n);

/* Energi i kWh med ét decimal, MWh over 1000. */
void zs_fmt_energy_wh(double wh, zs_num_t *out);

/* "Ingen data". Bruges naar en maaling mangler. */
void zs_fmt_none(zs_num_t *out);

/* Erstatter punktum med komma paa stedet. Returnerer s. */
char *zs_fmt_da_decimal(char *s);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FORMAT_H */
