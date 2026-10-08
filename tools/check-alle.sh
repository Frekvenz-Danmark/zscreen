#!/usr/bin/env bash
#
# zScreen - koerer alle vagtskripterne.
#
# HVORFOR DEN FINDES.
#
# Listen over vagter stod to steder: i tests/run-all.sh og i
# .github/workflows/test.yml. To lister der skal vaere ens, bliver det
# ikke. Lagde man en ny vagt ind og glemte CI, koerte den aldrig dér, og
# saa var der en regel vi trode blev haandhaevet og som ikke blev det.
#
# Nu staar listen ÉN gang, her. Baade testkoerslen og CI kalder det her
# script, saa de kan ikke komme ud af trit.
#
# Hver vagt koeres, ogsaa hvis en tidligere fejlede: man vil vide ALT hvad
# der er galt i én koersel, ikke én ting ad gangen.

set -uo pipefail
cd "$(dirname "$0")/.."

# PY: samme Python hele vejen. "python3" kan vaere en anden udgave end den
# der ligger i /usr/bin, og to udgaver opfoerer sig ikke altid ens.
PY="${ZS_PYTHON:-python3}"

VAGTER=(
    "Headere|./tools/check-headers.sh"
    "Bygningerne|${PY} tools/check-byg.py"
    "Tegnsæt|${PY} tools/check-text.py"
    "Farver|${PY} tools/check-colors.py"
    "Målene i fladen|${PY} tools/check-maal.py"
    "LVGL-låsen|${PY} tools/check-lvgl-laas.py"
    "Ingen blindgyde|${PY} tools/check-flow.py"
    "Udgivelsen|${PY} tools/check-udgivelse.py"
    "Dokumentationen|${PY} tools/check-dokumentation.py"
)

FEJL=0
FEJLEDE=()

for v in "${VAGTER[@]}"; do
    navn="${v%%|*}"
    cmd="${v#*|}"
    echo
    echo "── ${navn} ──────────────────────────────────────────"
    if ! ${cmd}; then
        FEJL=1
        FEJLEDE+=("${navn}")
    fi
done

echo
if [ "${FEJL}" -eq 0 ]; then
    echo "Alle ${#VAGTER[@]} vagter er tilfredse"
else
    echo -e "\033[1;31mDisse vagter sagde fra: ${FEJLEDE[*]}\033[0m"
fi
exit "${FEJL}"
