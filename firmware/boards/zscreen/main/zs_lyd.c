#include "zs_lyd.h"
#include "zs_config.h"
#include "../zs_log.h"

#include <string.h>

static const char *TAG = "lyd";

/* ------------------------------------------------------------------ */
/* Den rene del: COBS og pakken                                        */
/* ------------------------------------------------------------------ */
/*
 * COBS - Consistent Overhead Byte Stuffing.
 *
 * Formaalet er at faa NUL ud af en pakke, saa et enkelt nul kan bruges
 * som skilletegn mellem to pakker. Det er praecis hvad PacketSerial paa
 * RP2040'eren forventer.
 *
 * Reglen er kort: data deles i stykker ved hvert nul, og hvert stykke
 * faar en byte foran der siger hvor langt der er til det naeste nul.
 * Et nul i dataene bliver altsaa til en LAENGDE og forsvinder fra
 * ledningen.
 *
 * Den er skrevet her og ikke hentet fra Seeeds eksempelmappe, af to
 * grunde: deres ligger i examples/, som er et sted et submodul-opslag
 * kan flytte under os, og den her kan proeves. Se tests/host/test_lyd.c,
 * hvor den baade maales mod haandregnede pakker og afkodes igen af en
 * uafhaengig afkoder.
 */
static int cobs_indpak(uint8_t *ud, size_t plads, const uint8_t *ind, size_t n)
{
    if (ud == NULL || ind == NULL || plads < 1) {
        return -1;
    }

    size_t skriv = 1;       /* ud[0] er den foerste laengde-byte     */
    size_t laengde_paa = 0; /* hvor den laengde skal skrives til sidst */
    uint8_t laengde = 1;

    for (size_t laes = 0; laes < n; laes++) {
        if (ind[laes] == 0) {
            /* Stykket slutter her. Skriv laengden, og reserver pladsen
             * til den naeste. */
            ud[laengde_paa] = laengde;
            if (skriv >= plads) {
                return -1;
            }
            laengde_paa = skriv++;
            laengde = 1;
            continue;
        }
        if (skriv >= plads) {
            return -1;
        }
        ud[skriv++] = ind[laes];
        laengde++;
        /*
         * 255 er loftet: laengde-byten kan ikke sige mere. Saa lukkes
         * stykket uden at der var et nul, og et nyt begynder. Vores egne
         * pakker er fem bytes, saa den her gren bliver aldrig brugt i
         * drift - men en indpakker der er forkert over 254 bytes er en
         * fejl der venter paa den dag nogen sender noget laengere.
         */
        if (laengde == 0xFF) {
            ud[laengde_paa] = laengde;
            /*
             * OG DER AABNES KUN ET NYT STYKKE HVIS DER KOMMER MERE.
             *
             * Her stod der at der altid blev aabnet et nyt. Paa PRAECIS
             * 254 bytes - hvor det sidste stykke bliver fyldt helt op og
             * der ikke er mere - gav det en overfloedig 0x01 til sidst:
             * 256 bytes i stedet for de 255 en kanonisk indpakning
             * giver.
             *
             * Dataene kom rigtigt tilbage, ogsaa med den ekstra byte, og
             * grenen kan slet ikke naas af modulets egne pakker paa fem
             * bytes. Men en indpakker der er en byte for lang paa én
             * bestemt laengde, er en der en dag bliver sendt til en
             * strengere afkoder. Maalt og rettet 14. oktober 2026, og
             * der staar nu en proeve paa 252 til 258 bytes.
             *
             * SIZE_MAX betyder "der er ikke et aabent stykke". Uden den
             * ville linjen efter loekken skrive 1 hen over den 0xFF vi
             * lige satte.
             */
            laengde_paa = (size_t)-1;
            laengde = 1;
            if (laes + 1 < n) {
                if (skriv >= plads) {
                    return -1;
                }
                laengde_paa = skriv++;
            }
        }
    }

    if (laengde_paa != (size_t)-1) {
        ud[laengde_paa] = laengde;
    }
    return (int)skriv;
}

