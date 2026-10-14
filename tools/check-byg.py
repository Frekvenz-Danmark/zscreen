#!/usr/bin/env python3
"""
zScreen - tjek at hver bygning kan finde de headere dens egne filer beder om.

HVORFOR DEN FINDES.

Koden bliver bygget fire steder, og hver af dem har sin egen liste over
kilder og sine egne include-stier:

    firmware/main/CMakeLists.txt    zScreen, altsaa ESP32-S3
    firmware/boards/zdisplay/...     ZDisplay, altsaa ESP32-P4
    tests/host/run.sh               enhedstestene paa en almindelig maskine
    tools/zs-probe/build.sh         fejlsoegningsvaerktoejet
    tests/fuzz/koer.sh              fuzzerne

De to firmwarer deler kildelisten i firmware/kerne.cmake og laegger hver
sine tre-fire board-filer oveni. Begge bliver tjekket, og det er ikke
for symmetriens skyld: en header kan findes paa det ene board og mangle
paa det andet. Det ER sket - storage/zs_nvs.c bad om esp_phy_init.h, som
slet ikke findes for esp32p4, fordi radioen sidder i en anden chip.

Da util/zs_tekst.c blev lagt ind, blev den foejet til de to foerste og
glemt i den tredje. Enhedstestene var groenne, og saa faldt bygningen af
vaerktoejet med "zs_tekst.h file not found".

Det var til at se. Det der IKKE var til at se var det naeste: testkoerslen
gik videre, og ende til ende koerte mod den GAMLE binaer der stadig laa
der. Den sagde "70 tjek, alle bestaaet", og den loeg. Foerst da bygningen
virkede, kom de rigtige fejl frem, og saa havde de ikke noget at goere med
det jeg havde lavet.

Vi kraever IKKE at de fire har de samme stier. Vaerktoejet oversaetter
ingen skaermkode og skal ikke kende ui-mappen. Vi kraever det der faktisk
betyder noget: at hver bygning kan finde HVER header som dens egne filer
beder om, ogsaa de headere en header selv tager med.

Og dertil én regel mere: hver .c under firmware/main skal staa i
CMakeLists.txt. Ellers bliver den slet ikke oversat til skaermen, uden en
advarsel nogen steder.

Koeres af tests/run-all.sh, saa den ikke kan glemmes.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
SRC = ROD / "firmware/main"

# Genererede filer. De bliver lavet af tools/build-assets.sh.
SPRING_OVER = ("ui/assets/",)

# VORES EGNE headere, og kun dem.
#
# Det er en liste over hvad vi HAR og ikke en liste over alt det fremmede.
# Den anden vej rundt skulle den vedligeholdes hver gang ESP-IDF eller
# LVGL fik en ny mappe, og en glemt linje der ville give en falsk fejl som
# nogen saa ville slaa testen fra for.
VORES = re.compile(r"^(zs_[a-z0-9_]+|lv_port)\.h$")

fejl = []


def includes(fil):
    """Hvilke "egne" headere beder filen om? Kun dem i gaaseoejne."""
    try:
        tekst = fil.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    ud = []
    for m in re.finditer(r'^\s*#\s*include\s+"([^"]+)"', tekst, re.M):
        navn = m.group(1)
        if not VORES.match(navn.rsplit("/", 1)[-1]):
            continue
        ud.append(navn)
    return ud


def find(navn, fra, stier):
    """Kan include'en findes? Foerst ved siden af filen selv, som C goer,
    saa i hver af bygningens stier."""
    if navn.startswith("../") or "/" in navn:
        # En sti med mappe i: proev den som den staar, relativt til filen.
        p = (fra.parent / navn).resolve()
        if p.exists():
            return p
    if (fra.parent / navn).exists():
        return (fra.parent / navn)
    for s in stier:
        p = s / navn
        if p.exists():
            return p
    return None


def tjek_bygning(navn, kilder, stier):
    """Kan hver header findes, ogsaa de headere en header selv tager med?"""
    set_ = set()
    koe = list(kilder)
    while koe:
        fil = koe.pop()
        if fil in set_:
            continue
        set_.add(fil)
        for inc in includes(fil):
            fundet = find(inc, fil, stier)
            if fundet is None:
                rel = fil.relative_to(ROD).as_posix()
                fejl.append(f'{navn}: {rel} beder om "{inc}", og den kan ikke findes. '
                            f"Mangler der en -I eller en kilde?")
            else:
                koe.append(fundet)


def kilder_og_stier_fra_shell(sti_til_script):
    """Henter -I-stierne og .c-filerne ud af et byggescript."""
    tekst = sti_til_script.read_text(encoding="utf-8")
    base = sti_til_script.parent
    stier = [(base / m).resolve() for m in re.findall(r"-I(\S+)", tekst)]
    kilder = []
    for m in re.findall(r"(\S*\.\./\.\./firmware/main/\S+\.c)", tekst):
        p = (base / m).resolve()
        if p.exists():
            kilder.append(p)
    return kilder, stier


def kilder_og_stier_fra_fuzz():
    """Fuzzerne skriver det anderledes, saa de har deres egen laesning.

    De staar med stier regnet fra projektets rod, og med et mellemrum
    efter -I. Da util/ kom til, var det den FJERDE bygning der faldt, og
    den faldt efter at baade enhedstestene og ende til ende var groenne.
    Saa den skal med her.
    """
    sti = ROD / "tests/fuzz/koer.sh"
    tekst = sti.read_text(encoding="utf-8")
    stier = [(ROD / m).resolve()
             for m in re.findall(r"-I\s+(\S+)", tekst)]
    kilder = []
    for m in re.findall(r"((?:tests|firmware)/\S+\.c)", tekst):
        q = (ROD / m).resolve()
        if q.exists():
            kilder.append(q)
    return kilder, stier


def udfold(tekst):
    """Erstatter de to cmake-variabler kerne.cmake bruger i stierne."""
    return (tekst.replace("${ZS_MAIN_DIR}", (ROD / "firmware/main").as_posix())
                 .replace("${ZS_VENDOR_DIR}", (ROD / "firmware/vendor").as_posix()))


def blok_efter(tekst, navn):
    """Indholdet af et set(NAVN ... ) i en cmake-fil."""
    m = re.search(r"set\(" + re.escape(navn) + r"\b(.*?)\)", tekst, re.S)
    return m.group(1) if m else ""


def kerne():
    """Kilder og include-stier fra firmware/kerne.cmake.

    Den fil findes fordi to produkter skal bygge af SAMME liste. Stod
    listen to steder, ville en ny fil i net/ komme med i den ene skaerm
    og ikke i den anden - og det ville ikke vaere en byggefejl, men en
    manglende funktion hos en kunde.
    """
    tekst = udfold((ROD / "firmware/kerne.cmake").read_text(encoding="utf-8"))
    kilder = []
    for navn in ("ZS_MOTOR_SRCS", "ZS_FLADE_SRCS", "ZS_ASSET_SRCS"):
        for m in re.findall(r'"([^"]+\.c)"', blok_efter(tekst, navn)):
            p = pathlib.Path(m)
            if p.exists():
                kilder.append(p.resolve())
    stier = [pathlib.Path(m).resolve()
             for m in re.findall(r'"([^"]+)"', blok_efter(tekst, "ZS_KERNE_INCLUDE_DIRS"))]
    return kilder, stier


def tjek_board(navn, cmake, kerne_kilder, kerne_stier):
    """Ét produkts bygning: dens egne filer plus motoren."""
    tekst = cmake.read_text(encoding="utf-8")
    base = cmake.parent

    egne = []
    for m in re.findall(r'"([^"$]+\.c)"', tekst):
        p = (base / m).resolve()
        if p.exists():
            egne.append(p)
        else:
            rel = cmake.relative_to(ROD).as_posix()
            fejl.append(f"{rel}: SRCS naevner {m}, men filen findes ikke.")

    stier = list(kerne_stier)
    blok = tekst.split("INCLUDE_DIRS", 1)
    if len(blok) == 2:
        for m in re.findall(r'"([^"$]+)"', blok[1].split("REQUIRES", 1)[0]):
            stier.append((base / m).resolve())

    tjek_bygning(navn, egne + kerne_kilder, stier)
    return egne


def tjek_cmake():
    kerne_kilder, kerne_stier = kerne()
    if not kerne_kilder:
        fejl.append("firmware/kerne.cmake: fandt ingen kilder. Er listen skrevet om?")
        return

    s3 = tjek_board("zScreen", SRC / "CMakeLists.txt", kerne_kilder, kerne_stier)
    p4_cmake = ROD / "firmware/boards/zdisplay/main/CMakeLists.txt"
    p4 = []
    if p4_cmake.exists():
        p4 = tjek_board("ZDisplay", p4_cmake, kerne_kilder, kerne_stier)

    # Regel 1: hver .c under firmware/main skal bygges af MINDST ét
    # produkt. Ellers bliver den slet ikke oversat, uden en advarsel
    # nogen steder.
    bygget = set(kerne_kilder) | set(s3) | set(p4)
    for c in sorted(SRC.rglob("*.c")):
        rel = c.relative_to(SRC).as_posix()
        if any(rel.startswith(x) for x in SPRING_OVER):
            continue
        if c.resolve() not in bygget:
            fejl.append(f"firmware/main/{rel} findes, men staar hverken i "
                        f"kerne.cmake eller i et boards CMakeLists. Den bliver "
                        f"aldrig oversat.")

    # Og samme regel for ZDisplays egne filer.
    p4_dir = p4_cmake.parent
    if p4_cmake.exists():
        for c in sorted(p4_dir.rglob("*.c")):
            if c.resolve() not in bygget:
                rel = c.relative_to(ROD).as_posix()
                fejl.append(f"{rel} findes, men staar ikke i SRCS. Den bliver "
                            f"aldrig oversat til ZDisplay.")


def main():
    print("Tjekker at hver bygning kan finde sine headere ...")
    tjek_cmake()
    for navn, sti in (("enhedstestene", ROD / "tests/host/run.sh"),
                      ("vaerktoejet", ROD / "tools/zs-probe/build.sh")):
        kilder, stier = kilder_og_stier_fra_shell(sti)
        if not kilder:
            fejl.append(f"{navn}: fandt ingen kildefiler i {sti.name}. "
                        f"Er listen skrevet om?")
            continue
        tjek_bygning(navn, kilder, stier)

    kilder, stier = kilder_og_stier_fra_fuzz()
    if not kilder:
        fejl.append("fuzzerne: fandt ingen kildefiler i tests/fuzz/koer.sh. "
                    "Er listen skrevet om?")
    else:
        tjek_bygning("fuzzerne", kilder, stier)

    if fejl:
        for f in fejl:
            print(f"  \033[1;31mFEJL\033[0m {f}")
        print(f"\n  {len(fejl)} ting at rette.")
        return 1

    print("  alle kilder er med, og alle seks bygninger kan finde det de beder om")
    return 0


if __name__ == "__main__":
    sys.exit(main())
