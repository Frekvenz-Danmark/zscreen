/*
 * zScreen - SunSpec-lag.
 *
 * SunSpec er den faelles registerstandard som Fronius, Kostal, SMA,
 * Huawei og de fleste andre invertere taler. I stedet for at gaette
 * hvilket register der er hvad, ligger der en kaede af "modeller" i
 * inverterens hukommelse som man vandrer igennem.
 *
 * Kaeden ser saadan ud, med start ved register 40000 eller 40001:
 *
 *     "SunS"          2 registre, fast markoer 0x5375 0x6E53
 *     model-ID        1 register     fx 103 = trefaset inverter
 *     laengde         1 register     antal dataregistre der foelger
 *     data            laengde reg.
 *     model-ID        1 register     naeste model
 *     laengde         1 register
 *     data            ...
 *     0xFFFF          slut paa kaeden
 *
 * Alle offsets i denne fil er talt fra det FOERSTE DATAREGISTER i en
 * model, altsaa efter ID og laengde. Det er samme talning som i
 * SunSpec' egne smdx-XML-filer, saa man kan slaa op uden hovedregning.
 *
 * Offsets er verificeret mod https://github.com/sunspec/models
 * (smdx_00001, 00103, 00113, 00120, 00124, 00160, 00203, 00213)
 * den 25. august 2026, ikke skrevet efter hukommelse.
 */

#ifndef ZS_SUNSPEC_H
#define ZS_SUNSPEC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Model-ID'er                                                         */
/* ------------------------------------------------------------------ */

#define ZS_SS_COMMON            1     /* fabrikat, model, firmware, serienr */
#define ZS_SS_INV_1PH           101   /* inverter, heltal + skalafaktor     */
#define ZS_SS_INV_SPLIT         102
#define ZS_SS_INV_3PH           103
#define ZS_SS_INV_1PH_F         111   /* inverter, 32-bit flydende tal      */
#define ZS_SS_INV_SPLIT_F       112
#define ZS_SS_INV_3PH_F         113
#define ZS_SS_NAMEPLATE         120   /* maerkeplade, bl.a. batterikapacitet */
#define ZS_SS_STORAGE           124   /* batteri: SoC og ladetilstand        */
#define ZS_SS_MPPT              160   /* DC-kanaler: solstrenge og batteri   */
#define ZS_SS_METER_1PH         201   /* elmaaler, heltal + skalafaktor      */
#define ZS_SS_METER_SPLIT       202
#define ZS_SS_METER_3PH_WYE     203
#define ZS_SS_METER_3PH_DELTA   204
#define ZS_SS_METER_1PH_F       211   /* elmaaler, flydende tal              */
#define ZS_SS_METER_SPLIT_F     212
#define ZS_SS_METER_3PH_WYE_F   213
#define ZS_SS_METER_3PH_DELTA_F 214
#define ZS_SS_END               0xFFFF

/* ------------------------------------------------------------------ */
/* Offsets, alle fra foerste dataregister i modellen                   */
/* ------------------------------------------------------------------ */

/* Model 1, Common. smdx_00001.xml */
#define ZS_M1_MN                0     /* fabrikat, 16 reg / 32 tegn  */
#define ZS_M1_MN_LEN            16
#define ZS_M1_MD                16    /* model, 16 reg               */
#define ZS_M1_MD_LEN            16
#define ZS_M1_OPT               32    /* variant, 8 reg              */
#define ZS_M1_VR                40    /* firmware, 8 reg             */
#define ZS_M1_VR_LEN            8
#define ZS_M1_SN                48    /* serienummer, 16 reg         */
#define ZS_M1_SN_LEN            16
#define ZS_M1_DA                64    /* Modbus-adresse              */
#define ZS_M1_MIN_LEN           65

/* Model 101/102/103, inverter med skalafaktor. smdx_00103.xml */
#define ZS_M103_W               12    /* AC-effekt, int16            */
#define ZS_M103_W_SF            13
#define ZS_M103_HZ              14    /* netfrekvens, uint16         */
#define ZS_M103_HZ_SF           15
#define ZS_M103_WH              22    /* samlet produktion, acc32    */
#define ZS_M103_WH_SF           24
#define ZS_M103_ST              36    /* driftstilstand, enum16      */
#define ZS_M103_MIN_LEN         38

