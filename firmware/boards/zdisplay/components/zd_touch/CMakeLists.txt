# Touch til ZDisplay: Seeeds driver med tre bufferfejl rettet.
#
# esp_lcd_touch_gsl3670.c her i mappen er VORES rettede kopi - se
# hovedet af filen for hvad der er aendret og hvorfor.
#
# gsl_point_id.c er deres, 79 KB algoritme til at holde styr paa hvilken
# finger der er hvilken. Den bygges UAENDRET fra submodulet, saa vi
# foelger deres rettelser i den del.
#
# Deres egen komponent maa derfor IKKE registreres samtidig - se
# EXTRA_COMPONENT_DIRS i projektets CMakeLists.txt. To komponenter der
# definerer de samme navne giver en linkfejl, og hvis den ene vinder, er
# det tilfaeldigt hvilken.

get_filename_component(D1001_TOUCH
    "${CMAKE_CURRENT_LIST_DIR}/../../../../vendor/reTerminal_D1001/components/esp_lcd_touch_gsl3670"
    ABSOLUTE)

if(NOT EXISTS "${D1001_TOUCH}/gsl_point_id.c")
    message(FATAL_ERROR
        "Seeeds touch-driver mangler i ${D1001_TOUCH}.\n"
        "Hent den med:  git submodule update --init --depth 1")
endif()

idf_component_register(
    SRCS
        "esp_lcd_touch_gsl3670.c"
        "${D1001_TOUCH}/gsl_point_id.c"
    INCLUDE_DIRS
        "${D1001_TOUCH}/include"
    REQUIRES
        esp_lcd
        esp_lcd_touch
        # Touch-resettet sidder paa benudvideren, ikke paa et chip-ben.
        # Deres egen komponent fik den her med fra sin manifest via
        # "path: ../esp_io_expander_pca9535"; vi beder om den selv.
        esp_io_expander_pca9535
)
