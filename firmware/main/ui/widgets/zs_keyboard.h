/*
 * zScreen - dansk tastatur.
 *
 *     ┌─────────────────────────────────────────┐
 *     │ q  w  e  r  t  y  u  i  o  p  å         │
 *     │ a  s  d  f  g  h  j  k  l  æ  ø         │
 *     │ ⇧  z  x  c  v  b  n  m           ⌫      │
 *     │ 123      mellemrum            ✓         │
 *     └─────────────────────────────────────────┘
 *
 * Hvorfor ikke LVGL's eget lv_keyboard:
 *   Det har ikke æ, ø og å, og dets indbyggede haandtering leder efter
 *   bestemte FontAwesome-tegn til skift, slet og faerdig. De tegn
 *   findes ikke i vores skrifttyper, saa der ville staa firkanter paa
 *   tre af tasterne. Vi bruger LVGL's knapgitter i stedet og skriver
 *   selv de faa linjer der skal til. Saa er layoutet ogsaa vores eget
 *   og kan rettes uden at kaempe mod et bibliotek.
 *
 * Om tastestoerrelsen:
 *   Hver tast er omkring 43 x 48 pixels, altsaa smallere end de 44 vi
 *   ellers kraever. Det er med vilje. Et tastatur er den ene undtagelse:
 *   alle telefoner har smallere taster end det, fordi de staar i et
 *   gitter man kender i forvejen og sigter efter med tommelfingeren.
 *   Enkeltstaaende knapper er stadig 44 x 44.
 */

#ifndef ZS_KEYBOARD_H
#define ZS_KEYBOARD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZS_KB_HEIGHT   224

typedef struct zs_keyboard zs_keyboard_t;

/*
 * done_cb kaldes naar brugeren trykker paa flueben-tasten.
 * Teksten hentes med zs_keyboard_get_text().
 */
zs_keyboard_t *zs_keyboard_create(lv_obj_t *parent, lv_obj_t *target,
                                  lv_event_cb_t done_cb, void *user_data);

/* Teksten der er skrevet indtil nu. Aldrig NULL. */
const char *zs_keyboard_get_text(zs_keyboard_t *kb);

void zs_keyboard_clear(zs_keyboard_t *kb);

/* Viser eller skjuler tegnene i indtastningsfeltet. */
void zs_keyboard_set_password_hidden(zs_keyboard_t *kb, bool hidden);
bool zs_keyboard_get_password_hidden(zs_keyboard_t *kb);

#ifdef __cplusplus
}
#endif

#endif /* ZS_KEYBOARD_H */
