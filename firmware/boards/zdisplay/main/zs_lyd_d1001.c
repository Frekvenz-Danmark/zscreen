/*
 * Lyden paa ZDisplay - endnu ikke bygget.
 *
 * HVORFOR DER IKKE ER LYD ENDNU, og hvorfor det ikke er en fejl.
 *
 * Her er der en RIGTIG hoejttaler: 2 W paa 8 ohm gennem en ES8311-kodek
 * og en NS4150B-forstaerker, og stroemmen til forstaerkeren taendes af
 * bsp_power_init() paa BSP_POWER_AMP_EN. Et bip er altsaa ikke en pakke
 * til en anden chip, som paa zScreen, men en toene vi selv skal lave:
 * I2S op, kodekken sat til, og nogle tusinde sinus-proever ud.
 *
 * Det kan ikke skrives faerdigt uden hardwaren. Forstaerkningen, hvad
 * der er for hoejt i en stue, og om kodekken knalder naar den taendes,
 * er ting man HOERER - ikke noget man laeser i et datablad. Skrev vi
 * koden paa maa og faa, ville vi have hundrede linjer der ser rigtige
 * ud og ingen der ved om de virker.
 *
 * Imens: ingen lyd. Det er den samme tilstand zScreen staar i
 * (ZS_LYD_ENABLED er 0 der), og fladen er bygget til det - en skaerm
 * uden lyd pulserer stadig, baade i fladen og i baglyset. Se
 * firmware/kerne/hal/zs_lyd.h.
 *
 * zs_lyd_pakke() findes IKKE i den her fil. Den indpakker en COBS-pakke
 * til RP2040'eren paa zScreen, og der er ingen RP2040 her. Ingen i
 * motoren kalder den; kun tests/host/test_lyd.c, som proever
 * zScreen-filen.
 */

#include "zs_lyd.h"

#include "esp_log.h"

static const char *TAG = "lyd";

bool zs_lyd_init(void)
{
    ESP_LOGI(TAG, "ingen lyd i den her byggning: hoejttaleren mangler kode");
    return false;
}

void zs_lyd_bip(void)
{
    /* Med vilje tom. Kaldes fra app-loekken naar noget er galt, og den
     * skal ikke vide om der er en hoejttaler. */
}

bool zs_lyd_findes(void)
{
    return false;
}
