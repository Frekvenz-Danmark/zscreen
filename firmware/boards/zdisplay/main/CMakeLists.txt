# ZDisplay - vores egen kode. reTerminal D1001, ESP32-P4.
#
# Motoren og fladen staar i ../../../kerne/ og listes i kilder.cmake,
# fordi zScreen bygger af PRAECIS den samme liste. Her staar kun de fire
# filer der er hardware paa denne skaerm.
#
# ET NYT BOARD ER FIRE FILER: main.c, og én for hver kontrakt i
# kerne/hal/ - lv_port.h, zs_baglys.h og zs_lyd.h.
#
# Filerne staar skrevet ud og ikke som GLOB, af samme grund som i
# zScreen-projektet: en GLOB betyder at cmake ikke opdager en ny fil
# foer man rydder byggemappen.

include("${CMAKE_CURRENT_LIST_DIR}/../../../kerne/kilder.cmake")

idf_component_register(
    SRCS
        # ── Kun paa denne skaerm ───────────────────────────────────────
        "main.c"
        # LVGL 8.3 oven paa MIPI-DSI. Panelet skriver i en buffer i
        # PSRAM i stedet for at blive klokket ud pixel for pixel.
        "lv_port.c"
        "zs_baglys_d1001.c"
        # Hoejttaleren er ikke bygget endnu. Se filen.
        "zs_lyd_d1001.c"

        # ── Faelles med zScreen ────────────────────────────────────────
        ${ZS_MOTOR_SRCS}
        ${ZS_FLADE_SRCS}
        ${ZS_ASSET_SRCS}

    INCLUDE_DIRS
        "."
        ${ZS_KERNE_INCLUDE_DIRS}

    REQUIRES
        # Seeeds BSP, bygget uden LVGL. Se ../components/zd_bsp.
        zd_bsp
        # LVGL 8.3.1, samme komponent som zScreen bruger.
        lvgl
        ${ZS_KERNE_REQUIRES}
)

# Advarsler i vores egen kode er fejl. Se ZS_STRENGE_FLAG i kerne.cmake
# for hvilke fire, og for hvorfor en grep i byggeudskriften ikke er nok.
target_compile_options(${COMPONENT_LIB} PRIVATE ${ZS_STRENGE_FLAG})
