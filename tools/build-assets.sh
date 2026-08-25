#!/usr/bin/env bash
#
# zScreen - byg skrifttyper, ikoner og logoer til LVGL.
#
# Kilderne hentes fra nettet og lagres i tools/.assets-cache, som ikke
# kommer med i repoet. Resultatet, altsaa C-filerne, comittes derimod.
# Det betyder at man kan bygge firmwaren uden node og uden internet, og
# at en aendring i en skrifttype kan ses i git-historikken.
#
#   ./tools/build-assets.sh          byg det der mangler
#   ./tools/build-assets.sh --force  byg alt forfra
#
# Licenser:
#   Funnel Sans  SIL Open Font License 1.1  (maa indlejres frit)
#   Lucide       ISC                        (maa indlejres frit)
#   Frekvenz-logoerne er vores egne.

set -euo pipefail
cd "$(dirname "$0")"

CACHE=".assets-cache"
OUT="../firmware/main/ui/assets"
FORCE="${1:-}"

mkdir -p "${CACHE}" "${OUT}"

info() { printf '\033[1;36m==>\033[0m %s\n' "$1"; }
ok()   { printf '\033[1;32m OK\033[0m %s\n' "$1"; }
fail() { printf '\033[1;31mFEJL\033[0m %s\n' "$1" >&2; exit 1; }

command -v npx >/dev/null 2>&1 || fail "npx mangler. Installer Node: brew install node"
command -v curl >/dev/null 2>&1 || fail "curl mangler"

LVFC="npx --yes lv_font_conv@1.5.3"

# ---------------------------------------------------------------------
# Kilder
# ---------------------------------------------------------------------
# Funnel Sans fra Google Fonts. URL'erne peger paa version 3 af
# skrifttypen. De er skrevet ud i stedet for at blive slaaet op hver
# gang, saa et byg i dag giver praecis de samme bytes som et byg om et
# aar. Skal der opdateres, hentes CSS'en manuelt og URL'erne rettes her.
FUNNEL_BASE="https://fonts.gstatic.com/s/funnelsans/v3"
declare -a FONTS=(
    "500:${FUNNEL_BASE}/OpNfno8Dg9bX6Bsp3Wq69RB-VukSVv3aISFAp3mEfg.ttf"
    "600:${FUNNEL_BASE}/OpNfno8Dg9bX6Bsp3Wq69RB-VukSVv3aISFAS36Efg.ttf"
)
LUCIDE_TTF="https://unpkg.com/lucide-static@1.34.0/font/lucide.ttf"

fetch() {
    local url="$1" dst="$2"
    if [ -s "${dst}" ] && [ "${FORCE}" != "--force" ]; then
        return 0
    fi
    curl -sSfL "${url}" -o "${dst}" || fail "kunne ikke hente ${url}"
    [ -s "${dst}" ] || fail "${dst} blev tom"
}

info "Henter kilder"
for entry in "${FONTS[@]}"; do
    weight="${entry%%:*}"
    url="${entry#*:}"
    fetch "${url}" "${CACHE}/funnel-${weight}.ttf"
    ok "Funnel Sans ${weight} ($(wc -c < "${CACHE}/funnel-${weight}.ttf" | tr -d ' ') bytes)"
done
fetch "${LUCIDE_TTF}" "${CACHE}/lucide.ttf"
ok "Lucide ($(wc -c < "${CACHE}/lucide.ttf" | tr -d ' ') bytes)"

# ---------------------------------------------------------------------
# Tegnsaet
# ---------------------------------------------------------------------
# Almindeligt ASCII plus de danske bogstaver. Vi tager kun det vi
# bruger: hver skrifttype fylder i flash, og et fuldt Latin-1-saet er
# tre gange saa stort uden at give os noget.
#
#   0x20-0x7E   mellemrum til tilde
#   0xC5 0xC6 0xD8   AA AE OE
#   0xE5 0xE6 0xF8   aa ae oe
#   0xB7        midterprik, skiller to oplysninger paa samme linje
#   0x2022      punkttegn, skjuler tegnene i et kodeord
#
# De to sidste er ikke pynt. Bruger man et tegn skrifttypen ikke har,
# tegner LVGL ingenting: der kommer hverken en advarsel naar der bygges
# eller naar det koerer, bare et hul midt i en saetning.
# tools/check-text.py holder oeje med at hvert tegn vi skriver, ogsaa
# findes i alle skrifttyperne.
DANSK="0x20-0x7E,0xB7,0xC5,0xC6,0xD8,0xE5,0xE6,0xF8,0x2022"

# Til de store tal bruger vi kun cifre og skilletegn. Et fuldt tegnsaet
# i 64 pixels ville fylde over 200 KB uden at én af bogstaverne blev
# tegnet.
TAL="0x30-0x39,0x2C,0x2E,0x2D,0x25"

# ---------------------------------------------------------------------
# Skrifttyper
# ---------------------------------------------------------------------
# --no-compress med vilje. LVGL pakker ellers hver glyf ud igen hver
# gang den tegnes. Det store taltema bliver tegnet om hvert andet
# sekund, og skaermen skal foeles oejeblikkelig naar man trykker.
# Uden pakning fylder alle skrifttyper tilsammen omkring 85 KB, og vi
# har 2,5 MB tilbage i partitionen.
build_font() {
    local name="$1" src="$2" size="$3" range="$4"
    local dst="${OUT}/${name}.c"
    if [ -s "${dst}" ] && [ "${FORCE}" != "--force" ]; then
        ok "${name} findes"
        return 0
    fi
    ${LVFC} --font "${src}" --size "${size}" --bpp 4 --no-compress \
            --format lvgl --lv-include lvgl.h \
            --range "${range}" -o "${dst}" >/dev/null
    ok "${name} ($(wc -c < "${dst}" | tr -d ' ') bytes C-kode)"
}

