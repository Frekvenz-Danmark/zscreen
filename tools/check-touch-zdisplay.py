#!/usr/bin/env python3
"""
zScreen - sig fra naar Seeed retter i den touch-driver vi har en rettet
kopi af.

HVORFOR DEN FINDES.

firmware/boards/zdisplay/components/zd_touch/esp_lcd_touch_gsl3670.c er
en kopi af Seeeds fil med tre bufferfejl rettet. Se hovedet af den fil.

En kopi er en gaeld. Retter Seeed noget i originalen - en rigtig
rettelse, en ny kreds, en ny registeradresse - saa faar vi det ikke, og
ingen opdager det. Derfor staar deres fils fingeraftryk her. Skifter
det, skal nogen laese deres aendringer igennem og afgoere:

  * er vores tre rettelser blevet unoedvendige, saa kopien kan smides
    vaek og deres komponent bruges direkte igen?
  * eller skal rettelserne laegges oven paa deres nye udgave?

Og saa skrives det nye fingeraftryk ind her.

Koeres af tools/check-alle.sh.
"""
import hashlib
import pathlib
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

DERES = ROD / ("firmware/vendor/reTerminal_D1001/components/"
               "esp_lcd_touch_gsl3670/esp_lcd_touch_gsl3670.c")
VORES = ROD / ("firmware/boards/zdisplay/components/zd_touch/"
               "esp_lcd_touch_gsl3670.c")

# Fingeraftrykket paa den udgave vi forkede fra, 14. oktober 2026.
FORKET_FRA = "c9e278bcec2cc59533092354b05020adcb723e9bad10e4b9a36d01c52acf2254"


def main():
    print("Tjekker den rettede touch-driver ...")

    if not DERES.exists():
        print(f"  {ROED}FEJL{SLUT} {DERES.relative_to(ROD)} findes ikke. "
              f"Mangler submodulet?")
        return 1
    if not VORES.exists():
        print(f"  {ROED}FEJL{SLUT} {VORES.relative_to(ROD)} findes ikke. "
              f"Er kopien slettet? Saa bygger ZDisplay med en driver der "
              f"skriver ud over tre buffere.")
        return 1

    nu = hashlib.sha256(DERES.read_bytes()).hexdigest()
    if nu != FORKET_FRA:
        print(f"  {ROED}FEJL{SLUT} Seeed har rettet i deres "
              f"esp_lcd_touch_gsl3670.c.")
        print(f"         forket fra: {FORKET_FRA}")
        print(f"         nu:         {nu}")
        print(f"       Laes deres aendringer igennem. Er de tre "
              f"bufferfejl rettet hos dem, saa smid kopien vaek og brug "
              f"deres komponent. Ellers laeg rettelserne oven paa den "
              f"nye udgave og skriv det nye fingeraftryk i "
              f"{pathlib.Path(__file__).name}.")
        return 1

    # Og at vores kopi faktisk ER rettet. En kopi uden rettelserne er
    # det daarligste af begge verdener: vi slaeber en fil rundt OG har
    # fejlen.
    vores = VORES.read_text(encoding="utf-8")
    mangler = [k for k, v in (
        ("den stoerre buffer",      "uint8_t touch_data[48]"),
        ("tjekket af laesefejlen",  "if (err != ESP_OK) {"),
        ("graensen paa ti fingre",  "if (Finger_num > 10) {"),
        ("graensen paa XY-feltet",  "if (Finger_num > MAX_FINGER_NUM) {"),
    ) if v not in vores]
    if mangler:
        for m in mangler:
            print(f"  {ROED}FEJL{SLUT} kopien mangler {m}")
        return 1

    print(f"  {GROEN} OK{SLUT} kopien er rettet paa fire punkter, og Seeeds "
          f"original er uaendret siden vi forkede.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
