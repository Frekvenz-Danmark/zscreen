/*
 * zScreen - Fronius-lag.
 *
 * Her bliver SunSpec-registre til de fire tal skaermen viser:
 *
 *     SOLCELLER   hvor meget solcellerne producerer lige nu
 *     FORBRUG     hvor meget huset bruger lige nu
 *     BATTERI     ladetilstand i procent, plus lade/afladeeffekt
 *     NETTET      hvad der koebes eller saelges lige nu
 *
 * Laget er skrevet vendor-neutralt hvor det kan lade sig goere. Det
 * hedder "fronius" fordi det er den inverter Frekvenz bruger, og fordi
 * et par af faldgruberne er specifikke for Fronius Gen24. En Kostal
 * eller SMA med samme SunSpec-modeller vil virke uden aendringer.
 *
 * Alt er skrivebeskyttet. Se sikkerhedsnoten i zs_modbus_tcp.h.
 */

#ifndef ZS_FRONIUS_H
#define ZS_FRONIUS_H

#include "zs_modbus_tcp.h"
#include "zs_sunspec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stoerste model vi laeser i ét stykke: model 160 med 8 kanaler
 * = 8 + 20 * 8 = 168 registre. Laesningen deles automatisk op i
 * flere Modbus-kald, for FC3 kan hoejst tage 125 ad gangen. */
#define ZS_FR_MAX_MODEL_REGS   176

/* Unit-ID'er vi leder efter elmaaleren paa.
 *
 * Fronius' egen manual siger 200 til 204. I marken har Zbox-flaaden
 * fundet den paa 201, fordi maaleradressen laegges oven i offset'et:
 * offset 200 + RTU-adresse 1 = 201. Aeldre Datamanager-opsaetninger
 * bruger 240 og 241. De sidste faa daekker anlaeg hvor nogen har sat
 * adressen manuelt uden offset. */
#define ZS_FR_METER_CANDIDATES { 200, 201, 202, 203, 204, 240, 241, 2, 3, 100, 247 }

typedef struct {
    char     manufacturer[33];   /* "Fronius"                    */
    char     model[33];          /* "Symo GEN24 10.0"            */
    char     version[17];        /* firmware                     */
    char     serial[33];         /* serienummer                  */

    bool     has_inverter;
    bool     has_meter;
    bool     has_battery;
    bool     has_mppt;

    uint16_t inverter_model_id;  /* 103 eller 113 osv.           */
    uint16_t meter_model_id;     /* 203 eller 213 osv., 0 = ingen */
    uint8_t  meter_unit;         /* 0 = ingen elmaaler fundet     */
    uint8_t  channel_count;      /* DC-kanaler i model 160        */
    bool     labels_usable;      /* inverteren navngiver sine kanaler */

    float    battery_capacity_kwh;  /* 0 = ukendt                 */
    float    inverter_rated_kw;     /* 0 = ukendt                 */
} zs_fr_info_t;

/* Én DC-kanal, som den saa ud ved sidste aflaesning. Bruges baade til
 * beregningen og til fejlsoegningssiden. */
typedef struct {
    char         label[17];
    zs_ch_role_t role;
    zs_val_t     dcw;
    int32_t      dcst;
    bool         active;
} zs_fr_channel_t;

typedef struct {
    zs_val_t solar_w;        /* solproduktion, aldrig negativ       */
    zs_val_t house_w;        /* husets forbrug                      */
    zs_val_t battery_w;      /* plus = aflader, minus = lader       */
    zs_val_t soc_pct;        /* 0 til 100                           */
    zs_val_t grid_w;         /* plus = koeber, minus = saelger      */
    zs_val_t inverter_ac_w;  /* inverterens AC-effekt, til udledning */
    zs_val_t grid_hz;

    int32_t  charge_status;  /* Model 124 ChaSt, -1 = ukendt        */

    zs_fr_channel_t channels[ZS_M160_MAX_CH];
    uint8_t         channel_count;
} zs_fr_live_t;

