#!/usr/bin/env bash
#
# zScreen - installer ESP-IDF vaerktoejskaeden.
#
# Seeed SenseCAP Indicator SDK kraever ESP-IDF v5.1.x praecist. Hverken
# lavere eller hoejere virker: v5.0 mangler RGB-panel-API'et deres BSP
# bruger, og v5.2+ aendrede LCD-driver-signaturerne saa deres kode ikke
# oversaetter. Vi pinner v5.1.7, nyeste patch i v5.1-serien.
#
# Python-faelden (ramt paa Yassins Mac 2026-08-25):
#   ESP-IDF v5.1 er testet til og med Python 3.12, og bygger sit eget
#   virtuelle miljoe med "python -m venv". Det trin kan faejle af to
#   helt forskellige grunde:
#
#   1. For ny Python. Homebrew's "python3" er i dag 3.14, som ESP-IDF
#      v5.1 ikke kender. IDF's detect_python.sh tager altid det foerste
#      "python3" i PATH, saa vi kan ikke bare bede den om noget andet.
#
#   2. Oedelagt Homebrew-Python. Baade python@3.12 og python@3.14 paa
#      denne maskine har et pyexpat der loader systemets libexpat i
#      stedet for Homebrews, og derfor mangler et symbol. Alt der
#      bruger XML faejler, inklusive pip-installationen i venv'et.
#      Fejlen ser ud som "ensurepip returned non-zero exit status 1",
#      hvilket ikke afsloerer noget som helst.
#      Rettes med:  brew reinstall expat python@3.12
#
#   Derfor: vi proever hver kandidat af i praksis ved at bygge et
#   rigtigt venv med pip i, og tager den foerste der faktisk virker.
#   Vinderen kommer i en shim-mappe forrest i PATH. tools/env.sh
#   laegger den samme shim ind, saa det ogsaa holder i nye terminaler.
#
# Scriptet er idempotent. Koer det igen paa en maskine hvor alt allerede
# er paa plads, og det laver ingenting.
#
# Brug:   ./tools/setup-toolchain.sh
# Derefter: source tools/env.sh

set -euo pipefail

IDF_VERSION="v5.1.7"
IDF_DIR="${HOME}/esp/esp-idf"
SHIM_DIR="${HOME}/.espressif/zscreen-python-shim"
TARGET="esp32s3"

# Nyeste Python som ESP-IDF v5.1 er testet med. Se listen i
# ${IDF_DIR}/tools/detect_python.sh, den stopper ved python3.12.
PY_MAX_MINOR=12
PY_MIN_MINOR=8

info() { printf '\033[1;36m==>\033[0m %s\n' "$1"; }
ok()   { printf '\033[1;32m OK\033[0m %s\n' "$1"; }
warn() { printf '\033[1;33m !!\033[0m %s\n' "$1"; }
fail() { printf '\033[1;31mFEJL\033[0m %s\n' "$1" >&2; exit 1; }

# ---------------------------------------------------------------------------
# 1. Forudsaetninger
# ---------------------------------------------------------------------------
info "Tjekker forudsaetninger"
command -v git >/dev/null 2>&1 || fail "git mangler. Koer: xcode-select --install"

# ESP-IDF leverer selv compiler, openocd og Python-miljoe, men IKKE cmake
# og ninja paa macOS. Dem skal vi have fra Homebrew.
MISSING=""
command -v cmake >/dev/null 2>&1 || MISSING="${MISSING} cmake"
command -v ninja >/dev/null 2>&1 || MISSING="${MISSING} ninja"
if [ -n "${MISSING}" ]; then
    if command -v brew >/dev/null 2>&1; then
        info "Installerer${MISSING} via Homebrew"
        # shellcheck disable=SC2086
        brew install ${MISSING} || fail "brew install${MISSING} faejlede"
    else
        fail "Mangler${MISSING}. Installer Homebrew fra https://brew.sh og koer igen."
    fi
fi
ok "cmake $(cmake --version | head -1 | awk '{print $3}'), ninja $(ninja --version)"

# ---------------------------------------------------------------------------
# 2. Find en Python som ESP-IDF v5.1 kan bruge
# ---------------------------------------------------------------------------
info "Finder en Python mellem 3.${PY_MIN_MINOR} og 3.${PY_MAX_MINOR}"

ESP_PY=""

# Kandidater, nyeste foerst. /usr/bin/python3 er Apples egen (3.9.x paa
# macOS 14/15). Den er selvstaendig og roeres ikke af Homebrew, saa den
# er ofte den eneste der stadig virker naar Homebrew er ude af trit.
PY_CANDIDATES="
python3.12
/opt/homebrew/bin/python3.12
/usr/local/bin/python3.12
python3.11
/opt/homebrew/bin/python3.11
/usr/local/bin/python3.11
python3.10
/opt/homebrew/bin/python3.10
python3.9
/opt/homebrew/bin/python3.9
/usr/bin/python3
/usr/bin/python3.9
python3.8
"

for cand in ${PY_CANDIDATES}; do
    command -v "${cand}" >/dev/null 2>&1 || continue
    cand_path="$(command -v "${cand}")"

    # Er versionen i vindue? Vi spoerger fortolkeren selv i stedet for
    # at stole paa filnavnet, for "python3.12" kan sagtens vaere et
    # symlink til noget andet.
    if ! "${cand_path}" -c "
