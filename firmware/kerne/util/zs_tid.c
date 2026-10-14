#include "zs_tid.h"

/*
 * Dage siden epoken ud af aar, maaned og dag.
 *
 * Howard Hinnants days_from_civil. Ren heltalsregning, ingen
 * tabelopslag og ingen mktime: mktime ville laese tidszonen og kunne
 * give et andet svar midt i et sommertidsskifte, og det er praecis det
 * vi er ved at komme vaek fra. Skudaar og aarhundredeundtagelser ligger i
 * de fire linjer med 400, 100 og 4.
 *
 * Marts regnes som maaned 0, saa skuddagen havner til sidst i aaret og
 * ikke midt i. Det er hele grunden til at m flyttes.
 */
static int32_t dage_af_dato(int32_t aar, int32_t maaned, int32_t dag)
{
    aar -= (maaned <= 2) ? 1 : 0;
    /*
     * Hinnants egen udgave skriver (aar >= 0 ? aar : aar - 399) / 400,
     * saa divisionen runder NEDAD ogsaa for aar foer aar 0. Den gren er
     * doed hos os: aaret kommer fra localtime_r's tm_year, og den giver
     * ikke et negativt aarstal. Alt foer 1970 fanges desuden af d < 0
     * nede i zs_tid_dagnr.
     *
     * Den stod her indtil mutationstesten pegede paa den: mutanten
     * ">= 0" -> "> 0" kunne ikke slaas ihjel af nogen proeve, fordi de to
     * giver det SAMME svar for aar 0 - C trunkerer mod nul, saa baade
     * 0/400 og -399/400 er nul. En gren hvis to sider ikke kan skelnes,
     * er ikke en vagt. Den er vaek.
     */
    const int32_t aera = aar / 400;
    const int32_t aar_i_aera = aar - aera * 400;                /* 0..399   */
    const int32_t dag_i_aar =
        (153 * (maaned + (maaned > 2 ? -3 : 9)) + 2) / 5 + dag - 1;
    const int32_t dag_i_aera =
        aar_i_aera * 365 + aar_i_aera / 4 - aar_i_aera / 100 + dag_i_aar;
    return aera * 146097 + dag_i_aera - 719468;
}

uint16_t zs_tid_dagnr(const struct tm *lt)
{
    if (lt == NULL) {
        return 0;
    }
    /* tm_year er aar siden 1900, tm_mon er 0 til 11. */
    int32_t d = dage_af_dato((int32_t)lt->tm_year + 1900,
                             (int32_t)lt->tm_mon + 1,
                             (int32_t)lt->tm_mday);
    if (d < 0) {
        return 0;          /* foer 1970: uret er ikke sat */
    }
    if (d > 65535) {
        return 65535;      /* efter 2149. Klemmes, saa den ikke wrapper */
    }
    return (uint16_t)d;
}
