# Motoren og fladen: den kode der er FAELLES for alle skaerme.
#
# Ligger i firmware/kerne/ ved siden af den her fil. Hvert produkt har
# sin egen mappe under firmware/boards/ og tager den her liste:
#
#   firmware/boards/zscreen/      SenseCAP Indicator D1, ESP32-S3
#   firmware/boards/zdisplay/     reTerminal D1001, ESP32-P4
#
# HVORFOR LISTEN STAAR ÉT STED.
#
# Begge produkter laeser Modbus fra samme inverter, regner de samme fire
# tal ud, melder ind paa samme flaade og opdaterer sig selv samme vej.
# Stod kildelisten to steder, ville en ny fil i net/ komme med i den ene
# skaerm og ikke i den anden - og det ville ikke vaere en byggefejl, men
# en manglende funktion hos en kunde paa det ene produkt.
#
# Et nyt produkt skriver derfor ikke sin egen liste. Det tager den her
# og laegger sine egne fire filer oveni. tools/check-byg.py haandhaever
# det.
#
# HVORFOR kerne/ IKKE ER EN SELVSTAENDIG KOMPONENT.
#
# Det ville vaere det pæne svar i ESP-IDF, og det blev proevet paa
# papiret foerst. Det gaar ikke: koden her KALDER ind i boardet -
# zs_display.c kalder zs_baglys_set(), fladen kalder lv_port_sem_take(),
# app-loekken kalder zs_lyd_bip() - og boardet kalder tilbage ind i
# motoren. To komponenter med gensidige kald er en cirkulaer
# afhaengighed, og den slags haandterer ESP-IDF daarligt.
#
# "main" er netop den komponent der er bygget til at vaere enden paa
# alting. Derfor bliver den faelles kode oversat IND I hvert boards
# main, og det her er listen over hvad der skal med. Mapperne er stadig
# skilt, og hvad der maa roere hardware er stadig haandhaevet.
#
# STIERNE ER ABSOLUTTE. En relativ sti ville betyde noget forskelligt
# fra hvert boards mappe, og den slags bliver opdaget en halv time
# senere end den burde.
#
# Filerne staar skrevet ud og ikke som GLOB. En GLOB betyder at cmake
# ikke opdager en ny fil foer man rydder byggemappen.

get_filename_component(ZS_KERNE_DIR  "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(ZS_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/../vendor" ABSOLUTE)

# ---------------------------------------------------------------------
# ZS_MOTOR_SRCS - ingen LVGL, ingen hardware
# ---------------------------------------------------------------------
#
# Den her liste er den samme kode der koerer i tests/host paa en Mac,
# uden ESP-IDF. Det er derfor Modbus-afkodningen og fejlvagten kan
# proeves uden en skaerm, og den egenskab er vaerd at passe paa: der maa
# ikke komme et bsp_-kald, en driver/gpio.h eller en lvgl.h ned i nogen
# af de her filer.
#
# Det er HAANDHAEVET i tools/check-byg.py, se FORBUDT_I_MOTOREN.
set(ZS_MOTOR_SRCS
    "${ZS_KERNE_DIR}/util/zs_tekst.c"
    "${ZS_KERNE_DIR}/util/zs_tid.c"

    "${ZS_KERNE_DIR}/net/zs_modbus_tcp.c"
    "${ZS_KERNE_DIR}/net/zs_sunspec.c"
    "${ZS_KERNE_DIR}/net/zs_fronius.c"
    "${ZS_KERNE_DIR}/net/zs_wifi.c"
    "${ZS_KERNE_DIR}/net/zs_discovery.c"
    "${ZS_KERNE_DIR}/net/zs_locate.c"
    "${ZS_KERNE_DIR}/net/zs_price.c"
    "${ZS_KERNE_DIR}/net/zs_price_now.c"
    "${ZS_KERNE_DIR}/net/zs_ota.c"
    "${ZS_KERNE_DIR}/net/zs_version.c"
    "${ZS_KERNE_DIR}/net/zs_fleet.c"
    "${ZS_KERNE_DIR}/net/zs_fleet_msg.c"
    "${ZS_KERNE_DIR}/net/zs_maerker.c"
    "${ZS_KERNE_DIR}/net/zs_energi.c"
    "${ZS_KERNE_DIR}/net/zs_kvarter.c"
    "${ZS_KERNE_DIR}/net/zs_tilmeld.c"
    "${ZS_KERNE_DIR}/net/zs_ladeplan.c"
    "${ZS_KERNE_DIR}/net/zs_ladeordre.c"

    "${ZS_KERNE_DIR}/app/zs_format.c"
    "${ZS_KERNE_DIR}/app/zs_app.c"
    "${ZS_KERNE_DIR}/app/zs_selftest.c"
    "${ZS_KERNE_DIR}/app/zs_demo.c"
    "${ZS_KERNE_DIR}/app/zs_status.c"
    # Vagten der afgoer hvad der er galt. Ren C, ingen LVGL.
    "${ZS_KERNE_DIR}/app/zs_fejl.c"

    "${ZS_KERNE_DIR}/storage/zs_nvs.c")

