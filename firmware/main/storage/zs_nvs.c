#include "zs_nvs.h"
#include "zs_config.h"
/* Kun for de to standardvaerdier nedenfor. Headeren traekker
 * ingenting andet ind end stdint og stdbool. */
#include "zs_modbus_tcp.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"       /* esp_wifi_restore */
#include "esp_phy_init.h"  /* esp_phy_erase_cal_data_in_nvs */
#include "esp_log.h"

#include <string.h>

static const char *TAG = "nvs";

/* Alt ligger i ét navnerum. Noeglerne er korte fordi NVS hoejst tager
 * 15 tegn, hvilket er nemt at glemme indtil det pludselig fejler. */
#define NS          "zscreen"
/* Energirummet staar her og ikke kun nede ved sine egne
 * funktioner: zs_nvs_factory_reset skal ogsaa rydde det, og den
 * ligger foer dem. */
#define NS_ENERGI   "zsenergi"
#define K_SSID      "ssid"
#define K_PASS      "pass"
#define K_IP        "inv_ip"
#define K_PORT      "inv_port"
#define K_UNIT      "inv_unit"
#define K_SERIAL    "inv_sn"
#define K_METERSIGN "meter_pos"
#define K_ZONE      "pris_zone"
#define K_BRIGHT    "bright"
#define K_NIGHT     "night"
#define K_THEME     "theme"
#define K_CONF      "configured"

void zs_nvs_defaults(zs_settings_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->inverter_port = 502;
    s->inverter_unit = 1;
    s->meter_import_positive = true;
    s->brightness = ZS_BRIGHTNESS_DEFAULT;
    s->night_dimming = true;
    s->theme = 0;               /* moerkt */
    s->configured = false;
}

/* Laeser en streng. Efterlader dst tom hvis noeglen ikke findes. */
static void get_str(nvs_handle_t h, const char *key, char *dst, size_t len)
{
    size_t n = len;
    if (nvs_get_str(h, key, dst, &n) != ESP_OK) {
        dst[0] = '\0';
        return;
    }
    /* NVS lover ikke en afsluttende nulbyte hvis noget er gaaet galt
     * under skrivningen. Vi saetter den selv, ellers kan en beskadiget
     * post loebe ud over bufferen naar den senere bruges. */
    dst[len - 1] = '\0';
}

static void get_u8(nvs_handle_t h, const char *key, uint8_t *dst)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) {
        *dst = v;
    }
}

static void get_u16(nvs_handle_t h, const char *key, uint16_t *dst)
{
    uint16_t v;
    if (nvs_get_u16(h, key, &v) == ESP_OK) {
        *dst = v;
    }
}

static void get_bool(nvs_handle_t h, const char *key, bool *dst)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) {
        *dst = (v != 0);
    }
}

bool zs_nvs_load(zs_settings_t *s)
{
    if (s == NULL) {
        return false;
    }
    zs_nvs_defaults(s);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Foerste opstart efter flashning. Ikke en fejl. */
        ESP_LOGI(TAG, "ingen gemte indstillinger, skaermen skal saettes op");
        return false;
    }

    get_str(h, K_SSID, s->wifi_ssid, sizeof(s->wifi_ssid));
    get_str(h, K_PASS, s->wifi_pass, sizeof(s->wifi_pass));
    get_str(h, K_IP,   s->inverter_ip, sizeof(s->inverter_ip));
    get_u16(h, K_PORT, &s->inverter_port);
    get_u8(h, K_UNIT,  &s->inverter_unit);
    get_str(h, K_SERIAL, s->inverter_serial, sizeof(s->inverter_serial));
    get_bool(h, K_METERSIGN, &s->meter_import_positive);
    get_str(h, K_ZONE, s->price_zone, sizeof(s->price_zone));
    get_u8(h, K_BRIGHT, &s->brightness);
    get_bool(h, K_NIGHT, &s->night_dimming);
    get_u8(h, K_THEME, &s->theme);
    get_bool(h, K_CONF,  &s->configured);

    nvs_close(h);

    /*
     * Tjek at det vi laeste kan bruges til noget.
     *
     * En halvskrevet post, fx efter en stroemafbrydelse midt i en
     * gemning, kan give et tomt netvaerksnavn og en flaget der siger
     * "sat op". Saa ville skaermen gaa direkte til hovedskaermen og
     * staa og vente paa en forbindelse der aldrig kommer, uden at
     * fortaelle brugeren hvorfor.
     */
    if (s->configured) {
        if (s->wifi_ssid[0] == '\0' || s->inverter_ip[0] == '\0') {
            ESP_LOGW(TAG, "gemte indstillinger er ufuldstaendige, "
                          "skaermen skal saettes op igen");
            zs_nvs_defaults(s);
            return false;
        }
    }
    /* Nul betyder "ikke valgt", og saa tager vi standarden. Tallene
     * staar i zs_modbus_tcp.h og ikke her: en Fronius der skal paa 1502
     * skal kunne rettes ét sted. */
    if (s->inverter_port == 0) {
        s->inverter_port = ZS_MB_DEFAULT_PORT;
    }
    if (s->inverter_unit == 0) {
        s->inverter_unit = ZS_MB_DEFAULT_UNIT;
    }
    if (s->brightness < 5 || s->brightness > 100) {
        s->brightness = ZS_BRIGHTNESS_DEFAULT;
    }
    if (s->theme > ZS_THEME_MAKS) {
        /* En vaerdi vi ikke kender. Moerkt er standard, og en skaerm der
         * pludselig ser helt anderledes ud ville ligne en fejl.
         *
         * Graensen staar i zs_config.h og ikke som et tal her. Lageret
         * skal ikke kende brugerfladens opregning, men det skal vide
         * hvor mange temaer der findes, og det tal maa kun staa ét
         * sted. Der er en test der tjekker at de to er enige. */
        s->theme = 0;
    }

    ESP_LOGI(TAG, "indstillinger laest: netvaerk \"%s\", inverter %s:%u",
             s->wifi_ssid, s->inverter_ip, s->inverter_port);
    return s->configured;
}

