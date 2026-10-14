#!/usr/bin/env python3
"""
zScreen - finder hvilken seriel port en given skaerm sidder paa.

    ./tools/find-port.py zdisplay   ->  /dev/cu.usbmodem31201
    ./tools/find-port.py zscreen    ->  /dev/cu.usbmodem11201
    ./tools/find-port.py --vis      ->  alt hvad der sidder i

HVORFOR DEN FINDES.

Med begge skaerme i den samme Mac heder portene noget i retning af
usbmodem11201 og usbmodem31201, og der er ingenting i de navne der
siger hvad der sidder for enden. Det er ikke en bekvemmelighed: flasher
man ZDisplays firmware til en zScreen, skal den have besoeg bagefter.

De to melder sig forskelligt over USB, og DET kan laeses uden at
nulstille noget:

    zScreen    Seeed      "INDICATOR RP2040"
               Skaermens USB gaar gennem RP2040'eren, ikke gennem
               ESP32-S3'eren. Se noten om summeren i docs/hardware.md.
    ZDisplay   Espressif  "USB JTAG/serial debug unit"
               ESP32-P4 har USB-serial-JTAG indbygget.

GRAENSER. Sidder der TO af samme slags, kan den ikke vaelge, og saa
siger den fra med begge porte i stedet for at gaette. Og en skaerm i
download-tilstand kan melde sig anderledes - staar der ingenting, saa
tag porten fra `--vis` og giv den med i haanden.
"""

import plistlib
import subprocess
import sys

# Hvad hver skaerm melder sig som. Navnet er USB Product Name.
SKAERME = {
    "zdisplay": ("Espressif", "USB JTAG/serial debug unit"),
    "zscreen":  ("Seeed",     "INDICATOR RP2040"),
}


def portene():
    """[(port, vendor, produkt)] for alt der har en seriel port."""
    r = subprocess.run(["ioreg", "-a", "-l", "-r", "-c", "IOUSBHostDevice"],
                       capture_output=True)
    if r.returncode != 0 or not r.stdout:
        return []
    try:
        traeet = plistlib.loads(r.stdout)
    except Exception:
        return []

    ud = []

    def gaa(n, produkt=None, vendor=None):
        # Navnet staar paa USB-enheden, og porten paa et barn laengere
        # nede. Derfor baeres de med ned gennem traeet.
        p = n.get("USB Product Name", produkt)
        v = n.get("USB Vendor Name", vendor)
        co = n.get("IOCalloutDevice")
        if co:
            ud.append((co, v or "", p or ""))
        for b in n.get("IORegistryEntryChildren") or []:
            gaa(b, p, v)

    for n in traeet:
        gaa(n)
    return ud


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    alle = portene()

    if sys.argv[1] in ("--vis", "-v"):
        if not alle:
            print("  der sidder ingenting i, eller ioreg svarede ikke")
            return 1
        for port, v, p in sorted(alle):
            hvem = ""
            for navn, (vv, pp) in SKAERME.items():
                if v == vv and p == pp:
                    hvem = f"  <- {navn}"
            print(f"  {port:28} {v or '?':12} {p or '?'}{hvem}")
        return 0

    board = sys.argv[1]
    if board not in SKAERME:
        print(f"FEJL: kender ikke '{board}'. Der er: "
              f"{', '.join(sorted(SKAERME))}")
        return 2

    vendor, produkt = SKAERME[board]
    traef = [port for port, v, p in alle if v == vendor and p == produkt]

    if not traef:
        print(f"FEJL: fandt ingen {board}. Den melder sig som "
              f"{vendor} \"{produkt}\".", file=sys.stderr)
        print(f"      Se hvad der sidder i:  {sys.argv[0]} --vis",
              file=sys.stderr)
        return 1
    if len(traef) > 1:
        print(f"FEJL: der sidder {len(traef)} {board} i:", file=sys.stderr)
        for t in sorted(traef):
            print(f"        {t}", file=sys.stderr)
        print(f"      Sig hvilken i haanden.", file=sys.stderr)
        return 1

    print(traef[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
