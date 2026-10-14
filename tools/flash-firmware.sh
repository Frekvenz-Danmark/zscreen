#!/usr/bin/env bash
#
# Flasher ÉN skaerm over USB.
#
#     ./tools/flash-firmware.sh zdisplay
#     ./tools/flash-firmware.sh zscreen /dev/cu.usbmodem31201
#
# HVORFOR FILEN FINDES.
#
# Der stod en byggeopskrift i README og ingen flash-opskrift til
# ZDisplay. Det er ikke en detalje, for der er fire ting man kan tage
# fejl af, og tre af dem fejler TAVST eller med en besked der peger det
# forkerte sted hen:
#
#   1. DEN FORKERTE ESP-IDF. zScreen er 5.1.7 og ZDisplay 5.4.2, og en
#      terminal husker den export.sh man sidst hentede. Flasher man
#      ZDisplay fra en 5.1.7-terminal, siger den at esp32p4 ikke
#      findes - og det lyder som om kortet er i stykker.
#   2. DEN FORKERTE PYTHON. Homebrews python vaelter IDF's export.sh.
#      PATH=/usr/bin skal staa foerst. Se byg-firmware.sh, samme note.
#   3. DEN FORKERTE PORT. Sidder der to kort, rammer man det andet. Vi
#      finder porten selv naar der kun er én, og siger fra naar der er
#      flere.
#   4. EN BINAER DER IKKE ER BYGGET. idf.py flash bygger selv, men saa
#      kan man staa og vente paa en bygning man tror er en flashning.
#      Vi bygger foerst, saa de to ting er adskilt.
#
# Scriptet flasher IKKE begge skaerme. To produkter paa den samme
# ledning er to chancer for at ramme den forkerte, og en fejlflashet
# skaerm skal have besoeg.

set -euo pipefail
cd "$(dirname "$0")/.."

BOARD="${1:-}"
PORT="${2:-}"

case "${BOARD}" in
zdisplay)
    IDF="${HOME}/esp/esp-idf-5.4.2/export.sh"
    NAVN="ZDisplay"
    ;;
zscreen)
    IDF="${HOME}/esp/esp-idf/export.sh"
    NAVN="zScreen"
    ;;
*)
    echo "Brug: $0 <zdisplay|zscreen> [port]"
    echo
    echo "  zdisplay   reTerminal D1001, ESP32-P4, ESP-IDF 5.4.2"
    echo "  zscreen    SenseCAP Indicator D1, ESP32-S3, ESP-IDF 5.1.7"
    echo
    echo "Porten findes af sig selv naar der kun sidder ét kort."
    exit 2
    ;;
esac

if [ ! -f "${IDF}" ]; then
    echo "FEJL: ${IDF} findes ikke."
    echo "      ${NAVN} kraever sin egen ESP-IDF - se README."
    exit 1
fi

# ── porten ──
#
# HER STOD "er der kun ÉN usbmodem, saa tag den".
#
# Det holdt saa laenge der kun sad én skaerm i. Med begge i samme Mac
# heder de usbmodem11201 og usbmodem31201, og der er ingenting i de
# navne der siger hvad der sidder for enden - saa scriptet sagde fra og
# lod mennesket vaelge. Et menneske der vaelger mellem to numre, vaelger
# nogle gange det forkerte, og en zScreen med ZDisplays firmware skal
# have besoeg.
#
# De to melder sig forskelligt over USB, og det kan laeses uden at
# nulstille noget. find-port.py gOEr det.
if [ -z "${PORT}" ]; then
    if ! PORT="$(./tools/find-port.py "${BOARD}")"; then
        echo
        echo "      Se hvad der sidder i:  ./tools/find-port.py --vis"
        echo "      Eller sig porten selv: $0 ${BOARD} <port>"
        exit 1
    fi
    echo "  fandt ${NAVN} paa ${PORT}"
fi
if [ ! -e "${PORT}" ]; then
    echo "FEJL: ${PORT} findes ikke."
    exit 1
fi

echo "Flasher ${NAVN} paa ${PORT}"

# ── byg foerst, saa de to ting er adskilt ──
echo "  bygger ..."
UD="$(mktemp)"
if ! ( cd "firmware/boards/${BOARD}" \
       && export PATH="/usr/bin:${PATH}" \
       && . "${IDF}" >/dev/null 2>&1 \
       && idf.py build ) > "${UD}" 2>&1; then
    echo "  KUNNE IKKE BYGGES. Sidste linjer:"
    tail -25 "${UD}" | sed 's/^/    /'
    echo "  hele udskriften: ${UD}"
    exit 1
fi
grep -E "${BOARD}\.bin binary size" "${UD}" | tail -1 | sed 's/^/  /'
rm -f "${UD}"

# ── og saa flashningen ──
echo "  skriver til flashen. Tag ikke ledningen ud."
if ! ( cd "firmware/boards/${BOARD}" \
       && export PATH="/usr/bin:${PATH}" \
       && . "${IDF}" >/dev/null 2>&1 \
       && idf.py -p "${PORT}" flash ) > "${UD:=$(mktemp)}" 2>&1; then
    echo "  FLASHNINGEN MISLYKKEDES. Sidste linjer:"
    tail -25 "${UD}" | sed 's/^/    /'
    exit 1
fi
rm -f "${UD}"

echo "  ${NAVN} er flashet."
echo
echo "Se den starte op:"
echo "  (export PATH=/usr/bin:\$PATH; . ${IDF}; \\"
echo "   cd firmware/boards/${BOARD}; idf.py -p ${PORT} monitor)"