bool zs_nvs_save(const zs_settings_t *s)
{
    if (s == NULL) {
        return false;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke aabne lageret: %s", esp_err_to_name(err));
        return false;
    }

    bool ok = true;

    /*
     * Er skaermen IKKE sat op, ryddes flaget FOERST og forpligtes for
     * sig. Saa kan en stroemafbrydelse midt i resten ikke efterlade et
     * flag der siger "sat op" oven paa halvt skrevne vaerdier.
     *
     * I dag saetter ingen sti flaget tilbage til false, men den dag
     * nogen tilfoejer "skift inverter", skal det virke uden at nogen
     * skal huske det her.
     */
    if (!s->configured) {
        ok &= nvs_set_u8(h, K_CONF, 0) == ESP_OK;
        ok &= nvs_commit(h) == ESP_OK;
    }

    ok &= nvs_set_str(h, K_SSID, s->wifi_ssid) == ESP_OK;
    ok &= nvs_set_str(h, K_PASS, s->wifi_pass) == ESP_OK;
    ok &= nvs_set_str(h, K_IP,   s->inverter_ip) == ESP_OK;
    ok &= nvs_set_u16(h, K_PORT, s->inverter_port) == ESP_OK;
    ok &= nvs_set_u8(h,  K_UNIT, s->inverter_unit) == ESP_OK;
    ok &= nvs_set_str(h, K_SERIAL, s->inverter_serial) == ESP_OK;
    ok &= nvs_set_u8(h,  K_METERSIGN, s->meter_import_positive ? 1 : 0) == ESP_OK;
    ok &= nvs_set_str(h, K_ZONE, s->price_zone) == ESP_OK;
    ok &= nvs_set_u8(h,  K_BRIGHT, s->brightness) == ESP_OK;
    ok &= nvs_set_u8(h,  K_NIGHT, s->night_dimming ? 1 : 0) == ESP_OK;
    ok &= nvs_set_u8(h,  K_THEME, s->theme) == ESP_OK;

    /*
     * "Sat op"-flaget skrives SIDST og forpligtes i sin egen omgang.
     *
     * NVS skriver foerst rigtigt naar man kalder commit. Ryger stroemmen
     * midt i, er det bedre at flaget mangler end at det staar der uden
     * at resten naaede med: saa beder skaermen om at blive sat op igen,
     * i stedet for at staa og vente paa en inverter den ikke kender
     * adressen paa.
     */
    if (ok) {
        ok &= nvs_commit(h) == ESP_OK;
    }
    if (ok && s->configured) {
        ok &= nvs_set_u8(h, K_CONF, 1) == ESP_OK;
        ok &= nvs_commit(h) == ESP_OK;
    }

    nvs_close(h);

    if (!ok) {
        ESP_LOGE(TAG, "indstillingerne kunne ikke gemmes");
    } else {
        ESP_LOGI(TAG, "indstillinger gemt");
    }
    return ok;
}