/* Model 111/112/113, inverter med flydende tal. smdx_00113.xml */
#define ZS_M113_W               20    /* AC-effekt, float32          */
#define ZS_M113_HZ              22    /* netfrekvens, float32        */
#define ZS_M113_WH              30    /* samlet produktion, float32  */
#define ZS_M113_ST              46    /* driftstilstand, enum16      */
#define ZS_M113_MIN_LEN         48

/* Model 120, Nameplate. smdx_00120.xml */
#define ZS_M120_WRTG            1     /* inverterens AC-maerkeeffekt */
#define ZS_M120_WRTG_SF         2
#define ZS_M120_WHRTG           17    /* batterikapacitet i Wh       */
#define ZS_M120_WHRTG_SF        18
#define ZS_M120_MIN_LEN         19

/* Model 124, Storage. smdx_00124.xml */
#define ZS_M124_WCHA_MAX        0     /* maks ladeeffekt, uint16     */
#define ZS_M124_STOR_CTL_MOD    3
#define ZS_M124_MIN_RSV_PCT     5
#define ZS_M124_CHA_STATE       6     /* SoC i procent, uint16       */
#define ZS_M124_CHA_ST          9     /* ladetilstand, enum16        */
#define ZS_M124_WCHA_MAX_SF     16
#define ZS_M124_CHA_STATE_SF    20
#define ZS_M124_MIN_LEN         21

/* Model 124 ChaSt, enum-vaerdier */
#define ZS_CHAST_OFF            1
#define ZS_CHAST_EMPTY          2
#define ZS_CHAST_DISCHARGING    3
#define ZS_CHAST_CHARGING       4
#define ZS_CHAST_FULL           5
#define ZS_CHAST_HOLDING        6
#define ZS_CHAST_TESTING        7

/*
 * Model 160, MPPT. smdx_00160.xml
 *
 * OBS: N ligger paa offset 6, ikke 5.
 * Evt er en bitfield32 og fylder derfor BAADE offset 4 og 5. Laeser man
 * N paa offset 5, faar man den nederste halvdel af Evt. Den er som
 * regel 0, hvilket giver "nul kanaler", og saa forsvinder hele den
 * label-baserede kanalgenkendelse lydloest.
 * Verificeret mod smdx_00160.xml den 25. august 2026.
 */
#define ZS_M160_DCA_SF          0
#define ZS_M160_DCV_SF          1
#define ZS_M160_DCW_SF          2
#define ZS_M160_DCWH_SF         3
#define ZS_M160_EVT             4     /* bitfield32, fylder 4 og 5   */
#define ZS_M160_N               6     /* antal DC-kanaler            */
#define ZS_M160_TMSPER          7
#define ZS_M160_CH_BASE         8     /* foerste kanalblok           */
#define ZS_M160_CH_SIZE         20    /* registre pr. kanal          */
#define ZS_M160_MAX_CH          8     /* vi laeser hoejst saa mange  */

/* Offsets inde i én kanalblok */
#define ZS_M160_CH_ID           0
#define ZS_M160_CH_IDSTR        1     /* 8 reg / 16 tegn             */
#define ZS_M160_CH_IDSTR_LEN    8
#define ZS_M160_CH_DCA          9
#define ZS_M160_CH_DCV          10
#define ZS_M160_CH_DCW          11    /* DC-effekt, uint16           */
#define ZS_M160_CH_DCWH         12    /* acc32, fylder 12 og 13      */
#define ZS_M160_CH_TMS          14
#define ZS_M160_CH_TMP          16
#define ZS_M160_CH_DCST         17    /* driftstilstand, enum16      */
#define ZS_M160_CH_DCEVT        18

/*
 * DCSt, enum16 per smdx_00160.xml. 1-indekseret.
 *   1 OFF   2 SLEEPING   3 STARTING   4 MPPT   5 THROTTLING
 *   6 SHUTTING_DOWN      7 FAULT      8 STANDBY   9 TEST
 * Kun 4 og 5 betyder at kanalen rent faktisk leverer noget. 3 er en
 * overgang paa vej op, og skal ikke taelle med.
 */
