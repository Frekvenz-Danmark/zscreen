#!/usr/bin/env python3
"""
zScreen - tjekker at dokumentationen passer med koden.

HVORFOR DEN FINDES.

Dokumentation er det eneste i repoet som intet proever af. Koden har
tests, farverne har en vagt, layoutet har en, men en tabel i en md-fil
kan staa forkert i et aar uden at nogen opdager det.

Og netop registertabellen i docs/fronius-modbus.md er farlig at tage
fejl af: den er det man slaar op i naar man staar foran et rigtigt
anlaeg. Et forkert offset dér sender en paa vildspor i timevis.

Den tjekker tre ting, og kun ting der KAN tjekkes:

  1. Hver raekke i registertabellen mod konstanterne i zs_sunspec.h
  2. Hver ZS_-konstant der naevnes findes faktisk
  3. Hver filhenvisning peger paa en fil der findes

Den kan IKKE tjekke om teksten er sand. Det maa et menneske laese.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# Dokumentationens navne for felterne, oversat til vores konstanter.
NAVNE = {
    "Mn": "MN", "Md": "MD", "Vr": "VR", "SN": "SN",
    "W": "W", "W_SF": "W_SF", "WHRtg": "WHRTG",
    "ChaState": "CHA_STATE", "ChaSt": "CHA_ST",
    "ChaState_SF": "CHA_STATE_SF",
    "DCW_SF": "DCW_SF", "N": "N",
}

# Projekter der ligger ved siden af, ikke i vores repo.
UDENFOR = ("DESIGN.txt", "modbus_controller.py")


def konstanter():
    """Alle ZS_-konstanter med et tal, fra hele firmwaren."""
    ud = {}
    for f in (ROD / "firmware/main").rglob("*.h"):
        if "assets" in str(f):
            continue
        for m in re.finditer(r"#define\s+(ZS_\w+)\s+\(?\s*(-?\d+)\s*\)?",
                             f.read_text(encoding="utf-8")):
            ud[m.group(1)] = int(m.group(2))
    return ud


def tabelraekker(tekst):
    """| 103 | W effekt | 12 | int16 |  ->  (103, 'W', 12)"""
    ud = []
    for nr, l in enumerate(tekst.split("\n"), 1):
        m = re.match(r"\s*\|\s*(\d+)\s*\|\s*\**([A-Za-z_]+)[^|]*\|"
                     r"\s*\**(\d+)\**\s*\|", l)
        if m:
            ud.append((nr, int(m.group(1)), m.group(2), int(m.group(3))))
    return ud


def main():
    print("Tjekker at dokumentationen passer med koden ...")
    kon = konstanter()
    fejl = []

    # 1. Registertabellen
    sti = ROD / "docs/fronius-modbus.md"
    raekker = 0
    if sti.exists():
        for nr, mdl, felt, off in tabelraekker(sti.read_text(encoding="utf-8")):
            navn = NAVNE.get(felt)
            if navn is None:
                continue          # en raekke vi ikke kan slaa op, fx "første kanal"
            key = f"ZS_M{mdl}_{navn}"
            raekker += 1
            if key not in kon:
                fejl.append(f"docs/fronius-modbus.md:{nr}: {key} findes ikke "
                            f"i koden, men tabellen naevner model {mdl} {felt}")
            elif kon[key] != off:
                fejl.append(f"docs/fronius-modbus.md:{nr}: tabellen siger "
                            f"model {mdl} {felt} = {off}, men {key} er "
                            f"{kon[key]}")

    # 2. og 3. Konstanter og filer der naevnes
    doks = [ROD / "README.md"] + sorted((ROD / "docs").glob("*.md"))
    nk = nf = 0
    for d in doks:
        for nr, l in enumerate(d.read_text(encoding="utf-8").split("\n"), 1):
            for m in re.finditer(r"`(ZS_[A-Z0-9_]+)`", l):
                nk += 1
                navn = m.group(1)
                if navn not in kon and not any(
                        (ROD / "firmware/main").rglob("*.h")
                        and navn in f.read_text(encoding="utf-8")
                        for f in (ROD / "firmware/main").rglob("*.h")):
                    fejl.append(f"{d.name}:{nr}: {navn} findes ikke i koden")
            for m in re.finditer(
                    r"`([a-zA-Z0-9_./-]+\.(?:c|h|py|sh|yml|csv|txt|json|pem))`", l):
                p = m.group(1)
                nf += 1
                if Path(p).name in UDENFOR:
                    continue
                if any((ROD / k).exists() for k in
                       (p, f"firmware/main/{p}", f"firmware/{p}",
                        f"tools/{p}", f"tests/{p}", f"docs/{p}")):
                    continue
                if list(ROD.rglob(Path(p).name)):
                    continue
                fejl.append(f"{d.name}:{nr}: filen {p} findes ikke")

    if fejl:
        for f in fejl:
            print(f"  {ROED}FEJL{SLUT} {f}")
        return 1
    print(f"  {GROEN} OK{SLUT} {raekker} registerrækker, {nk} konstanter og "
          f"{nf} filhenvisninger passer med koden.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
