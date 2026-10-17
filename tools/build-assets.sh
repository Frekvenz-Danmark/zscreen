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
ASSETS="../firmware/kerne/ui/assets"
FORCE="${1:-}"

# ---------------------------------------------------------------------
# TO SAET, ET PR. SKAERMSTOERRELSE
# ---------------------------------------------------------------------
#
# HVORFOR.
#
# En skrifttype i LVGL er et bitmap. Den kan ikke skaleres paa skaermen
# - en 64 px skrift er 64 px, ogsaa paa en skaerm der er dobbelt saa
# stor. Da fladen blev bygget til 800 x 1280, blev kasserne 1,67 gange
# saa store, og teksten i dem blev praecis lige saa lille. Det var det
# man kunne SE var galt.
#
# Derfor genereres der ét saet pr. skaerm, med de SAMME navne. Navnene
# siger hvad skriften bruges TIL og ikke hvor stor den er:
#
#   zs_font_tal      det store tal i et kort
#   zs_font_stor     enheder og overskrifter
#   zs_font_rad      listerader og knapper
#   zs_font_broed    undertekst og broedtekst
#   zs_font_etiket   etiketter med versaler
#   zs_font_kb       tastaturet, med ikoner i samme skrift
#   zs_ikon_lille    ikoner i statuslinjen og paa pile
#   zs_ikon_stor     ikoner i kort og paa overskrifter
#
# Her stod der foer zs_font_num_64 og zs_font_20. Det var praecise navne
# saa laenge der var ÉN skaerm. Med to er de direkte misvisende: paa
# ZDisplay er "zs_font_20" 33 px.
#
# STOERRELSERNE REGNES MED SAMME BROEK SOM LAYOUTET: v * bredde / 480.
# Det er ZS_G_SK i zs_layout.h, og det er med vilje det samme regnestykke
# - saa en kasse paa 90 px faar en skrift paa 106, praecis som en kasse
# paa 54 faar 64 paa referenceskaermen.
REF_W=480
SAET_BREDDER="480 800"

# Skalerer et maal til det saet vi er i gang med.
sk() { echo $(( $1 * SAET_W / REF_W )); }

mkdir -p "${CACHE}"

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
# 0xC9 og 0xE9 er É og é. Dansk klarer sig uden dem i de fleste ord,
# men ikke i alle: "én" med accent betyder tallet ét og adskiller sig
# fra artiklen "en", og det er forskellen paa "kun én kasse" og "kun en
# kasse". To glyffer koster under 200 bytes i alt.
DANSK="0x20-0x7E,0xB7,0xC5,0xC6,0xC9,0xD8,0xE5,0xE6,0xE9,0xF8,0x2022"

# OG NAVNE, der kommer UDEFRA.
#
# DANSK daekker vores egne tekster, og tools/check-text.py holder dem op
# mod skrifttyperne. Men KUNDENS NAVN kommer fra tilmeldingen, altsaa
# fra det en montoer har tastet i en app, og det bliver tegnet paa
# skaermen. Det blev maalt 13. oktober 2026:
#
#   Müller      ->  M□ller
#   Özdemir     ->  □zdemir
#   Wiśniewska  ->  Wi□niewska
#
# Firkanten er LVGL's pladsholder, CONFIG_LV_USE_FONT_PLACEHOLDER=1. Og
# tilmeldingens tjek_kunde godtager dem alle, saa de naar hele vejen.
# Netop det navn er det eneste der kan vise montoeren at koden endte
# hos den rigtige kunde.
#
#   0xC0-0xFF    Latin-1's bogstaver: ä ö ü ñ ç á é í ó ú ø å æ
#                Tysk, svensk, spansk, fransk, portugisisk, tyrkisk
#   0x100-0x17F  Latin Extended-A: ł ś ż ć ę ą ğ ş č š ž
#                Polsk, tjekkisk, tyrkisk, baltisk
#
# VIETNAMESISK ER IKKE MED, OG DET KAN DET IKKE BLIVE MED DEN HER
# SKRIFTTYPE.
#
# Nguyễn er ikke et saeldent efternavn i Danmark, saa jeg proevede
# 0x180-0x24F og 0x1E00-0x1EFF oveni. Maalt 13. oktober 2026: af de 368
# kodepunkter har Funnel Sans **16**. lv_font_conv springer i stilhed
# over det en TTF ikke har, saa binaeren voksede ikke én byte, og
# skrifttyperne fik hverken ễ, ư eller ơ.
#
# Vietnamesisk kraever altsaa en ANDEN eller en EKSTRA skrifttypefil, og
# det er en beslutning om udseende og licens, ikke om plads. Det staar i
# zScreen-server/docs/til-afgoerelse.md.
#
# PRISEN FOR DET DER VIRKER ER MAALT, ikke skoennet: zscreen.bin voksede
# 65536 bytes, altsaa 4,2 %, fra 50,1 % til 52,2 % af app-partitionen.
# Der er 1,4 MB ledigt. De fire tekst-skrifttyper gik fra 105 til 297
# tegn, maalt paa cmap-tabellerne og deres unicode_list - IKKE paa
# range_length, som kun er spaendet og taeller alt for hoejt.
#
# TASTATURET FAAR DEM IKKE. Det har faste taster, saa et tegn man ikke
# kan taste, er kun plads. Derfor er der to omraader og ikke ét. Maalt:
# tastaturet har 108 tegn, de fire andre 297.
NAVNE="${DANSK},0xC0-0xFF,0x100-0x17F"

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