typedef struct {
    zs_mb_t      mb;
    char         host[46];
    uint16_t     port;
    uint8_t      inverter_unit;

    zs_ss_map_t  inv_map;
    zs_ss_map_t  meter_map;      /* tom hvis maaleren ligger i inverterens kaede */
    bool         meter_in_inverter_chain;

    zs_fr_info_t info;

    /*
     * Fortegn paa elmaaleren.
     *
     * SunSpec siger at Model 20x W er "total real power", men ikke
     * entydigt hvilken vej der er positiv. Det afhaenger af hvordan
     * stroemtangen er vendt ved installationen. Standard hos Fronius
     * er at positiv betyder koeb fra nettet.
     *
     * Vi har ikke kunnet efterproeve det mod et levende anlaeg endnu,
     * saa i stedet for at gaette én gang for alle, kan det vendes fra
     * Indstillinger paa skaermen. Saa kan en montoer rette det paa fem
     * sekunder i stedet for at vente paa ny firmware.
     */
    bool         meter_import_positive;

    /* Taeller hvor mange gange det udregnede forbrug er kommet ud
     * negativt. Et hus kan ikke bruge minus strøm, saa loeber den op,
     * er fortegnet ovenfor sandsynligvis vendt forkert. Vises paa
     * Detaljer-siden, og vi retter ikke selv: en skaerm der skifter
     * fortegn af sig selv midt i en maaling er vaerre end en der tager
     * fejl konsekvent. */
    uint32_t     negative_house_count;
    uint32_t     poll_count;
    uint32_t     poll_error_count;
    uint32_t     reconnect_count;

    bool         connected;

    /* Arbejdsbuffer. Ligger her og ikke paa stakken, fordi FreeRTOS-
     * tasks paa ESP32 har smaa stakke og 176 registre er 352 bytes. */
    uint16_t     block[ZS_FR_MAX_MODEL_REGS];
} zs_fr_t;

/* Nulstiller. Skal kaldes foer alt andet. */
void zs_fr_init(zs_fr_t *fr);

/*
 * Forbinder, vandrer SunSpec, laeser identitet og finder elmaaleren.
 *
 * connected saettes foerst naar ALT er paa plads. Det er med vilje:
 * saetter man flaget tidligt, kan en anden traad naa at spoerge om data
 * mens modelkortet stadig er tomt, og faa "ingen batteri" tilbage paa
 * et anlaeg der har ét.
 */
bool zs_fr_connect(zs_fr_t *fr, const char *host, uint16_t port, uint8_t unit);

void zs_fr_disconnect(zs_fr_t *fr);
bool zs_fr_is_connected(const zs_fr_t *fr);

/*
 * Én aflaesningsrunde. Fylder live.
 *
 * Bruger faa, store Modbus-kald i stedet for mange smaa: hele modellen
 * hentes i ét hug, hvilket giver 4 forespoergsler pr. runde i stedet
 * for omkring 30. Fronius anbefaler selv at man spoerger sekventielt og
 * ikke parallelt, og det passer godt med at holde antallet nede.
 *
 * Returnerer false hvis forbindelsen gik i stykker undervejs. De
 * felter der naaede at blive laest, staar stadig i live.
 */
bool zs_fr_poll(zs_fr_t *fr, zs_fr_live_t *live);

/*
 * Undersoeger om der sidder en SunSpec-enhed paa host:port.
 * Bruges af netvaerksscanningen i opsaetningen. Fylder info med det
 * den kan naa at laese. Egen kortvarig forbindelse, roerer ikke fr.
 */
bool zs_fr_probe(const char *host, uint16_t port, uint32_t timeout_ms,
                 zs_fr_info_t *info);

/* Tekst til Detaljer-siden, fx "Lader" eller "Aflader". Aldrig NULL. */
const char *zs_fr_charge_status_text(int32_t chast);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FRONIUS_H */
