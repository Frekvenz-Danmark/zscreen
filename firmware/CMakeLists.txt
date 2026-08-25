# zScreen - firmware til Seeed SenseCAP Indicator D1
#
# Seeed leverer selv BSP, LVGL 8.3.1 og driverne til skaerm og touch.
# Vi bruger deres komponenter som de er, via et pinnet submodul, i
# stedet for at kopiere dem ind i vores eget repo. Saa kan vi hente
# rettelser fra dem med én kommando, og det er tydeligt hvad der er
# vores kode og hvad der er deres.
#
# ESP-IDF skal vaere v5.1.x. Se tools/setup-toolchain.sh for hvorfor.

cmake_minimum_required(VERSION 3.16)

set(SEEED_SDK "${CMAKE_CURRENT_LIST_DIR}/vendor/SenseCAP_Indicator_ESP32")

if(NOT EXISTS "${SEEED_SDK}/components/bsp/CMakeLists.txt")
    message(FATAL_ERROR
        "Seeed-SDK'et mangler i ${SEEED_SDK}.\n"
        "Hent det med:  git submodule update --init --depth 1")
endif()

set(EXTRA_COMPONENT_DIRS "${SEEED_SDK}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(zscreen)

# ---------------------------------------------------------------------
# Seeeds komponenter og ESP-IDF v5.1.7
# ---------------------------------------------------------------------
#
# ESP-IDF slaar "-Werror=all" til for ALT der bygges, ogsaa kode vi ikke
# har skrevet. Seeeds SDK er skrevet til en aeldre ESP-IDF hvor
# esp_log's tidsstempel var en int. I v5.1 er det en uint32_t, og saa
# brokker oversaetteren sig over hver eneste ESP_LOGx med "%d", fx i
# iot_button/button_adc.c.
#
# Det er ikke kun logningen. Deres gt1151-driver har ogsaa en rigtig
# fejl hvor x og y kan bruges uinitialiserede. Vi bruger ikke den driver
# paa D1, men den bliver oversat alligevel.
#
# Vi kunne rette i deres kode, men saa faar vi konflikter hver gang vi
# henter en opdatering fra dem, og vi ville jagte den naeste advarsel
# hver gang enten de eller Espressif aendrer noget. I stedet slaar vi
# -Werror FRA for deres komponenter. Advarslerne bliver stadig skrevet
# ud, saa vi kan se dem, de stopper bare ikke vores byg.
#
# Vores egen kode i main/ roeres ikke. Der er "-Werror=all" stadig slaaet
# til, og det skal den blive ved med at vaere: en advarsel i Modbus-
# afkodningen er ikke noget vi vil opdage ude hos en kunde.

# Hvorfor listen er saa lang og ikke bare "-Wno-error":
#
# GCC laver "-Werror=all" om til en fejl-indstilling pr. advarsel. Hverken
# "-Wno-error" eller "-Wno-error=all" ruller det tilbage bagefter, det er
# proevet af. Kun det praecise navn virker, altsaa "-Wno-error=format".
# Derfor staar de skrevet ud.
#
# Hvert navn er afproevet mod den compiler vi bruger
# (xtensa-esp32s3-elf-gcc 12.2.0). Et navn compileren ikke kender giver
# en hard fejl, ikke en advarsel, saa listen maa ikke gaettes.
# "enum-int-mismatch" er fx IKKE med, fordi den foerst findes fra GCC 13.
set(ZS_VENDOR_RELAX
    -Wno-error=format               -Wno-error=format-extra-args
    -Wno-error=format-truncation    -Wno-error=format-overflow
    -Wno-error=format-zero-length   -Wno-error=maybe-uninitialized
    -Wno-error=uninitialized        -Wno-error=incompatible-pointer-types
    -Wno-error=int-conversion       -Wno-error=implicit-function-declaration
    -Wno-error=implicit-int         -Wno-error=pointer-sign
    -Wno-error=discarded-qualifiers -Wno-error=return-type
    -Wno-error=misleading-indentation -Wno-error=char-subscripts
    -Wno-error=sizeof-pointer-memaccess -Wno-error=address
    -Wno-error=strict-aliasing      -Wno-error=array-bounds
    -Wno-error=stringop-truncation  -Wno-error=stringop-overflow
    -Wno-error=type-limits          -Wno-error=clobbered
    -Wno-error=old-style-declaration -Wno-error=override-init
    -Wno-error=ignored-qualifiers   -Wno-error=missing-field-initializers
    -Wno-error=empty-body           -Wno-error=shift-negative-value
    -Wno-error=unused-value         -Wno-error=parentheses
    -Wno-error=switch               -Wno-error=sequence-point
    -Wno-error=restrict             -Wno-error=nonnull
    -Wno-error=nonnull-compare      -Wno-error=memset-elt-size
    -Wno-error=logical-not-parentheses -Wno-error=bool-operation
    -Wno-error=tautological-compare -Wno-error=enum-compare
    -Wno-error=unused-result        -Wno-error=unused-but-set-variable
    -Wno-error=dangling-pointer     -Wno-error=use-after-free
    -Wno-error=infinite-recursion)

file(GLOB ZS_VENDOR_DIRS RELATIVE "${SEEED_SDK}/components" "${SEEED_SDK}/components/*")
idf_build_get_property(ZS_BUILT_COMPONENTS BUILD_COMPONENTS)

foreach(comp IN LISTS ZS_VENDOR_DIRS)
    if(IS_DIRECTORY "${SEEED_SDK}/components/${comp}")
        if("${comp}" IN_LIST ZS_BUILT_COMPONENTS)
            idf_component_get_property(comp_lib ${comp} COMPONENT_LIB)
            if(TARGET ${comp_lib})
                target_compile_options(${comp_lib} PRIVATE ${ZS_VENDOR_RELAX})
            endif()
        endif()
    endif()
endforeach()