# ---------------------------------------------------------------------
# Ét saet ad gangen
# ---------------------------------------------------------------------
#
# SAET_W er skaermens bredde, og alt herinde skaleres med sk().
# OUT er saettets egen mappe, saa de to saet ikke skriver over hinanden.
byg_saet() {
    SAET_W="$1"
    OUT="${ASSETS}/${SAET_W}"
    mkdir -p "${OUT}"
    info "===== Saet til ${SAET_W} px bredde ====="

    info "Bygger skrifttyper"
    build_font zs_font_tal    "${CACHE}/funnel-600.ttf" "$(sk 64)" "${TAL}"
    build_font zs_font_stor   "${CACHE}/funnel-600.ttf" "$(sk 28)" "${NAVNE}"
    build_font zs_font_rad    "${CACHE}/funnel-500.ttf" "$(sk 20)" "${NAVNE}"
    build_font zs_font_broed  "${CACHE}/funnel-500.ttf" "$(sk 16)" "${NAVNE}"
    build_font zs_font_etiket "${CACHE}/funnel-600.ttf" "$(sk 13)" "${NAVNE}"

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
    if [ ! -s "${OUT}/zs_font_kb.c" ] || [ "${FORCE}" = "--force" ]; then
        ${LVFC} --size "$(sk 24)" --bpp 4 --no-compress --format lvgl --lv-include lvgl.h \
                --font "${CACHE}/funnel-500.ttf" --range "${DANSK}" \
                --font "${CACHE}/lucide.ttf"     --range "${KB_ICONS}" \
                -o "${OUT}/zs_font_kb.c" >/dev/null
        ok "zs_font_kb ($(wc -c < "${OUT}/zs_font_kb.c" | tr -d ' ') bytes C-kode)"
    else
        ok "zs_font_kb findes"
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

    # Tredje argument er et interval, hvis man kun vil have NOGLE
    # ikoner. Det er ikke en bekvemmelighed: zs_ikon_stor er 139 KB i
    # 800-saettet, og en hel ikonskrift i en stoerre grad ville koste
    # omkring 370. Fejlsiden skal have ÉT ikon stort, og ét glyf paa 66
    # px koster omkring 2 KB.
    build_icons() {
        local name="$1" size="$2" omraade="${3:-${ICONS}}"
        local dst="${OUT}/${name}.c"
        if [ -s "${dst}" ] && [ "${FORCE}" != "--force" ]; then
            ok "${name} findes"
            return 0
        fi
        ${LVFC} --font "${CACHE}/lucide.ttf" --size "${size}" --bpp 4 --no-compress \
                --format lvgl --lv-include lvgl.h \
                --range "${omraade}" -o "${dst}" >/dev/null
        ok "${name} ($(wc -c < "${dst}" | tr -d ' ') bytes C-kode)"
    }

    info "Bygger ikoner"
    build_icons zs_ikon_lille "$(sk 20)"
    build_icons zs_ikon_stor  "$(sk 28)"
    # Kun batteriet, og stort. Fejlsiden viser det naar stroemmen er
    # vaek, og dér er det sidens eneste billede - se ZS_G_FEJL_IKON_Y.
    # KODEPUNKTET SPOERGES OM, DET SKRIVES IKKE AF.
    #
    # Her stod "0xE053" med en note om at det var ZS_ICON_BATTERY.
    # icons.py's egen docstring siger "listen over ikoner. ÉT sted" -
    # og det var den ikke, for det her var en kopi.
    #
    # Aendrede eller ombyttede man ikonet i icons.py, fulgte
    # zs_icons.h med (tjek_genererede() i check-byg.py haandhaever det
    # byte for byte), mens den her linje blev ved at bage den GAMLE
    # glyf ind i det kaempe skrift. ZS_ICON_BATTERY ville saa blive
    # tegnet i et skrift der ikke indeholder den, og LVGL tegner sin
    # pladsholder-firkant som det ENESTE billede paa alarmsiden
    # "Stroemforsyningen er fjernet". Ingen vagt fangede det:
    # check-text.py springer det private omraade over med vilje, og
    # tjek_genererede() sammenligner kun zs_icons.h med icons.py.
    KAEMPE_CP="$(python3 icons.py --kodepunkt BATTERY)" \
        || fail "kunne ikke laese kodepunktet for BATTERY fra icons.py"
    build_icons zs_ikon_kaempe "$(sk 40)" "${KAEMPE_CP}"
    # zs_icons.h er kodepunkter og ikke pixels, saa den er faelles for
    # begge saet og skrives kun én gang.
    if [ "${SAET_W}" = "${REF_W}" ]; then
        python3 icons.py --header "../firmware/kerne/ui/zs_icons.h" \
            || fail "kunne ikke skrive zs_icons.h"
    fi

    # ── Skrifternes FAKTISKE linjehoejder ─────────────────────────
    #
    # HVORFOR DE SKRIVES UD.
    #
    # Layoutet reserverer en hoejde til hver tekstlinje i et kort. De tal
    # var skaleringer af 480-tallene, og de passede ikke med hvad
    # skriften faktisk bruger. Maalt paa enheden:
    #
    #                reserveret   skriften bruger
    #   broed  480        18            20      2 px for lidt
    #   broed  800        30            33      3 px for lidt
    #
    # Resultatet var at underteksten stak 5 px ned under kortets
    # inderkant paa alle fire kort. Maalebaandet i
    # firmware/kerne/ui/zs_ui_maal.c fandt det paa enheden.
    #
    # lv_font_conv skriver selv linjehoejden i den genererede fil. Her
    # tages den DERFRA og laegges i en header, saa layoutet kan reservere
    # det der faktisk skal bruges i stedet for at gaette.
    info "Skriver skrifternes maal"
    # Nabofilen ryddes op hvis vi falder undervejs. Uden den her laa
    # der et zs_skrift_maal.h.ny tilbage i asset-mappen efter hver
    # mislykket koersel - harmloest, men affald man skal gaette paa.
    trap 'rm -f "${OUT}/zs_skrift_maal.h.ny"' EXIT
    {
        echo "/* GENERERET af tools/build-assets.sh. Ret den ikke. */"
        echo "/*"
        echo " * Linjehoejderne som lv_font_conv selv har regnet dem, for"
        echo " * saettet til ${SAET_W} px bredde. zs_layout.h reserverer"
        echo " * plads efter DE HER tal og ikke efter et skaleret"
        echo " * overslag - se ZS_G_CARD_SUB_HEIGHT."
        echo " */"
        echo "/* Guarden slutter paa _INC og ikke _H: et navn der"
        echo " * slutter paa _H er forbeholdt headere der ligger"
        echo " * hvor tools/check-headers.sh kigger, og den her"
        echo " * ligger dybere. Se husreglen i check-headers.sh. */"
        echo "#ifndef ZS_SKRIFT_MAAL_INC"
        echo "#define ZS_SKRIFT_MAAL_INC"
        echo "/* Hvilken skaermbredde saettet er bygget til. zs_layout.h"
        echo " * holder den op mod ZS_G_SCR_WIDTH, saa en skaerm der"
        echo " * peger paa det forkerte saet ikke bygger. Vaelger"
        echo " * ZDisplay saettet til 480, bliver skriften en tredjedel"
        echo " * for lille, og INTET andet siger fra: layoutet regner"
        echo " * videre paa 800, kortene bliver store og tomme, og det"
        echo " * ser ud som et designvalg. */"
        printf '#define ZS_SKRIFT_SAET_WIDTH %s\n' "${SAET_W}"
        for n in tal stor rad broed etiket kb; do
            lh=$(grep -o "\.line_height = [0-9]*" "${OUT}/zs_font_${n}.c" \
                 | head -1 | grep -o "[0-9]*")
            [ -n "${lh}" ] || fail "kunne ikke laese linjehoejden i zs_font_${n}.c"
            printf '#define ZS_LH_%-8s %s\n' "$(echo ${n} | tr a-z A-Z)" "${lh}"
        done
        # Ikonskrifterne med, og de SKAL vaere der.
        #
        # Her stod "[ -f ... ] || continue", fordi ikonerne blev bygget
        # EFTER det her afsnit. Det gav to fejl der begge var tavse:
        # foerste byg af et nyt saet skrev headeren UDEN
        # ZS_LH_IKON_*, saa ZDisplay ikke kunne oversaettes - og en
        # koersel med --force og en ny ikonstoerrelse beholdt forrige
        # koersels tal, saa layoutet reserverede en forkert hoejde
        # indtil nogen tilfaeldigvis byggede to gange.
        #
        # Ikonerne bygges nu foerst, saa filerne er der, og saa maa
        # loekken kraeve dem i stedet for at se forbi.
        for n in lille stor kaempe; do
            f="${OUT}/zs_ikon_${n}.c"
            [ -f "${f}" ] || fail "${f} blev ikke bygget - uden den kan"\
" zs_layout.h ikke reservere plads til ikonet"
            lh=$(grep -o "\.line_height = [0-9]*" "${f}" \
                 | head -1 | grep -o "[0-9]*")
            [ -n "${lh}" ] || fail "kunne ikke laese linjehoejden i ${f}"
            printf '#define ZS_LH_IKON_%-3s %s\n' \
                "$(echo ${n} | tr a-z A-Z)" "${lh}"
        done
        echo "#endif /* ZS_SKRIFT_MAAL_INC */"
    } > "${OUT}/zs_skrift_maal.h.ny"
    #
    # SKREVET TIL NABOFILEN OG BYTTET TIL SIDST.
    #
    # Her blev der skrevet direkte i zs_skrift_maal.h. Omdirigeringen
    # TOMMER filen foer det foerste echo, og loekkerne ovenfor kalder
    # fail naar en linjehoejde ikke kan laeses eller en ikonfil
    # mangler. En koersel der faldt dér, efterlod altsaa en HALV
    # header - og fordi kilder.cmake laegger ${ZS_ASSET_DIR} paa
    # include-stien, finder naeste ZDisplay-byg den, mangler
    # ZS_LH_IKON_KAEMPE og falder langt inde i zs_layout.h.
    #
    # Det er praecis den fejl noten ovenfor siger blev rettet. Den var
    # flyttet, ikke fjernet: raekkefoelgen gjorde den sjaeldnere, men
    # en manglende ikonfil udloeser den stadig.
    mv -f "${OUT}/zs_skrift_maal.h.ny" "${OUT}/zs_skrift_maal.h" \
        || fail "kunne ikke bytte zs_skrift_maal.h paa plads"
    trap - EXIT
    # Tallet er LINJEHOEJDER og ikke alle #define. Guarden og
    # saetbredden er ogsaa #define, saa "grep -c" meldte to for mange.
    ok "zs_skrift_maal.h ($(grep -c '^#define ZS_LH_' "${OUT}/zs_skrift_maal.h") linjehoejder)"

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
            --name zs_img_zmark --height "$(sk 26)"
        python3 png2lvgl.py "../brand/Z-logo.png" "${OUT}/zs_img_zmark_pos.c" \
            --name zs_img_zmark_pos --height "$(sk 26)"
        python3 png2lvgl.py "../brand/Frekvenz - logo payoff-neg.png" \
            "${OUT}/zs_img_wordmark.c" --name zs_img_wordmark --width "$(sk 260)"
        python3 png2lvgl.py "../brand/Frekvenz - logo payoff.png" \
            "${OUT}/zs_img_wordmark_pos.c" --name zs_img_wordmark_pos --width "$(sk 260)"
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
}

for b in ${SAET_BREDDER}; do
    byg_saet "${b}"
done

info "Faerdig"
TOTAL=$(cat "${OUT}"/*.c | wc -c | tr -d ' ')
echo
echo "  ${OUT} fylder ${TOTAL} bytes C-kode i alt."
echo "  Selve dataen i flash er en del mindre, C-kilde er meget kommaer."
