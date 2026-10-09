#include "zs_qr.h"

#include <string.h>

#include "qrcodegen.h"

int zs_qr_skala(int moduler, int px)
{
    if (moduler <= 0 || px <= 0) {
        return 0;
    }
    /* Luften taeller med i det der skal vaere plads til. Glemte vi den,
     * ville koden fylde hele pladsen og have NUL luft, og saa bliver den
     * ikke laest. */
    int i_alt = moduler + 2 * ZS_QR_KANT_MODULER;
    int skala = px / i_alt;          /* rundes ned, se headeren */
    return skala > 0 ? skala : 0;
}

int zs_qr_px(int moduler, int skala)
{
    if (moduler <= 0 || skala <= 0) {
        return 0;
    }
    return (moduler + 2 * ZS_QR_KANT_MODULER) * skala;
}

int zs_qr_kant_px(int skala)
{
    if (skala <= 0) {
        return 0;
    }
    return ZS_QR_KANT_MODULER * skala;
}

bool zs_qr_lav(const char *tekst, uint8_t *ud, size_t ud_len, int *moduler)
{
    if (tekst == NULL || ud == NULL || moduler == NULL
        || ud_len < (size_t)ZS_QR_BUF) {
        return false;
    }
    /*
     * Arbejdsbufferen er lige saa stor som resultatet og bliver kun
     * brugt inde i kaldet. Den staar STATISK og ikke paa stakken:
     * brugerfladens opgave har en maalt stak, og et par hundrede bytes
     * mere der kun bruges paa én side, er ikke vaerd at risikere.
     */
    static uint8_t arbejde[ZS_QR_BUF];

    /*
     * Fejlrettelsen er MEDIUM og ikke LOW.
     *
     * Koden bliver laest af en telefon der holdes paa skraa foran en
     * skaerm der lyser, tit med et fingeraftryk paa. MEDIUM taaler at en
     * del af billedet er daarligt, og den koster os ingenting: seks tegn
     * kan stadig vaere i den mindste udgave.
     */
    if (!qrcodegen_encodeText(tekst, arbejde, ud, qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, ZS_QR_MAKS_UDGAVE,
                              qrcodegen_Mask_AUTO, true)) {
        return false;
    }
    *moduler = qrcodegen_getSize(ud);
    return *moduler > 0;
}

bool zs_qr_modul(const uint8_t *qr, int x, int y)
{
    if (qr == NULL) {
        return false;
    }
    return qrcodegen_getModule(qr, x, y);
}
