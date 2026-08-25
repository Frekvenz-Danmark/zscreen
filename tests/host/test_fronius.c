/*
 * Test af Fronius-laget: hvordan kanaler og maalinger bliver til de
 * fire tal paa skaermen.
 *
 * Vi inkluderer .c-filen i stedet for at linke til den, saa vi kan naa
 * de interne beregningsfunktioner. Det er en almindelig maade at
 * enhedsteste C paa, og den koster ingenting: filen linkes IKKE
 * separat ind i testbinaeren (se run.sh).
 *
 * De to scenarier der har kostet rigtige penge i marken staar foerst:
 * spoegelses-solen hos Kruses, og batteriets fortegn.
 */

#include "zs_test.h"
#include "../../firmware/main/net/zs_fronius.c"

static void ch_set(zs_fr_channel_t *c, const char *label, float w, int32_t dcst)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->label, sizeof(c->label), "%s", label);
    c->role   = zs_ss_classify_channel(label);
    c->dcw    = zs_val(w);
    c->dcst   = dcst;
    c->active = zs_ss_channel_active(dcst);
}

void test_fronius(void)
{
    ZS_SUITE("Fronius: sol og batteri ud fra kanalnavne");

    {
        /* Typisk Gen24 med to solstrenge og batteri der lader. */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "MPPT 1",   2000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "MPPT 2",   2200.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[2], "STCHA",    1400.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[3], "STDISCHA",    0.0f, ZS_DCST_MPPT);
        lv.channel_count = 4;

        compute_dc(&fr, &lv);
        CHECK("solproduktion laest", lv.solar_w.ok);
        CHECK_F("to strenge lagt sammen", lv.solar_w.v, 4200.0, 0.01);
        CHECK("batterieffekt laest", lv.battery_w.ok);
        CHECK_F("lader 1400 W giver minus", lv.battery_w.v, -1400.0, 0.01);
        CHECK("kanalnavnene blev brugt", fr.info.labels_usable);
    }

    {
        /* Samme anlaeg, nu aflader batteriet. Fortegnet skal vende. */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "MPPT 1",      0.0f, ZS_DCST_SLEEPING);
        ch_set(&lv.channels[1], "MPPT 2",      0.0f, ZS_DCST_SLEEPING);
        ch_set(&lv.channels[2], "STCHA",       0.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[3], "STDISCHA", 1500.0f, ZS_DCST_MPPT);
        lv.channel_count = 4;

        compute_dc(&fr, &lv);
        CHECK_F("nat: ingen sol", lv.solar_w.v, 0.0, 0.01);
        CHECK_F("aflader 1500 W giver plus", lv.battery_w.v, 1500.0, 0.01);
    }

    {
        /*
         * Spoegelses-solen.
         *
         * Anlaeg uden solceller hvor MPPT 1 alligevel rapporterer 824 W,
         * men med tilstanden SLEEPING. Uden filteret paa tilstanden
         * ville skaermen paastaa at solen skinner om natten. Det er
         * praecis den fejl der stod paa et rigtigt anlaeg.
         */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "MPPT 1",    824.0f, ZS_DCST_SLEEPING);
        ch_set(&lv.channels[1], "STCHA",       0.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[2], "STDISCHA", 2000.0f, ZS_DCST_MPPT);
        lv.channel_count = 3;

        compute_dc(&fr, &lv);
        CHECK_F("sovende streng taeller ikke med", lv.solar_w.v, 0.0, 0.01);
        CHECK_F("batteriet aflader stadig korrekt", lv.battery_w.v, 2000.0, 0.01);
    }

    {
        /* En streng der er ved at starte op. STARTING maa ikke taelle:
         * effekten er ikke troværdig endnu. */
        zs_fr_t fr; zs_fr_init(&fr);
        fr.info.has_battery = false;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "MPPT 1", 1000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "MPPT 2",  300.0f, ZS_DCST_STARTING);
        lv.channel_count = 2;

        compute_dc(&fr, &lv);
        CHECK_F("kun den kanal der sporer maksimalpunkt taeller", lv.solar_w.v, 1000.0, 0.01);
    }

    {
        /* Kanal med THROTTLING skal taelle: den leverer, bare begraenset. */
        zs_fr_t fr; zs_fr_init(&fr);
        fr.info.has_battery = false;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "MPPT 1", 1000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "MPPT 2",  800.0f, ZS_DCST_THROTTLING);
        lv.channel_count = 2;

        compute_dc(&fr, &lv);
        CHECK_F("begraenset streng taeller med", lv.solar_w.v, 1800.0, 0.01);
    }

    ZS_SUITE("Fronius: naar kanalerne ikke er navngivet");

    {
        /* Fronius med fire unavngivne kanaler og batteri. Fronius'
         * egen manual siger kanal 3 = lade, kanal 4 = aflade. */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "", 1500.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "", 1700.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[2], "",  900.0f, ZS_DCST_MPPT);  /* lade   */
        ch_set(&lv.channels[3], "",    0.0f, ZS_DCST_MPPT);  /* aflade */
        lv.channel_count = 4;

        compute_dc(&fr, &lv);
        CHECK_F("kanal 1 og 2 er sol", lv.solar_w.v, 3200.0, 0.01);
        CHECK_F("kanal 3 er lade, kanal 4 er aflade", lv.battery_w.v, -900.0, 0.01);
        CHECK("og vi noterer at navnene ikke kunne bruges", !fr.info.labels_usable);
    }

    {
        /*
         * Fronius med ÉN solstreng og batteri, uden navne: tre kanaler.
         *
         * Her skal der taelles bagfra. Gaar man ud fra at batteriet
         * altid ligger paa kanal 3 og 4, laeser man solstreng nummer
         * to som ladeside, og paa et anlaeg med kun én streng
         * forsvinder solen ind i batteriet.
         */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "", 3100.0f, ZS_DCST_MPPT);  /* eneste streng */
        ch_set(&lv.channels[1], "",    0.0f, ZS_DCST_MPPT);  /* lade          */
        ch_set(&lv.channels[2], "", 1200.0f, ZS_DCST_MPPT);  /* aflade        */
        lv.channel_count = 3;

        compute_dc(&fr, &lv);
        CHECK_F("én streng giver hele solproduktionen", lv.solar_w.v, 3100.0, 0.01);
        CHECK_F("de to sidste kanaler er batteriet",    lv.battery_w.v, 1200.0, 0.01);
    }

    {
        /* Tre solstrenge og batteri: fem kanaler, batteriet er de to sidste. */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Fronius");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "", 1000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "", 1100.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[2], "", 1200.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[3], "",  700.0f, ZS_DCST_MPPT);   /* lade   */
        ch_set(&lv.channels[4], "",    0.0f, ZS_DCST_MPPT);   /* aflade */
        lv.channel_count = 5;

        compute_dc(&fr, &lv);
        CHECK_F("tre strenge lagt sammen", lv.solar_w.v, 3300.0, 0.01);
        CHECK_F("batteriet lader 700 W",   lv.battery_w.v, -700.0, 0.01);
    }

    {
        /* Ren solcelleinverter uden batteri og uden navne. Alt er sol. */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Ukendt");
        fr.info.has_battery = false;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "", 1000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "",  500.0f, ZS_DCST_MPPT);
        lv.channel_count = 2;

        compute_dc(&fr, &lv);
        CHECK_F("alle aktive kanaler er sol", lv.solar_w.v, 1500.0, 0.01);
        CHECK("intet batteri at rapportere", !lv.battery_w.ok);
    }

    {
        /*
         * Ukendt fabrikat, batteri til stede, ingen navne.
         *
         * Her VIL vi have "ingen data". Den gamle Zbox-formel gaettede
         * i denne situation og gaettede forkert. En aerlig streg er
         * bedre end et forkert tal paa en skaerm der haenger paa
         * kundens vaeg.
         */
        zs_fr_t fr; zs_fr_init(&fr);
        snprintf(fr.info.manufacturer, sizeof(fr.info.manufacturer), "Mystisk Inverter");
        fr.info.has_battery = true;

        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        ch_set(&lv.channels[0], "", 1000.0f, ZS_DCST_MPPT);
        ch_set(&lv.channels[1], "",  500.0f, ZS_DCST_MPPT);
        lv.channel_count = 2;

        compute_dc(&fr, &lv);
        CHECK("vi gaetter IKKE paa solproduktionen", !lv.solar_w.ok);
        CHECK("og heller ikke paa batteriet",        !lv.battery_w.ok);
    }

    {
        /* Ingen kanaler overhovedet. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.channel_count = 0;
        compute_dc(&fr, &lv);
        CHECK("ingen kanaler giver ingen sol",     !lv.solar_w.ok);
        CHECK("ingen kanaler giver intet batteri", !lv.battery_w.ok);
    }

    ZS_SUITE("Fronius: husets forbrug");

    {
        /* Sol producerer 4200 W, huset bruger 1800, 2400 saelges. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(4200.0f);
        lv.grid_w        = zs_val(-2400.0f);
        compute_house(&fr, &lv);
        CHECK_F("dag med eksport: 4200 + (-2400) = 1800 W", lv.house_w.v, 1800.0, 0.01);
    }
    {
        /* Nat, batteriet leverer 1000 W, huset bruger 1500, 500 koebes. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(1000.0f);
        lv.grid_w        = zs_val(500.0f);
        compute_house(&fr, &lv);
        CHECK_F("nat med import: 1000 + 500 = 1500 W", lv.house_w.v, 1500.0, 0.01);
    }
    {
        /* Batteriet lades fra nettet med 2000 W, huset bruger 500. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(-2000.0f);
        lv.grid_w        = zs_val(2500.0f);
        compute_house(&fr, &lv);
        CHECK_F("ladning fra net: -2000 + 2500 = 500 W", lv.house_w.v, 500.0, 0.01);
    }
    {
        /* Uden elmaaler kan forbruget ikke udledes. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(4200.0f);
        lv.grid_w        = ZS_VAL_NONE;
        compute_house(&fr, &lv);
        CHECK("uden elmaaler er forbruget ukendt", !lv.house_w.ok);
    }
    {
        /* Uden inverter-effekt heller ikke. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = ZS_VAL_NONE;
        lv.grid_w        = zs_val(500.0f);
        compute_house(&fr, &lv);
        CHECK("uden AC-effekt er forbruget ukendt", !lv.house_w.ok);
    }
    {
        /*
         * Negativt forbrug kan ikke lade sig goere. Sker det, sidder
         * elmaalerens fortegn eller placering forkert. Vi klipper til
         * nul saa skaermen ikke viser noget umuligt, men vi taeller det
         * saa en montoer kan se det paa Detaljer-siden.
         */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(1000.0f);
        lv.grid_w        = zs_val(-3000.0f);
        compute_house(&fr, &lv);
        CHECK_F("umuligt negativt forbrug klippes til 0", lv.house_w.v, 0.0, 0.01);
        CHECK_INT("og taelles som et tegn paa forkert fortegn",
                  fr.negative_house_count, 1);
    }
    {
        /* Lige under graensen taelles ikke. Smaa maalefejl er normale. */
        zs_fr_t fr; zs_fr_init(&fr);
        zs_fr_live_t lv; memset(&lv, 0, sizeof(lv));
        lv.inverter_ac_w = zs_val(1000.0f);
        lv.grid_w        = zs_val(-1030.0f);
        compute_house(&fr, &lv);
        CHECK_INT("smaa afrundinger taelles ikke som fejl",
                  fr.negative_house_count, 0);
        CHECK_F("men vises stadig som 0", lv.house_w.v, 0.0, 0.01);
    }

    ZS_SUITE("Fronius: heltalsmodel eller flydende tal");

    /*
     * Denne test findes fordi den samme fejl har vaeret der én gang.
     *
     * Foerste udgave brugte "id >= 111", som er rigtigt for inverteren
     * og forkert for alle fire elmaaler-modeller. Skaermen viste 0 W
     * paa NETTET mens maaleren meldte 5 kW eksport, og FORBRUG blev
     * dermed ogsaa forkert. Ingen fejlmeddelelse, bare et forkert tal.
     */
    CHECK("inverter 101 er heltal",  !model_is_float(101));
    CHECK("inverter 102 er heltal",  !model_is_float(102));
    CHECK("inverter 103 er heltal",  !model_is_float(103));
    CHECK("inverter 111 er float",    model_is_float(111));
    CHECK("inverter 112 er float",    model_is_float(112));
    CHECK("inverter 113 er float",    model_is_float(113));

    CHECK("elmaaler 201 er heltal",  !model_is_float(201));
    CHECK("elmaaler 202 er heltal",  !model_is_float(202));
    CHECK("elmaaler 203 er heltal",  !model_is_float(203));
    CHECK("elmaaler 204 er heltal",  !model_is_float(204));
    CHECK("elmaaler 211 er float",    model_is_float(211));
    CHECK("elmaaler 212 er float",    model_is_float(212));
    CHECK("elmaaler 213 er float",    model_is_float(213));
    CHECK("elmaaler 214 er float",    model_is_float(214));

    /* Modeller vi aldrig spoerger om, men som ikke maa svare "float"
     * hvis nogen kommer til det. */
    CHECK("Common (1) er ikke float",     !model_is_float(1));
    CHECK("Nameplate (120) er ikke float", !model_is_float(120));
    CHECK("Storage (124) er ikke float",   !model_is_float(124));
    CHECK("MPPT (160) er ikke float",      !model_is_float(160));

    ZS_SUITE("Fronius: elmaalerens fortegn");

    {
        /* Standard: positiv betyder koeb fra nettet. */
        zs_fr_t fr; zs_fr_init(&fr);
        CHECK("standard er at positiv betyder koeb", fr.meter_import_positive);
    }

    ZS_SUITE("Fronius: ladetilstand i tekst");

    CHECK_STR("lader",    zs_fr_charge_status_text(ZS_CHAST_CHARGING),    "Lader");
    CHECK_STR("aflader",  zs_fr_charge_status_text(ZS_CHAST_DISCHARGING), "Aflader");
    CHECK_STR("fuldt",    zs_fr_charge_status_text(ZS_CHAST_FULL),        "Fuldt");
    CHECK_STR("ukendt",   zs_fr_charge_status_text(-1),                   "Ukendt");
    CHECK_STR("vaerdi vi ikke kender", zs_fr_charge_status_text(99),      "Ukendt");
}
