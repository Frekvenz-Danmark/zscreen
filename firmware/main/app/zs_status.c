#include "zs_status.h"
#include "zs_config.h"

#include <stdio.h>
#include <string.h>

/* Teksten der staar naar vi ikke kan saette ord paa en kode. Bygget
 * her, saa telefonnummeret kun findes ét sted i hele programmet. */
#define UKENDT_HJAELP  "Ring til " ZS_SUPPORT_NAME " på " ZS_SUPPORT_PHONE

static void tilfoej(zs_status_list_t *l, zs_sev_t sev,
                    const char *tekst, const char *detalje)
{
    if (l->antal >= ZS_STATUS_MAX) {
        l->afkortet = true;
        return;
    }
    zs_status_item_t *p = &l->poster[l->antal++];
    p->sev = sev;
    snprintf(p->tekst, sizeof(p->tekst), "%s", tekst != NULL ? tekst : "");
    snprintf(p->detalje, sizeof(p->detalje), "%s", detalje != NULL ? detalje : "");
    if (sev > l->vaerst) {
        l->vaerst = sev;
    }
}

/*
 * Gaar et bitfelt igennem og skriver en linje for hver bit der er sat.
 *
 * Bits vi ikke har en tekst til bliver IKKE sprunget over. De faar en
 * linje der siger at koden er ukendt, med den raa vaerdi og et
 * telefonnummer. At tie om en fejl fordi man ikke kender den, er den
 * daarligste af alle muligheder: saa staar der "alt virker" paa en
 * skaerm mens inverteren melder fejl.
 */
static void gennemgaa(zs_status_list_t *l, uint32_t felt,
                      const zs_bit_text_t *tabel, size_t n_tabel,
                      const char *felt_navn)
{
    if (felt == 0) {
        return;
    }
    for (int b = 0; b < 32; b++) {
        if ((felt & (1u << b)) == 0) {
            continue;
        }
        const zs_bit_text_t *fundet = NULL;
        for (size_t i = 0; i < n_tabel; i++) {
            if (tabel[i].bit == (uint8_t)b) {
                fundet = &tabel[i];
                break;
            }
        }
        char d[72];
        if (fundet != NULL) {
            if (strcmp(fundet->koder, "SunSpec") == 0) {
                snprintf(d, sizeof(d), "%s bit %d", felt_navn, b);
            } else {
                snprintf(d, sizeof(d), "Fronius-kode %s", fundet->koder);
            }
            tilfoej(l, fundet->sev, fundet->tekst, d);
        } else {
            char t[56];
            snprintf(t, sizeof(t), "Ukendt fejl fra inverteren");
            snprintf(d, sizeof(d), "%s bit %d. " UKENDT_HJAELP, felt_navn, b);
            tilfoej(l, ZS_SEV_FAULT, t, d);
        }
    }
}

const char *zs_status_state_text(int32_t st)
{
    switch (st) {
    case 1: return "Slukket";
    case 2: return "Sover";
    case 3: return "Starter op";
    case 4: return "Producerer";
    case 5: return "Begrænset";
    case 6: return "Lukker ned";
    case 7: return "Fejl";
    case 8: return "Standby";
    case 9: return "Test";
    default: return "Ukendt";
    }
}

