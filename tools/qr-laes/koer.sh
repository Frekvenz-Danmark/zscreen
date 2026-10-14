#!/usr/bin/env bash
#
# zScreen - kan QR'en paa tilmeldingssiden FAKTISK laeses?
#
# HVORFOR DEN FINDES.
#
# test_qr.c proever matricen: at modulerne staar hvor de skal, at
# kanten er der, at skalaen regnes rigtigt. Men den proever ikke det
# eneste der betyder noget for kunden: at en telefon kan laese koden.
#
# Og det kunne vi ikke proeve, fordi vi ikke har en skaerm. Det her er
# vejen udenom: firmwarens EGEN kode laver matricen, den skrives som et
# billede, og macOS' egen Vision-ramme laeser den tilbage. Kommer der
# ikke praecis den samme kode ud, er QR'en ubrugelig - og saa er det
# fundet her i stedet for hos en kunde der staar og scanner.
#
# DEN PROEVER VED SKAERMENS EGEN SKALA, OG DEN LAESER TALLET UD AF
# KODEN. ZS_QR_SKAERM_MAAL_PX i zs_qr.h siger hvor mange pixels QR'en
# fylder paa skaermen, og zs_qr_skala regner det om til pixels per
# modul. Skalaen staar foerst i listen nedenfor og er den vigtigste.
#
# Den blev skrevet af HER foer, som et 5 i en liste. Da QR'en blev gjort
# stoerre, beviste proeven stadig 5 px per modul - en stoerrelse der ikke
# fandtes paa skaermen laengere. Nu kan de to ikke skride fra hinanden.
#
# HVAD DEN IKKE PROEVER: den rigtige skaerms kontrast, synsvinkel og
# LVGL-tegningen. Det kraever hardware, og det staar i
# docs/test-plan.md.
#
# KUN macOS. Vision er Apples egen, og der er ingen QR-laeser i
# vaertstestene - de skal kunne koere paa Linux i CI.
#
#   ./tools/qr-laes/koer.sh
#   ./tools/qr-laes/koer.sh ABCDEF         kun én kode
set -euo pipefail
cd "$(dirname "$0")/../.."
UD="$(mktemp -d)"
trap 'rm -rf "$UD"' EXIT

cc -std=c11 -O1 -Wall -Werror -DZS_HOST_BUILD=1 \
   -I firmware/kerne -I firmware/kerne/ui -I firmware/vendor/qrcodegen \
   tools/qr-laes/qr-bmp.c firmware/kerne/ui/zs_qr.c \
   firmware/vendor/qrcodegen/qrcodegen.c -o "$UD/qr-bmp"
swiftc -O tools/qr-laes/laes.swift -o "$UD/laes" 2>/dev/null

# Rigtige koder fra skaermens eget alfabet: Crockford base32 UDEN
# I, L, O og U. Saa der er ingen kode med dem her, og det skal proeven
# ogsaa afspejle.
KODER=( "${1:-}" )
if [ -z "${KODER[0]}" ]; then
    KODER=( ABCDEF 234567 ZZZZZZ 0123AB HJKMNP QRSTVW X9Y8Z7 )
fi
# Skaermens egen skala foerst, saa de andre. Tallet kommer fra
# zs_qr.h, saa listen foelger med naar QR'en bliver gjort stoerre.
MAAL_PX="$(sed -n 's/^#define ZS_QR_SKAERM_MAAL_PX *\([0-9]*\).*/\1/p' \
           "$(dirname "$0")/../../firmware/kerne/ui/zs_qr.h")"
if [ -z "$MAAL_PX" ]; then
    echo "  kunne ikke laese ZS_QR_SKAERM_MAAL_PX ud af zs_qr.h" >&2
    exit 1
fi
# 21 moduler for en kode paa seks tegn, plus 2 x 4 moduler luft.
EGEN=$(( MAAL_PX / 29 ))
SKALAER=("$EGEN")
for s in 1 2 3 4 5 6 7 8; do
    [ "$s" = "$EGEN" ] || SKALAER+=("$s")
done

FEJL=0
for kode in "${KODER[@]}"; do
    for s in "${SKALAER[@]}"; do
        "$UD/qr-bmp" "$kode" "$UD/q.bmp" "$s" >/dev/null
        laest="$("$UD/laes" "$UD/q.bmp" 2>&1 | sed -n 's/^laest: \([^ ]*\).*/\1/p' | head -1)"
        maerke=""
        [ "$s" = "$EGEN" ] && maerke="  <- skaermens egen skala ($MAAL_PX px)"
        if [ "$laest" = "$kode" ]; then
            printf "  \033[1;32m ok \033[0m %s ved %s px/modul%s\n" "$kode" "$s" "$maerke"
        else
            printf "  \033[1;31mFEJL\033[0m %s ved %s px/modul: laest %s%s\n" \
                   "$kode" "$s" "${laest:-INGENTING}" "$maerke"
            FEJL=1
        fi
    done
done
echo
if [ "$FEJL" != "0" ]; then
    echo -e "  \033[1;31mQR'en kunne ikke laeses i alle tilfaelde\033[0m"
    exit 1
fi
echo -e "  \033[1;32mQR'en kan laeses, ogsaa ved skaermens egen skala\033[0m"
