#!/usr/bin/env bash
# Bygger zs-probe. Samme kildekode som firmwaren, oversat til denne maskine.
#
# ZS_SANITIZE=1 bygger en udgave MED adressesanitizer til ende til
# ende-testene, under navnet zs-probe-san.
#
# HVORFOR TO UDGAVER.
#
# Enhedstestene har altid koert med sanitizer, men ende til ende-testene
# koerte mod den almindelige binaer. Og det er netop dér de interessante
# fejl ville vaere: enhedstestene fodrer opdigtede rammer, mens ende til
# ende koerer rigtige sockets mod en rigtig simulator, altsaa den vej
# netvaerksdata faktisk tager.
#
# Den almindelige udgave bliver staaende, for den er vaerktoejet vi
# fejlsoeger med, og en sanitizer goer den langsom og stoejende.
set -euo pipefail
cd "$(dirname "$0")"
CC="${CC:-cc}"

UD="zs-probe"
EKSTRA=()
if [ "${ZS_SANITIZE:-0}" = "1" ]; then
    UD="zs-probe-san"
    # -O1 og ikke -O2: sanitizeren vil have rammer der kan laeses.
    EKSTRA=( -fsanitize=address,undefined -fno-sanitize-recover=all
             -fno-omit-frame-pointer -O1 )
else
    EKSTRA=( -O2 )
fi

"${CC}" -std=c11 -DZS_HOST_BUILD=1 -g "${EKSTRA[@]}" \
    -Wall -Wextra -Werror -Wshadow -Wpointer-arith -Wstrict-prototypes \
    -Wno-unused-parameter \
    -I../../firmware/kerne -I../../firmware/kerne/net -I../../firmware/kerne/app \
    -I../../firmware/kerne/util \
    zs_probe.c \
    ../../firmware/kerne/util/zs_tekst.c \
    ../../firmware/kerne/net/zs_modbus_tcp.c \
    ../../firmware/kerne/net/zs_sunspec.c \
    ../../firmware/kerne/net/zs_fronius.c \
    ../../firmware/kerne/net/zs_discovery.c \
    ../../firmware/kerne/net/zs_locate.c \
    ../../firmware/kerne/app/zs_format.c \
    ../../firmware/kerne/net/zs_maerker.c \
    ../../firmware/kerne/app/zs_status.c \
    -lm -o "${UD}"
echo "${UD} bygget"
