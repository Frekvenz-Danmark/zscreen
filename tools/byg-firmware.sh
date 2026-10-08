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
# Udfald:
#   0  bygget, eller sprunget over fordi vaerktoejskaeden mangler
#   1  vaerktoejskaeden er her, og bygningen FEJLEDE

set -uo pipefail
cd "$(dirname "$0")/.."

if [ ! -f "${HOME}/esp/esp-idf/export.sh" ] || \
   [ ! -d "${HOME}/.espressif/zscreen-python-shim" ]; then
    echo "  SPRINGET OVER: ESP-IDF er ikke sat op paa denne maskine."
    echo "  Saet den op med:  ./tools/setup-toolchain.sh"
    echo "  (ikke en fejl, men firmwaren er saa IKKE oversat her)"
    exit 0
fi

echo "  bygger firmwaren. Det tager et par minutter."
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
    echo "  firmwaren bygger"
    rm -f "${UD}"
    exit 0
fi

echo "  FIRMWAREN KUNNE IKKE BYGGES. Sidste linjer:"
tail -25 "${UD}" | sed 's/^/    /'
echo
echo "  hele udskriften: ${UD}"
exit 1
