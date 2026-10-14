#include "zs_tilegrid.h"
#include "zs_layout.h"

#include <stddef.h>

int zs_tilegrid(int n, zs_rect_t *ud)
{
    if (ud == NULL || n < 1 || n > ZS_TILES_MAX) {
        return 0;
    }

    const int bred  = ZS_G_CONTENT_WIDTH;                 /* 456 */
    const int halv  = (bred - ZS_G_GRID_GAP) / 2;         /* 222 */
    const int hoej  = ZS_G_CARD_HEIGHT;                   /* 186 */
    const int v     = ZS_G_EDGE;                          /* 12  */

    /* Hvor mange raekker fylder opsaetningen. */
    const int raekker = (n >= 3) ? 2 : n;

    /*
     * Blokken midtstilles lodret i sidens hoejde.
     *
     * Med to raekker giver det praecis ZS_G_EDGE foroven og forneden,
     * altsaa det samme som foer. Med én raekke lander den midt paa
     * skaermen i stedet for at ligge og klistre i toppen med et stort
     * tomt felt under sig.
     */
    const int blok = raekker * hoej + (raekker - 1) * ZS_G_GRID_GAP;
    const int y0   = (ZS_G_PAGE_HEIGHT - blok) / 2;

    switch (n) {
    case 4:
        ud[0] = (zs_rect_t){ v,                        y0,                        halv, hoej };
        ud[1] = (zs_rect_t){ v + halv + ZS_G_GRID_GAP, y0,                        halv, hoej };
        ud[2] = (zs_rect_t){ v,                        y0 + hoej + ZS_G_GRID_GAP, halv, hoej };
        ud[3] = (zs_rect_t){ v + halv + ZS_G_GRID_GAP, y0 + hoej + ZS_G_GRID_GAP, halv, hoej };
        break;
    case 3:
        ud[0] = (zs_rect_t){ v,                        y0,                        halv, hoej };
        ud[1] = (zs_rect_t){ v + halv + ZS_G_GRID_GAP, y0,                        halv, hoej };
        /* Den sidste fylder hele bredden. Uden det stod der et tomt
         * felt ved siden af, og et hul i et gitter ligner en fejl. */
        ud[2] = (zs_rect_t){ v,                        y0 + hoej + ZS_G_GRID_GAP, bred, hoej };
        break;
    case 2:
        ud[0] = (zs_rect_t){ v, y0,                        bred, hoej };
        ud[1] = (zs_rect_t){ v, y0 + hoej + ZS_G_GRID_GAP, bred, hoej };
        break;
    default: /* 1 */
        ud[0] = (zs_rect_t){ v, y0, bred, hoej };
        break;
    }
    return n;
}
