/*
 * Test af demoens tal.
 *
 * Demoen er ikke pynt. Den bliver vist til kunder og paa messer, og
 * staar der noget der ikke kan lade sig goere, opdager en elektriker
 * det paa to sekunder. Derfor tjekker vi to ting:
 *
 *   at tallene er realistiske for et dansk parcelhus
 *   at de haenger sammen indbyrdes, doegnet rundt
 *
 * Det sidste er det vigtigste. Sol, forbrug, batteri og net er fire
 * tal der ikke kan vaelges frit: nettet er det der er tilbage naar
 * huset har taget sit og batteriet har taget eller givet sit. Passer
 * regnestykket ikke, staar der fire tal paa skaermen der tilsammen
 * siger noget umuligt.
 */

#include "zs_test.h"
#include "../../firmware/main/app/zs_demo.h"

#include <math.h>
#include <stdbool.h>

/* Et helt doegn i demoens egen tid. Demoen tager 180 sekunder om et
 * doegn, og vi kalder den med 2 sekunder ad gangen som skaermen goer. */
#define SKRIDT_MS    2000
#define SKRIDT       90

void test_demo(void)
{
    ZS_SUITE("Demoen: regnestykket haenger sammen");

    {
        zs_demo_reset();
        zs_fr_live_t l;
        bool balance = true, alle_ok = true;
        float vaerst = 0.0f;

        for (int i = 0; i < SKRIDT * 3; i++) {
            zs_demo_step(&l, SKRIDT_MS);

            if (!l.solar_w.ok || !l.house_w.ok || !l.battery_w.ok
                || !l.grid_w.ok || !l.soc_pct.ok || !l.inverter_ac_w.ok) {
                alle_ok = false;
            }
            /* Huset = det inverteren leverer + det nettet leverer. */
            float rest = l.house_w.v - (l.inverter_ac_w.v + l.grid_w.v);
            if (fabsf(rest) > fabsf(vaerst)) { vaerst = rest; }
            if (fabsf(rest) > 1.0f) { balance = false; }
        }
        CHECK("alle fire tal er udfyldt hele doegnet", alle_ok);
        CHECK("forbrug = inverter + net, hele doegnet igennem", balance);
        CHECK("og afvigelsen er under en watt", fabsf(vaerst) < 1.0f);
    }

    ZS_SUITE("Demoen: tal et dansk anlaeg kan levere");

    {
        zs_demo_reset();
        zs_fr_live_t l;
        float sol_top = 0.0f, hus_top = 0.0f, bat_top = 0.0f;
        float sol_wh = 0.0f, hus_wh = 0.0f;
        float soc_min = 200.0f, soc_maks = -1.0f;
        bool soc_i_omraade = true, sol_aldrig_negativ = true;

        for (int i = 0; i < SKRIDT; i++) {
            zs_demo_step(&l, SKRIDT_MS);
            if (l.solar_w.v > sol_top)            { sol_top = l.solar_w.v; }
            if (l.house_w.v > hus_top)            { hus_top = l.house_w.v; }
            if (fabsf(l.battery_w.v) > bat_top)   { bat_top = fabsf(l.battery_w.v); }
            if (l.solar_w.v < 0.0f)               { sol_aldrig_negativ = false; }
            if (l.soc_pct.v < 0.0f || l.soc_pct.v > 100.0f) { soc_i_omraade = false; }
            if (l.soc_pct.v < soc_min)            { soc_min = l.soc_pct.v; }
            if (l.soc_pct.v > soc_maks)           { soc_maks = l.soc_pct.v; }
            /* Hvert skridt er et doegn delt i 90, altsaa 16 minutter. */
            sol_wh += l.solar_w.v * (24.0f / SKRIDT);
            hus_wh += l.house_w.v * (24.0f / SKRIDT);
        }

        /* 7 kWp yder ikke 7 kW. Solen staar lavt herhjemme og panelet
         * bliver varmt, saa 75 til 85 procent er hvad man ser. */
        CHECK("solen topper mellem 5 og 6 kW", sol_top > 5000.0f && sol_top < 6000.0f);
        CHECK("solen bliver aldrig negativ", sol_aldrig_negativ);

        /* En klar sommerdag for 7 kWp herhjemme. PVGIS siger 31 kWh som
         * maanedsgennemsnit for maj og juni, graavejr medregnet. */
        CHECK("doegnets sol ligger mellem 30 og 45 kWh",
              sol_wh / 1000.0f > 30.0f && sol_wh / 1000.0f < 45.0f);

        /* Et parcelhus med lidt elvarme: 4000 til 8000 kWh om aaret. */
        CHECK("doegnets forbrug ligger mellem 11 og 22 kWh",
              hus_wh / 1000.0f > 11.0f && hus_wh / 1000.0f < 22.0f);
        CHECK("forbruget topper under 5 kW", hus_top < 5000.0f);
        CHECK("forbruget topper over 1 kW",  hus_top > 1000.0f);

        CHECK("batteriet holder sig under 5 kW", bat_top <= 5001.0f);
        CHECK("ladetilstanden bliver i 0 til 100", soc_i_omraade);
        CHECK("og den bevaeger sig, saa der er noget at se",
              soc_maks - soc_min > 10.0f);
    }

    ZS_SUITE("Demoen: batteriet opfoerer sig som et batteri");

    {
        zs_demo_reset();
        zs_fr_live_t l;
        bool lader_ikke_naar_fuldt = true;
        bool aflader_ikke_naar_tomt = true;

        for (int i = 0; i < SKRIDT * 3; i++) {
            zs_demo_step(&l, SKRIDT_MS);
            /* Negativ effekt er ladning, positiv er afladning. */
            if (l.soc_pct.v >= 100.0f && l.battery_w.v < -1.0f) {
                lader_ikke_naar_fuldt = false;
            }
            if (l.soc_pct.v <= 5.0f && l.battery_w.v > 1.0f) {
                aflader_ikke_naar_tomt = false;
            }
        }
        CHECK("et fuldt batteri lader ikke videre", lader_ikke_naar_fuldt);
        CHECK("et tomt batteri aflader ikke videre", aflader_ikke_naar_tomt);
    }

    ZS_SUITE("Demoen: spotpriser i danske stoerrelser");

    {
        zs_price_day_t d;
        zs_demo_price(&d);

        CHECK("der kommer priser", d.ok);
        CHECK("et helt doegn", d.antal == 24);

        bool i_omraade = true;
        for (int i = 0; i < 24; i++) {
            /* Spotpris uden afgifter. Under nul sker det naar det
             * blaeser, men ikke i demoen, og over tre kroner er en
             * krise og ikke en almindelig dag. */
            if (d.timer[i].dkk < 0.0f || d.timer[i].dkk > 3.0f) {
                i_omraade = false;
            }
            CHECK("timerne staar i raekkefoelge", d.timer[i].hour == i);
        }
        CHECK("alle priser er i en almindelig stoerrelse", i_omraade);
        CHECK("gennemsnittet er mellem 30 oere og halvanden krone",
              d.gennemsnit > 0.30f && d.gennemsnit < 1.50f);
        CHECK("den billigste er billigere end den dyreste",
              d.timer[d.billigst].dkk < d.timer[d.dyrest].dkk);
        CHECK("den dyreste time ligger om morgenen eller aftenen",
              d.dyrest < 10 || d.dyrest > 16);
        CHECK("den billigste ligger midt paa dagen",
              d.billigst >= 10 && d.billigst <= 16);
    }

    ZS_SUITE("Demoen: anlaegget den udgiver sig for at vaere");

    {
        zs_fr_info_t info;
        zs_demo_info(&info);
        CHECK("en Fronius", info.has_inverter && info.has_meter && info.has_battery);
        CHECK("fire kanaler: to strenge og to til batteriet",
              info.channel_count == 4);
        CHECK("batteriets stoerrelse er sat",
              info.battery_capacity_kwh > 5.0f && info.battery_capacity_kwh < 20.0f);
    }
}
