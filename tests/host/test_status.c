/*
 * zScreen - inverterens fejl, oversat til det kunden LAESER.
 *
 * HVORFOR DEN HER PROEVE KOM TIL SENT.
 *
 * zs_status.c er 249 linjer ren regning uden en linje LVGL, og den havde
 * INGEN proever. Den blev fundet ved at gaa modulerne igennem og spoerge
 * hvilke der kunne proeves paa en Mac og ikke blev det.
 *
 * Og det er netop den fil det er vaerst at tage fejl i: den afgoer hvad
 * der staar paa skaermen naar noget er galt. En fejl her betyder at der
 * staar "alt virker" mens inverteren melder en jordfejl - og modulets
 * egen kommentar siger det hoejt: "At tie om en fejl fordi man ikke
 * kender den, er den daarligste af alle muligheder."
 *
 * Saa det er praecis den regel der proeves her.
 */

#include "zs_test.h"
#include "../../firmware/main/app/zs_status.h"
#include "../../firmware/main/zs_config.h"

#include <string.h>

/* En aflaesning hvor intet er galt. */
static zs_fr_live_t rent(void)
{
    zs_fr_live_t l;
    memset(&l, 0, sizeof(l));
    l.status_ok = true;         /* UDEN den melder modulet "ingen tilstand" */
    l.inverter_state = 4;       /* 4 = producerer, se zs_status_state_text */
    l.vendor_state = -1;        /* -1 = ingen kode */
    l.charge_status = -1;
    return l;
}

static bool nogen_post_har(const zs_status_list_t *l, const char *naal)
{
    for (uint8_t i = 0; i < l->antal; i++) {
        if (strstr(l->poster[i].tekst, naal) != NULL
            || strstr(l->poster[i].detalje, naal) != NULL) {
            return true;
        }
    }
    return false;
}

void test_status(void)
{
    zs_status_list_t l;
    ZS_SUITE("Inverterens fejl paa dansk");

    /* Intet galt: ingen linjer, og intet der ser ud som en fejl. */
    {
        zs_fr_live_t live = rent();
        zs_status_build(&l, &live, NULL);
        CHECK_INT("en ren aflaesning giver ingen linjer", l.antal, 0);
        CHECK_INT("og vaerste alvor er OK", l.vaerst, ZS_SEV_OK);
        CHECK("og listen er ikke afkortet", !l.afkortet);
    }

    /* Én kendt bit: én linje, med teksten fra tabellen. */
    {
        zs_fr_live_t live = rent();
        live.evt1 = 1u << 0;            /* Jordfejl, ZS_SEV_FAULT */
        zs_status_build(&l, &live, NULL);
        CHECK_INT("én kendt bit giver én linje", l.antal, 1);
        CHECK_STR("og teksten er tabellens", l.poster[0].tekst, "Jordfejl");
        CHECK_INT("alvoren er fejl", l.poster[0].sev, ZS_SEV_FAULT);
        CHECK_INT("og vaerste alvor foelger med", l.vaerst, ZS_SEV_FAULT);
    }

    /* VAERST er den HOEJESTE, ikke den sidste. */
    {
        zs_fr_live_t live = rent();
        live.evt1 = (1u << 7) | (1u << 6);  /* FAULT og INFO */
        zs_status_build(&l, &live, NULL);
        CHECK_INT("to bits giver to linjer", l.antal, 2);
        CHECK_INT("vaerst er den hoejeste alvor, uanset raekkefoelgen",
                  l.vaerst, ZS_SEV_FAULT);
    }
    {
        /* Og omvendt: kun en INFO maa ikke blive meldt som en fejl. */
        zs_fr_live_t live = rent();
        live.evt1 = 1u << 6;                /* Slukket manuelt, INFO */
        zs_status_build(&l, &live, NULL);
        CHECK_INT("kun en info giver kun info", l.vaerst, ZS_SEV_INFO);
    }

    /*
     * DEN VIGTIGSTE REGEL I HELE MODULET.
     *
     * En bit vi IKKE har en tekst til maa ikke blive sprunget over. Saa
     * ville skaermen tie om en fejl inverteren melder. Den skal give en
     * linje med den raa vaerdi og et telefonnummer.
     */
    {
        zs_fr_live_t live = rent();
        live.evt1 = 1u << 31;           /* ingen tekst til bit 31 */
        zs_status_build(&l, &live, NULL);
        CHECK("en UKENDT bit giver en linje, den bliver ikke sprunget over",
              l.antal > 0);
        CHECK("og telefonnummeret staar der, saa kunden kan ringe",
              nogen_post_har(&l, ZS_SUPPORT_PHONE));
        CHECK("sammen med hvem der skal ringes til",
              nogen_post_har(&l, ZS_SUPPORT_NAME));
        CHECK("og den raa bit staar der, saa den kan slaas op",
              nogen_post_har(&l, "31"));
    }

    /*
     * ALLE 32 BITS, ÉN AD GANGEN. Ingen af dem maa give nul linjer -
     * hverken de kendte eller de ukendte. Det er den eneste maade at
     * vide at der ikke er et hul i tabellen hvor en fejl forsvinder.
     */
    {
        int tavse = 0;
        for (int bit = 0; bit < 32; bit++) {
            zs_fr_live_t live = rent();
            live.evt1 = 1u << bit;
            zs_status_build(&l, &live, NULL);
            if (l.antal == 0) {
                tavse++;
            }
        }
        CHECK_INT("ingen af de 32 bits i evt1 forsvinder i stilhed",
                  tavse, 0);
    }
    {
        int tavse = 0;
        for (int bit = 0; bit < 32; bit++) {
            zs_fr_live_t live = rent();
            live.evtvnd1 = 1u << bit;
            zs_status_build(&l, &live, NULL);
            if (l.antal == 0) {
                tavse++;
            }
        }
        CHECK_INT("og ingen i Fronius' eget felt evtvnd1", tavse, 0);
    }

    /*
     * FOR MANGE FEJL. Listen har plads til ZS_STATUS_MAX, og bliver der
     * flere, skal `afkortet` siges - ikke bare de sidste tabes.
     */
    {
        zs_fr_live_t live = rent();
        live.evt1 = 0xFFFFFFFFu;
        live.evt2 = 0xFFFFFFFFu;
        live.evtvnd1 = 0xFFFFFFFFu;
        live.evtvnd2 = 0xFFFFFFFFu;
        zs_status_build(&l, &live, NULL);
        CHECK_INT("listen loeber ikke over", l.antal, ZS_STATUS_MAX);
        CHECK("og den siger at der var flere", l.afkortet);
        CHECK_INT("med alt saat er vaerste alvor en fejl",
                  l.vaerst, ZS_SEV_FAULT);
    }

    /*
     * INGEN AFLAESNING ER IKKE DET SAMME SOM INGEN FEJL.
     *
     * Her forventede proeven foerst at der kom NUL linjer. Det var
     * forkert, og modulet har ret: svarer inverteren ikke, SKAL der staa
     * noget paa skaermen. Ellers ser en inverter der er holdt op med at
     * svare, ud som en inverter hvor alt er godt.
     */
    {
        zs_status_build(&l, NULL, NULL);
        CHECK("uden en aflaesning staar der NOGET", l.antal > 0);
        CHECK("og det siger at tilstanden ikke meldes",
              nogen_post_har(&l, "melder ikke"));
        CHECK("og vi har ikke faaet svar", !l.har_svar);
        CHECK("men det meldes ikke som en FEJL - vi ved det ikke",
              l.vaerst < ZS_SEV_FAULT);
    }
    {
        /* Og det samme naar der ER en aflaesning, men den er ugyldig. */
        zs_fr_live_t live = rent();
        live.status_ok = false;
        zs_status_build(&l, &live, NULL);
        CHECK("en aflaesning uden status_ok staar heller ikke tom",
              l.antal > 0);
        CHECK("og har_svar er falsk", !l.har_svar);
    }
    {
        zs_fr_live_t live = rent();
        zs_status_build(&l, &live, NULL);
        CHECK("med en aflaesning HAR vi faaet svar", l.har_svar);
    }
}

