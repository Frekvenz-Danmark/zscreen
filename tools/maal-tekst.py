#!/usr/bin/env python3
"""
zScreen - maaler hvor bred og hvor mange linjer en tekst bliver.

HVORFOR DEN FINDES.

"Den skal vaere to linjer" er ikke noget man kan se i en kildefil.
Linjeskiftet afgoeres af skriftens glyf-bredder og af kassens bredde,
og de to tal staar i hver sin ende af projektet. Indtil nu har svaret
vaeret at flashe og kigge - og det er en runde paa fem minutter hver
gang et ord aendres.

Scriptet laeser den GENEREREDE skriftfil, altsaa den der faktisk ligger
i flashen, og regner ombrydningen som LVGL gOEr det: bredden er summen
af adv_w, og der brydes paa mellemrum.

    ./tools/maal-tekst.py 800 broed 708 "Sæt venligst strømkablet i."

    skrift zs_font_broed, saet 800, linjehoejde 33
    bredde 708 px
      2 linjer:
        1: 'Sæt venligst strømkablet i. Skærmen' 612 px
        2: 'slukker, naar batteriet er brugt op.' 598 px
      hoejde i alt: 66 px

GRAENSER. Den regner med letter_space 0, som er det fladen bruger paa
broedteksten. Saetter en kalder lv_obj_set_style_text_letter_space, skal
tallet gives med --mellemrum. Og den kender ikke LVGLs orddeling af ord
der er laengere end linjen - der svarer den med en advarsel i stedet for
et forkert tal.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent

GLYF = re.compile(r"\.adv_w\s*=\s*(\d+)")
CMAP = re.compile(
    r"\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),"
    r"\s*\.glyph_id_start\s*=\s*(\d+),"
    r"\s*\.unicode_list\s*=\s*(\w+),"
    r"\s*\.glyph_id_ofs_list\s*=\s*(\w+),"
    r"\s*\.list_length\s*=\s*(\d+),\s*\.type\s*=\s*(\w+)", re.S)
LISTE = re.compile(r"static const uint8_t (glyph_id_ofs_list_\d+)\[\] = \{(.*?)\};", re.S)
LH = re.compile(r"\.line_height\s*=\s*(\d+)")


def laes(sti):
    t = sti.read_text(encoding="utf-8", errors="replace")

    # Kun glyph_dsc-blokken, ikke kerning eller andet med adv_w i.
    m = re.search(r"glyph_dsc\[\] = \{(.*?)\n\};", t, re.S)
    if m is None:
        raise SystemExit(f"{sti.name}: fandt ikke glyph_dsc[]")
    adv = [int(x) for x in GLYF.findall(m.group(1))]

    lister = {}
    for navn, krop in LISTE.findall(t):
        lister[navn] = [int(x) for x in re.findall(r"\d+", krop)]

    omraader = []
    for rs, rl, gs, ul, gol, ll, typ in CMAP.findall(t):
        omraader.append({
            "start": int(rs), "laengde": int(rl), "gid": int(gs),
            "ofs": lister.get(gol), "type": typ,
        })

    lh = LH.search(t)
    if lh is None:
        raise SystemExit(f"{sti.name}: fandt ikke line_height")
    return adv, omraader, int(lh.group(1))


def gid(kode, omraader):
    """Glyf-nummeret for ét tegn, eller None hvis skriften ikke har det."""
    for o in omraader:
        if not (o["start"] <= kode < o["start"] + o["laengde"]):
            continue
        i = kode - o["start"]
        if o["type"].endswith("FORMAT0_TINY"):
            return o["gid"] + i
        if o["type"].endswith("FORMAT0_FULL") and o["ofs"] is not None:
            if o["ofs"][i] == 0 and i != 0:
                return None          # hul i omraadet
            return o["gid"] + o["ofs"][i]
        return None
    return None


def bredde(s, adv, omraader, mellemrum):
    """Bredden i hele pixels. adv_w er i 1/16 px."""
    sum16 = 0
    mangler = []
    for ch in s:
        g = gid(ord(ch), omraader)
        if g is None or g >= len(adv):
            mangler.append(ch)
            continue
        sum16 += adv[g]
    px = sum16 // 16 + max(0, len(s) - 1) * mellemrum
    return px, mangler


def ombryd(tekst, maks, adv, omraader, mellemrum):
    """Som LVGL: bryd paa mellemrum, aldrig midt i et ord."""
    linjer = []
    for afsnit in tekst.split("\n"):
        ord_ = afsnit.split(" ")
        nu = ""
        for o in ord_:
            proev = o if nu == "" else nu + " " + o
            b, _ = bredde(proev, adv, omraader, mellemrum)
            if b <= maks or nu == "":
                nu = proev
            else:
                linjer.append(nu)
                nu = o
        linjer.append(nu)
    return linjer


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        print("Brug: maal-tekst.py <saet> <skrift> <bredde> <tekst> "
              "[--mellemrum N]")
        return 2
    saet, skrift, maks = sys.argv[1], sys.argv[2], int(sys.argv[3])
    tekst = sys.argv[4]
    mellemrum = 0
    if "--mellemrum" in sys.argv:
        mellemrum = int(sys.argv[sys.argv.index("--mellemrum") + 1])

    sti = ROD / f"firmware/kerne/ui/assets/{saet}/zs_font_{skrift}.c"
    if not sti.exists():
        print(f"FEJL: {sti.relative_to(ROD)} findes ikke.")
        return 1
    adv, omraader, lh = laes(sti)

    _, mangler = bredde(tekst, adv, omraader, mellemrum)
    linjer = ombryd(tekst, maks, adv, omraader, mellemrum)

    print(f"skrift zs_font_{skrift}, saet {saet}, linjehoejde {lh}")
    print(f"bredde {maks} px")
    print(f"  {len(linjer)} linjer:")
    for i, l in enumerate(linjer, 1):
        b, _ = bredde(l, adv, omraader, mellemrum)
        flag = "  ← FOR BRED" if b > maks else ""
        print(f"    {i}: {l!r} {b} px{flag}")
    print(f"  hoejde i alt: {len(linjer) * lh} px")
    if mangler:
        print(f"  ADVARSEL: skriften har ikke tegnene "
              f"{sorted(set(mangler))!r}. De er regnet som 0 px bred, og "
              f"paa glasset bliver de firkanter. Se tools/check-text.py")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
