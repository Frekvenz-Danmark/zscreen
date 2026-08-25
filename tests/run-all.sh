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
koer "Enhedstest"     ./tests/host/run.sh
./tools/zs-probe/build.sh >/dev/null 2>&1 || true
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
