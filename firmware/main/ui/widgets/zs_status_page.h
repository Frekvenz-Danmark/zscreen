/*
 * zScreen - side 3: inverterens tilstand og fejl.
 *
 * ┌────────────────────────────────────────┐
 * │            ✓                           │
 * │        Alt virker                      │
 * │        Producerer                      │
 * ├────────────────────────────────────────┤
 * │  ⚠  For høj temperatur                 │
 * │     Fronius-kode 303, 304, 322         │
 * │  ⚠  Fejl på blæser                     │
 * │     Fronius-kode 314, 315              │
 * └────────────────────────────────────────┘
 *
 * Er der ingen fejl, staar der ét groent felt og en enkelt linje om
 * hvad inverteren laver. Er der fejl, bliver feltet gult eller roedt
 * og listen fyldes ud nedenunder.
 *
 * Ved koder vi ikke kan saette ord paa, staar den raa vaerdi og et
 * telefonnummer. Vi gaetter aldrig: en forkert forklaring paa en fejl
 * er vaerre end ingen.
 */

#ifndef ZS_STATUS_PAGE_H
#define ZS_STATUS_PAGE_H

#include "lvgl.h"
#include "zs_screen_home.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *page;
    lv_obj_t *hero;         /* det store felt foroven      */
    lv_obj_t *hero_icon;
    lv_obj_t *hero_title;
    lv_obj_t *hero_sub;
    lv_obj_t *list;         /* fejlene nedenunder          */
} zs_status_page_t;

void zs_status_page_create(zs_status_page_t *p, lv_obj_t *parent);
void zs_status_page_update(zs_status_page_t *p, const zs_home_data_t *d);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STATUS_PAGE_H */
