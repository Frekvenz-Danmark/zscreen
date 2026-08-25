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
)

# Sanitizers fanger laesning uden for bufferen og heltalsoverloeb.
# Det er praecis den slags en forvansket Modbus-pakke kan udloese.
if [ "${ZS_NO_SANITIZE:-0}" != "1" ]; then
    CFLAGS+=( -fsanitize=address,undefined -fno-sanitize-recover=all )
fi

# zs_fronius.c linkes IKKE separat: test_fronius.c inkluderer den for
# at naa de interne beregningsfunktioner. Linker vi den ogsaa her,
# faar vi dobbeltdefinerede symboler.
SRC=(
    main.c
    test_modbus.c
    test_sunspec.c
    test_format.c
    test_fronius.c
    ../../firmware/main/net/zs_modbus_tcp.c
    ../../firmware/main/net/zs_sunspec.c
    ../../firmware/main/app/zs_format.c
)

# Headerne tjekkes foerst. Et navnesammenstoed mellem en konstant og en
# include-guard giver ikke en advarsel, kun en mystisk fejl et helt
# andet sted, saa det skal fanges hver gang og ikke naar nogen husker det.
../../tools/check-headers.sh

echo "Oversaetter ..."
"${CC}" "${CFLAGS[@]}" "${SRC[@]}" -lm -o "${OUT}/run"
echo "Kører ..."
exec "${OUT}/run" "$@"
