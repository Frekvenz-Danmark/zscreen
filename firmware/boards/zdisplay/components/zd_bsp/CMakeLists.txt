# Seeeds egen BSP til reTerminal D1001, bygget med VORES
# afhaengighedsliste.
#
# HVORFOR DEN IKKE BARE BLIVER BRUGT SOM KOMPONENT.
#
# Deres komponent ligger i submodulet og kunne registreres direkte. Men
# deres idf_component.yml beder om esp_lvgl_port ^2, som kraever LVGL 9,
# og om esp-sr, esp_cam_sensor og esp_codec_dev. Vores flade er LVGL
# 8.3, og en energiskaerm har ikke brug for kamera eller taleopfattelse.
#
# Vi retter IKKE i deres fil. Et submodul kan opdateres med én kommando,
# og det skal det blive ved med at kunne. I stedet registrerer vi deres
# KILDER - uaendrede, fra submodulet - og skriver selv listen over hvad
# de skal have. Saa er vi med paa deres rettelser, og de trækker ikke
# halvdelen af deres SDK med ind.
#
# Hele LVGL-delen af deres kilde ligger bag
# #if (BSP_CONFIG_NO_GRAPHIC_LIB == 0), som projektets CMakeLists
# slaar fra. Tilbage staar panel, touch, stroem, benudvider, batteri,
# ur og IMU - alt det der er hardware og ikke tegning.
#
# NAAR SUBMODULET OPDATERES: tilfoejer Seeed en kildefil, kommer den
# ikke med af sig selv. tools/check-bsp-zdisplay.py sammenligner den
# her liste med deres egen CMakeLists og siger fra.

get_filename_component(D1001_BSP
    "${CMAKE_CURRENT_LIST_DIR}/../../../../vendor/reTerminal_D1001/components/esp32_p4_re_terminal_d1001"
    ABSOLUTE)

if(NOT EXISTS "${D1001_BSP}/src/esp32_p4_re_terminal_d1001.c")
    message(FATAL_ERROR
        "Seeeds BSP mangler i ${D1001_BSP}.\n"
        "Hent det med:  git submodule update --init --depth 1")
endif()

idf_component_register(
    SRCS
        "${D1001_BSP}/src/esp32_p4_re_terminal_d1001.c"
        "${D1001_BSP}/src/bsp_lsm6ds3.c"
        "${D1001_BSP}/src/bsp_pcf8563.c"
        # 220 KB genereret registerbibliotek til IMU'en. Den bliver
        # linket fordi bsp_power_init() kalder
        # bsp_lsm6ds3_config_default(). Det ubrugte bliver smidt vaek af
        # linkeren, saa det koster ikke flash.
        "${D1001_BSP}/src/lsm6ds3tr-c_reg.c"

    INCLUDE_DIRS
        "${D1001_BSP}/include"

    PRIV_INCLUDE_DIRS
        "${D1001_BSP}/priv_include"

    # Offentlige: deres header udstiller typer fra de her komponenter,
    # saa alle der henter esp32_p4_re_terminal_d1001.h skal kunne se dem.
    REQUIRES
        driver
        esp_lcd
        esp_lcd_touch
        esp_codec_dev
        esp_lcd_jd9365_8
        esp_lcd_touch_gsl3670
        esp_io_expander_pca9535

    PRIV_REQUIRES
        esp_adc
        esp_timer
        esp_mm
        esp_driver_i2s
        esp_driver_sdmmc
        esp_pm
        usb
        spiffs
        fatfs
        button
)

# ---------------------------------------------------------------------
# esp_timer.h tvinges ind
# ---------------------------------------------------------------------
#
# Deres esp32_p4_re_terminal_d1001.c kalder esp_timer_get_time() i
# batteristyringen uden at hente esp_timer.h. Det bygger alligevel hos
# dem, fordi esp_lvgl_port's headere trækker den med - og den bruger vi
# ikke. Hos os er det derfor en fejl:
#
#   esp32_p4_re_terminal_d1001.c:1057: error: implicit declaration of
#   function 'esp_timer_get_time'
#
# Vi retter IKKE i deres fil; en rettelse i et submodul er væk næste
# gang det opdateres. "-include" lægger headeren ind foerst i
# oversaettelsen, praecis som hvis linjen havde staaet der. Det gaelder
# kun den her komponent.
#
# Kan fjernes den dag Seeed selv tilfoejer #include "esp_timer.h".
target_compile_options(${COMPONENT_LIB} PRIVATE "-include" "esp_timer.h")
