#!/usr/bin/env bash
#
# zScreen - koer enhedstestene paa denne maskine.
#
# Ingen cmake, ingen ESP-IDF, ingen hardware. Bare cc. Meningen er at
# testene skal vaere saa nemme at koere at der ikke er nogen undskyldning
# for at lade vaere.
#
#   ./tests/host/run.sh          normal
#   ./tests/host/run.sh -v       med log fra selve koden

set -euo pipefail
cd "$(dirname "$0")"

CC="${CC:-cc}"
OUT="build"
mkdir -p "${OUT}"

# ZS_HOST_BUILD kobler log-makroerne over paa fprintf i stedet for
# esp_log, saa den samme kildekode kan oversaettes her.
#
# Advarsler er slaaet haardt til og behandles som fejl. En implicit
# konvertering eller en ubrugt variabel i Modbus-afkodningen er ikke
# noget vi vil opdage ude hos en kunde.
CFLAGS=(
    -std=c11
    -DZS_HOST_BUILD=1
    -O1
    -g
    -fno-omit-frame-pointer
    -Wall -Wextra -Werror
    -Wshadow
    -Wpointer-arith
    -Wcast-align
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wno-unused-parameter
    # Modulerne finder hinanden med korte navne, praecis som naar
    # firmwaren bygges. Uden det skulle hver include skrives med sti,
    # og saa ville de to bygninger ikke se ens ud.
    -I../../firmware/kerne
    # hal/ er kontrakterne mellem motoren og boardet: zs_baglys.h,
    # zs_lyd.h og lv_port.h. test_lyd.c inkluderer zScreens egen
    # zs_lyd.c, og den beder om zs_lyd.h herfra.
    -I../../firmware/kerne/hal
    # test_layout_800.c saetter 800 x 1280 og henter derfor
    # skrifternes maal fra DET saet. zs_layout.h henter kun
    # headeren naar skaermen IKKE er referenceskaermen.
    -I../../firmware/kerne/ui/assets/800
    -I../../firmware/kerne/app
    -I../../firmware/kerne/net
    -I../../firmware/kerne/ui
    -I../../firmware/vendor/qrcodegen
    -I../../firmware/kerne/storage
    -I../../firmware/kerne/util
    # Hvor proevekludene ligger. Eksplicit, saa en test ikke skal gaette
    # paa hvilken mappe den bliver koert fra.
    -DZS_FIXTURES=\"../fixtures\"
)

# Sanitizers fanger laesning uden for bufferen og heltalsoverloeb.
# Det er praecis den slags en forvansket Modbus-pakke kan udloese.
if [ "${ZS_NO_SANITIZE:-0}" != "1" ]; then
    CFLAGS+=( -fsanitize=address,undefined -fno-sanitize-recover=all )
fi

# zs_fronius.c og zs_lyd.c linkes IKKE separat: test_fronius.c og
# test_lyd.c inkluderer dem for
# at naa de interne beregningsfunktioner. Linker vi den ogsaa her,
# faar vi dobbeltdefinerede symboler.
SRC=(
    main.c
    test_modbus.c
    test_sunspec.c
    test_format.c
    test_fronius.c
    test_version.c
    test_tilegrid.c
    test_demo.c
    test_fleet.c
    test_locate.c
    test_pris.c
    test_kvarter.c
    test_tid.c
    test_tilmeld.c
    test_qr.c
    test_ladeplan.c
    test_ladeordre.c
    test_status.c
    test_fejl.c
    test_lyd.c
    test_layout.c
    test_layout_800.c
    test_maerker.c
    test_tekst.c
    test_energi.c
    ../../firmware/kerne/util/zs_tekst.c
    ../../firmware/kerne/util/zs_tid.c
    ../../firmware/kerne/net/zs_tilmeld.c
    ../../firmware/kerne/net/zs_ladeplan.c
    ../../firmware/kerne/net/zs_ladeordre.c
    ../../firmware/kerne/app/zs_status.c
    ../../firmware/kerne/app/zs_fejl.c
    ../../firmware/kerne/ui/zs_qr.c
    ../../firmware/vendor/qrcodegen/qrcodegen.c
    ../../firmware/kerne/net/zs_modbus_tcp.c
    ../../firmware/kerne/net/zs_sunspec.c
    ../../firmware/kerne/net/zs_maerker.c
    ../../firmware/kerne/net/zs_energi.c
    ../../firmware/kerne/net/zs_kvarter.c
    ../../firmware/kerne/app/zs_format.c
    ../../firmware/kerne/net/zs_version.c
    ../../firmware/kerne/ui/zs_tilegrid.c
    ../../firmware/kerne/app/zs_demo.c
    ../../firmware/kerne/net/zs_price_now.c
    ../../firmware/kerne/net/zs_fleet_msg.c
    ../../firmware/kerne/net/zs_locate.c
    ../../firmware/kerne/net/zs_discovery.c
)

# Headerne tjekkes foerst. Et navnesammenstoed mellem en konstant og en
# include-guard giver ikke en advarsel, kun en mystisk fejl et helt
# andet sted, saa det skal fanges hver gang og ikke naar nogen husker det.
../../tools/check-headers.sh

echo "Oversaetter ..."
"${CC}" "${CFLAGS[@]}" "${SRC[@]}" -lm -o "${OUT}/run"
echo "Kører ..."

# ── EN PROEVE DER HAENGER SKAL FEJLE, IKKE VENTE ─────────────────────
#
# Her stod "exec ${OUT}/run". Alle proever ligger i ÉN binaer, saa
# haenger én af dem, haenger hele koerslen - uden en linje udskrift,
# fordi den sidste proeve aldrig bliver faerdig.
#
# MAALT 15. OKTOBER 2026. En mutation i zs_tilmeld.c der gjorde
# "n >= sizeof(s->sidst)" til "n > sizeof", altsaa et overloeb paa én
# byte, fik koerslen til at haenge i over 40 MINUTTER. To gange i
# traek. En ren koersel tager 5 sekunder.
#
# Og det er den værste slags: en koersel der haenger ser ud som en der
# ikke er faerdig endnu. Man venter, man venter laengere, og saa gaar
# man i gang med noget andet - i stedet for at se at der ER en fejl.
#
# "timeout" findes ikke paa en Mac, saa graensen staar her i haanden.
# ZS_PROEVE_GRAENSE kan saette den, hvis en langsom maskine har brug
# for mere.
GRAENSE="${ZS_PROEVE_GRAENSE:-60}"
"${OUT}/run" "$@" &
PID=$!
I=0
while kill -0 "${PID}" 2>/dev/null && [ "${I}" -lt "$(( GRAENSE * 10 ))" ]; do
    sleep 0.1
    I=$(( I + 1 ))
done
if kill -0 "${PID}" 2>/dev/null; then
    echo
    echo "  PROEVERNE HAENGER. Over ${GRAENSE} sekunder, og en ren"
    echo "  koersel tager under ti. Noget loeber i ring, eller venter"
    echo "  paa noget der ikke kommer."
    echo
    echo "  Udskriften ovenfor stopper ved den proeve der haenger."
    kill -9 "${PID}" 2>/dev/null
    # "|| true" fordi scriptet koerer med set -e: wait giver 137 efter
    # et SIGKILL, og uden den her slutter scriptet DER - med 137 i
    # stedet for den 1 vi vil sige. Begge er en fejl, men 1 er den
    # der kan laeses.
    wait "${PID}" 2>/dev/null || true
    exit 1
fi
wait "${PID}"
exit $?