#define ZS_DCST_OFF             1
#define ZS_DCST_SLEEPING        2
#define ZS_DCST_STARTING        3
#define ZS_DCST_MPPT            4
#define ZS_DCST_THROTTLING      5
#define ZS_DCST_SHUTTING_DOWN   6
#define ZS_DCST_FAULT           7
#define ZS_DCST_STANDBY         8
#define ZS_DCST_TEST            9

/* Model 201-204, elmaaler med skalafaktor. smdx_00203.xml */
#define ZS_M203_W               16    /* samlet effekt, int16        */
#define ZS_M203_W_SF            20
#define ZS_M203_MIN_LEN         21

/* Model 211-214, elmaaler med flydende tal. smdx_00213.xml */
#define ZS_M213_W               26    /* samlet effekt, float32      */
#define ZS_M213_MIN_LEN         28

/* ------------------------------------------------------------------ */
/* "Ikke implementeret"-vaerdier                                       */
/*                                                                     */
/* SunSpec har ingen NULL. I stedet har hver datatype en vaerdi der    */
/* betyder "det her kan jeg ikke maale". Filtrerer man dem ikke fra,   */
/* faar man tal som -32768 W eller 65535 % vist paa skaermen, eller    */
/* endnu vaerre: 10 oploeftet i -32768, som bliver til 0 og ligner     */
/* et rigtigt maalt nul.                                               */
/* ------------------------------------------------------------------ */
#define ZS_SS_NA_UINT16         0xFFFFu
#define ZS_SS_NA_INT16          ((int16_t)0x8000)   /* -32768 */
#define ZS_SS_NA_SUNSSF         ((int16_t)0x8000)
#define ZS_SS_NA_ENUM16         0xFFFFu
#define ZS_SS_NA_ACC32          0u
/* float32: NaN. Testes med isnan(). */

/* ------------------------------------------------------------------ */
/* Datatyper                                                           */
/* ------------------------------------------------------------------ */

/*
 * En maaling der enten findes eller ikke findes.
 *
 * Vi bruger den overalt i stedet for at lade 0 betyde baade "nul watt"
 * og "ved ikke". Paa en energiskaerm er der forskel: 0 W fra solcellerne
 * om natten er rigtigt, mens 0 W fordi vi ikke kunne laese registret er
 * en loegn brugeren ikke kan gennemskue.
 */
typedef struct {
    bool  ok;
    float v;
} zs_val_t;

#define ZS_VAL_NONE  ((zs_val_t){ .ok = false, .v = 0.0f })

static inline zs_val_t zs_val(float v) { zs_val_t r = { true, v }; return r; }

/* Én model i kaeden. */
typedef struct {
    uint16_t id;
    uint16_t addr;   /* 0-baseret adresse paa foerste DATAregister */
    uint16_t len;    /* antal dataregistre                         */
} zs_ss_model_t;

#define ZS_SS_MAX_MODELS 24

typedef struct {
    zs_ss_model_t models[ZS_SS_MAX_MODELS];
    uint8_t       count;
    uint16_t      base;       /* hvor "SunS" blev fundet   */
    bool          truncated;  /* kaeden var laengere end vi har plads til */
} zs_ss_map_t;

/*
 * Laesefunktion som vandringen bruger. Returnerer true ved succes.
 * Findes for at kunne teste hele SunSpec-laget paa en Mac med et
 * simuleret registerkort, uden socket og uden inverter.
 */
typedef bool (*zs_ss_read_fn)(void *ctx, uint8_t unit, uint16_t addr,
                              uint16_t count, uint16_t *out);

/* ------------------------------------------------------------------ */
/* Vandring og opslag                                                  */
/* ------------------------------------------------------------------ */

/*
 * Gaar kaeden igennem og fylder map.
 * Returnerer false hvis "SunS"-markoeren ikke blev fundet, altsaa hvis
 * enheden ikke taler SunSpec paa den unit.
 */
bool zs_ss_walk(zs_ss_read_fn read, void *ctx, uint8_t unit, zs_ss_map_t *map);

/* Foerste model med det ID, eller NULL. */
const zs_ss_model_t *zs_ss_find(const zs_ss_map_t *map, uint16_t id);