int zs_lyd_pakke(uint8_t *ud, size_t plads, uint8_t cmd, uint32_t ms)
{
    if (ud == NULL) {
        return -1;
    }

    /* Nyttelasten: kommandoen, og tonens laengde som fire bytes med den
     * mindste foerst. Det er den raekkefoelge baade ESP32 og RP2040
     * bruger, saa der skal ikke byttes om. */
    uint8_t raa[5];
    size_t n = 0;
    raa[n++] = cmd;
    if (ms > 0) {
        raa[n++] = (uint8_t)(ms & 0xFF);
        raa[n++] = (uint8_t)((ms >> 8) & 0xFF);
        raa[n++] = (uint8_t)((ms >> 16) & 0xFF);
        raa[n++] = (uint8_t)((ms >> 24) & 0xFF);
    }

    int len = cobs_indpak(ud, plads, raa, n);
    if (len < 0) {
        return -1;
    }
    /*
     * Og nullet til sidst. Det er DET der siger "pakken er slut", og
     * uden det sidder RP2040'eren og venter paa mere. Seeeds eget
     * eksempel skriver ogsaa én byte mere end indpakkeren gav, men faar
     * nullet med fordi deres buffer tilfaeldigvis var nulstillet. Her
     * staar det skrevet, saa det ikke afhaenger af et held.
     */
    if ((size_t)len >= plads) {
        return -1;
    }
    ud[len] = 0;
    return len + 1;
}

/* ------------------------------------------------------------------ */
/* Hardwaredelen                                                       */
/* ------------------------------------------------------------------ */

#if defined(ZS_HOST_BUILD) || !ZS_LYD_ENABLED

/*
 * Ingen lyd: hverken paa en Mac, hvor der ikke er en UART, eller i en
 * byggning hvor ZS_LYD_ENABLED er nul.
 *
 * Hele UART-delen falder ud af den byggede fil, praecis som demoen gOEr
 * det. Det er ikke for at spare pladsen - det er for at en byggning
 * uden lyd heller ikke kan komme til at roere to ben der gaar til den
 * anden chip.
 */
bool zs_lyd_init(void)
{
    ZS_LOGI(TAG, "lyden er slaaet fra i denne byggning");
    return false;
}

void zs_lyd_bip(void) { }

bool zs_lyd_findes(void) { return false; }

#else /* rigtig hardware, og lyden er slaaet til */

#include "driver/uart.h"
#include "esp_timer.h"

#define LYD_UART     UART_NUM_2
#define LYD_TX_GPIO  19
#define LYD_BAUD     115200

/*
 * RX-BENET ROERES IKKE, og det er et valg.
 *
 * RP2040'eren sender selv pakker den anden vej - maalinger fra de
 * sensorer der sidder paa de stoerre modeller, og paa D1'eren
 * Grove-indgangenes raa vaerdier. Vi bruger ingen af dem. Satte vi
 * benet op alligevel, ville driverens ringbuffer langsomt loebe fuld af
 * noget ingen laeser, og saa staar der en advarsel i loggen hvert
 * sekund om noget der ikke er en fejl.
 *
 * Vi SKRIVER kun. Derfor UART_PIN_NO_CHANGE paa RX.
 */
#define LYD_RX_GPIO  UART_PIN_NO_CHANGE

static bool               s_klar;
static esp_timer_handle_t s_timer;
static int                s_toner_igen;
static int64_t            s_bip_slut_us;

/* Hvor lang tid ét helt bip tager, fra foerste tone til sidste er
 * faerdig. Bruges til at afvise et bip der kommer oven i et andet. */
#define BIP_LAENGDE_MS \
    (ZS_LYD_TONER * ZS_LYD_TONE_MS + (ZS_LYD_TONER - 1) * ZS_LYD_TONE_PAUSE_MS)

