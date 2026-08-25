# zScreen - byggemiljoe. Skal SOURCES, ikke koeres.
#
#   source tools/env.sh
#
# Den goer to ting:
#   1. Laegger python3-shim'en forrest i PATH, saa ESP-IDF finder en
#      Python den kan bruge (se tools/setup-toolchain.sh for hvorfor).
#   2. Kalder ESP-IDF's egen export.sh.
#
# Sikker at koere flere gange i samme terminal.

_zs_shim="${HOME}/.espressif/zscreen-python-shim"
_zs_idf="${HOME}/esp/esp-idf"

if [ ! -d "${_zs_shim}" ]; then
    echo "zScreen: python-shim mangler. Koer foerst: ./tools/setup-toolchain.sh" >&2
    return 1 2>/dev/null || exit 1
fi
if [ ! -f "${_zs_idf}/export.sh" ]; then
    echo "zScreen: ESP-IDF mangler i ${_zs_idf}. Koer foerst: ./tools/setup-toolchain.sh" >&2
    return 1 2>/dev/null || exit 1
fi

# Undgaa at PATH vokser hver gang man sourcer.
case ":${PATH}:" in
    *":${_zs_shim}:"*) : ;;
    *) PATH="${_zs_shim}:${PATH}"; export PATH ;;
esac

# shellcheck disable=SC1091
. "${_zs_idf}/export.sh"

echo "zScreen: python3 = $(python3 --version 2>&1), idf = $(idf.py --version 2>&1 | head -1)"

unset _zs_shim _zs_idf
