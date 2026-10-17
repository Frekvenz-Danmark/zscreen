#!/usr/bin/python3
"""
zScreen - finder hvilken seriel port en given skaerm sidder paa.

SYSTEMETS PYTHON, IKKE "env python3". Det er med vilje.

Homebrews python er i stykker paa den her maskine: plistlib henter
pyexpat, og pyexpat er bygget mod en anden libexpat end den der ligger
i /usr/lib. Det falder med

    Symbol not found: _XML_SetAllocTrackerActivationThreshold

og det siger ingenting om hvad man skal goere. Samme faelde som ved
byggeriet, hvor ESP-IDF skal have /usr/bin foerst.

Og scriptet er macOS-kun i forvejen - det laeser ioreg - saa der er
ingenting tabt ved at pege direkte paa /usr/bin/python3.

    ./tools/find-port.py zdisplay   ->  /dev/cu.usbmodem31201
    ./tools/find-port.py zscreen    ->  /dev/cu.usbserial-1110
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

# Hvad hver skaerm melder sig som, og hvad den HEDDER.
#
# Noeglen er den man skriver paa kommandolinjen, og den er med smaa
# bogstaver fordi den ogsaa er mappenavnet under firmware/boards/.
# Produktnavnet er det mennesker bruger, og det er DET der skrives i
# udskriften - "zscreen" er en mappe, "zScreen" er en skaerm.
SKAERME = {
    "zdisplay": ("ZDisplay", "Espressif", "USB JTAG/serial debug unit"),
    "zscreen":  ("zScreen",  "Seeed",     "INDICATOR RP2040"),
}


# ZSCREENS PROGRAMMERINGSPORT ER EN ANDEN END DEN DER IDENTIFICERER DEN.
#
# docs/hardware.md siger det selv: "Skaermen flashes over UART0, som
# foer - Seeed skriver selv at ESP32'en melder sig som USB-SERIAL
# CH340." ESP32-S3'ens GPIO 19 og 20 gaar til RP2040'en og har INTET
# USB-stik, saa den port RP2040'en melder sig paa, naar ikke ESP32'en.
#
# Her returnerede scriptet RP2040-porten for "zscreen". Maalt 17.
# oktober 2026: esptool read_mac svarede paa cu.usbserial-1110 (CH340,
# ESP32-S3) og slet ikke paa cu.usbmodem11201 (RP2040). Altsaa pegede
# flash-firmware.sh zscreen paa den forkerte chip - og det er praecis
# den slags den her fil findes for at forhindre.
#
# ZDisplay har ikke problemet: ESP32-P4's USB-serial-JTAG er baade det
# der identificerer den og det der flashes over.
#
# CH340'en er en almindelig bro som enhver kan have siddende, saa den
# ALENE beviser ingenting. Derfor kraeves begge: RP2040'en viser at der
# ER en zScreen, og CH340'en er dens vej ind.
PROG_PORT = {
    "zscreen": ("", "USB Serial"),
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
            for noegle, (produkt, vv, pp) in SKAERME.items():
                if v == vv and p == pp:
                    hvem = f"  <- {produkt}  ({noegle})"
                    if noegle in PROG_PORT:
                        hvem += ", men IKKE porten der flashes"
            for noegle, (pv, pp) in PROG_PORT.items():
                if v == pv and p == pp:
                    hvem = f"  <- {SKAERME[noegle][0]}s ESP32, flash her"
            print(f"  {port:28} {v or '?':12} {p or '?'}{hvem}")
        return 0

    board = sys.argv[1]
    if board not in SKAERME:
        print(f"FEJL: kender ikke '{board}'. Der er: "
              f"{', '.join(sorted(SKAERME))}")
        return 2

    navn, vendor, usbnavn = SKAERME[board]
    traef = [port for port, v, p in alle if v == vendor and p == usbnavn]

    if not traef:
        print(f"FEJL: fandt ingen {navn}. Den melder sig som "
              f"{vendor} \"{usbnavn}\".", file=sys.stderr)
        print(f"      Se hvad der sidder i:  {sys.argv[0]} --vis",
              file=sys.stderr)
        return 1
    if len(traef) > 1:
        print(f"FEJL: der sidder {len(traef)} {navn} i:", file=sys.stderr)
        for t in sorted(traef):
            print(f"        {t}", file=sys.stderr)
        print(f"      Sig hvilken i haanden.", file=sys.stderr)
        return 1

    if board in PROG_PORT:
        pv, pp = PROG_PORT[board]
        proge = [port for port, v, p in alle if v == pv and p == pp]
        if not proge:
            print(f"FEJL: fandt en {navn} paa {traef[0]}, men ikke dens "
                  f"programmeringsport.", file=sys.stderr)
            print(f"      ESP32'en melder sig som \"{pp}\" - se "
                  f"docs/hardware.md.", file=sys.stderr)
            print(f"      RP2040-porten ovenfor naar IKKE ESP32'en.",
                  file=sys.stderr)
            print(f"      Mange USB-C-kabler kan kun stroem. Proev et andet,",
                  file=sys.stderr)
            print(f"      eller den anden port paa bagsiden.", file=sys.stderr)
            return 1
        if len(proge) > 1:
            print(f"FEJL: der sidder {len(proge)} \"{pp}\" i, og de ser "
                  f"ens ud udefra:", file=sys.stderr)
            for t in sorted(proge):
                print(f"        {t}", file=sys.stderr)
            print(f"      Sig hvilken i haanden - der gaettes ikke paa en "
                  f"port der flashes.", file=sys.stderr)
            return 1
        print(proge[0])
        return 0

    print(traef[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
