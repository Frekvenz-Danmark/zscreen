#!/usr/bin/env python3
"""
Kan tastaturet skrive et hvilket som helst wifi-kodeord?

HVORFOR DEN FINDES.

Tasten "#+=" var en TOM tast i tolv dage. Den stod i gitteret, den kunne
trykkes, og der skete ingenting - med en kommentar i koden om at laget
var "reserveret til flere tegn". Resultatet var at

    * + = # < > [ ] { } \\ | ^ ~ `

slet ikke kunne tastes. Et wifi-kodeord med en stjerne i kunne altsaa
ikke skrives, og montoeren havde ingen vej videre. Det er en spaerre,
ikke en mangel - og ingenting sagde fra.

WPA2 tillader hele det synlige ASCII, 33 til 126, plus mellemrum. Den
her vagt laeser de fire lag ud af zs_keyboard.c og tjekker at hvert
eneste af dem findes paa en tast. Saa kan et lag ikke aendres uden at et
tabt tegn bliver fanget her.

DEN LAESER KILDEN OG KENDER IKKE LAGENE SELV.

Stod listen over tegn ogsaa her, ville den skulle holdes i takt med
koden - og saa er der to steder at glemme noget. Den finder lagene ved
deres navne i filen.

    python3 tools/check-tastatur.py
"""
import os
import re
import sys

HER = os.path.dirname(os.path.abspath(__file__))
ROD = os.path.dirname(HER)
KB = os.path.join(ROD, "firmware/kerne/ui/widgets/zs_keyboard.c")

GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# Taster der er en HANDLING og ikke et tegn. De staar som #define i
# filen, og deres vaerdier skal ikke taelle som skrivbare tegn.
HANDLINGER = ("K_SHIFT", "K_DEL", "K_OK", "K_NUM", "K_ABC", "K_SYM")


def lagene(kilde):
    """{navn: [taster]} for hvert map_* i filen."""
    ud = {}
    for m in re.finditer(r"static const char \*(map_\w+)\[\] = \{(.*?)\};",
                         kilde, re.S):
        navn, krop = m.group(1), m.group(2)
        taster = []
        for t in re.finditer(r'"((?:[^"\\]|\\.)*)"', krop):
            raa = t.group(1)
            if raa in ("\\n", ""):
                continue
            # C-escapes tilbage til tegn: \\ er en backslash, \" er et
            # anfoerselstegn.
            taster.append(raa.replace('\\\\', '\\').replace('\\"', '"'))
        # Og de navngivne taster, som ikke staar i anfoerselstegn.
        for h in HANDLINGER:
            if re.search(rf"\b{h}\b", krop):
                taster.append(f"<{h}>")
        ud[navn] = taster
    return ud


def main():
    if not os.path.exists(KB):
        print(f"  {ROED}FEJL{SLUT} {KB} findes ikke")
        return 1
    kilde = open(KB, encoding="utf-8").read()

    print("Tjekker at tastaturet kan skrive alle lovlige kodeord ...")
    lag = lagene(kilde)
    if not lag:
        print(f"  {ROED}FEJL{SLUT} fandt ingen map_* i {os.path.basename(KB)}")
        return 1

    #
    # EN TAST DER SKRIVER, SKRIVER ÉT TEGN.
    #
    # Det var netop her fejlen gemte sig: "#+=" stod som en almindelig
    # streng i gitteret, saa den SAA ud som en tast der skrev tre tegn -
    # og den skrev ingenting. Havde den her vagt kun talt tegn, ville
    # den have meldt #, + og = som daekkede, fordi de stod i en etiket.
    #
    # Derfor: en streng paa mere end ét tegn er en ETIKET og ikke et
    # tegn. Den skal vaere en navngivet tast - K_NUM, K_SYM, K_ABC - saa
    # on_key kan genkende den, og saa den ikke kan komme til at taelle
    # som noget man kan skrive.
    etiketter = []
    kan = set()
    for navn, taster in lag.items():
        for t in taster:
            if t.startswith("<") and t.endswith(">"):
                continue
            if len(t) != 1:
                etiketter.append((navn, t))
                continue
            kan.add(t)
    # Mellemrum kommer fra K_SPACE, som er " ".
    kan.add(" ")

    for navn in sorted(lag):
        rigtige = [t for t in lag[navn] if not t.startswith("<")]
        print(f"  {navn:<12} {len(lag[navn]):>2} taster, "
              f"{len(rigtige):>2} der skriver noget")

    if etiketter:
        print()
        print(f"  {ROED}FEJL{SLUT} {len(etiketter)} tast(er) er en tekst og "
              f"ikke ét tegn:")
        for navn, t in etiketter:
            print(f"        {navn}: \"{t}\"")
        print()
        print("  En tast med mere end ét tegn er en ETIKET. Den skal vaere")
        print("  en navngivet tast - se K_NUM, K_SYM og K_ABC - saa on_key")
        print("  kan genkende den. Ellers staar den i gitteret og ser ud")
        print("  som en tast der skriver, mens den ikke goer noget.")
        return 1

    # WPA2: synligt ASCII plus mellemrum.
    skal = {chr(c) for c in range(32, 127)}
    mangler = sorted(skal - kan)
    print()
    if mangler:
        print(f"  {ROED}FEJL{SLUT} {len(mangler)} tegn kan ikke tastes:")
        print("        " + " ".join(mangler))
        print()
        print("  WPA2 tillader hele det synlige ASCII. Et tegn der ikke")
        print("  kan tastes, er et kodeord der ikke kan skrives - og")
        print("  montoeren kan ikke komme videre.")
        return 1

    # Og hvert lag skal have en vej VIDERE. Et lag man sidder fast i, er
    # lige saa spaerrende som et tegn der mangler.
    fejl = 0
    for navn, taster in sorted(lag.items()):
        ud_af = [t for t in taster
                 if t in ("<K_ABC>", "<K_NUM>", "<K_SYM>", "<K_SHIFT>")]
        if not ud_af:
            print(f"  {ROED}FEJL{SLUT} laget {navn} har ingen vej til et "
                  f"andet lag")
            fejl += 1

    if fejl:
        return 1
    print(f"  {GROEN} OK{SLUT} {len(lag)} lag, alle {len(skal)} tegn i "
          f"synligt ASCII kan tastes, og hvert lag har en vej videre.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
