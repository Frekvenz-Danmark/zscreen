/*
 * Stroemmen paa zScreen: der er ikke noget at maale.
 *
 * SenseCAP Indicator D1 har intet batteri. Den sidder i en USB-stik og
 * er vaek i samme sekund stroemmen gaar - saa der er ingen tilstand
 * "koerer paa batteri", og ingen at sige den til.
 *
 * Filen findes alligevel, og det er med vilje. Motoren spoerger om det
 * samme paa begge produkter, og svaret "det har jeg ikke" skal komme
 * fra boardet og ikke fra en #ifdef inde i motoren. Ellers skulle
 * app/zs_fejl.c kende hvilken skaerm den koerer paa, og det er praecis
 * den slags der gOEr at en funktion kommer til at mangle paa det ene
 * produkt uden at nogen opdager det.
 */

#include "zs_stroem.h"

#include <string.h>

bool zs_stroem_init(void)
{
    return false;
}

void zs_stroem_laes(zs_stroem_t *ud)
{
    if (ud == NULL) {
        return;
    }
    memset(ud, 0, sizeof(*ud));
    ud->findes  = false;
    ud->procent = -1;
    /*
     * paa_stroem er SAND, og det er ikke en loegn der er glemt.
     *
     * Skaermen koerer. Koerer den, er der stroem paa den - den har
     * ingen anden kilde. Sagde vi falsk, ville fejlvagten melde
     * stroemsvigt paa hver zScreen i marken for evigt.
     */
    ud->paa_stroem = true;
}