import sys
mi, ma = ${PY_MIN_MINOR}, ${PY_MAX_MINOR}
sys.exit(0 if sys.version_info[0] == 3 and mi <= sys.version_info[1] <= ma else 1)
" >/dev/null 2>&1; then
        continue
    fi

    # Virker den i praksis? Det er her de oedelagte Homebrew-pythons
    # falder. Vi bygger et rigtigt venv MED pip, praecis som ESP-IDF
    # goer det, i stedet for kun at tjekke at modulet kan importeres.
    tmpdir="$(mktemp -d)"
    if "${cand_path}" -m venv "${tmpdir}/probe" >/dev/null 2>&1 \
       && "${tmpdir}/probe/bin/python3" -m pip --version >/dev/null 2>&1; then
        ESP_PY="${cand_path}"
        rm -rf "${tmpdir}"
        break
    fi
    rm -rf "${tmpdir}"
    warn "${cand_path} ($("${cand_path}" -V 2>&1)) kan ikke bygge et brugbart venv, springer over"
done

if [ -z "${ESP_PY}" ]; then
    fail "Fandt ingen Python 3.${PY_MIN_MINOR}-3.${PY_MAX_MINOR} der kan bygge et venv med pip.

Er dine Homebrew-pythons oedelagte (typisk pyexpat mod forkert libexpat), saa koer:
    brew reinstall expat python@3.12
Findes der slet ingen, saa koer:
    brew install python@3.12
Koer derefter dette script igen."
fi
ok "Bruger $(${ESP_PY} --version 2>&1) fra ${ESP_PY}"

# ---------------------------------------------------------------------------
# 3. Byg shim-mappen
# ---------------------------------------------------------------------------
info "Laegger python3-shim i ${SHIM_DIR}"
mkdir -p "${SHIM_DIR}"
ln -sf "${ESP_PY}" "${SHIM_DIR}/python3"
ln -sf "${ESP_PY}" "${SHIM_DIR}/python"
ok "shim klar"

export PATH="${SHIM_DIR}:${PATH}"
SHIM_VER="$(python3 --version 2>&1)"
python3 -c "
import sys
mi, ma = ${PY_MIN_MINOR}, ${PY_MAX_MINOR}
sys.exit(0 if sys.version_info[0] == 3 and mi <= sys.version_info[1] <= ma else 1)
" || fail "shim virkede ikke, python3 i PATH er stadig ${SHIM_VER}"
ok "python3 i PATH er nu ${SHIM_VER}"

# ---------------------------------------------------------------------------
# 4. Hent ESP-IDF
# ---------------------------------------------------------------------------
if [ -d "${IDF_DIR}/.git" ]; then
    CURRENT="$(git -C "${IDF_DIR}" describe --tags --exact-match 2>/dev/null || echo 'ukendt')"
    if [ "${CURRENT}" = "${IDF_VERSION}" ]; then
        ok "ESP-IDF ${IDF_VERSION} findes allerede"
    else
        warn "ESP-IDF staar paa ${CURRENT}, skifter til ${IDF_VERSION}"
        git -C "${IDF_DIR}" fetch --depth 1 origin "refs/tags/${IDF_VERSION}:refs/tags/${IDF_VERSION}"
        git -C "${IDF_DIR}" checkout "${IDF_VERSION}"
        git -C "${IDF_DIR}" submodule update --init --recursive --depth 1
    fi
else
    info "Henter ESP-IDF ${IDF_VERSION} (ca. 1 GB)"
    mkdir -p "${HOME}/esp"
    git clone --branch "${IDF_VERSION}" --depth 1 --recursive --shallow-submodules \
        https://github.com/espressif/esp-idf.git "${IDF_DIR}"
    ok "ESP-IDF hentet"
fi

# ---------------------------------------------------------------------------
# 5. Ryd et halvfaerdigt miljoe fra et tidligere forsoeg
# ---------------------------------------------------------------------------
for broken in "${HOME}"/.espressif/python_env/idf5.1_py3.1[34]_env; do
    if [ -d "${broken}" ]; then
        warn "Fjerner ubrugeligt miljoe fra tidligere forsoeg: ${broken}"
        rm -rf "${broken}"
    fi
done

# ---------------------------------------------------------------------------
# 6. Installer toolchain
# ---------------------------------------------------------------------------
info "Installerer toolchain til ${TARGET} (ca. 1-2 GB)"
( cd "${IDF_DIR}" && ./install.sh "${TARGET}" ) || fail "install.sh faejlede, se loggen ovenfor"
ok "Toolchain installeret"

# ---------------------------------------------------------------------------
# 7. Verificer for alvor
# ---------------------------------------------------------------------------
info "Verificerer"
set +u
# shellcheck disable=SC1091
source "${IDF_DIR}/export.sh" >/dev/null 2>&1 || true
set -u

command -v idf.py >/dev/null 2>&1 || fail "idf.py kom ikke i PATH. Miljoeet er ikke klar."
ok "idf.py: $(idf.py --version 2>&1 | head -1)"
command -v cmake >/dev/null 2>&1 && ok "cmake: $(cmake --version | head -1)"
command -v ninja >/dev/null 2>&1 && ok "ninja: $(ninja --version)"
command -v xtensa-esp32s3-elf-gcc >/dev/null 2>&1 \
    && ok "compiler: $(xtensa-esp32s3-elf-gcc --version | head -1)" \
    || fail "xtensa-esp32s3-elf-gcc mangler"

cat <<TXT

------------------------------------------------------------------
 Faerdig.

 I hver ny terminal:
     source tools/env.sh

 Byg og flash:
     cd firmware
     idf.py build
     idf.py -p /dev/cu.usbmodemXXXX flash monitor

 Find porten naar D1'eren sidder i USB:
     ls /dev/cu.usbmodem*
------------------------------------------------------------------
TXT