static void send_tone(void)
{
    uint8_t p[8];
    int n = zs_lyd_pakke(p, sizeof(p), ZS_RP_BEEP_ON, ZS_LYD_TONE_MS);
    if (n <= 0) {
        ZS_LOGE(TAG, "pakken kunne ikke bygges");
        return;
    }
    /*
     * Der er en TX-ringbuffer, saa det her er en memcpy og ikke en
     * ventetid. Det betyder noget: kaldet kommer fra esp_timer's egen
     * opgave, og den maa ikke staa stille.
     */
    int skrevet = uart_write_bytes(LYD_UART, p, (size_t)n);
    if (skrevet != n) {
        ZS_LOGW(TAG, "kun %d af %d bytes kom ud", skrevet, n);
    }
}

static void naeste_tone(void *arg)
{
    (void)arg;
    if (s_toner_igen <= 0) {
        return;
    }
    s_toner_igen--;
    send_tone();
    if (s_toner_igen > 0) {
        (void)esp_timer_start_once(
            s_timer, (uint64_t)(ZS_LYD_TONE_MS + ZS_LYD_TONE_PAUSE_MS) * 1000);
    }
}

bool zs_lyd_init(void)
{
    if (s_klar) {
        return true;
    }

    const uart_config_t cfg = {
        .baud_rate  = LYD_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /*
     * Modtagebufferen skal vaere stoerre end FIFO'en, ellers afviser
     * driveren opsaetningen. Vi laeser aldrig af den - se noten ved
     * LYD_RX_GPIO - men den skal angives.
     */
    esp_err_t err = uart_driver_install(LYD_UART, 256, 256, 0, NULL, 0);
    if (err != ESP_OK) {
        ZS_LOGE(TAG, "UART'en kunne ikke saettes op (%d), ingen lyd", (int)err);
        return false;
    }
    err = uart_param_config(LYD_UART, &cfg);
    if (err == ESP_OK) {
        err = uart_set_pin(LYD_UART, LYD_TX_GPIO, LYD_RX_GPIO,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        ZS_LOGE(TAG, "benene kunne ikke saettes (%d), ingen lyd", (int)err);
        (void)uart_driver_delete(LYD_UART);
        return false;
    }

    const esp_timer_create_args_t targs = {
        .callback = naeste_tone,
        .arg      = NULL,
        .name     = "zs_lyd",
    };
    err = esp_timer_create(&targs, &s_timer);
    if (err != ESP_OK) {
        ZS_LOGE(TAG, "timeren kunne ikke laves (%d), ingen lyd", (int)err);
        (void)uart_driver_delete(LYD_UART);
        return false;
    }

    s_klar = true;
    ZS_LOGI(TAG, "lyden er klar: UART2 paa GPIO %d, %d toner a %d ms",
            LYD_TX_GPIO, ZS_LYD_TONER, ZS_LYD_TONE_MS);
    /*
     * Der bippes IKKE som en proeve ved opstart.
     *
     * En skaerm der mister stroemmen om natten og kommer igen, ville
     * ellers bippe klokken tre - og det er netop den slags der faar
     * nogen til at tage den ned af vaeggen. Virker lyden, hoeres det
     * foerste gang der er noget at sige.
     */
    return true;
}

void zs_lyd_bip(void)
{
    if (!s_klar) {
        return;
    }
    int64_t nu = esp_timer_get_time();
    if (nu < s_bip_slut_us) {
        /* Der er et bip i gang. To oven i hinanden bliver én lang
         * ulyd, og det er ikke det der er meningen. */
        return;
    }
    s_bip_slut_us = nu + (int64_t)BIP_LAENGDE_MS * 1000;

    s_toner_igen = ZS_LYD_TONER - 1;
    send_tone();
    if (s_toner_igen > 0) {
        (void)esp_timer_start_once(
            s_timer, (uint64_t)(ZS_LYD_TONE_MS + ZS_LYD_TONE_PAUSE_MS) * 1000);
    }
}

bool zs_lyd_findes(void)
{
    return s_klar;
}

#endif /* ZS_HOST_BUILD || !ZS_LYD_ENABLED */
