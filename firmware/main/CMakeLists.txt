# zScreen - vores egen kode.
#
# Filerne staar skrevet ud i stedet for at bruge en GLOB. En GLOB betyder
# at cmake ikke opdager en ny fil foer man rydder byggemappen, og det er
# en halv times fejlsoegning der kunne have vaeret undgaaet.

idf_component_register(
    SRCS
        "main.c"
        "lv_port.c"

        # Modbus, SunSpec og udregningen af de fire tal. Samme kode
        # koerer i tests/host paa en Mac, uden ESP-IDF.
        "net/zs_modbus_tcp.c"
        "net/zs_sunspec.c"
        "net/zs_fronius.c"

        "app/zs_format.c"

        # Skaermen
        "ui/zs_display.c"
        "ui/zs_theme.c"
        "ui/zs_screen_home.c"
        "ui/widgets/zs_tile.c"
        "ui/widgets/zs_statusbar.c"

        # Genererede skrifttyper, ikoner og logoer.
        # Bygges af tools/build-assets.sh.
        "ui/assets/zs_font_num_64.c"
        "ui/assets/zs_font_28.c"
        "ui/assets/zs_font_20.c"
        "ui/assets/zs_font_16.c"
        "ui/assets/zs_font_13.c"
        "ui/assets/zs_icons_20.c"
        "ui/assets/zs_icons_28.c"
        "ui/assets/zs_img_zmark.c"
        "ui/assets/zs_img_wordmark.c"

    INCLUDE_DIRS
        "."
        "net"
        "app"
        "ui"
        "ui/widgets"

    REQUIRES
        bsp
        lvgl
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        lwip
        esp_timer
        driver
)
