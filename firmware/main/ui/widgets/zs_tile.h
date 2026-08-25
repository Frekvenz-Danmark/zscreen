/*
 * zScreen - ét af de fire kort paa hovedskaermen.
 *
 * Kortet har tre baand: en overskrift med ikon, et stort tal med
 * enhed, og en linje undertekst der siger hvad der sker.
 *
 *     ┌────────────────────────────┐
 *     │ SOLCELLER              ☀   │
 *     │                            │
 *     │ 4,2 kW                     │
 *     │                            │
 *     │ Producerer nu              │
 *     └────────────────────────────┘
 *
 * Farvereglen er én saetning: tallet staar i Frekvenz-orange naar der
 * sker noget paa kortet, og i hvidt naar der ikke goer. Saa lyser
 * skaermen op naar anlaegget arbejder, uden at alle fire kort raaber
 * ad brugeren hele tiden.
 */

#ifndef ZS_TILE_H
#define ZS_TILE_H

#include "lvgl.h"
#include "zs_sunspec.h"   /* zs_val_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *card;
    lv_obj_t *head_label;
    lv_obj_t *head_icon;
    lv_obj_t *value_box;   /* holder tallet og enheden side om side */
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *sub_icon;
    lv_obj_t *sub_text;
    bool      stale;
} zs_tile_t;

/*
 * Bygger kortet paa den angivne plads i 2x2-gitteret.
 * col og row er 0 eller 1. Placeringen regnes ud af maalene i
 * zs_theme.h, saa der ikke staar pixelvaerdier spredt ud i koden.
 */
void zs_tile_create(zs_tile_t *t, lv_obj_t *parent, int col, int row,
                    const char *label, const char *icon);

/* Effekt i watt. Tallet vises som stoerrelse, retningen siges med ord
 * i underteksten. active bestemmer om tallet staar i accentfarven. */
void zs_tile_set_power(zs_tile_t *t, zs_val_t watt, bool active);

/* Ladetilstand i procent. */
void zs_tile_set_percent(zs_tile_t *t, zs_val_t pct, bool active);

/* Ingen maaling. Viser en streg i stedet for et tal, og en forklaring
 * i underteksten. Aldrig et nul, for nul betyder noget andet. */
void zs_tile_set_none(zs_tile_t *t, const char *why);

/* Underteksten. icon og text maa vaere NULL. */
void zs_tile_set_sub(zs_tile_t *t, const char *icon, const char *text,
                     uint32_t color);

/*
 * Gammel maaling.
 *
 * Vi rydder ikke kortet naar forbindelsen ryger. Det sidst kendte tal,
 * tydeligt daempet, fortaeller mere end et tomt felt: man kan se hvad
 * anlaegget lavede lige inden det holdt op med at svare.
 */
void zs_tile_set_stale(zs_tile_t *t, bool stale);

#ifdef __cplusplus
}
#endif

#endif /* ZS_TILE_H */
