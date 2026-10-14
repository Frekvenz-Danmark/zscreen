# ZDisplay - firmware til Seeed reTerminal D1001, ESP32-P4.
#
# Samme motor som zScreen. Se ../../kerne/kilder.cmake og
# docs/zdisplay.md.
#
# ESP-IDF skal vaere v5.4.2. ESP32-P4 kraever mindst v5.3, og 5.4.2 er
# den Seeed selv bygger og proever deres SDK med.
#
#     export PATH=/usr/bin:$PATH
#     source ~/esp/esp-idf-5.4.2/export.sh
#     idf.py set-target esp32p4
#     idf.py build
#
# zScreen koerer stadig paa 5.1.7 fra ~/esp/esp-idf. De to kan staa ved
# siden af hinanden; det er hvilken export.sh man har hentet i den
# terminal der afgoer hvilken der bruges.

cmake_minimum_required(VERSION 3.16)

get_filename_component(ZS_FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(D1001_SDK   "${ZS_FW_DIR}/vendor/reTerminal_D1001")
set(SEEED_SDK   "${ZS_FW_DIR}/vendor/SenseCAP_Indicator_ESP32")

if(NOT EXISTS "${D1001_SDK}/components/esp_lcd_jd9365_8/CMakeLists.txt")
    message(FATAL_ERROR
        "Seeeds D1001-SDK mangler i ${D1001_SDK}.\n"
        "Hent det med:  git submodule update --init --depth 1")
endif()
if(NOT EXISTS "${SEEED_SDK}/components/lvgl/CMakeLists.txt")
    message(FATAL_ERROR
        "LVGL mangler i ${SEEED_SDK}.\n"
        "Hent det med:  git submodule update --init --depth 1")
endif()

# ---------------------------------------------------------------------
# Komponenterne
# ---------------------------------------------------------------------
#
# HVORFOR DE TRE DRIVERE STAAR ÉN FOR ÉN, og ikke bare hele
# components-mappen som i zScreen-projektet:
#
# ESP-IDF bygger ALT hvad den finder i en component-mappe, ogsaa det
# ingen bruger. Seeeds D1001-SDK indeholder esp-brookesia (en hel
# telefonlignende brugerflade), kamera, video, H.264 og taleopfattelse.
# Pegede vi paa hele mappen, kom det med i en energiskaerm.
#
# LVGL kommer fra Indicator-SDK'et, altsaa SAMME LVGL 8.3.1 som zScreen.
# Det er med vilje: fladen er 33.000 linjer skrevet til 8.3, og to
# forskellige LVGL-udgaver i to produkter ville vaere to flader.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components"
    "${D1001_SDK}/components/esp_lcd_jd9365_8"
    "${D1001_SDK}/components/esp_lcd_touch_gsl3670"
    "${D1001_SDK}/components/esp_io_expander_pca9535"
    "${SEEED_SDK}/components/lvgl")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ---------------------------------------------------------------------
# Skaermens maal, sat fra byggeriet
# ---------------------------------------------------------------------
#
# ui/zs_layout.h regner HELE fladen ud fra de her to tal, og kraever at
# de kommer herfra og ikke fra en header - ellers ville den foerste fil
# der glemte at hente headeren stille og roligt faa 480 igen. Se
# forklaringen i zs_layout.h.
#
# Panelet er 800 x 1280 i hardwaren, altsaa hoejkant. Se docs/zdisplay.md
# for hvorfor det ikke bare kan drejes.
idf_build_set_property(COMPILE_DEFINITIONS "ZS_G_SCR_WIDTH=800" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "ZS_G_SCR_HEIGHT=1280" APPEND)

# Hvilket board det er. zs_config.h har et afsnit pr. board.
idf_build_set_property(COMPILE_DEFINITIONS "ZS_BOARD_D1001=1" APPEND)

# ---------------------------------------------------------------------
# Seeeds BSP skal bygges UDEN LVGL
# ---------------------------------------------------------------------
#
# Deres bsp_display_start() returnerer en lv_display_t*, som kun findes
# i LVGL 9. Hele den del af deres kilde ligger bag
# #if (BSP_CONFIG_NO_GRAPHIC_LIB == 0), netop saa den kan slaas fra, og
# display.h og touch.h findes til dem der ikke vil bruge LVGL.
#
# Med den her linje faar vi deres opstart af panel, touch, stroem og
# benudvider - og vores egen LVGL 8.3-port ovenpaa. Se main/lv_port.c.
idf_build_set_property(COMPILE_DEFINITIONS "BSP_CONFIG_NO_GRAPHIC_LIB=1" APPEND)

# Versionsnummeret kommer fra firmware/version.txt, som BEGGE produkter
# deler.
#
# HER STOD DER FOER BARE "project(zdisplay)". Saa faldt ESP-IDF tilbage
# paa "git describe", fordi der ikke laa en version.txt i projektmappen,
# og appen hed "v0.27.0-dirty". Det er IKKE et versionsnummer
# zs_version_parse kan laese, og en skaerm der ikke forstaar sin egen
# version opdaterer aldrig. Se ZS_VERSION i kerne/kilder.cmake.
include("${ZS_FW_DIR}/kerne/kilder.cmake")
project(zdisplay VERSION ${ZS_VERSION})
