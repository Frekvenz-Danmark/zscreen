#!/usr/bin/env python3
"""
zScreen - tjek at hvert eneste tegn vi skriver, faktisk findes i
skrifttypen.

Baggrund: LVGL tegner ingenting hvis et tegn mangler i skrifttypen.
Der kommer ingen advarsel, hverken naar der bygges eller naar det
koerer. Der staar bare et hul, eller en firkant, midt i en saetning.
Og det opdager man foerst naar skaermen haenger paa en vaeg.

Vi laeser den faktiske daekning ud af de genererede LVGL-filer, ikke ud
af det tegnsaet vi BAD om. Bad vi om aa og skrifttypen ikke havde det,
ville lv_font_conv bare springe det over.

    python3 check-text.py            tjek alt
    python3 check-text.py --list     vis hvad hver skrifttype daekker
"""

import pathlib
import re
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "firmware/main/ui/assets"
SRC = ROOT / "firmware/main"

# Skrifttyper der bruges til tekst brugeren laeser. Ikon-skrifttyperne
# staar ikke her: de daekker med vilje kun et par private kodepunkter,
# og de tjekkes ved at blive genereret fra den samme liste som
# navnene i zs_icons.h.
TEXT_FONTS = [
    "zs_font_28", "zs_font_20", "zs_font_16", "zs_font_13", "zs_font_kb_24",
]
# Det store taltema har med vilje kun cifre og skilletegn.
NUMBER_FONTS = ["zs_font_num_64"]


def parse_font_coverage(path: pathlib.Path) -> set:
    """Laeser hvilke kodepunkter en genereret LVGL-skrifttype daekker."""
    txt = path.read_text(encoding="utf-8", errors="replace")

    # De sparsomme lister: static const uint16_t unicode_list_1[] = {...}
    lists = {}
    for m in re.finditer(r"static const uint16_t (unicode_list_\w+)\[\]\s*=\s*\{(.*?)\}",
                         txt, re.S):
        vals = [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\d+", m.group(2))]
        lists[m.group(1)] = vals

    block = re.search(r"lv_font_fmt_txt_cmap_t cmaps\[\]\s*=\s*\{(.*?)\n\};", txt, re.S)
    if block is None:
        return set()

    cover = set()
    for entry in re.finditer(r"\{(.*?)\}", block.group(1), re.S):
        e = entry.group(1)
        rs = re.search(r"\.range_start\s*=\s*(\d+)", e)
        rl = re.search(r"\.range_length\s*=\s*(\d+)", e)
        ul = re.search(r"\.unicode_list\s*=\s*(\w+)", e)
        if rs is None or rl is None:
            continue
        start, length = int(rs.group(1)), int(rl.group(1))

        if ul is not None and ul.group(1) != "NULL":
            for off in lists.get(ul.group(1), []):
                cover.add(start + off)
        else:
            for i in range(length):
                cover.add(start + i)
    return cover


def collect_strings():
    """Alle strengliteraler i vores egen kode, med hvor de staar."""
    out = []
    for f in sorted(list(SRC.rglob("*.c")) + list(SRC.rglob("*.h"))):
        sp = str(f.relative_to(ROOT))
        if "/assets/" in sp or sp.endswith("zs_icons.h"):
            continue
        txt = f.read_text(encoding="utf-8", errors="replace")
        # Kommentarer taeller ikke med: de bliver aldrig tegnet.
        txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
        txt = re.sub(r"//.*", "", txt)
        for i, line in enumerate(txt.splitlines(), 1):
            for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', line):
                s = m.group(1)
                if s:
                    out.append((sp, i, s))
    return out


def main():
    if not ASSETS.is_dir():
        print("FEJL: skrifttyperne er ikke bygget. Koer ./tools/build-assets.sh",
              file=sys.stderr)
        return 1

    fonts = {}
    for name in TEXT_FONTS + NUMBER_FONTS:
        p = ASSETS / f"{name}.c"
        if not p.exists():
            print(f"FEJL: {name}.c mangler. Koer ./tools/build-assets.sh",
                  file=sys.stderr)
            return 1
        fonts[name] = parse_font_coverage(p)

    if "--list" in sys.argv:
        for name, cov in fonts.items():
            printable = sorted(c for c in cov if c >= 0x20)
            print(f"\n{name}: {len(cov)} tegn")
            line = "".join(chr(c) for c in printable if c < 0xE000)
            print("  " + line)
        return 0

    # Tegn der SKAL findes i alle tekst-skrifttyper. En streng kan
    # havne i hvilken som helst stoerrelse, saa det nytter ikke at
    # kun én af dem har aa.
    fail = 0
    print("Tjekker tekst mod skrifttyperne ...")

    # 1. De danske bogstaver skal vaere der. Ellers er alt andet ligegyldigt.
    dansk = "æøåÆØÅ"
    for name in TEXT_FONTS:
        mangler = [c for c in dansk if ord(c) not in fonts[name]]
        if mangler:
            print(f"  FEJL: {name} mangler {''.join(mangler)}")
            fail = 1

    # 2. Hvert tegn vi faktisk skriver, skal findes.
    seen = {}
    for path, line, s in collect_strings():
        for ch in s:
            cp = ord(ch)
            if cp < 0x20:
                continue
            if 0xE000 <= cp <= 0xF8FF:
                continue           # ikon fra det private omraade
            for name in TEXT_FONTS:
                if cp not in fonts[name]:
                    key = (cp, name)
                    if key not in seen:
                        seen[key] = (path, line, s)

    for (cp, name), (path, line, s) in sorted(seen.items()):
        try:
            navn = unicodedata.name(chr(cp))
        except ValueError:
            navn = "uden navn"
        vis = s if len(s) < 46 else s[:43] + "..."
        print(f"  FEJL: {name} har ikke U+{cp:04X} {chr(cp)!r} ({navn})")
        print(f"        brugt i {path}:{line}  \"{vis}\"")
        fail = 1

    if fail == 0:
        n = min(len(fonts[f]) for f in TEXT_FONTS)
        print(f"  {len(TEXT_FONTS)} tekst-skrifttyper, mindst {n} tegn hver, "
              f"alt hvad vi skriver kan tegnes.")
    return fail


if __name__ == "__main__":
    sys.exit(main())
