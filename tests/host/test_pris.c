/*
 * Test af hvilken times pris der er den rigtige lige nu.
 *
 * Den beslutning var FORKERT i hele projektets levetid, og den kunne kun
 * ses én nat om aaret. Opslaget sammenlignede kun klokketimen, og den
 * nat sommertiden slutter findes klokken to to gange: én gang med
 * forskydning +02 og én gang med +01. Filen fra elprisenligenu.dk har
 * begge, 25 poster i alt. Opslaget tog den foerste begge gange, saa i den
 * anden time stod der den forkerte pris i en hel time.
 *
 * Koden vidste godt at doegnet kan have 25 timer, der er plads til dem.
 * Det var kun opslaget der ikke vidste det.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_price.h"

#include <string.h>

/* Et doegn med 24 almindelige timer, alle med samme forskydning. */
static void almindelig_dag(zs_price_day_t *d, int8_t offset)
{
    memset(d, 0, sizeof(*d));
    d->ok = true;
    for (int i = 0; i < 24; i++) {
        d->timer[i].hour = (uint8_t)i;
        d->timer[i].utc_offset_h = offset;
        d->timer[i].dkk = 1.0f + (float)i / 100.0f;
    }
    d->antal = 24;
}

/*
 * Natten sommertiden slutter, sidste soendag i oktober. Klokken tre
 * bliver til to, saa raekken er 0,1,2,2,3,...,23 og der er 25 poster.
 * De to toere har forskellig forskydning.
 */
static void nat_med_25_timer(zs_price_day_t *d)
{
    memset(d, 0, sizeof(*d));
    d->ok = true;
    uint8_t i = 0;
    for (int h = 0; h <= 1; h++) {
        d->timer[i].hour = (uint8_t)h;
        d->timer[i].utc_offset_h = 2;        /* stadig sommertid */
        d->timer[i].dkk = 0.50f;
        i++;
    }
    d->timer[i].hour = 2; d->timer[i].utc_offset_h = 2; d->timer[i].dkk = 0.10f; i++;
    d->timer[i].hour = 2; d->timer[i].utc_offset_h = 1; d->timer[i].dkk = 2.90f; i++;
    for (int h = 3; h <= 23; h++) {
        d->timer[i].hour = (uint8_t)h;
        d->timer[i].utc_offset_h = 1;        /* normaltid resten af doegnet */
        d->timer[i].dkk = 1.00f;
        i++;
    }
    d->antal = i;
}

void test_pris_time(void)
{
    ZS_SUITE("Hvilken times pris gælder nu");

    {
        zs_price_day_t d;
        almindelig_dag(&d, 2);
        CHECK_INT("almindelig dag, klokken 0", zs_price_find_hour(&d, 0, 2), 0);
        CHECK_INT("almindelig dag, klokken 13", zs_price_find_hour(&d, 13, 2), 13);
        CHECK_INT("almindelig dag, klokken 23", zs_price_find_hour(&d, 23, 2), 23);
    }

    {
        /* DET DER VAR GALT. */
        zs_price_day_t d;
        nat_med_25_timer(&d);
        CHECK_INT("der er 25 poster den nat", d.antal, 25);

        int8_t foerste = zs_price_find_hour(&d, 2, 2);
        int8_t anden   = zs_price_find_hour(&d, 2, 1);
        CHECK_INT("første gang klokken to: den med sommertid", foerste, 2);
        CHECK_INT("anden gang klokken to: den ANDEN post", anden, 3);
        CHECK("og de to er ikke den samme", foerste != anden);
        CHECK("prisen er forskellig, og det er hele pointen",
              d.timer[foerste].dkk != d.timer[anden].dkk);

        CHECK_INT("klokken ét er stadig klokken ét", zs_price_find_hour(&d, 1, 2), 1);
        CHECK_INT("klokken tre er efter de to toere", zs_price_find_hour(&d, 3, 1), 4);
        CHECK_INT("klokken 23 er sidste post", zs_price_find_hour(&d, 23, 1), 24);
    }

    {
        /* Natten sommertiden begynder: klokken to springes over, 23 poster. */
        zs_price_day_t d;
        memset(&d, 0, sizeof(d));
        d.ok = true;
        uint8_t i = 0;
        for (int h = 0; h <= 23; h++) {
            if (h == 2) { continue; }       /* den time findes ikke */
            d.timer[i].hour = (uint8_t)h;
            d.timer[i].utc_offset_h = (h < 2) ? 1 : 2;
            d.timer[i].dkk = 1.0f;
            i++;
        }
        d.antal = i;
        CHECK_INT("der er 23 poster den nat", d.antal, 23);
        CHECK_INT("klokken ét findes", zs_price_find_hour(&d, 1, 1), 1);
        CHECK_INT("klokken to findes IKKE", zs_price_find_hour(&d, 2, 2), -1);
        CHECK_INT("klokken tre findes", zs_price_find_hour(&d, 3, 2), 2);
    }

    {
        /*
         * Gamle gemte priser uden forskydning, og kilder der ikke
         * oplyser den. Saa skal det virke praecis som foer, i stedet for
         * pludselig ikke at vise nogen pris.
         */
        zs_price_day_t d;
        almindelig_dag(&d, ZS_PRICE_OFFSET_UKENDT);
        CHECK_INT("uden forskydning tæller kun klokketimen",
                  zs_price_find_hour(&d, 13, 2), 13);
        CHECK_INT("og en anden forskydning ændrer intet",
                  zs_price_find_hour(&d, 13, 1), 13);
    }

    {
        /* Det der ikke giver mening. */
        zs_price_day_t d;
        almindelig_dag(&d, 2);
        CHECK_INT("time 24 findes ikke", zs_price_find_hour(&d, 24, 2), -1);
        CHECK_INT("negativ time findes ikke", zs_price_find_hour(&d, -1, 2), -1);
        CHECK_INT("NULL giver minus én", zs_price_find_hour(NULL, 5, 2), -1);
        d.antal = 0;
        CHECK_INT("tom dag giver minus én", zs_price_find_hour(&d, 5, 2), -1);
    }
}