info "Bygger skrifttyper"
build_font zs_font_num_64 "${CACHE}/funnel-600.ttf" 64 "${TAL}"
build_font zs_font_28     "${CACHE}/funnel-600.ttf" 28 "${DANSK}"
build_font zs_font_20     "${CACHE}/funnel-500.ttf" 20 "${DANSK}"
build_font zs_font_16     "${CACHE}/funnel-500.ttf" 16 "${DANSK}"
build_font zs_font_13     "${CACHE}/funnel-600.ttf" 13 "${DANSK}"

# Tastaturets skrifttype er en blanding.
#
# En knap i et LVGL-knapgitter kan kun have ÉN skrifttype, og
# tastaturet har baade bogstaver og tre ikoner: skift, slet og faerdig.
# Derfor flettes Funnel Sans og de tre Lucide-glyffer sammen til én
# skrifttype. lv_font_conv kan tage flere kilder, og et --range gaelder
# for den --font der staar lige foer.
#
#   0xE04A  arrow-up   skift mellem store og smaa bogstaver
#   0xE0AE  delete     slet et tegn
#   0xE06C  check      faerdig
KB_ICONS="0xe04a,0xe0ae,0xe06c"
if [ ! -s "${OUT}/zs_font_kb_24.c" ] || [ "${FORCE}" = "--force" ]; then
    ${LVFC} --size 24 --bpp 4 --no-compress --format lvgl --lv-include lvgl.h \
            --font "${CACHE}/funnel-500.ttf" --range "${DANSK}" \
            --font "${CACHE}/lucide.ttf"     --range "${KB_ICONS}" \
            -o "${OUT}/zs_font_kb_24.c" >/dev/null
    ok "zs_font_kb_24 ($(wc -c < "${OUT}/zs_font_kb_24.c" | tr -d ' ') bytes C-kode)"
else
    ok "zs_font_kb_24 findes"
fi

# ---------------------------------------------------------------------
# Ikoner
# ---------------------------------------------------------------------
# Lucide er den samme ikonpakke som Zbox-webfladen bruger, saa en
# advarselstrekant ser ens ud paa skaermen og i browseren.
#
# Listen over ikoner staar i tools/icons.py og bruges to steder: her,
# til at vaelge hvad der kommer med i skrifttypen, og til at skrive
# firmwarens zs_icons.h. Ét sted, saa navn og tegning ikke kan komme
# til at pege paa hver sit.
ICONS="$(python3 icons.py --range)" || fail "kunne ikke laese ikonlisten"

build_icons() {
    local name="$1" size="$2"
    local dst="${OUT}/${name}.c"
    if [ -s "${dst}" ] && [ "${FORCE}" != "--force" ]; then
        ok "${name} findes"
        return 0
    fi
    ${LVFC} --font "${CACHE}/lucide.ttf" --size "${size}" --bpp 4 --no-compress \
            --format lvgl --lv-include lvgl.h \
            --range "${ICONS}" -o "${dst}" >/dev/null
    ok "${name} ($(wc -c < "${dst}" | tr -d ' ') bytes C-kode)"
}

info "Bygger ikoner"
build_icons zs_icons_20 20
build_icons zs_icons_28 28
python3 icons.py --header "../firmware/main/ui/zs_icons.h" \
    || fail "kunne ikke skrive zs_icons.h"

# ---------------------------------------------------------------------
# Logoer
# ---------------------------------------------------------------------
# Logoerne bygges kun hvis brand-mappen er der.
#
# De raa logofiler er ikke i det offentlige repo, men de FAERDIGE
# C-filer er. Derfor kan enhver hente projektet og bygge firmwaren, og
# kun den der har brand-mappen kan lave logoerne om.
if [ -d "../brand" ]; then
    info "Bygger logoer"
    # To saet: negativ til moerkt tema, positiv til lyst. Samme maal, saa
    # de kan byttes ud uden at noget flytter sig paa skaermen.
    python3 png2lvgl.py "../brand/Z-logo-neg.png" "${OUT}/zs_img_zmark.c" \
        --name zs_img_zmark --height 26
    python3 png2lvgl.py "../brand/Z-logo.png" "${OUT}/zs_img_zmark_pos.c" \
        --name zs_img_zmark_pos --height 26
    python3 png2lvgl.py "../brand/Frekvenz - logo payoff-neg.png" \
        "${OUT}/zs_img_wordmark.c" --name zs_img_wordmark --width 260
    python3 png2lvgl.py "../brand/Frekvenz - logo payoff.png" \
        "${OUT}/zs_img_wordmark_pos.c" --name zs_img_wordmark_pos --width 260
else
    info "Springer logoer over"
    if [ -s "${OUT}/zs_img_zmark.c" ] && [ -s "${OUT}/zs_img_wordmark.c" ] \
       && [ -s "${OUT}/zs_img_zmark_pos.c" ] \
       && [ -s "${OUT}/zs_img_wordmark_pos.c" ]; then
        ok "brand-mappen er ikke her, men de faerdige logoer findes"
    else
        fail "brand-mappen mangler, og logoerne er ikke bygget."
    fi
fi

# ---------------------------------------------------------------------
info "Faerdig"
TOTAL=$(cat "${OUT}"/*.c | wc -c | tr -d ' ')
echo
echo "  ${OUT} fylder ${TOTAL} bytes C-kode i alt."
echo "  Selve dataen i flash er en del mindre, C-kilde er meget kommaer."
