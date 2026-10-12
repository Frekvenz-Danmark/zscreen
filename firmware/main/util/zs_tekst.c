/* zScreen - se zs_tekst.h for hvorfor den findes. */

#include "zs_tekst.h"

#include <string.h>

char zs_tekst_smaa(char c)
{
    if (c >= 'A' && c <= 'Z') {
        /* Regnes i int saa der ikke sker et overloeb i char paa vejen,
         * og laegges tilbage bagefter. Afstanden mellem 'A' og 'a' er den
         * samme for alle 26, ogsaa paa en maskine der ikke bruger ASCII
         * for andet. */
        return (char)(c - 'A' + 'a');
    }
    return c;
}

char zs_tekst_stort(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

bool zs_tekst_kopi(char *ud, size_t ud_len, const char *fra)
{
    if (ud == NULL || ud_len == 0) {
        return false;
    }
    if (fra == NULL) {
        ud[0] = '\0';
        return false;
    }

    size_t n = strlen(fra);
    bool hele = (n < ud_len);
    if (!hele) {
        n = ud_len - 1;
        /*
         * OG KLIPNINGEN SKAL LANDE MELLEM TO TEGN, IKKE INDE I ÉT.
         *
         * Teksten er UTF-8, og æ, ø og å fylder TO bytes. Klippede vi
         * midt i et saadant par, stod der en halv byte-foelge til sidst,
         * og den kan ikke tegnes: LVGL tegner det som ingenting eller
         * som et firkantet hul, og det staar paa en vaeg hos en kunde.
         *
         * Det blev fundet 12. oktober 2026, da serveren begyndte at
         * sende KUNDENS NAVN ned til skaermen. Vores egne tekster er
         * proevet af check-text.py, men et navn kommer udefra, det maa
         * fylde 63 bytes, og "Søndergaard" har et ø der kan ligge
         * praecis paa graensen.
         *
         * En fortsaettelses-byte har de to oeverste bit sat til 10. Vi
         * gaar tilbage mens den byte vi er ved at DROPPE er en saadan:
         * saa ender det vi beholder paa en hel foelge. Er teksten slet
         * ikke gyldig UTF-8, kan n gaa i nul, og en tom streng er stadig
         * bedre end noget der ikke kan tegnes.
         */
        while (n > 0 && ((unsigned char)fra[n] & 0xC0) == 0x80) {
            n--;
        }
    }
    memcpy(ud, fra, n);
    ud[n] = '\0';
    return hele;
}

bool zs_tekst_indeholder(const char *hoejstak, const char *naal)
{
    if (hoejstak == NULL || naal == NULL || naal[0] == '\0') {
        return false;
    }

    size_t n = strlen(naal);
    size_t h = strlen(hoejstak);
    if (h < n) {
        return false;
    }

    /* i + n <= h, ikke i < h: den sidste mulige plads skal med, ellers
     * kan en naal der staar helt ude i enden ikke findes. */
    for (size_t i = 0; i + n <= h; i++) {
        size_t k = 0;
        while (k < n && zs_tekst_smaa(hoejstak[i + k]) == zs_tekst_smaa(naal[k])) {
            k++;
        }
        if (k == n) {
            return true;
        }
    }
    return false;
}
