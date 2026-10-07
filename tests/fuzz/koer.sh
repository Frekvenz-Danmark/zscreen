#!/usr/bin/env bash
#
# zScreen - kaster oedelagte rammer ind i parserne.
#
# HVORFOR.
#
# Modbus- og SunSpec-koden laeser data direkte fra en enhed vi ikke
# styrer. En inverter med en fejl i firmwaren, eller noget helt andet der
# svarer paa port 502, kan sende hvad som helst. Enhedstestene proever de
# tilfaelde VI har taenkt paa. Fuzzeren proever dem vi ikke har.
#
#   ./tests/fuzz/koer.sh            kort koersel, som i testpakken
#   ./tests/fuzz/koer.sh 500000     laengere
#   ./tests/fuzz/koer.sh 100000 42  med et bestemt froe
#
# EN GROEN FUZZER DER IKKE NAAR KODEN ER VAERRE END INGEN FUZZER.
# Foerste udgave af SunSpec-fuzzeren svarede med rent skrald og naaede
# aldrig ind i kaeden: nul af 200.000 runder gav et kort, og den var
# groen. Den bygger nu en GYLDIG enhed og oedelaegger den, og den siger
# FRA hvis den ikke naaede ind.
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNDER="${1:-20000}"
FROE="${2:-1}"
CC="${CC:-cc}"
UD="$(mktemp -d)"
trap 'rm -rf "$UD"' EXIT

FLAG=( -std=c11 -DZS_HOST_BUILD=1 -O1 -g -fno-omit-frame-pointer
       -fsanitize=address,undefined -fno-sanitize-recover=all
       -Wall -Wextra -Wno-unused-parameter
       -I firmware/main -I firmware/main/net -I firmware/main/app )

"${CC}" "${FLAG[@]}" tests/fuzz/fuzz_modbus.c \
    firmware/main/net/zs_modbus_tcp.c firmware/main/net/zs_sunspec.c \
    -lm -o "$UD/modbus"
"${CC}" "${FLAG[@]}" tests/fuzz/fuzz_sunspec.c \
    firmware/main/net/zs_sunspec.c -lm -o "$UD/sunspec"

echo "  Modbus-rammer:"
"$UD/modbus" "$RUNDER" "$FROE" 2>/dev/null
echo "  SunSpec-kaeden:"
"$UD/sunspec" "$RUNDER" "$FROE" 2>/dev/null
