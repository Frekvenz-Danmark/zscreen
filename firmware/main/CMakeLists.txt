# zScreen - vores egen kode. SenseCAP Indicator D1, ESP32-S3.
#
# Motoren og fladen staar i ../kerne.cmake, fordi ZDisplay bygger af
# PRAECIS den samme liste. Her staar kun det der er den HER skaerm:
# opstarten, LVGL-porten til RGB-panelet, baglyset paa GPIO 45 og
# summeren paa den anden chip.
#
# Filerne staar skrevet ud i stedet for at bruge en GLOB. En GLOB betyder
# at cmake ikke opdager en ny fil foer man rydder byggemappen, og det er
# en halv times fejlsoegning der kunne have vaeret undgaaet.

include("${CMAKE_CURRENT_LIST_DIR}/../kerne.cmake")

idf_component_register(
    SRCS
        # ── Kun paa denne skaerm ───────────────────────────────────────
        #
        # Opstartsraekkefoelgen er Seeeds egen, og LVGL-porten taler med
        # deres bsp_lcd. Begge ting er anderledes paa ZDisplay, hvor
        # panelet sidder paa MIPI-DSI.
        "main.c"
        "lv_port.c"
        "ui/zs_baglys_d1.c"
        # Lyden paa DENNE skaerm. Summeren sidder paa RP2040'eren og kan
        # kun naas med en COBS-pakke over en seriel ledning - se
        # ui/zs_lyd.h. ZDisplay har en rigtig hoejttaler og sin egen fil.
        "ui/zs_lyd.c"

        # ── Faelles med ZDisplay ───────────────────────────────────────
        ${ZS_MOTOR_SRCS}
        ${ZS_FLADE_SRCS}
        ${ZS_ASSET_SRCS}

    INCLUDE_DIRS
        ${ZS_KERNE_INCLUDE_DIRS}

    REQUIRES
        # Seeeds BSP til Indicator D1, og LVGL fra deres SDK.
        bsp
        lvgl
        ${ZS_KERNE_REQUIRES}
)
