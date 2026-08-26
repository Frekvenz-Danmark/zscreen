/*
 * zScreen - faste vaerdier ét sted.
 *
 * Alt der kan taenkes at skulle justeres senere staar her, saa man ikke
 * skal lede i seks filer efter det tal der skal aendres.
 */

#ifndef ZS_CONFIG_H
#define ZS_CONFIG_H

/* ── Version ───────────────────────────────────────────────────────── */
/*
 * Versionsnummeret staar KUN i firmware/version.txt.
 *
 * ESP-IDF laeser den fil og skriver tallet ind i den byggede fil, hvor
 * esp_app_get_description() henter det. Stod det ogsaa her, ville de
 * to komme ud af trit den dag nogen kun rettede det ene, og saa ville
 * skaermen vise ét nummer mens opdateringen sammenlignede med et
 * andet.
 *
 * Brug zs_version() fra zs_app.h.
 */
#define ZS_PRODUCT_NAME      "zScreen"

/* ── Skaerm ────────────────────────────────────────────────────────── */
/*
 * Maalene staar i ui/zs_theme.h, ikke her.
 *
 * De stod OGSAA her engang, og det gik galt: konstanten for
 * statuslinjens hoejde hed ZS_STATUSBAR_H, praecis som include-guarden
 * i zs_statusbar.h. Fordi zs_config.h blev laest foerst, troede
 * praeprocessoren at headeren allerede var med, og sprang hele filen
 * over. Der kom ingen advarsel, kun en fejl et helt andet sted om en
 * type der ikke fandtes.
 *
 * Derfor to regler:
 *   1. Et maal staar ét sted. Layout hoerer til i zs_theme.h.
 *   2. Ingen konstant slutter paa _H. Det er forbeholdt include-guards.
 *      tools/check-headers.sh haandhaever det.
 */

/* ── Aflaesning ────────────────────────────────────────────────────── */
/*
 * Hvor tit vi spoerger inverteren.
 *
 * To sekunder er rigeligt til en skaerm man kigger paa, og det holder os
 * pænt inden for Fronius' egen anbefaling om at spoerge sekventielt med
 * mindst ét sekunds timeout. Hver runde er fire Modbus-kald.
 */
#define ZS_POLL_INTERVAL_MS  2000

/*
 * Hvor laenge et tal maa vaere gammelt foer vi daemper det paa skaermen.
 *
 * Vi rydder ikke kortene ved forbindelsestab. Det sidst kendte tal med
 * en tydelig markering af at det er gammelt, er mere brugbart end fire
 * tomme felter, og det fortaeller ogsaa hvad der skete lige foer.
 */
#define ZS_STALE_AFTER_MS    10000

/* Efter saa laenge uden svar giver vi op og forbinder helt forfra. */
#define ZS_RECONNECT_AFTER_MS 30000

/* ── Genforbindelse ────────────────────────────────────────────────── */
/*
 * En Fronius Gen24 holder sin side af en afbrudt forbindelse aaben i
 * op til et kvarter og afviser nye forsoeg imens. Derfor proever vi
 * flere gange hurtigt efter hinanden foer vi gaar over til at vente
 * laengere og laengere.
 */
#define ZS_RECONNECT_BURST      3
#define ZS_RECONNECT_BURST_MS   500
#define ZS_RECONNECT_MIN_MS     2000
#define ZS_RECONNECT_MAX_MS     60000

/* ── Netvaerksscanning ─────────────────────────────────────────────── */
#define ZS_SCAN_PARALLEL        12      /* samtidige sockets            */
#define ZS_SCAN_PORT_TIMEOUT_MS 250     /* er der noget paa port 502    */
#define ZS_SCAN_SUNSPEC_TIMEOUT_MS 800  /* taler den SunSpec            */
#define ZS_SCAN_MAX_FOUND       12      /* invertere vi kan vise        */

/* ── Wifi ─────────────────────────────────────────────────────────── */
#define ZS_WIFI_SCAN_MAX        20
#define ZS_WIFI_CONNECT_TIMEOUT_MS 20000

/* ── Ur ───────────────────────────────────────────────────────────── */
/*
 * Uret er pynt, ikke en forudsaetning. Er der ingen internetforbindelse,
 * skjuler vi klokkeslaettet og viser alt det andet som normalt.
 */
#define ZS_NTP_SERVER_1      "dk.pool.ntp.org"
#define ZS_NTP_SERVER_2      "pool.ntp.org"
/* Dansk tid med sommertid, i POSIX-format. */
#define ZS_TIMEZONE          "CET-1CEST,M3.5.0,M10.5.0/3"

/* ── Lysstyrke ────────────────────────────────────────────────────── */
#define ZS_BRIGHTNESS_DEFAULT   80

/*
 * Hoejeste gyldige tema-nummer i lageret.
 *
 * Staar her og ikke i zs_theme.h, fordi lageret ikke maa afhaenge af
 * brugerfladen: zs_nvs.c skal kunne oversaettes uden LVGL. Der er en
 * test der tjekker at tallet passer med ZS_THEME_COUNT, saa de to ikke
 * kan komme ud af trit naar der tilfoejes et tema.
 */
#define ZS_THEME_MAKS           2

/*
 * Selvtest af sideombygning og temaskift.
 *
 * 0 i alt der sendes ud. Saettes til 1 naar der er roert ved en skaerm
 * eller ved temaet, saa man kan se paa enheden at intet siver og at
 * alle sider stadig kan tegnes. Se zs_selftest.c.
 */
#ifndef ZS_SELFTEST
#define ZS_SELFTEST             0
#endif
#define ZS_BRIGHTNESS_NIGHT     25
#define ZS_NIGHT_START_HOUR     22
#define ZS_NIGHT_END_HOUR       6

/* ── Opdatering ───────────────────────────────────────────────────── */
/*
 * Hvor tit skaermen ser efter ny firmware.
 *
 * En halv time. Det giver to forespoergsler i timen mod GitHub, som
 * tillader tres i timen uden noegle, saa der er rigelig luft. Ved
 * opstart tjekkes der efter et halvt minut, saa wifi og ur naar at
 * komme op foerst.
 */
#define ZS_OTA_CHECK_INTERVAL_MS   (30 * 60 * 1000)

/* ── Hjælp ────────────────────────────────────────────────────────── */
/*
 * Hvem kunden skal ringe til.
 *
 * Staar ét sted, fordi det er den slags der bliver aendret en dag og
 * saa skal findes igen. Bruges naar inverteren melder en fejlkode vi
 * ikke kan saette ord paa: saa er den eneste rigtige besked at
 * fortaelle hvem der kan.
 */
#define ZS_SUPPORT_NAME   "ZOL Energi"
#define ZS_SUPPORT_PHONE  "+45 7060 3676"

/* ── Demo ─────────────────────────────────────────────────────────── */
/*
 * Demo-tilstand: hovedskaermen med opdigtede tal, saa man kan se
 * hvordan skaermen opfoerer sig uden et anlaeg i naerheden.
 *
 * Saettes den til 0, findes hverken knappen "Se demo" eller koden bag
 * den i den byggede fil. Det er hovedafbryderen til produktionsenheder.
 *
 * Uanset hvad gemmes demo ALDRIG. En genstart slaar den altid fra, saa
 * en enhed hos en kunde kan ikke starte op i demo.
 */
#define ZS_DEMO_ENABLED   1

#endif /* ZS_CONFIG_H */
