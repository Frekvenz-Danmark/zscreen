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
        "net/zs_wifi.c"
        "net/zs_discovery.c"

        "app/zs_format.c"
        "app/zs_app.c"
        "app/zs_demo.c"

        "storage/zs_nvs.c"

        # Skaermen
        "ui/zs_display.c"
        "ui/zs_theme.c"
        "ui/zs_ui.c"
        "ui/zs_screen_home.c"
        "ui/zs_screen_setup.c"
        "ui/zs_screen_settings.c"
        "ui/widgets/zs_tile.c"
        "ui/widgets/zs_flow.c"
        "ui/widgets/zs_statusbar.c"
        "ui/widgets/zs_keyboard.c"

        # Genererede skrifttyper, ikoner og logoer.
        # Bygges af tools/build-assets.sh.
        "ui/assets/zs_font_num_64.c"
        "ui/assets/zs_font_28.c"
        "ui/assets/zs_font_20.c"
        "ui/assets/zs_font_16.c"
        "ui/assets/zs_font_13.c"
        "ui/assets/zs_font_kb_24.c"
        "ui/assets/zs_icons_20.c"
        "ui/assets/zs_icons_28.c"
        "ui/assets/zs_img_zmark.c"
        "ui/assets/zs_img_wordmark.c"

    INCLUDE_DIRS
        "."
        "net"
        "app"
        "storage"
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
        esp_hw_support
)
