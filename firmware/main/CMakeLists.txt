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
        "util/zs_tekst.c"
        "util/zs_tid.c"

        "net/zs_modbus_tcp.c"
        "net/zs_sunspec.c"
        "net/zs_fronius.c"
        "net/zs_wifi.c"
        "net/zs_discovery.c"
        "net/zs_locate.c"
        "net/zs_price.c"
        "net/zs_price_now.c"
        "net/zs_ota.c"
        "net/zs_version.c"
        "net/zs_fleet.c"
        "net/zs_fleet_msg.c"

        "app/zs_format.c"
        "app/zs_app.c"
        "app/zs_selftest.c"
        "app/zs_demo.c"
        "net/zs_maerker.c"
        "net/zs_energi.c"
        "net/zs_kvarter.c"
        "net/zs_tilmeld.c"
        "net/zs_ladeplan.c"
        "net/zs_ladeordre.c"
        "app/zs_status.c"
        # Vagten der afgoer hvad der er galt. Ren C, ingen LVGL: samme
        # kode koerer i tests/host.
        "app/zs_fejl.c"

        "storage/zs_nvs.c"

        # Skaermen
        "ui/zs_display.c"
        # Lyden. Summeren sidder paa den ANDEN chip, se ui/zs_lyd.h.
        "ui/zs_lyd.c"
        "ui/zs_theme.c"
        "ui/zs_tilegrid.c"
        "ui/zs_ui.c"
        "ui/zs_screen_home.c"
        "ui/zs_screen_tilmeld.c"
        "ui/zs_screen_fejl.c"
        "ui/zs_qr.c"
        # QR til tilmeldingen. Udefra, MIT. Se
        # vendor/qrcodegen/HVORFOR.md for maalingerne.
        "../vendor/qrcodegen/qrcodegen.c"
        "ui/zs_screen_setup.c"
        "ui/zs_screen_settings.c"
        "ui/widgets/zs_tile.c"
        "ui/widgets/zs_flow.c"
        "ui/widgets/zs_price_page.c"
        "ui/widgets/zs_statusbar.c"
        "ui/widgets/zs_keyboard.c"
        "ui/widgets/zs_step.c"

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
        "ui/assets/zs_img_zmark_pos.c"
        "ui/assets/zs_img_wordmark.c"
        "ui/assets/zs_img_wordmark_pos.c"

    INCLUDE_DIRS
        "."
        "net"
        "app"
        "storage"
        "util"
        "ui"
        "ui/widgets"
        "../vendor/qrcodegen"

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
        esp_http_client
        esp-tls
        json
        mqtt
        mbedtls
        esp_app_format
        app_update
        esp_https_ota
        esp_hw_support
)