void zs_status_build(zs_status_list_t *ud, const zs_fr_live_t *live)
{
    if (ud == NULL) {
        return;
    }
    memset(ud, 0, sizeof(*ud));
    ud->vaerst = ZS_SEV_OK;

    if (live == NULL || !live->status_ok) {
        /* Inverteren udfylder ikke felterne. Det er ikke det samme som
         * at der ingen fejl er, og det skal der staa. */
        ud->har_svar = false;
        tilfoej(ud, ZS_SEV_INFO, "Inverteren melder ikke sin tilstand",
                "Den understøtter ikke statusfelterne");
        return;
    }
    ud->har_svar = true;

    /* ── driftstilstand ── */
    switch (live->inverter_state) {
    case 7:
        tilfoej(ud, ZS_SEV_FAULT, "Inverteren står i fejl",
                "Driftstilstand 7");
        break;
    case 5:
        tilfoej(ud, ZS_SEV_WARN, "Produktionen er begrænset",
                "Driftstilstand 5");
        break;
    case 1:
        tilfoej(ud, ZS_SEV_INFO, "Inverteren er slukket", "Driftstilstand 1");
        break;
    case 6:
        tilfoej(ud, ZS_SEV_INFO, "Inverteren lukker ned", "Driftstilstand 6");
        break;
    default:
        break;
    }

    /* ── standard-SunSpec fejlflag ── */
    gennemgaa(ud, live->evt1, zs_evt1_bits,
              sizeof(zs_evt1_bits) / sizeof(zs_evt1_bits[0]), "SunSpec Evt1");

    /*
     * Evt2 er reserveret i standarden og har ingen navngivne bits.
     * Er der noget i den, viser vi den raa vaerdi. Vi opfinder ikke en
     * betydning.
     */
    if (live->evt2 != 0) {
        char d[72];
        snprintf(d, sizeof(d), "Evt2 = 0x%08lX. " UKENDT_HJAELP,
                 (unsigned long)live->evt2);
        tilfoej(ud, ZS_SEV_WARN, "Inverteren melder en kode vi ikke kender", d);
    }

    /* ── Fronius' egne fejlflag ── */
    gennemgaa(ud, live->evtvnd1, zs_evtvnd1_bits,
              sizeof(zs_evtvnd1_bits) / sizeof(zs_evtvnd1_bits[0]), "EvtVnd1");
    gennemgaa(ud, live->evtvnd2, zs_evtvnd2_bits,
              sizeof(zs_evtvnd2_bits) / sizeof(zs_evtvnd2_bits[0]), "EvtVnd2");
    gennemgaa(ud, live->evtvnd3, zs_evtvnd3_bits,
              sizeof(zs_evtvnd3_bits) / sizeof(zs_evtvnd3_bits[0]), "EvtVnd3");

    /* EvtVnd4 har Fronius ikke udgivet en liste for. Er der noget i
     * den, siger vi det raat og henviser videre. */
    if (live->evtvnd4 != 0) {
        char d[72];
        snprintf(d, sizeof(d), "EvtVnd4 = 0x%08lX. " UKENDT_HJAELP,
                 (unsigned long)live->evtvnd4);
        tilfoej(ud, ZS_SEV_FAULT, "Inverteren melder en kode vi ikke kender", d);
    }

    /* ── de enkelte DC-kanaler ── */
    for (uint8_t i = 0; i < live->channel_count; i++) {
        const zs_fr_channel_t *c = &live->channels[i];

        if (c->dcst == 7) {
            char t[56];
            const char *navn = c->label[0] ? c->label : "DC-kanal";
            snprintf(t, sizeof(t), "%s står i fejl", navn);
            tilfoej(ud, ZS_SEV_FAULT, t, "DC-tilstand 7");
        }
        if (c->dcevt != 0) {
            /* Kanalnavnet saettes foran, saa man kan se HVILKEN
             * streng eller batteriside der fejler. */
            zs_status_list_t midlertidig;
            memset(&midlertidig, 0, sizeof(midlertidig));
            gennemgaa(&midlertidig, c->dcevt, zs_dcevt_bits,
                      sizeof(zs_dcevt_bits) / sizeof(zs_dcevt_bits[0]),
                      "DCEvt");
            for (uint8_t k = 0; k < midlertidig.antal; k++) {
                char t[56];
                const char *navn = c->label[0] ? c->label : "DC-kanal";
                snprintf(t, sizeof(t), "%s: %s", navn, midlertidig.poster[k].tekst);
                tilfoej(ud, midlertidig.poster[k].sev, t,
                        midlertidig.poster[k].detalje);
            }
        }
    }

    /*
     * Fronius' egen tilstandskode.
     *
     * Den staar til sidst og kun hvis der ER noget galt. Under normal
     * drift er den bare et tal ingen skal forholde sig til, men naar
     * noget fejler, er det praecis det tal en montoer skal bruge for at
     * slaa fejlen op hos Fronius eller i Solar.web.
     */
    if (ud->vaerst >= ZS_SEV_WARN && live->vendor_state > 0) {
        char d[72];
        snprintf(d, sizeof(d), "Fronius-tilstand %ld",
                 (long)live->vendor_state);
        tilfoej(ud, ZS_SEV_INFO, "Kode til opslag hos Fronius", d);
    }
}

const char *zs_status_summary(const zs_status_list_t *l, const zs_fr_live_t *live)
{
    if (l == NULL) {
        return "Ingen oplysninger";
    }
    if (!l->har_svar) {
        return "Ingen oplysninger";
    }
    switch (l->vaerst) {
    case ZS_SEV_FAULT:
        return "Der er en fejl";
    case ZS_SEV_WARN:
        return "Noget kræver opmærksomhed";
    case ZS_SEV_INFO:
    case ZS_SEV_OK:
    default:
        break;
    }
    if (live != NULL && live->inverter_state == 4) {
        return "Alt virker";
    }
    if (live != NULL && (live->inverter_state == 2 || live->inverter_state == 1)) {
        return "Alt virker, inverteren hviler";
    }
    return "Alt virker";
}
