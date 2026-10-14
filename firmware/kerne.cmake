# Motoren: den kode der er FAELLES for begge skaerme.
#
# To produkter deler den her liste:
#
#   firmware/                     zScreen   - SenseCAP Indicator D1, ESP32-S3
#   firmware/boards/zdisplay/     ZDisplay  - reTerminal D1001, ESP32-P4
#
# HVORFOR EN .cmake-FIL OG IKKE TO LISTER.
#
# Begge produkter laeser Modbus fra samme inverter, regner de samme fire
# tal ud, melder ind paa samme flaade og opdaterer sig selv samme vej.
# Stod kildelisten to steder, ville en ny fil i net/ komme med i den ene
# skaerm og ikke i den anden - og det ville ikke vaere en byggefejl, men
# en manglende funktion hos en kunde paa det ene produkt.
#
# Derfor staar listen ÉT sted. Kommer der et nyt produkt, skriver det
# ikke sin egen liste; det tager den her og laegger sine egne tre
# board-filer oveni.
#
# STIERNE ER ABSOLUTTE. En relativ sti ville betyde noget forskelligt
# fra firmware/main/ og fra firmware/boards/zdisplay/main/, og den slags
# bliver opdaget en halv time senere end den burde.
#
# Filerne staar skrevet ud og ikke som GLOB. En GLOB betyder at cmake
# ikke opdager en ny fil foer man rydder byggemappen.

get_filename_component(ZS_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/main" ABSOLUTE)
get_filename_component(ZS_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/vendor" ABSOLUTE)

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
# Det er HAANDHAEVET i tools/check-byg.py, se FORBUDT_I_MOTOREN. Her
# stod der foer at check-headers.sh gjorde det, og det passede ikke:
# den tjekker kun at ingen konstant kolliderer med en include-guard.
# Paastanden hele opdelingen hviler paa var altsaa uhaandhaevet, og det
# blev fundet af en gennemgang og ikke af et byg.
set(ZS_MOTOR_SRCS
    "${ZS_MAIN_DIR}/util/zs_tekst.c"
    "${ZS_MAIN_DIR}/util/zs_tid.c"

    "${ZS_MAIN_DIR}/net/zs_modbus_tcp.c"
    "${ZS_MAIN_DIR}/net/zs_sunspec.c"
    "${ZS_MAIN_DIR}/net/zs_fronius.c"
    "${ZS_MAIN_DIR}/net/zs_wifi.c"
    "${ZS_MAIN_DIR}/net/zs_discovery.c"
    "${ZS_MAIN_DIR}/net/zs_locate.c"
    "${ZS_MAIN_DIR}/net/zs_price.c"
    "${ZS_MAIN_DIR}/net/zs_price_now.c"
    "${ZS_MAIN_DIR}/net/zs_ota.c"
    "${ZS_MAIN_DIR}/net/zs_version.c"
    "${ZS_MAIN_DIR}/net/zs_fleet.c"
    "${ZS_MAIN_DIR}/net/zs_fleet_msg.c"
    "${ZS_MAIN_DIR}/net/zs_maerker.c"
    "${ZS_MAIN_DIR}/net/zs_energi.c"
    "${ZS_MAIN_DIR}/net/zs_kvarter.c"
    "${ZS_MAIN_DIR}/net/zs_tilmeld.c"
    "${ZS_MAIN_DIR}/net/zs_ladeplan.c"
    "${ZS_MAIN_DIR}/net/zs_ladeordre.c"

    "${ZS_MAIN_DIR}/app/zs_format.c"
    "${ZS_MAIN_DIR}/app/zs_app.c"
    "${ZS_MAIN_DIR}/app/zs_selftest.c"
    "${ZS_MAIN_DIR}/app/zs_demo.c"
    "${ZS_MAIN_DIR}/app/zs_status.c"
    # Vagten der afgoer hvad der er galt. Ren C, ingen LVGL.
    "${ZS_MAIN_DIR}/app/zs_fejl.c"

    "${ZS_MAIN_DIR}/storage/zs_nvs.c")

# ---------------------------------------------------------------------
# ZS_FLADE_SRCS - brugerfladen, LVGL 8.3
# ---------------------------------------------------------------------
#
# Faelles, men ikke gratis faelles: alt heri regner ud fra
# ZS_G_SCR_WIDTH og ZS_G_SCR_HEIGHT i ui/zs_layout.h, som boardet
# saetter fra byggeriet. Derfor passer den samme flade paa 480 x 480 og
# paa 800 x 1280. Se zs_layout.h og tests/host/test_layout_800.c.
set(ZS_FLADE_SRCS
    "${ZS_MAIN_DIR}/ui/zs_display.c"
    "${ZS_MAIN_DIR}/ui/zs_theme.c"
    "${ZS_MAIN_DIR}/ui/zs_tilegrid.c"
    "${ZS_MAIN_DIR}/ui/zs_ui.c"
    "${ZS_MAIN_DIR}/ui/zs_screen_home.c"
    "${ZS_MAIN_DIR}/ui/zs_screen_tilmeld.c"
    "${ZS_MAIN_DIR}/ui/zs_screen_fejl.c"
    "${ZS_MAIN_DIR}/ui/zs_screen_setup.c"
    "${ZS_MAIN_DIR}/ui/zs_screen_settings.c"
    "${ZS_MAIN_DIR}/ui/zs_qr.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_tile.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_flow.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_price_page.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_statusbar.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_keyboard.c"
    "${ZS_MAIN_DIR}/ui/widgets/zs_step.c"
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
    "${ZS_MAIN_DIR}/ui/assets/zs_font_num_64.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_font_28.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_font_20.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_font_16.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_font_13.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_font_kb_24.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_icons_20.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_icons_28.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_img_zmark.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_img_zmark_pos.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_img_wordmark.c"
    "${ZS_MAIN_DIR}/ui/assets/zs_img_wordmark_pos.c")

# ---------------------------------------------------------------------
# ZS_KERNE_INCLUDE_DIRS
# ---------------------------------------------------------------------
set(ZS_KERNE_INCLUDE_DIRS
    "${ZS_MAIN_DIR}"
    "${ZS_MAIN_DIR}/net"
    "${ZS_MAIN_DIR}/app"
    "${ZS_MAIN_DIR}/storage"
    "${ZS_MAIN_DIR}/util"
    "${ZS_MAIN_DIR}/ui"
    "${ZS_MAIN_DIR}/ui/widgets"
    "${ZS_VENDOR_DIR}/qrcodegen")

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
