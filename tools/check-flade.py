#!/usr/bin/env python3
"""
zScreen - ingen raa pixeltal i fladens geometri.

HVORFOR DEN FINDES.

Fladen blev tegnet paa 480 x 480, og tallene stod som de var: 14 px her,
26 px der, "(ZS_BAR_HEIGHT - 20) / 2" for at centrere noget der var 20
px hoejt. Det virkede paa den skaerm de blev skrevet til.

Da den samme flade blev bygget til 800 x 1280, fulgte kasserne med og
tallene inde i dem stod stille. Maalt paa statuslinjen:

  * maerket "Forbundet" startede paa x = 14 + 26 + 10 = 50, mens logoet
    fyldte 14..57. Det OVERLAPPEDE logoet med 7 px
  * wifi-ikonet var centreret som om det var 20 px hoejt. Det er 33, saa
    det sad 6 px FOR LAVT
  * tandhjulet var 44 x 44 i en linje paa 73, placeret i y = 0, altsaa
    FOR HOEJT - og det raakte 19 px ud over hoejre kant, fordi x blev
    regnet med 44 og knappen tegnet med 73
  * klokkeslaettet stod i y = 10, midten af en linje der ikke findes
    laengere

Alle fire er samme fejl: et tal der kun gaelder én skaerm, skrevet ind i
et kald. Vagten her finder dem FOER de bliver til noget man kan se.

REGLEN: et tal i et geometri-kald skal komme fra zs_layout.h eller vaere
pakket i ZS_G_SK, ZS_G_SK_WIDTH eller ZS_G_SK_HEIGHT. 0, 1 og 2 er
undtaget - de er sjaeldent et maal, og en ramme paa 1 px er en ramme paa
1 px paa enhver skaerm.

Koeres af tools/check-alle.sh.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
UI = ROD / "firmware/kerne/ui"
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# De kald hvor et tal ER et maal paa skaermen.
KALD = re.compile(
    r"\b(lv_obj_set_pos|lv_obj_set_size|lv_obj_set_width|lv_obj_set_height"
    r"|lv_obj_align|lv_obj_align_to|lv_obj_set_style_pad_\w+"
    r"|lv_obj_set_style_radius|lv_obj_set_style_border_width"
    r"|lv_obj_set_style_translate_\w+|lv_obj_set_style_size"
    r"|lv_obj_set_style_line_width|lv_obj_set_style_outline_width"
    # Dertil de maal der ogsaa er pixels, og som blev glemt foerste
    # gang: buens tykkelse paa en spinder, skyggen, afstanden mellem
    # bogstaver og mellem linjer, og graenserne for et objekts stoerrelse.
    r"|lv_obj_set_style_arc_width|lv_obj_set_style_shadow_\w+"
    r"|lv_obj_set_style_text_letter_space|lv_obj_set_style_text_line_space"
    r"|lv_obj_set_style_max_width|lv_obj_set_style_min_width"
    r"|lv_obj_set_style_max_height|lv_obj_set_style_min_height)\s*\(",
    re.S)
TAL = re.compile(r"(?<![\w.])(\d+)(?![\w.])")

# 0, 1 og 2 er ikke maal man skalerer.
SMAA = {0, 1, 2}

# Det der er pakket ind, eller kommer fra layoutet, taeller ikke med.
FRI = (
    # PROCENT ER IKKE PIXELS, og den her skal staa FOERST.
    #
    # LV_PCT(100) betyder "hele foraeldrens bredde". Moensteret for
    # LV_-navne nedenfor fjerner macroens NAVN og lader argumentet staa,
    # saa 100 saa ud som et raa pixeltal. Det blev pakket ind, og 100
    # procent blev 166 procent - et kort halvanden gang bredere end
    # siden. Raekkefoelgen her ER rettelsen.
    re.compile(r"\b(?:LV_PCT|lv_pct)\s*\([^()]*\)"),
    re.compile(r"ZS_G_SK(?:_WIDTH|_HEIGHT)?\([^()]*(?:\([^()]*\)[^()]*)*\)"),
    # ET TAL FORAN ET GANGETEGN ER EN FAKTOR, IKKE EN LAENGDE.
    #
    # "ZS_SCR_WIDTH - 4 * ZS_EDGE" betyder "fire kanter". Da de raa tal
    # blev pakket ind, blev de fire til ZS_G_SK_WIDTH(4), altsaa SEKS
    # kanter paa den store skaerm - en bredde der var 2 kanter for lille.
    # Fire af dem stod i koden. Tallet er et ANTAL, og antal skaleres
    # ikke; det er ZS_EDGE selv der goer det.
    re.compile(r"\b\d+\s*\*\s*ZS_[A-Z_0-9]+"),
    re.compile(r"ZS_[A-Z_0-9]+\s*\*\s*\d+\b"),
    re.compile(r"\bZS_[A-Z_0-9]+\b"),
    re.compile(r"\bLV_[A-Z_0-9]+\b"),
)

fejl = []


def argumenter(tekst, i):
    """Teksten mellem parenteserne i et kald der starter paa i."""
    dybde = 0
    start = tekst.index("(", i)
    for j in range(start, len(tekst)):
        if tekst[j] == "(":
            dybde += 1
        elif tekst[j] == ")":
            dybde -= 1
            if dybde == 0:
                return tekst[start + 1:j]
    return ""


def uden_kommentarer(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def main():
    print("Tjekker at fladen ikke har raa pixeltal ...")
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI.relative_to(ROD)} findes ikke")
        return 1

    kald = 0
    for f in sorted(UI.rglob("*.c")):
        if "/assets/" in f.as_posix():
            continue
        tekst = uden_kommentarer(f.read_text(encoding="utf-8"))
        linjer = tekst.split("\n")
        for m in KALD.finditer(tekst):
            kald += 1
            arg = argumenter(tekst, m.start())
            rest = arg
            for r in FRI:
                rest = r.sub("", rest)
            raa = sorted({int(d.group(1)) for d in TAL.finditer(rest)}
                         - SMAA)
            if raa:
                nr = tekst[:m.start()].count("\n") + 1
                navn = m.group(1).replace("lv_obj_", "")
                fejl.append(
                    f"{f.relative_to(ROD)}:{nr}: {navn}() har raa "
                    f"pixeltal {raa}. Pak dem i ZS_G_SK (former), "
                    f"ZS_G_SK_WIDTH (bredder) eller ZS_G_SK_HEIGHT "
                    f"(hoejder) - eller giv dem et navn i zs_layout.h")

    if fejl:
        for e in fejl:
            print(f"  {ROED}FEJL{SLUT} {e}")
        print(f"\n  {len(fejl)} steder at rette.")
        return 1

    print(f"  {GROEN} OK{SLUT} {kald} geometri-kald i fladen, ingen af dem "
          f"har et tal der kun passer paa én skaerm.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
