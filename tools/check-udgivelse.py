#!/usr/bin/env python3
"""
zScreen - tjekker at vi SENDER det vi har TESTET.

HVORFOR DEN FINDES.

Vi har to arbejdsgange: test.yml proever hver aendring, og release.yml
bygger den fil skaermene henter. Bygger de to med hver sin ESP-IDF, saa
tester vi med én oversaetter og sender en anden ud til kunderne.

Det er ikke teoretisk. Da ESP-IDF blev loeftet fra 5.1 til 5.3 blev
test.yml rettet og release.yml glemt. Forskellen mellem de to
oversaettere er maalt til naesten tredive kilobyte i den faerdige fil, og
GCC 13 fanger fejl GCC 12 lader ligge. Saa den fil kunderne ville have
faaet, var bygget af en oversaetter ingen test havde set.

Scriptet tjekker ogsaa at det IKKE er et flydende maerke som "latest"
eller "release-v5.3": en udgivelse skal kunne bygges igen om et aar og
give det samme.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

BILLEDE = re.compile(r'^\s*container:\s*(\S+)\s*$', re.M)
# v5.3.6 er fast. v5.3, release-v5.3 og latest flytter sig.
FAST = re.compile(r'^espressif/idf:v\d+\.\d+\.\d+$')


def main():
    print("Tjekker at vi sender det vi har testet ...")
    fundet = {}
    for navn in ("test.yml", "release.yml"):
        sti = ROD / ".github/workflows" / navn
        if not sti.exists():
            print(f"  {ROED}FEJL{SLUT} {navn} findes ikke")
            return 1
        billeder = set(BILLEDE.findall(sti.read_text(encoding="utf-8")))
        if not billeder:
            print(f"  {ROED}FEJL{SLUT} {navn} bygger ikke i nogen container")
            return 1
        if len(billeder) > 1:
            print(f"  {ROED}FEJL{SLUT} {navn} bruger flere billeder: "
                  f"{sorted(billeder)}")
            return 1
        fundet[navn] = billeder.pop()

    fejl = []
    if fundet["test.yml"] != fundet["release.yml"]:
        fejl.append(f"test.yml bygger med {fundet['test.yml']} men "
                    f"release.yml med {fundet['release.yml']}")
    for navn, b in fundet.items():
        if not FAST.match(b):
            fejl.append(f"{navn} bruger {b}, som kan flytte sig. "
                        f"Skriv en fast version som espressif/idf:v5.3.6, "
                        f"saa en udgivelse kan bygges igen om et aar")

    if fejl:
        for f in fejl:
            print(f"  {ROED}FEJL{SLUT} {f}")
        return 1

    print(f"  {GROEN} OK{SLUT} begge bygger med {fundet['test.yml']}, "
          f"og versionen er fast.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