/* Sletter ét rum i NVS. Findes rummet ikke, er der intet at gOEre, og
 * det er ikke en fejl. */
static bool ryd_rum(const char *rum)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(rum, NVS_READWRITE, &h);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        return true;              /* har aldrig vaeret skrevet */
    }
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "kunne ikke aabne rummet %s: %s", rum,
                 esp_err_to_name(e));
        return false;
    }
    bool ok = (nvs_erase_all(h) == ESP_OK);
    ok &= (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

bool zs_nvs_factory_reset(void)
{
    /*
     * EN FABRIKSNULSTILLING SKAL EFTERLADE EN SKAERM DER IKKE VED NOGET.
     *
     * Her stod kun nvs_erase_all paa rummet "zscreen". Det var ikke nok,
     * og det var ikke til at se:
     *
     *   nvs.net80211   ESP'ens EGEN wifi-driver gemmer den sidste
     *                  forbindelse her, uden at spoerge os. En
     *                  "nulstillet" skaerm gik derfor paa det gamle net
     *                  af sig selv. Maalt 13. oktober 2026 paa den
     *                  rigtige skaerm: efter en nulstilling stod der
     *                  stadig ssid "H155-381_191B" og et kodeord, og
     *                  skaermen forbandt uden at nogen tastede noget.
     *                  esp_wifi_restore() er ESP-IDF's egen maade at
     *                  rydde det.
     *   zsenergi       livstaellernes udgangspunkt. Stod det tilbage,
     *                  ville den foerste time efter en nulstilling blive
     *                  regnet mod en taeller fra et andet anlaeg.
     *
     * zsfleet BLIVER, med vilje. Det er enhedens certifikat og noegle,
     * altsaa dens identitet, og den kan IKKE laves igen ude i marken:
     * noeglen ligger kun i ca/ paa en Mac. Slettede vi den, ville en
     * kunde der trykker nulstil, saette skaermen ud af drift indtil den
     * kom tilbage paa et bord. Besluttet 13. oktober 2026.
     *
     * phy BLEV OGSAA BEHOLDT ÉN GANG. Det var en fejl, og den kostede
     * en halv dag at finde - se noten ved esp_phy_erase_cal_data_in_nvs
     * nedenfor.
     */
    bool ok = ryd_rum(NS);
    ok &= ryd_rum(NS_ENERGI);

    /*
     * Og wifi-driverens eget. esp_wifi_restore() rydder rummet
     * nvs.net80211 og saetter driveren tilbage til standard.
     *
     * Den kan kun kaldes mens wifi'en er startet. Er den ikke det,
     * svarer den ESP_ERR_WIFI_NOT_INIT, og saa er der heller ikke noget
     * gemt der kan genbruges foer naeste start - saa det er ikke en
     * fejl, men det skal staa i loggen.
     */
    esp_err_t w = esp_wifi_restore();
    if (w != ESP_OK && w != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "wifi-driverens eget kunne ikke ryddes: %s",
                 esp_err_to_name(w));
        ok = false;
    } else if (w == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "wifi'en var ikke startet, saa driverens eget rum "
                      "blev ikke roert");
    }

    /*
     * OG RADIOENS KALIBRERING. DEN ER GRUNDEN TIL AT SOEGNINGEN SVIGTEDE.
     *
     * HVAD DER BLEV MAALT. Efter en fabriksnulstilling kunne skaermen
     * ikke finde inverteren ved at soege - men den kunne forbinde til
     * den med det samme hvis adressen blev skrevet i haanden. Samme net,
     * samme inverter, samme sekund. Og nulstillede man i stedet med
     * lokalt/nulstil.sh og flashede igen, virkede soegningen.
     *
     * DEN ENESTE FORSKEL mellem de to var det her rum. nulstil.sh
     * sletter HELE NVS-partitionen med erase_region og tager
     * kalibreringen med; fabriksnulstillingen beholdt den med vilje,
     * fordi en fuld kalibrering koster et oejeblik ved naeste opstart.
     *
     * HVORFOR DET RAMMER NETOP SOEGNINGEN. Byggeriet bruger DELVIS
     * kalibrering oven paa de gemte tal:
     *
     *     CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE 1
     *     CONFIG_ESP_PHY_CALIBRATION_MODE             0  (partial)
     *
     * Er de tal daarlige, bliver radioen svag uden at noget melder fejl.
     * ÉN forbindelse til ÉN adresse maerker det ikke - TCP sender bare
     * igen. Men soegningen banker paa 254 adresser med 12 samtidige
     * sockets og ZS_SCAN_PORT_TIMEOUT_MS per stykke, og dér falder en
     * marginal radio igennem. Derfor virkede den manuelle vej og ikke
     * soegningen, og derfor saa det ud som en fejl i soegningen.
     *
     * esp_phy_erase_cal_data_in_nvs() er ESP-IDF's EGEN loesning. Deres
     * dokumentation kalder den "a last-resort remedy if partial
     * calibration is used". Samme grund som esp_wifi_restore() ovenfor:
     * vi rydder rummet med bibliotekets egen funktion og ikke ved at
     * skrive dets navn af - navnet er en detalje der kan skifte.
     *
     * Prisen er den oprindelige begrundelse: naeste opstart kalibrerer
     * fuldt og tager et oejeblik laengere. Det er en engangsudgift paa
     * en skaerm der netop er nulstillet, og den er intet mod en soegning
     * der ikke finder noget.
     *
     * Nu er en fabriksnulstilling det samme som nulstil.sh plus en
     * flashning, PAA ÉN undtagelse: zsfleet. Og den staar skrevet
     * ovenfor.
     */
    esp_err_t pe = esp_phy_erase_cal_data_in_nvs();
    if (pe != ESP_OK) {
        /* Ikke faldt over: er der ingen kalibrering gemt, er der heller
         * ingenting der kan vaere daarligt. Men det skal staa i loggen,
         * for det er den slags man leder efter bagefter. */
        ESP_LOGW(TAG, "radioens kalibrering kunne ikke ryddes: %s",
                 esp_err_to_name(pe));
    }

    if (ok) {
        ESP_LOGW(TAG, "alt er slettet: indstillinger, energigrundlag, "
                      "wifi-driverens eget og radioens kalibrering. "
                      "Certifikatet er beholdt.");
    }
    return ok;
}

