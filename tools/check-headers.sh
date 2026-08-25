#!/usr/bin/env bash
#
# zScreen - tjek at ingen konstant kolliderer med en include-guard.
#
# Baggrund: konstanten for statuslinjens hoejde hed engang
# ZS_STATUSBAR_H, praecis som include-guarden i zs_statusbar.h. Fordi
# zs_config.h blev laest foerst, troede praeprocessoren at headeren
# allerede var med og sprang HELE filen over. Ingen advarsel. Fejlen
# dukkede op et helt andet sted som "unknown type name", og det tog en
# halv time at finde tilbage til.
#
# Derfor to regler, som dette script haandhaever:
#   1. Ingen anden #define maa hedde det samme som en include-guard.
#   2. Ingen konstant maa slutte paa _H. Det er forbeholdt guards.
#
# Koeres af tests/host/run.sh, saa den ikke kan glemmes.

set -euo pipefail
cd "$(dirname "$0")/.."

SRC="firmware/main"
FAIL=0

# Alle include-guards, altsaa det navn der staar lige efter #ifndef paa
# den foerste linje i hver header.
GUARDS=$(grep -h "^#ifndef ZS_" "${SRC}"/*.h "${SRC}"/*/*.h "${SRC}"/*/*/*.h 2>/dev/null \
         | awk '{print $2}' | sort -u)

# Alle andre #define'r. Vi springer assets over: de er genererede, og
# lv_font_conv laver sine egne navne.
DEFINES=$(grep -rhn "^#define ZS_" "${SRC}" --include="*.h" --include="*.c" 2>/dev/null \
          | grep -v "/assets/" \
          | awk '{print $2}' | sed 's/(.*//' | sort -u)

echo "Tjekker headere ..."

# Regel 1: en guard maa kun defineres i sin egen header.
for g in ${GUARDS}; do
    hits=$(grep -rln "^#define ${g}\b" "${SRC}" --include="*.h" --include="*.c" 2>/dev/null \
           | grep -v "/assets/" | wc -l | tr -d ' ')
    if [ "${hits}" -gt 1 ]; then
        echo "  FEJL: ${g} defineres ${hits} steder:"
        grep -rln "^#define ${g}\b" "${SRC}" --include="*.h" --include="*.c" | sed 's/^/         /'
        FAIL=1
    fi
done

# Regel 2: ingen konstant slutter paa _H, medmindre den ER en guard.
for d in ${DEFINES}; do
    case "${d}" in
        *_H)
            if ! echo "${GUARDS}" | grep -qx "${d}"; then
                echo "  FEJL: ${d} slutter paa _H uden at vaere en include-guard."
                echo "        Doeb den om, fx til ${d}EIGHT, ellers kan den"
                echo "        komme til at slaa en header fra."
                FAIL=1
            fi
            ;;
    esac
done

# Regel 3: hver header skal have en guard. Uden én kan den blive laest
# to gange og give dobbeltdefinitioner.
for f in $(find "${SRC}" -name "*.h" | grep -v "/assets/"); do
    if ! head -40 "${f}" | grep -q "^#ifndef "; then
        echo "  FEJL: ${f} mangler en include-guard."
        FAIL=1
    fi
done

if [ "${FAIL}" -eq 0 ]; then
    NG=$(echo "${GUARDS}" | wc -w | tr -d ' ')
    ND=$(echo "${DEFINES}" | wc -w | tr -d ' ')
    echo "  ${NG} guards og ${ND} konstanter, ingen sammenstoed."
fi
exit "${FAIL}"