void test_status_tekster(void)
{
    ZS_SUITE("Tilstanden og opsummeringen som ord");

    /*
     * Driftstilstanden. SunSpec definerer 1 til 8, og alt andet er
     * noget vi ikke kender - og saa skal der staa det, ikke en tilstand
     * vi har gaettet paa.
     */
    for (int32_t st = 1; st <= 8; st++) {
        char navn[56];
        const char *t = zs_status_state_text(st);
        snprintf(navn, sizeof(navn), "tilstand %d har en tekst", (int)st);
        CHECK(navn, t != NULL && t[0] != '\0');
    }
    const int32_t vilde[] = { -1, 0, 9, 99, 2147483647, -2147483647 - 1 };
    for (size_t i = 0; i < sizeof(vilde) / sizeof(vilde[0]); i++) {
        char navn[64];
        const char *t = zs_status_state_text(vilde[i]);
        snprintf(navn, sizeof(navn), "tilstand %d giver en tekst, ikke NULL",
                 (int)vilde[i]);
        CHECK(navn, t != NULL);
    }

    /* Opsummeringen skal altid give en saetning, ogsaa paa det tomme. */
    {
        zs_status_list_t l;
        zs_fr_live_t live = rent();
        zs_status_build(&l, &live, NULL);
        const char *s = zs_status_summary(&l, &live);
        CHECK("en ren aflaesning har en opsummering", s != NULL && s[0] != '\0');

        live.evt1 = 1u << 0;
        zs_status_build(&l, &live, NULL);
        const char *s2 = zs_status_summary(&l, &live);
        CHECK("og en med en fejl har ogsaa én", s2 != NULL && s2[0] != '\0');
        CHECK("og de to er ikke den samme", strcmp(s, s2) != 0);

        CHECK("NULL-liste giver stadig en tekst",
              zs_status_summary(NULL, &live) != NULL);
        CHECK("NULL-aflaesning ogsaa",
              zs_status_summary(&l, NULL) != NULL);
        CHECK("og to NULL", zs_status_summary(NULL, NULL) != NULL);
    }
}
