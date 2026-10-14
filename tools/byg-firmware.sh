#!/usr/bin/env bash
#
# zScreen - bygger firmwaren til skaermen, hvis vaerktoejskaeden er her.
#
# HVORFOR DEN FINDES.
#
# tests/run-all.sh byggede ALT undtagen det der skal ud paa skaermen. Den
# koerte 1034 proever paa maskinen, hele datavejen mod en simuleret
# inverter, fuzzerne og otte vagter, og sagde "Alt bestaaet" uden
# nogensinde at have oversat koden til en ESP32.
#
# Det er ikke teoretisk. En header der mangler en include, en konstant der
# kun findes paa host, en _Static_assert der falder paa en anden
# arkitektur: alt det ser man foerst her. CI gjorde det i et andet job, saa
# man fik det at vide efter et push, og ikke foer.
#
# Vaerktoejskaeden er ikke paa enhver maskine, og det skal ikke vaere en
# fejl. Mangler den, siger vi det og lader det vaere. Er den der, bygger vi.
#
# TO SKAERME, OG TO ESP-IDF'er. zScreen bygges med 5.1.7 i ~/esp/esp-idf,
# ZDisplay med 5.4.2 i ~/esp/esp-idf-5.4.2, fordi ESP32-P4 ikke findes i
# 5.1. Begge bygges her, hver med sin.
#
# OG DER TJEKKES FOR ADVARSLER I VORES EGEN KODE.
#
# Det gjorde CI, og det gjorde den her ikke. Saa 14. oktober 2026 gik et
# byg fint igennem lokalt, blev skubbet, og faldt i CI paa én advarsel:
# esp_lcd_touch_get_coordinates er udfaset. Advarslen laa i tusind linjer
# byggeudskrift hvor den var nem at se forbi, og den eneste der kiggede
# efter den, var en maskine man foerst hoerer fra efter et push.
#
# Udfald:
#   0  bygget, eller sprunget over fordi vaerktoejskaeden mangler
#   1  vaerktoejskaeden er her, og bygningen FEJLEDE - eller der er en
#      advarsel i vores egen kode

set -uo pipefail
cd "$(dirname "$0")/.."

# Advarsler i VORES egen kode, altsaa ikke i Seeeds BSP eller i LVGL.
# Samme moenster som .github/workflows/test.yml bruger.
VORES_KODE="(firmware/main|boards/zdisplay/main)/.*(warning|error):"

tjek_advarsler() {
    # $1 = byggeudskriften, $2 = navnet paa skaermen
    if grep -E "${VORES_KODE}" "$1"; then
        echo "  DER ER ADVARSLER I VORES EGEN KODE ($2)."
        echo "  De er ikke kosmetik: en udfaset funktion forsvinder i"
        echo "  naeste udgave, og en uinitialiseret variabel er en fejl"
        echo "  der endnu ikke er sket. CI faelder paa dem alligevel."
        return 1
    fi
    return 0
}

# ---------------------------------------------------------------------
# ZDisplay, ESP32-P4. Bygges foerst, fordi den er den nye.
# ---------------------------------------------------------------------
FEJL=0
if [ ! -f "${HOME}/esp/esp-idf-5.4.2/export.sh" ]; then
    echo "  SPRINGET OVER: ESP-IDF 5.4.2 er ikke sat op, saa ZDisplay er"
    echo "  IKKE oversat. Se docs/zdisplay.md. (ikke en fejl)"
else
    echo "  bygger ZDisplay. Det tager et par minutter."
    ZUD="$(mktemp)"
    # PATH=/usr/bin foerst: homebrews python vaelter IDF's export.sh.
    if ( cd firmware/boards/zdisplay \
         && export PATH="/usr/bin:${PATH}" \
         && . "${HOME}/esp/esp-idf-5.4.2/export.sh" >/dev/null 2>&1 \
         && idf.py build ) > "${ZUD}" 2>&1; then
        grep -E "zdisplay\.bin binary size" "${ZUD}" | tail -1 | sed 's/^/  /'
        if tjek_advarsler "${ZUD}" "ZDisplay"; then
            echo "  ZDisplay bygger"
            rm -f "${ZUD}"
        else
            FEJL=1
            echo "  hele udskriften: ${ZUD}"
        fi
    else
        echo "  ZDisplay KUNNE IKKE BYGGES. Sidste linjer:"
        tail -25 "${ZUD}" | sed 's/^/    /'
        echo "  hele udskriften: ${ZUD}"
        FEJL=1
    fi
fi

# ---------------------------------------------------------------------
# zScreen, ESP32-S3
# ---------------------------------------------------------------------
if [ ! -f "${HOME}/esp/esp-idf/export.sh" ] || \
   [ ! -d "${HOME}/.espressif/zscreen-python-shim" ]; then
    echo "  SPRINGET OVER: ESP-IDF er ikke sat op paa denne maskine."
    echo "  Saet den op med:  ./tools/setup-toolchain.sh"
    echo "  (ikke en fejl, men zScreen er saa IKKE oversat her)"
    exit "${FEJL}"
fi

echo "  bygger zScreen. Det tager et par minutter."
UD="$(mktemp)"
if ( cd firmware && . ../tools/env.sh >/dev/null 2>&1 && idf.py build ) \
        > "${UD}" 2>&1; then
    # APPENS stoerrelse, ikke bootloaderens.
    #
    # Foerst stod her en grep paa "binary size", og den fangede
    # bootloaderen fordi den kommer sidst. Saa stod der "30% free" om en
    # linje der intet havde med firmwaren at goere. Nu staar navnet paa
    # filen i moensteret, saa der kun kan komme ét svar.
    grep -E "zscreen\.bin binary size" "${UD}" | tail -1 | sed 's/^/  /'

    # Signaturen staar kun i udskriften naar binaeren blev lavet om. Ved et
    # inkrementelt byg sker der ingenting, og det er ikke en fejl. Vi
    # spoerger filen i stedet for udskriften.
    if [ -f firmware/build/zscreen.bin ]; then
        echo "  signeret binaer: $(wc -c < firmware/build/zscreen.bin | tr -d ' ') bytes"
    fi
    if tjek_advarsler "${UD}" "zScreen"; then
        echo "  zScreen bygger"
        rm -f "${UD}"
    else
        FEJL=1
        echo "  hele udskriften: ${UD}"
    fi
    exit "${FEJL}"
fi

echo "  zScreen KUNNE IKKE BYGGES. Sidste linjer:"
tail -25 "${UD}" | sed 's/^/    /'
echo
echo "  hele udskriften: ${UD}"
exit 1
