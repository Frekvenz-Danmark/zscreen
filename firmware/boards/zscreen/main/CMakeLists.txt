# zScreen - det der kun er DEN HER skaerm.
#
# SenseCAP Indicator D1, ESP32-S3, 480 x 480.
#
# Motoren og fladen staar i ../../../kerne/ og listes i kilder.cmake,
# fordi ZDisplay bygger af PRAECIS den samme liste. Her staar kun de
# fire filer der er hardware paa denne skaerm.
#
# ET NYT BOARD ER FIRE FILER: main.c, og én for hver kontrakt i
# kerne/hal/ - lv_port.h, zs_baglys.h og zs_lyd.h.

include("${CMAKE_CURRENT_LIST_DIR}/../../../kerne/kilder.cmake")

idf_component_register(
    SRCS
        # ── Kun paa denne skaerm ───────────────────────────────────────
        # Opstartsraekkefoelgen er Seeeds egen, og LVGL-porten taler med
        # deres bsp_lcd. Begge ting er anderledes paa ZDisplay, hvor
        # panelet sidder paa MIPI-DSI.
        "main.c"
        "lv_port.c"
        # Baglyset: LEDC paa GPIO 45. Tallet gaelder kun D1.
        "zs_baglys_d1.c"
        # Lyden: summeren sidder paa RP2040'eren og kan kun naas med en
        # COBS-pakke over en seriel ledning. Se kerne/hal/zs_lyd.h.
        "zs_lyd.c"

        # ── Faelles med ZDisplay ───────────────────────────────────────
        ${ZS_MOTOR_SRCS}
        ${ZS_FLADE_SRCS}
        ${ZS_ASSET_SRCS}

    INCLUDE_DIRS
        "."
        ${ZS_KERNE_INCLUDE_DIRS}

    REQUIRES
        # Seeeds BSP til Indicator D1, og LVGL fra deres SDK.
        bsp
        lvgl
        ${ZS_KERNE_REQUIRES}
)

# Advarsler i vores egen kode er fejl. Se ZS_STRENGE_FLAG i
# kerne/kilder.cmake for hvilke fire, og for hvorfor en grep i
# byggeudskriften ikke er nok.
target_compile_options(${COMPONENT_LIB} PRIVATE ${ZS_STRENGE_FLAG})
