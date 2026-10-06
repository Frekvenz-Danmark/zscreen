#!/usr/bin/env python3
"""
zScreen - tjekker at LVGL ikke bliver roert uden laasen.

HVORFOR DEN FINDES.

LVGL 8 er ikke bygget til at blive kaldt fra flere opgaver paa én gang.
Skaermopgaven tegner i sit eget tempo, og roerer en anden opgave de samme
objekter imens, kan listerne LVGL gaar igennem skifte under den. Det
viser sig som en skaerm der fryser eller et billede der gaar i stykker,
og det sker en gang om ugen uden moenster. Den slags fejl kan ikke
fejlsoeges bagefter, den skal forhindres.

Reglen i huset er at ALT i firmware/main/ui selv tager laasen, saa ingen
kalder skal huske det. Den regel stod kun som en kommentar, og en
kommentar holder ingen i haanden.

Det her script holder den fast: en funktion i ui/ som kaldes fra app,
net eller main, og som roerer LVGL, SKAL tage laasen.

Den fangede én: zs_theme_set_mode roerte LVGL tolv steder uden laasen.
Det gik godt indtil nu, men kun fordi den ene kalder tilfaeldigvis koerer
foer brugerfladen findes. Et sammentraef, ikke en garanti.

GRAENSER, saa ingen tror den kan mere end den kan. Den laeser teksten,
ikke programmet. Kaldes en funktion gennem en pegepind, eller bygges
navnet sammen af stumper, ser den det ikke. Den er en bund, ikke et
bevis.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
UI = ROD / "firmware/main/ui"
UDEFRA = [ROD / "firmware/main/app", ROD / "firmware/main/net"]
MAIN = ROD / "firmware/main/main.c"

GROEN = "\033[1;32m"
ROED = "\033[1;31m"
SLUT = "\033[0m"

LAAS = "lv_port_sem_take"
# lv_port_sem_* er selve laasen, ikke noget der skal laases om.
LV_KALD = re.compile(r"\blv_(?!port_sem)\w+\s*\(")
# En definition: noget der ikke er static, med en krop bagefter.
DEFINITION = re.compile(r"^(?!static)(?:\w[\w \*]*?)\b(\w+)\s*\([^;{]*?\)\s*\{", re.M)
NOEGLEORD = {"if", "for", "while", "switch", "return", "sizeof", "else"}


def uden_kommentarer(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def krop_af(s, start):
    """Fra den foerste { til den der lukker den."""
    i = s.index("{", start)
    dybde = 0
    for j in range(i, len(s)):
        if s[j] == "{":
            dybde += 1
        elif s[j] == "}":
            dybde -= 1
            if dybde == 0:
                return s[i:j + 1]
    return s[i:]


def main():
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI} findes ikke")
        return 1

    print("Tjekker at LVGL ikke roeres uden laasen ...")

    # Hvad findes der i brugerfladen, og hvad gør hver funktion.
    funktioner = {}
    for f in sorted(UI.glob("*.c")):
        tekst = f.read_text(encoding="utf-8")
        for m in DEFINITION.finditer(tekst):
            navn = m.group(1)
            if navn in NOEGLEORD:
                continue
            krop = krop_af(tekst, m.start())
            funktioner[navn] = {
                "fil": f.relative_to(ROD),
                "lv": len(LV_KALD.findall(krop)),
                "laas": LAAS in krop,
            }

    # Hvem bliver kaldt fra udenfor brugerfladen.
    kilder = [MAIN] if MAIN.exists() else []
    for d in UDEFRA:
        kilder += sorted(d.glob("*.c"))

    kaldt = {}
    for f in kilder:
        tekst = uden_kommentarer(f.read_text(encoding="utf-8"))
        for navn in funktioner:
            if re.search(r"\b" + re.escape(navn) + r"\s*\(", tekst):
                kaldt.setdefault(navn, []).append(f.relative_to(ROD).name)

    fejl = []
    for navn, d in sorted(funktioner.items()):
        if navn in kaldt and d["lv"] > 0 and not d["laas"]:
            fejl.append(f"{d['fil']}: {navn}() roerer LVGL {d['lv']} "
                        f"sted(er) uden at tage laasen, og kaldes fra "
                        f"{', '.join(kaldt[navn])}")

    if fejl:
        for linje in fejl:
            print(f"  {ROED}FEJL{SLUT} {linje}")
        print()
        print("  Laeg lv_port_sem_take() og lv_port_sem_give() om den del")
        print("  der roerer LVGL. Mutexen er rekursiv, og lv_port_sem_take")
        print("  goer ingenting naar den kaldes fra skaermopgaven selv, saa")
        print("  det er trygt ogsaa naar en kalder allerede holder den.")
        return 1

    roerer = sum(1 for d in funktioner.values() if d["lv"] > 0)
    print(f"  {GROEN} OK{SLUT} {len(funktioner)} funktioner i ui/, {roerer} "
          f"roerer LVGL, {len(kaldt)} kaldes udefra, alle tager laasen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
