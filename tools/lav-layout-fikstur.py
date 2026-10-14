#!/usr/bin/env python3
"""
zScreen - skriver tests/fixtures/layout-480.h ud af zs_layout.h.

HVORFOR DEN FINDES.

Fiksturen er den eneste maade vi kan vise at zScreens flade staar
stille. Et byg kan ikke bruges: to byg af samme kode giver forskellige
filer, fordi der staar et tidsstempel i app-beskrivelsen og
underskriften har tilfaeldigt salt. Maalt.

Men filen stod der med ordet GENERERET i hovedet og INTET der kunne
generere den. Altsaa var den i praksis haandskrevet, og en haandskrevet
liste over 85 tal er praecis saa god som den der skrev den var
opmaerksom. Det her script lukker det hul.

SAADAN BRUGES DEN. Naar du har lavet noget om i zs_layout.h:

    ./tools/lav-layout-fikstur.py
    git diff tests/fixtures/layout-480.h

Er der KUN tilfoejede linjer i diffen, har ingen af zScreens tal
flyttet sig. Staar der en aendret linje, skal du kunne sige hvorfor
zScreens flade skulle flytte sig - og svaret er naesten altid at det
skulle den ikke.

Med --tjek skriver den ingenting og siger bare fra hvis filen ikke
passer med headeren laengere. Det er den form vagterne bruger.
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
HEADER = ROD / "firmware/kerne/ui/zs_layout.h"
FIKSTUR = ROD / "tests/fixtures/layout-480.h"

HOVED = '''/*
 * Hver eneste ZS_G-konstant paa referenceskaermen, 480 x 480.
 *
 * GENERERET af tools/lav-layout-fikstur.py. Ret den ikke i haanden.
 *
 * HVORFOR FILEN FINDES.
 *
 * zScreen er et produkt der virker. Da fladen blev gjort skalerbar,
 * blev HVER vaerdi i zs_layout.h lagt om til et regnestykke - og
 * paastanden var at de alle giver praecis det samme paa 480 x 480.
 * Det er aritmetik og ikke en hensigt, men en paastand man ikke kan
 * proeve, er ikke vaerd at tro paa.
 *
 * Et BYG kan ikke bruges til at proeve det: to byg af samme kode
 * giver forskellige filer, fordi der staar et tidsstempel i
 * app-beskrivelsen og underskriften har tilfaeldigt salt. Maalt.
 *
 * Derfor staar tallene her, og test_layout.c regner dem efter.
 * Aendrer et tal sig paa 480, falder proeven - og saa skal nogen
 * sige HVORFOR zScreens flade skulle flytte sig.
 */

'''

# Et navn med en parentes lige efter er en makro man kalder, fx
# ZS_G_SK(x). Den har ingen vaerdi i sig selv og kan ikke staa i
# listen.
DEFINE = re.compile(r"^#define\s+(ZS_G_[A-Z0-9_]+)(\s|$)", re.M)


def navnene():
    tekst = HEADER.read_text(encoding="utf-8")
    ud, set_ = [], set()
    for m in DEFINE.finditer(tekst):
        navn = m.group(1)
        if navn not in set_:
            set_.add(navn)
            ud.append(navn)
    return ud


def vaerdierne(navne):
    """Spoerger oversaetteren, ikke mig selv.

    Tallene regnes af cc med headeren som den er, saa listen kan ikke
    komme i utakt med et regnestykke jeg har laest forkert.
    """
    linjer = "\n".join(
        f'    printf("ZS_FORVENTET({n}, %d)\\n", (int)({n}));' for n in navne)
    src = ("#include <stdio.h>\n"
           f'#include "{HEADER}"\n'
           "int main(void){\n" + linjer + "\n    return 0;\n}\n")
    with tempfile.TemporaryDirectory() as d:
        c = Path(d) / "dump.c"
        c.write_text(src, encoding="utf-8")
        exe = Path(d) / "dump"
        inkl = HEADER.parent
        r = subprocess.run(
            ["cc", "-std=c11", "-I", str(inkl), "-o", str(exe), str(c)],
            capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr.strip(), file=sys.stderr)
            print("kunne ikke oversaette zs_layout.h. Er der en "
                  "_Static_assert der siger fra?", file=sys.stderr)
            return None
        return subprocess.run([str(exe)], capture_output=True,
                              text=True, check=True).stdout


def main():
    navne = navnene()
    if not navne:
        print("fandt ingen ZS_G-konstanter i zs_layout.h", file=sys.stderr)
        return 1
    ud = vaerdierne(navne)
    if ud is None:
        return 1
    ny = HOVED + ud

    tjek = "--tjek" in sys.argv
    gammel = FIKSTUR.read_text(encoding="utf-8") if FIKSTUR.exists() else ""
    if ny == gammel:
        print(f"  \033[1;32m OK\033[0m {len(navne)} konstanter, "
              f"fiksturen passer med zs_layout.h.")
        return 0
    if tjek:
        print(f"  \033[1;31mFEJL\033[0m tests/fixtures/layout-480.h passer "
              f"ikke med zs_layout.h laengere.")
        print(f"         Koer ./tools/lav-layout-fikstur.py og se paa "
              f"diffen: er der ANDET end tilfoejede linjer, har "
              f"zScreens flade flyttet sig.")
        return 1
    FIKSTUR.write_text(ny, encoding="utf-8")
    print(f"  skrev {len(navne)} konstanter til "
          f"{FIKSTUR.relative_to(ROD)}")
    print(f"  SE PAA DIFFEN: git diff {FIKSTUR.relative_to(ROD)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
