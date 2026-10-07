#!/usr/bin/env python3
"""
zScreen - tjekker at ingen side er en blindgyde.

HVORFOR DEN FINDES.

Skaermen haenger paa en vaeg og har kun én slags input: en finger. Lander
en kunde paa en side uden vej tilbage, er der ingen tast at trykke paa,
ingen mus og ingen menu. Saa er skaermen i praksis gaaet i staa, og den
eneste udvej er at tage stroemmen.

Den fangede én: tilbage fra netvaerkslisten foerte til velkomstsiden, hvor
den eneste knap er "Kom i gang", som foerer tilbage til netvaerkslisten.
En kunde der trykkede paa Netvaerk i indstillingerne og fortrod, kunne
derfor ikke komme hjem igen uden at gaa hele opsaetningen igennem,
inklusive en ny soegning efter inverteren paa op til fire minutter.

REGLEN: hver side der laves med zs_page_create SKAL have en tilbage-knap.
Undtagelserne staar herunder, med en grund hver.

GRAENSER. Den laeser teksten, ikke programmet. Den kan se at der ER en
tilbage-knap, ikke at den foerer et fornuftigt sted hen. Den er en bund,
ikke et bevis.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
UI = ROD / "firmware/main/ui"

GROEN = "\033[1;32m"
ROED = "\033[1;31m"
SLUT = "\033[0m"

# Sider der med vilje ikke har en tilbage-knap, og hvorfor.
UNDTAGELSER = {
    "": "velkomstsiden: der er ingenting foer den, og den har "
        "\"Kom i gang\" fremad",
}

# zs_page_create(&var, "Titel", tilbage_cb, ...)
SIDE = re.compile(
    r'zs_page_create\(\s*&(\w+)\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,')


def main():
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI} findes ikke")
        return 1

    print("Tjekker at ingen side er en blindgyde ...")

    sider = []
    for f in sorted(UI.glob("zs_screen_*.c")):
        tekst = f.read_text(encoding="utf-8")
        for m in SIDE.finditer(tekst):
            nr = tekst[:m.start()].count("\n") + 1
            sider.append({
                "fil": f.relative_to(ROD),
                "linje": nr,
                "var": m.group(1),
                "titel": m.group(2),
                "tilbage": m.group(3),
            })

    if not sider:
        print(f"  {ROED}FEJL{SLUT} fandt ingen sider. Er zs_page_create "
              f"blevet omdoebt?")
        return 1

    fejl = []
    undtaget = 0
    for s in sider:
        if s["tilbage"] != "NULL":
            continue
        if s["titel"] in UNDTAGELSER:
            undtaget += 1
            continue
        fejl.append(f"{s['fil']}:{s['linje']}: siden \"{s['titel']}\" har "
                    f"ingen tilbage-knap, og staar ikke paa listen over "
                    f"undtagelser")

    if fejl:
        for linje in fejl:
            print(f"  {ROED}FEJL{SLUT} {linje}")
        print()
        print("  Giv siden en tilbage-knap, eller skriv den paa UNDTAGELSER")
        print("  i det her script MED en grund. En side uden vej tilbage er")
        print("  en skaerm der er gaaet i staa for den der staar foran den.")
        return 1

    print(f"  {GROEN} OK{SLUT} {len(sider)} sider, {len(sider) - undtaget} "
          f"med tilbage-knap, {undtaget} undtaget med en grund.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