# ---------------------------------------------------------------------
# ZS_FLADE_SRCS - brugerfladen, LVGL 8.3
# ---------------------------------------------------------------------
#
# Faelles, men ikke gratis faelles: alt heri regner ud fra
# ZS_G_SCR_WIDTH og ZS_G_SCR_HEIGHT i ui/zs_layout.h, som boardet
# saetter fra byggeriet. Derfor passer den samme flade paa 480 x 480 og
# paa 800 x 1280. Se zs_layout.h og tests/host/test_layout_800.c.
set(ZS_FLADE_SRCS
    "${ZS_KERNE_DIR}/ui/zs_display.c"
    "${ZS_KERNE_DIR}/ui/zs_theme.c"
    "${ZS_KERNE_DIR}/ui/zs_tilegrid.c"
    "${ZS_KERNE_DIR}/ui/zs_ui.c"
    "${ZS_KERNE_DIR}/ui/zs_screen_home.c"
    "${ZS_KERNE_DIR}/ui/zs_screen_tilmeld.c"
    "${ZS_KERNE_DIR}/ui/zs_screen_fejl.c"
    "${ZS_KERNE_DIR}/ui/zs_screen_setup.c"
    "${ZS_KERNE_DIR}/ui/zs_screen_settings.c"
    "${ZS_KERNE_DIR}/ui/zs_qr.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_tile.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_flow.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_price_page.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_statusbar.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_keyboard.c"
    "${ZS_KERNE_DIR}/ui/widgets/zs_step.c"
    # QR til tilmeldingen. Udefra, MIT. Se vendor/qrcodegen/HVORFOR.md.
    "${ZS_VENDOR_DIR}/qrcodegen/qrcodegen.c")

# ---------------------------------------------------------------------
# ZS_ASSET_SRCS - genererede skrifttyper, ikoner og logoer
# ---------------------------------------------------------------------
#
# Bygges af tools/build-assets.sh.
#
# De er FAELLES indtil videre, og det er en midlertidig tilstand: de er
# skaaret til en 4 tommer skaerm. Paa 8 tommer er 64 px tal for smaa til
# at blive laest fra en sofa. Naar ZDisplays flade skal se rigtig ud,
# skal der genereres et saet mere, og saa bliver den her liste pr. board.
set(ZS_ASSET_SRCS
    "${ZS_KERNE_DIR}/ui/assets/zs_font_num_64.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_font_28.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_font_20.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_font_16.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_font_13.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_font_kb_24.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_icons_20.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_icons_28.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_img_zmark.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_img_zmark_pos.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_img_wordmark.c"
    "${ZS_KERNE_DIR}/ui/assets/zs_img_wordmark_pos.c")

# ---------------------------------------------------------------------
# ZS_KERNE_INCLUDE_DIRS
# ---------------------------------------------------------------------
#
# hal/ staar foerst, fordi det er DER kontrakterne mellem motoren og
# boardet ligger: zs_baglys.h, zs_lyd.h og lv_port.h. Et nyt board
# skriver en fil for hver af de tre, plus sin egen main.c.
set(ZS_KERNE_INCLUDE_DIRS
    "${ZS_KERNE_DIR}/hal"
    "${ZS_KERNE_DIR}"
    "${ZS_KERNE_DIR}/net"
    "${ZS_KERNE_DIR}/app"
    "${ZS_KERNE_DIR}/storage"
    "${ZS_KERNE_DIR}/util"
    "${ZS_KERNE_DIR}/ui"
    "${ZS_KERNE_DIR}/ui/widgets"
    "${ZS_VENDOR_DIR}/qrcodegen")

