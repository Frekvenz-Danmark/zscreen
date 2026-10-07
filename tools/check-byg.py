#!/usr/bin/env python3
"""
zScreen - tjek at hver bygning kan finde de headere dens egne filer beder om.

HVORFOR DEN FINDES.

Koden bliver bygget fire steder, og hver af dem har sin egen liste over
kilder og sine egne include-stier:

    firmware/main/CMakeLists.txt    firmwaren til skaermen
    tests/host/run.sh               enhedstestene paa en almindelig maskine
    tools/zs-probe/build.sh         fejlsoegningsvaerktoejet
    tests/fuzz/koer.sh              fuzzerne

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


def tjek_cmake():
    cmake = SRC / "CMakeLists.txt"
    tekst = cmake.read_text(encoding="utf-8")

    # Regel 1: hver .c skal vaere med.
    alle = []
    for c in sorted(SRC.rglob("*.c")):
        rel = c.relative_to(SRC).as_posix()
        if any(rel.startswith(s) for s in SPRING_OVER):
            continue
        alle.append(c)
        if f'"{rel}"' not in tekst:
            fejl.append(f"firmware/main/CMakeLists.txt: {rel} findes, men staar "
                        f"ikke i SRCS. Den bliver aldrig oversat til skaermen.")

    # Og at firmwaren selv kan finde sine headere.
    blok = tekst.split("INCLUDE_DIRS", 1)
    stier = []
    if len(blok) == 2:
        for m in re.findall(r'"([^"]+)"', blok[1].split("REQUIRES", 1)[0]):
            stier.append((SRC / m).resolve())
    tjek_bygning("firmwaren", alle, stier)


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

    print("  alle kilder er med, og alle fire bygninger kan finde det de beder om")
    return 0


if __name__ == "__main__":
    sys.exit(main())
