/*
 * zScreen - hvor kasserne paa hovedskaermen ligger.
 *
 * Udregningen ligger for sig selv og bruger ikke LVGL, saa maalene kan
 * efterproeves af tests i stedet for af oejet. Det er ikke pedanteri:
 * et kort der stikker to pixels ud over kanten, eller to kort der
 * overlapper med én pixel, ser ud som daarligt haandvaerk paa en skaerm
 * der haenger paa en vaeg og bliver set hver dag.
 *
 * Antallet af kasser afhaenger af anlaegget:
 *
 *   4   inverter, elmaaler og batteri
 *   3   inverter og elmaaler, intet batteri
 *   2   inverter og batteri, ingen elmaaler
 *   1   kun inverter
 *
 * Vi viser IKKE en tom kasse der siger "intet batteri". Kunden har ikke
 * et batteri og skal ikke mindes om det hver gang han gaar forbi.
 */

#ifndef ZS_TILEGRID_H
#define ZS_TILEGRID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x, y, w, h;
} zs_rect_t;

/* Hoejst saa mange kasser er der plads til. */
#define ZS_TILES_MAX  4

/*
 * Fylder ud[0..n-1] med kassernes pladser, i den raekkefoelge de skal
 * laeses. Returnerer false hvis n ligger udenfor 1 til ZS_TILES_MAX,
 * og roerer saa ikke ud.
 *
 * Opsaetningerne:
 *
 *   n=4        n=3        n=2        n=1
 *   ┌──┬──┐    ┌──┬──┐    ┌─────┐    ┌─────┐
 *   │0 │1 │    │0 │1 │    │  0  │    │  0  │
 *   ├──┼──┤    ├──┴──┤    ├─────┤    └─────┘
 *   │2 │3 │    │  2  │    │  1  │    (midtstillet)
 *   └──┴──┘    └─────┘    └─────┘
 *
 * Raekkehoejden er den samme i alle fire, saa det store tal fylder lige
 * meget uanset hvor mange kasser der er. Med faerre end tre raekker
 * bliver hele blokken midtstillet lodret.
 */
int zs_tilegrid(int n, zs_rect_t *ud);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TILEGRID_H */
