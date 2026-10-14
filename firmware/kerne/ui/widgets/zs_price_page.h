/*
 * zScreen - side 3: spotprisen i dag.
 *
 * ┌────────────────────────────────────────┐
 * │ SPOTPRIS NU                       DK2  │
 * │                                        │
 * │ 0,62 kr/kWh                            │
 * │ Billigt lige nu                        │
 * │                                        │
 * │ ▁▂▃▅▆█▇▅▃▂▁▁▂▃▅▇█▆▄▂▁▁▂▃               │
 * │ 00     06     12     18     23         │
 * │                                        │
 * │ ┌───────────────┐ ┌───────────────┐    │
 * │ │ BILLIGST      │ │ DYREST        │    │
 * │ │ 0,62 kr       │ │ 1,58 kr       │    │
 * │ │ kl. 13        │ │ kl. 19        │    │
 * │ └───────────────┘ └───────────────┘    │
 * │                                        │
 * │ Spotpris uden transport, afgifter      │
 * │ og moms                                │
 * └────────────────────────────────────────┘
 *
 * Soejlerne er groenne under dagens gennemsnit og roede over. Den time
 * vi er i lige nu staar i Frekvenz-orange.
 *
 * Ordet "spotpris" staar to steder med vilje. Det er den raa boerspris,
 * ikke det kunden betaler, og forskellen er mere end det dobbelte.
 */

#ifndef ZS_PRICE_PAGE_H
#define ZS_PRICE_PAGE_H

#include "lvgl.h"
#include "zs_price.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hoejst saa mange soejler. Samme graense som i zs_price.h, fordi et
 * doegn har 25 timer den nat sommertiden slutter. */
#define ZS_PRICE_BARS   ZS_PRICE_MAX_HOURS

typedef struct {
    lv_obj_t *page;
    lv_obj_t *label;
    lv_obj_t *zone;
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *word;
    lv_obj_t *chart;
    lv_obj_t *bar[ZS_PRICE_BARS];
    lv_obj_t *axis[4];
    lv_obj_t *low_card,  *low_value,  *low_time;
    lv_obj_t *high_card, *high_value, *high_time;
    lv_obj_t *note;
    lv_obj_t *error;
} zs_price_page_t;

void zs_price_page_create(zs_price_page_t *p, lv_obj_t *parent);
void zs_price_page_update(zs_price_page_t *p, const zs_price_day_t *d);

#ifdef __cplusplus
}
#endif

#endif /* ZS_PRICE_PAGE_H */
