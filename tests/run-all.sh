#!/usr/bin/env bash
#
# zScreen - koer alt der kan koeres uden hardware.
#
#   1. Headere        navnesammenstoed mellem konstanter og guards
#   2. Tegnsaet       skriver vi tegn skrifttyperne ikke har
#   3. Enhedstest     Modbus, SunSpec, beregning, tal paa dansk
#   4. Hele vejen     firmwarens egen kode mod en simuleret Fronius
#
# Tager under et minut. Der er ingen undskyldning for ikke at koere den.

set -uo pipefail
cd "$(dirname "$0")/.."

FEJL=0
koer() {
    echo
    echo "════════════════════════════════════════════════════════════"
    echo " $1"
    echo "════════════════════════════════════════════════════════════"
    shift
    if "$@"; then
        return 0
    fi
    FEJL=1
    return 1
}

koer "Headere"        ./tools/check-headers.sh
koer "Tegnsæt"        python3 tools/check-text.py
koer "Farver"         python3 tools/check-colors.py
koer "LVGL-låsen"     python3 tools/check-lvgl-laas.py
koer "Ingen blindgyde" python3 tools/check-flow.py
koer "Udgivelsen"     python3 tools/check-udgivelse.py
koer "Enhedstest"     ./tests/host/run.sh
# Bygget gaar gennem koer som alt andet.
#
# Foer stod der "build.sh >/dev/null 2>&1 || true". Baade udskriften og
# fejlen blev smidt vaek, saa holdt vaerktoejet op med at kunne bygge,
# sagde testkoerslen ingenting og koerte videre mod en GAMMEL binaer. Et
# byg der ikke virker skal se ud som en fejl, ikke som ingenting.
#
# ZS_SANITIZE=1: ende til ende proeves med adressesanitizer. Det er dér
# de interessante fejl ville vaere, for her koerer rigtige sockets mod en
# rigtig simulator og ikke opdigtede rammer.
koer "Byg værktøjet"  ./tools/zs-probe/build.sh
koer "Byg med sanitizer" env ZS_SANITIZE=1 ./tools/zs-probe/build.sh
koer "Hele datavejen" python3 tests/e2e/run.py

echo
echo "════════════════════════════════════════════════════════════"
if [ "${FEJL}" -eq 0 ]; then
    echo -e " \033[1;32mAlt bestået\033[0m"
else
    echo -e " \033[1;31mNoget fejlede, se ovenfor\033[0m"
fi
echo "════════════════════════════════════════════════════════════"
exit "${FEJL}"
