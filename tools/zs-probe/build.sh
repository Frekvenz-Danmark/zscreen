#!/usr/bin/env bash
# Bygger zs-probe. Samme kildekode som firmwaren, oversat til denne maskine.
set -euo pipefail
cd "$(dirname "$0")"
CC="${CC:-cc}"
"${CC}" -std=c11 -DZS_HOST_BUILD=1 -O2 -g \
    -Wall -Wextra -Werror -Wshadow -Wpointer-arith -Wstrict-prototypes \
    -Wno-unused-parameter \
    -I../../firmware/main -I../../firmware/main/net -I../../firmware/main/app \
    zs_probe.c \
    ../../firmware/main/net/zs_modbus_tcp.c \
    ../../firmware/main/net/zs_sunspec.c \
    ../../firmware/main/net/zs_fronius.c \
    ../../firmware/main/app/zs_format.c \
    ../../firmware/main/app/zs_status.c \
    -lm -o zs-probe
echo "zs-probe bygget"
