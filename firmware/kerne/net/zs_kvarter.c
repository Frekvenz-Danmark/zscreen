#include "zs_kvarter.h"

#include <string.h>

int32_t zs_kvarter_boette(uint16_t dagnr, int kvarter)
{
    return (int32_t)dagnr * ZS_KVARTER_PR_DOEGN + (int32_t)kvarter;
}

int zs_kvarter_nummer(int time, int minut)
{
    if (time < 0 || time > 23 || minut < 0 || minut > 59) {
        return -1;
    }
    return time * 4 + minut / 15;
}

/* Nulstiller til en ny boette. */
static void begynd(zs_kvarter_t *s, int32_t boette)
{
    memset(s->sum, 0, sizeof(s->sum));
    memset(s->antal, 0, sizeof(s->antal));
    s->boette = boette;
    s->begyndt = true;
}

zs_kvarter_ud_t zs_kvarter_tik(zs_kvarter_t *s, const float *vaerdi,
                               const bool *har, int32_t boette_nu)
{
    zs_kvarter_ud_t ud;
    memset(&ud, 0, sizeof(ud));

    if (s == NULL || vaerdi == NULL || har == NULL) {
        return ud;
    }

    if (!s->begyndt) {
        /* Foerste gang. Der er intet at sende, vi begynder bare.
         *
         * Og vi sender IKKE et gennemsnit af det foerste kvarter vi
         * tilfaeldigvis lander midt i. Starter skaermen 14 minutter inde i
         * et kvarter, daekker det ene minut ikke kvarteret, og det ville se
         * ud som en maaling der gjaldt hele kvarteret. */
        begynd(s, boette_nu);
    } else if (boette_nu != s->boette) {
        /*
         * Kvarteret er skiftet. Har vi noget, og er det det kvarter der
         * ligger LIGE FOER dette, saa send det.
         *
         * Er der et hul, smider vi det vaek. Det sker hvis skaermen har
         * vaeret slukket, hvis uret er blevet rettet, eller ved aarsskiftet
         * hvor dagnummeret gaar tilbage til nul. Et gennemsnit vi ikke kan
         * placere rigtigt, er vaerre end ingenting: det ville ligge paa
         * tidslinjen som om det var nyt.
         */
        if (boette_nu == s->boette + 1) {
            bool noget = false;
            for (int i = 0; i < ZS_KV_ANTAL; i++) {
                if (s->antal[i] > 0) {
                    ud.vaerdi[i] = (float)(s->sum[i] / (double)s->antal[i]);
                    ud.har[i] = true;
                    noget = true;
                }
            }
            if (noget) {
                ud.klar = true;
                ud.boette = s->boette;
                ud.kvarter = (int8_t)(s->boette % ZS_KVARTER_PR_DOEGN);
            }
        }
        begynd(s, boette_nu);
    }

    /* Og saa tallene med i den boette vi er i nu. */
    for (int i = 0; i < ZS_KV_ANTAL; i++) {
        if (har[i]) {
            s->sum[i] += (double)vaerdi[i];
            s->antal[i]++;
        }
    }
    return ud;
}