# ---------------------------------------------------------------------
# ZS_VERSION - versionsnummeret, ét sted for begge produkter
# ---------------------------------------------------------------------
#
# ESP-IDF laeser normalt version.txt fra PROJEKTETS mappe. Med to
# produkter ville det vaere to filer, og de ville komme ud af trit: de
# to udgives med SAMME versionsmaerke fra samme repo.
#
# Her stod der foer ingenting for ZDisplay, og saa faldt den tilbage paa
# "git describe". Det gav "v0.27.0-dirty" i en byggemappe med
# uncommitterede aendringer, og det er IKKE et versionsnummer
# zs_version_parse kan laese. Konsekvensen ville vaere en skaerm der
# aldrig opdaterer, fordi den ikke forstaar sin egen version.
#
# Derfor: ét version.txt i firmware/, laest her, sat for begge.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../version.txt" ZS_VERSION LIMIT_COUNT 1)
string(STRIP "${ZS_VERSION}" ZS_VERSION)
if(NOT ZS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "firmware/version.txt siger '${ZS_VERSION}'. Det skal vaere tre "
        "tal med punktummer, fx 0.27.0 - ellers kan skaermen ikke "
        "sammenligne sin egen version med et maerke paa GitHub, og saa "
        "opdaterer den aldrig.")
endif()

# ---------------------------------------------------------------------
# ZS_STRENGE_FLAG - advarsler i VORES kode er fejl
# ---------------------------------------------------------------------
#
# ESP-IDF slaar "-Werror=all" til, men tager fire undtagelser tilbage
# igen for alt der bygges. Det er rimeligt for kode vi ikke har
# skrevet. For VORES egen er de fire praecis de advarsler der betyder
# noget senere: en udfaset funktion forsvinder i naeste udgave af en
# komponent, og en variabel der saettes uden at bruges er som regel en
# halv rettelse.
#
# HVORFOR DET SKAL VAERE EN FEJL OG IKKE EN ADVARSEL.
#
# Vi havde en vagt der greppede byggeudskriften for advarsler. Den
# virkede én gang: 14. oktober 2026 fangede den en udfaset
# touch-funktion. Men ved NAESTE byg blev filen ikke oversat igen - den
# var uaendret - saa advarslen kom ikke, og vagten gik groen med fejlen
# stadig i koden. Det blev fundet ved at proeve vagten af.
#
# En advarsel er forbigaaende. En FEJL bliver siddende, fordi
# objektfilen aldrig bliver lavet.
set(ZS_STRENGE_FLAG
    -Werror=deprecated-declarations
    -Werror=unused-function
    -Werror=unused-variable
    -Werror=unused-but-set-variable)

# ---------------------------------------------------------------------
# ZS_KERNE_REQUIRES - det motoren og fladen skal have fra ESP-IDF
# ---------------------------------------------------------------------
#
# bsp og lvgl staar IKKE her: hvilken bsp der er, er boardets sag.
set(ZS_KERNE_REQUIRES
    esp_wifi
    # esp_phy staar her selvom den foer kom med af sig selv.
    #
    # storage/zs_nvs.c kalder esp_phy_erase_cal_data_in_nvs() i
    # fabriksnulstillingen. Paa ESP32-S3 fulgte esp_phy med esp_wifi, saa
    # headeren var der uden at nogen havde bedt om den. Paa ESP32-P4 er
    # der ingen radio i chippen: der leverer esp_wifi kun headere og
    # netif-bindinger, og saa stopper byggeriet paa
    #
    #     zs_nvs.c:10: fatal error: esp_phy_init.h: No such file
    #
    # OBS paa hvad kaldet GOER paa ZDisplay: radioen sidder i C6'eren, og
    # dens kalibrering ligger i DENS flash. At rydde P4'erens NVS rydder
    # altsaa ikke radioen der. Se docs/zdisplay.md.
    esp_phy
    esp_netif
    esp_event
    nvs_flash
    lwip
    esp_timer
    driver
    esp_http_client
    esp-tls
    json
    mqtt
    mbedtls
    esp_app_format
    app_update
    esp_https_ota
    esp_hw_support)
