/*
 * zScreen - energiflow.
 *
 * Side 2 paa hovedskaermen. Samme fire tal som side 1, men tegnet som
 * stroemmens vej gennem huset. Den viser noget de fire kasser ikke
 * kan: hvilken RETNING stroemmen loeber.
 *
 *                  ☀
 *                4,2 kW
 *                   │
 *                   ▼
 *      ⌂  ◄─────── ( ) ───────►  ⚡
 *    1,8 kW                    2,1 kW
 *    Forbrug                   Sælger
 *                   │
 *                   ▼
 *                  ▤
 *                 78 %
 *              Lader 1,4 kW
 *
 * Linjerne er graa naar der ikke loeber noget, og faar farve og en pil
 * naar der goer. Ingen animation: pilen og farven siger det hele, og
 * en skaerm der bevaeger sig hele tiden er traettende at have paa en
 * vaeg.
 *
 * Mangler en maaling, staar den som en streg og linjen forbliver graa.
 * Vi opfinder aldrig et nul.
 */

#ifndef ZS_FLOW_H
#define ZS_FLOW_H

#include "lvgl.h"
#include "zs_screen_home.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *page;

    lv_obj_t *sun_icon,  *sun_value;
    lv_obj_t *house_icon,*house_value, *house_label;
    lv_obj_t *grid_icon, *grid_value,  *grid_label;
    lv_obj_t *bat_icon,  *bat_value,   *bat_sub;

    lv_obj_t *hub;
    lv_obj_t *line_sun,  *arrow_sun;
    lv_obj_t *line_house,*arrow_house;
    lv_obj_t *line_grid, *arrow_grid;
    lv_obj_t *line_bat,  *arrow_bat;
} zs_flow_t;

/* Bygger siden. parent skal vaere ZS_SCR_WIDTH x ZS_PAGE_HEIGHT. */
void zs_flow_create(zs_flow_t *f, lv_obj_t *parent);

/* Tegner om ud fra de samme data som side 1 faar. */
void zs_flow_update(zs_flow_t *f, const zs_home_data_t *d);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLOW_H */