/* Foerste model der matcher ét af ID'erne i listen, eller NULL. */
const zs_ss_model_t *zs_ss_find_any(const zs_ss_map_t *map,
                                    const uint16_t *ids, size_t n_ids);

/* ------------------------------------------------------------------ */
/* Afkodning. Alle arbejder paa et registerblok der allerede er laest.  */
/* n er antal registre i blokken, saa vi aldrig laeser udenfor.         */
/* ------------------------------------------------------------------ */

int16_t  zs_ss_i16(uint16_t raw);

/* 10 oploeftet i sf. sf uden for -10..10 giver 0 og betragtes som fejl. */
bool     zs_ss_sf_valid(int16_t sf);
float    zs_ss_apply_sf(float raw, int16_t sf);

/* int16-vaerdi med skalafaktor et andet sted i blokken. */
zs_val_t zs_ss_dec_i16_sf(const uint16_t *regs, size_t n, size_t off, size_t sf_off);
/* uint16-vaerdi med skalafaktor. */
zs_val_t zs_ss_dec_u16_sf(const uint16_t *regs, size_t n, size_t off, size_t sf_off);
/* 32-bit IEEE float, big-endian, hoej halvdel foerst. */
zs_val_t zs_ss_dec_f32(const uint16_t *regs, size_t n, size_t off);
/*
 * acc32, en 32-bit taeller. Returneres som uint32 og IKKE som float.
 *
 * Levetidsproduktion i Wh loeber hurtigt over 16,7 millioner, og der
 * begynder en 32-bit float at springe hele tal over. Et anlaeg paa
 * 20 MWh ville faa sin taeller til at staa stille i ryk. Derfor holder
 * vi acc32 i heltal helt ud til det sted hvor den skal vises.
 *
 * 0 betyder "ikke implementeret" per SunSpec.
 */
bool zs_ss_dec_acc32(const uint16_t *regs, size_t n, size_t off, uint32_t *out);
/* enum16. Returnerer -1 hvis udenfor blokken eller 0xFFFF. */
int32_t  zs_ss_dec_enum16(const uint16_t *regs, size_t n, size_t off);

/*
 * SunSpec-streng til C-streng.
 *
 * Strengene er pakket to tegn pr. register, hoejeste byte foerst. De er
 * ikke altid nul-afsluttede, kan vaere polstret med mellemrum eller
 * nul-bytes, og maa ikke antages at vaere gyldig UTF-8. Vi skriver altid
 * en nul-afsluttet streng ud, klipper polstring af i begge ender, og
 * erstatter tegn uden for det printbare ASCII-omraade med mellemrum saa
 * en snavset streng ikke oedelaegger skaermen.
 *
 * Returnerer laengden af det der blev skrevet.
 */
size_t zs_ss_dec_string(const uint16_t *regs, size_t n, size_t off,
                        size_t n_regs, char *out, size_t out_len);

/* ------------------------------------------------------------------ */
/* Kanal-roller i model 160                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    ZS_CH_UNKNOWN = 0,
    ZS_CH_PV,                 /* solstreng                    */
    ZS_CH_BATTERY_CHARGE,     /* batteriets ladeside          */
    ZS_CH_BATTERY_DISCHARGE,  /* batteriets afladeside        */
} zs_ch_role_t;

/*
 * Bestemmer en kanals rolle ud fra dens IDStr-label.
 *
 * Producent-uafhaengigt: det er inverteren selv der fortaeller hvad
 * kanalen er, i stedet for at vi gaetter ud fra kanalnummeret. Fronius
 * bruger "MPPT 1" og "Storage Charge"/"STCHA", Kostal bruger "DC_STRING".
 * Vi normaliserer foerst, fordi firmwareversioner varierer mellem
 * "STCHA", "ST CHA" og "  STCHA  ".
 *
 * STDISCHA tjekkes FOER STCHA, fordi "STCHA" er en delstreng af
 * "STDISCHA". Byttes de om, bliver afladekanalen kaldt ladekanal, og
 * batteriets fortegn vender forkert.
 */
zs_ch_role_t zs_ss_classify_channel(const char *idstr);

/* Er kanalen aktiv lige nu? Kun MPPT og THROTTLING taeller. */
bool zs_ss_channel_active(int32_t dcst);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SUNSPEC_H */
