#!/usr/bin/env bash
#
# zScreen - koer alt der kan koeres uden hardware.
#
#   1. Headere        navnesammenstoed mellem konstanter og guards
#   1b. Bygningerne   kan alle tre bygninger finde de headere de beder om
#   2. Tegnsaet       skriver vi tegn skrifttyperne ikke har
#   3. Enhedstest     Modbus, SunSpec, beregning, tal paa dansk
#   4. Hele vejen     firmwarens egen kode mod en simuleret Fronius
#
# Tager under et minut. Der er ingen undskyldning for ikke at koere den.

set -uo pipefail
cd "$(dirname "$0")/.."

FEJL=0
# Springer et trin over, synligt. Et overspringet trin maa ikke kunne
# forveksles med et der bestod.
spring() {
    echo
    echo "════════════════════════════════════════════════════════════"
    echo " $1"
    echo "════════════════════════════════════════════════════════════"
    echo -e " \033[1;33mSPRUNGET OVER: $2\033[0m"
    FEJL=1
}

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

# Vagterne staar i tools/check-alle.sh og ikke her, saa CI og den her
# koersel bruger PRAECIS den samme liste.
koer "Vagterne"       ./tools/check-alle.sh
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
BYG_OK=1
koer "Byg værktøjet"  ./tools/zs-probe/build.sh || BYG_OK=0
koer "Byg med sanitizer" env ZS_SANITIZE=1 ./tools/zs-probe/build.sh || BYG_OK=0

# Faldt bygningen, SKAL de proever der bruger binaeren springes over.
#
# Foer koerte de videre, og de koerte mod den GAMLE binaer der stadig laa
# paa disken. De meldte "70 tjek, alle bestaaet". De loej, og man kunne
# ikke se det paa noget. Et resultat fra en binaer vi ikke byggede, er
# ingen oplysning, det er en falsk tryghed.
if [ "${BYG_OK}" -eq 1 ]; then
    koer "Hele datavejen" python3 tests/e2e/run.py
else
    spring "Hele datavejen" "værktøjet kunne ikke bygges"
fi

# Og til sidst det der skal ud paa skaermen.
#
# Her stod ingenting. Testkoerslen byggede ALT undtagen firmwaren, og sagde
# "Alt bestaaet" uden nogensinde at have oversat koden til en ESP32. En
# header der mangler en include, eller en _Static_assert der falder paa en
# anden arkitektur, saa man foerst efter et push. Den staar sidst, saa den
# hurtige besked kommer foerst.
koer "Firmwaren" ./tools/byg-firmware.sh

# Kort koersel, saa fuzzerne BLIVER ved at virke. En laengere kampagne
# koeres i haanden: ./tests/fuzz/koer.sh 500000
koer "Ødelagte rammer" ./tests/fuzz/koer.sh 20000

echo
echo "════════════════════════════════════════════════════════════"
if [ "${FEJL}" -eq 0 ]; then
    echo -e " \033[1;32mAlt bestået\033[0m"
else
    echo -e " \033[1;31mNoget fejlede, se ovenfor\033[0m"
fi
echo "════════════════════════════════════════════════════════════"
exit "${FEJL}"
