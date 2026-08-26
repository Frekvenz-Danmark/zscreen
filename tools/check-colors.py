#!/usr/bin/env python3
"""
zScreen - tjekker farverne.

Tre ting, og de faejler alle sammen bygningen:

  1. Begge paletter skal have en vaerdi for hvert eneste farvenavn.
     Mangler én, giver LVGL sort paa sort uden at sige noget.

  2. Kontrasten skal holde. Tallene er WCAG 2.1: 4,5:1 for tekst,
     3,0:1 for det der kun er en flade eller en streg. Det er ikke
     pedanteri: skaermen haenger paa en vaeg og bliver set paa afstand,
     og i modlys fra et vindue er det den svageste kombination der
     afgoer om man kan aflaese sit forbrug.

  3. Ingen raa farvekode i brugerfladen. Alt skal gaa gennem paletten,
     ellers findes der en farve der ikke skifter med temaet, og den
     bliver foerst opdaget af en kunde.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
TEMA_C = ROD / "firmware/main/ui/zs_theme.c"
TEMA_H = ROD / "firmware/main/ui/zs_theme.h"
UI = ROD / "firmware/main/ui"

GROEN = "\033[1;32m"
ROED = "\033[1;31m"
GUL = "\033[1;33m"
SLUT = "\033[0m"

fejl = []


def lin(k):
    k = k / 255
    return k / 12.92 if k <= 0.03928 else ((k + 0.055) / 1.055) ** 2.4


def lum(h):
    return (0.2126 * lin((h >> 16) & 255)
            + 0.7152 * lin((h >> 8) & 255)
            + 0.0722 * lin(h & 255))


def kontrast(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


# --- 1. Laes navnene og de to paletter -----------------------------------
navne = re.findall(r"^\s*(ZS_ID_[A-Z_]+)\s*(?:=\s*\d+\s*)?,", TEMA_H.read_text(), re.M)
navne = [n for n in navne if n != "ZS_ID_COUNT"]

kilde = TEMA_C.read_text()

# Temaerne laeses ud af opregningen, ikke skrevet i en liste her.
# Ellers kan et nyt tema blive tilfoejet uden at nogen opdager at det
# aldrig blev maalt.
temaer = [t for t in re.findall(r"^\s*(ZS_THEME_[A-Z]+)\s*(?:=\s*\d+\s*)?,",
                                TEMA_H.read_text(), re.M)
          if t != "ZS_THEME_COUNT"]
if not temaer:
    fejl.append("kunne ikke finde temaerne i zs_theme.h")

paletter = {}
for tema in temaer:
    m = re.search(r"\[" + tema + r"\]\s*=\s*\{(.*?)\n    \},", kilde, re.S)
    if m is None:
        fejl.append(f"paletten {tema} findes ikke i zs_theme.c")
        continue
    p = {}
    for navn, vaerdi in re.findall(r"\[(ZS_ID_[A-Z_]+)\]\s*=\s*([^,]+),", m.group(1)):
        vaerdi = vaerdi.strip()
        if vaerdi == "ZS_BRAND_ORANGE":
            vaerdi = "0xFBAC18"
        elif vaerdi == "ZS_BRAND_GREEN":
            vaerdi = "0x174A48"
        p[navn] = int(vaerdi, 16)
    paletter[tema] = p

for tema, p in paletter.items():
    mangler = [n for n in navne if n not in p]
    if mangler:
        fejl.append(f"{tema} mangler: {', '.join(mangler)}")

# --- 2. Kontrasten -------------------------------------------------------
# Hvad hver farve bruges til, og hvad den derfor skal kunne.
#   tekst   laeses som ord og tal        4,5:1
#   flade   en streg eller en flade      3,0:1
#   daempet med vilje svag, men synlig   3,0:1
BRUG = {
    "ZS_ID_TEXT":     ("tekst",  4.5),
    "ZS_ID_TEXT_DIM": ("tekst",  4.5),
    "ZS_ID_LABEL":    ("tekst",  4.5),
    "ZS_ID_VALUE":    ("tekst",  4.5),
    "ZS_ID_ACCENT":   ("tekst",  4.5),
    "ZS_ID_GOOD":     ("tekst",  4.5),
    "ZS_ID_BAD":      ("tekst",  4.5),
    "ZS_ID_WARN":     ("tekst",  4.5),
    "ZS_ID_STALE":    ("daempet", 3.0),
}

print("Tjekker farverne ...")
for tema, p in paletter.items():
    if not p:
        continue
    kort = p["ZS_ID_CARD"]
    bund = p["ZS_ID_BG"]

    for navn, (slags, mindst) in BRUG.items():
        for flade, fnavn in ((kort, "kort"), (bund, "bund")):
            k = kontrast(p[navn], flade)
            if k < mindst:
                fejl.append(
                    f"{tema} {navn} paa {fnavn}: {k:.2f}:1, "
                    f"skal mindst vaere {mindst:.2f}:1 ({slags})")

    # Maerkerne i toplinjen.
    #
    # Et maerke er en fyldt flade med et ord paa. To ting skal holde,
    # og de er ikke det samme:
    #
    #   ordet skal kunne laeses paa fladen          4,50:1
    #   fladen skal kunne ses mod siden bagved      3,00:1
    #
    # Foer blev der kun tjekket ét tal, fordi skriften altid var
    # bundfarven. Med et tredje tema holder den antagelse ikke, og
    # skriftfarven er nu en del af paletten.
    maerketekst = p["ZS_ID_BADGE_TEXT"]
    for navn in ("ZS_ID_GOOD", "ZS_ID_BAD", "ZS_ID_WARN",
                 "ZS_ID_ACCENT", "ZS_ID_LABEL"):
        k = kontrast(p[navn], maerketekst)
        if k < 4.5:
            fejl.append(f"{tema} ordet paa et {navn}-maerke: {k:.2f}:1, "
                        f"skal vaere 4,50:1")
        k = kontrast(p[navn], bund)
        if k < 3.0:
            fejl.append(f"{tema} et {navn}-maerke kan ikke ses mod siden: "
                        f"{k:.2f}:1, skal vaere 3,00:1")

    # Kortet skal kunne ses mod baggrunden, og kanten mod kortet.
    if kontrast(kort, bund) < 1.03:
        fejl.append(f"{tema} kortet kan ikke ses mod baggrunden: "
                    f"{kontrast(kort, bund):.2f}:1")
    if kontrast(p["ZS_ID_BORDER"], kort) < 1.25:
        fejl.append(f"{tema} kanten kan ikke ses paa kortet: "
                    f"{kontrast(p['ZS_ID_BORDER'], kort):.2f}:1")

# --- 3. Ingen raa farvekode udenfor paletten -----------------------------
# lv_color_hex(0x...) og lignende. zs_theme.c er stedet hvor tallene
# HOERER hjemme, saa den er undtaget.
raa = []
for f in sorted(UI.rglob("*.c")) + sorted(UI.rglob("*.h")):
    if "assets" in f.parts or f.name in ("zs_theme.c", "zs_theme.h"):
        continue
    for nr, linje in enumerate(f.read_text().splitlines(), 1):
        if re.search(r"lv_color_hex\s*\(\s*0x[0-9a-fA-F]{6}", linje) \
           or re.search(r"lv_color_make\s*\(", linje):
            raa.append(f"{f.relative_to(ROD)}:{nr}: {linje.strip()}")
if raa:
    fejl.append("raa farvekoder udenfor paletten:\n      "
                + "\n      ".join(raa))

# --- Resultat ------------------------------------------------------------
if fejl:
    for f in fejl:
        print(f"  {ROED}FEJL{SLUT} {f}")
    sys.exit(1)

antal = len(navne)
print(f"  {GROEN} OK{SLUT} {len(paletter)} paletter a {antal} farver, "
      f"kontrasten holder, ingen raa farvekoder.")