/* ------------------------------------------------------------------ */
/* Timeenergiens udgangspunkt. Se zs_nvs.h for hvorfor.                */
/* ------------------------------------------------------------------ */

#define K_BASIS     "basis"

bool zs_nvs_save_energi(const zs_energi_basis_t *b)
{
    if (b == NULL) {
        return false;
    }
    nvs_handle_t h;
    if (nvs_open(NS_ENERGI, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    /*
     * Hele strukturen som én klump.
     *
     * Felt for felt ville vaere paenere, men det her skrives hver time,
     * og hvert felt er en skrivning for sig. Som én klump er det ét
     * slid i timen i stedet for tolv. Og i modsaetning til kundens
     * indstillinger er der ingen opgraderingssti at tage hensyn til:
     * passer stoerrelsen ikke, smider vi den vaek og starter forfra, og
     * saa koster det én time.
     */
    esp_err_t err = nvs_set_blob(h, K_BASIS, b, sizeof(*b));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err == ESP_OK;
}

bool zs_nvs_load_energi(zs_energi_basis_t *b)
{
    if (b == NULL) {
        return false;
    }
    memset(b, 0, sizeof(*b));

    nvs_handle_t h;
    if (nvs_open(NS_ENERGI, NVS_READONLY, &h) != ESP_OK) {
        return false;           /* foerste gang, ikke en fejl */
    }
    size_t n = sizeof(*b);
    esp_err_t err = nvs_get_blob(h, K_BASIS, b, &n);
    nvs_close(h);

    if (err != ESP_OK || n != sizeof(*b)) {
        /* Enten ingenting, eller noget fra en aeldre udgave hvor
         * strukturen saa anderledes ud. Begge dele koster én time. */
        memset(b, 0, sizeof(*b));
        return false;
    }
    if (b->time < 0 || b->time > 23) {
        /* Skrald. Hellere starte forfra end at regne paa det. */
        memset(b, 0, sizeof(*b));
        return false;
    }
    /*
     * Serienummeret kommer fra flashen, og flashen kan vaere skrald.
     * Mangler nulbyten, ville enhver laesning af det loebe forbi enden af
     * feltet. Vi saetter den paa plads i stedet for at stole paa at den
     * er der.
     */
    b->serienr[sizeof(b->serienr) - 1] = '\0';
    if (b->serienr[0] == '\0') {
        /* Et udgangspunkt uden en inverter kan vi ikke bruge til noget:
         * vi ved ikke hvis taellerstand det er. Start forfra. */
        memset(b, 0, sizeof(*b));
        return false;
    }
    return b->gyldig;
}
