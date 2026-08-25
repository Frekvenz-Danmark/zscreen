/*
 * Test af hvor kasserne paa hovedskaermen ligger.
 *
 * Det ser ud som pynt at teste pixelvaerdier, men det er det ikke. Et
 * kort der stikker to pixels ud over kanten, to kort der overlapper med
 * én, eller en bund der ikke har lige saa meget luft som toppen, er
 * praecis det man ser paa en skaerm der haenger paa en vaeg. Og det er
 * ikke noget man opdager ved at kigge, kun ved at maale.
 */

#include "zs_test.h"
#include "../../firmware/main/ui/zs_tilegrid.h"
#include "../../firmware/main/ui/zs_layout.h"

#include <stdbool.h>

static bool overlapper(const zs_rect_t *a, const zs_rect_t *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w
        && a->y < b->y + b->h && b->y < a->y + a->h;
}

void test_tilegrid(void)
{
    ZS_SUITE("Kassernes pladser: hvor mange");

    {
        zs_rect_t r[ZS_TILES_MAX];
        CHECK("nul kasser giver ingenting", zs_tilegrid(0, r) == 0);
        CHECK("fem kasser giver ingenting", zs_tilegrid(5, r) == 0);
        CHECK("minus giver ingenting",      zs_tilegrid(-1, r) == 0);
        CHECK("NULL giver ingenting",       zs_tilegrid(4, NULL) == 0);
        for (int n = 1; n <= ZS_TILES_MAX; n++) {
            CHECK("et til fire virker", zs_tilegrid(n, r) == n);
        }
    }

    ZS_SUITE("Kassernes pladser: alt indenfor kanten");

    {
        const int venstre = ZS_G_EDGE;
        const int hoejre  = ZS_G_SCR_WIDTH - ZS_G_EDGE;

        for (int n = 1; n <= ZS_TILES_MAX; n++) {
            zs_rect_t r[ZS_TILES_MAX];
            zs_tilegrid(n, r);
            for (int i = 0; i < n; i++) {
                CHECK("intet stikker ud til venstre",  r[i].x >= venstre);
                CHECK("intet stikker ud til hoejre",   r[i].x + r[i].w <= hoejre);
                CHECK("intet stikker op over siden",   r[i].y >= 0);
                CHECK("intet stikker ned under siden",
                      r[i].y + r[i].h <= ZS_G_PAGE_HEIGHT);
                CHECK("ingen kasse er tom",            r[i].w > 0 && r[i].h > 0);
            }
        }
    }

    ZS_SUITE("Kassernes pladser: ingen overlap");

    {
        for (int n = 1; n <= ZS_TILES_MAX; n++) {
            zs_rect_t r[ZS_TILES_MAX];
            zs_tilegrid(n, r);
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    CHECK("to kasser ligger aldrig oven i hinanden",
                          !overlapper(&r[i], &r[j]));
                }
            }
        }
    }

    ZS_SUITE("Kassernes pladser: alt flugter");

    {
        const int venstre = ZS_G_EDGE;
        const int hoejre  = ZS_G_SCR_WIDTH - ZS_G_EDGE;

        for (int n = 1; n <= ZS_TILES_MAX; n++) {
            zs_rect_t r[ZS_TILES_MAX];
            zs_tilegrid(n, r);

            /* Mindst én kasse skal roere venstre kant, og mindst én
             * skal roere hoejre. Ellers staar blokken skaevt. */
            bool roerer_v = false, roerer_h = false;
            int  hoejest = 0, laveste = ZS_G_PAGE_HEIGHT;
            for (int i = 0; i < n; i++) {
                if (r[i].x == venstre)             { roerer_v = true; }
                if (r[i].x + r[i].w == hoejre)     { roerer_h = true; }
                if (r[i].y < laveste)              { laveste = r[i].y; }
                if (r[i].y + r[i].h > hoejest)     { hoejest = r[i].y + r[i].h; }
                CHECK("alle kasser er lige hoeje", r[i].h == ZS_G_CARD_HEIGHT);
            }
            CHECK("noget roerer venstre kant", roerer_v);
            CHECK("noget roerer hoejre kant",  roerer_h);

            /* Lige meget luft foroven og forneden. */
            int over = laveste;
            int under = ZS_G_PAGE_HEIGHT - hoejest;
            CHECK("lige meget luft over og under", over == under
                  || over + 1 == under || under + 1 == over);
        }
    }

    ZS_SUITE("Kassernes pladser: de fire opsaetninger");

    {
        zs_rect_t r[ZS_TILES_MAX];
        const int halv = (ZS_G_CONTENT_WIDTH - ZS_G_GRID_GAP) / 2;

        /* To halve plus mellemrummet skal give hele bredden. Gaar det
         * ikke op, staar der en pixel tilbage i hoejre side, og den
         * ses. */
        CHECK("to halve kasser fylder bredden praecis",
              2 * halv + ZS_G_GRID_GAP == ZS_G_CONTENT_WIDTH);

        zs_tilegrid(4, r);
        CHECK("fire: to raekker a to",  r[0].w == halv && r[3].w == halv);
        CHECK("fire: to oeverst",       r[0].y == r[1].y);
        CHECK("fire: to nederst",       r[2].y == r[3].y);
        CHECK("fire: nederste raekke er under den oeverste", r[2].y > r[0].y);
        CHECK("fire: mellemrummet er praecis",
              r[2].y - (r[0].y + r[0].h) == ZS_G_GRID_GAP);
        CHECK("fire: samme lodrette placering som foer", r[0].y == ZS_G_EDGE);

        zs_tilegrid(3, r);
        CHECK("tre: to halve oeverst",  r[0].w == halv && r[1].w == halv);
        CHECK("tre: den sidste fylder hele bredden",
              r[2].w == ZS_G_CONTENT_WIDTH);
        CHECK("tre: den sidste staar nederst", r[2].y > r[0].y);
        CHECK("tre: samme hoejde som med fire", r[2].h == ZS_G_CARD_HEIGHT);

        zs_tilegrid(2, r);
        CHECK("to: begge fylder bredden",
              r[0].w == ZS_G_CONTENT_WIDTH && r[1].w == ZS_G_CONTENT_WIDTH);
        CHECK("to: den ene over den anden", r[1].y > r[0].y);

        zs_tilegrid(1, r);
        CHECK("en: fylder bredden", r[0].w == ZS_G_CONTENT_WIDTH);
        CHECK("en: staar midt paa skaermen",
              r[0].y * 2 + r[0].h == ZS_G_PAGE_HEIGHT
              || r[0].y * 2 + r[0].h == ZS_G_PAGE_HEIGHT - 1);
    }
}
