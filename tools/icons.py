#!/usr/bin/env python3
"""
zScreen - listen over ikoner. ÉT sted.

Den samme liste bruges to steder: til at bestemme hvilke ikoner der
kommer med i skrifttypen, og til at skrive C-headeren med navnene.
Stod den to steder, ville en tilfoejet ikon det ene sted og ikke det
andet give et tomt felt paa skaermen uden en eneste fejlmeddelelse.

Kodepunkterne kommer fra Lucides egen font/info.json. Tilfoejer du en
ikon: slaa navnet op der, og skriv koden ind her.

    python3 icons.py --range     kodepunkter til lv_font_conv
    python3 icons.py --header X  skriv C-headeren
"""

import argparse
import sys

# navn i C            kode    navn hos Lucide      hvad vi bruger det til
ICONS = [
    ("SUN",            0xE178, "sun",              "solceller"),
    ("HOUSE",          0xE0F5, "house",            "forbrug i huset"),
    ("BATTERY_CHARGE", 0xE054, "battery-charging", "batteri der lader eller aflader"),
    ("BATTERY",        0xE053, "battery",          "batteri i hvile"),
    ("ZAP",            0xE1B4, "zap",              "elnettet"),
    ("WIFI",           0xE1AE, "wifi",             "godt signal"),
    ("WIFI_HIGH",      0xE5F7, "wifi-high",        "middel signal"),
    ("WIFI_LOW",       0xE5F8, "wifi-low",         "svagt signal"),
    ("WIFI_OFF",       0xE1AF, "wifi-off",         "ingen forbindelse"),
    ("SETTINGS",       0xE154, "settings",         "indstillinger"),
    ("ARROW_UP",       0xE04A, "arrow-up",         "ud af huset, saelger"),
    ("ARROW_DOWN",     0xE042, "arrow-down",       "ind i huset, koeber"),
    ("ARROW_LEFT",     0xE048, "arrow-left",       "tilbage"),
    ("ARROW_RIGHT",    0xE049, "arrow-right",      "videre"),
    ("ALERT",          0xE193, "triangle-alert",   "advarsel"),
    ("SPINNER",        0xE10A, "loader-circle",    "arbejder"),
    ("CHECK",          0xE06C, "check",            "valgt eller faerdig"),
    ("CHEVRON_LEFT",   0xE06E, "chevron-left",     "tilbage i en liste"),
    ("CHEVRON_RIGHT",  0xE06F, "chevron-right",    "ind i en liste"),
    ("CHEVRON_DOWN",   0xE06D, "chevron-down",     "fold ud"),
    ("LOCK",           0xE10B, "lock",             "netvaerk med kodeord"),
    ("REFRESH",        0xE145, "refresh-cw",       "soeg igen"),
    ("PLUG",           0xE37F, "plug",             "inverter"),
    ("CLOSE",          0xE1B2, "x",                "luk"),
    ("EYE",            0xE0BA, "eye",              "vis kodeordet"),
    ("EYE_OFF",        0xE0BB, "eye-off",          "skjul kodeordet"),
    ("CIRCLE_CHECK",   0xE226, "circle-check",     "det lykkedes"),
    ("CIRCLE_ALERT",   0xE077, "circle-alert",     "der er noget galt"),
    ("POWER",          0xE140, "power",            "genstart"),
    ("INFO",           0xE0F9, "info",             "detaljer om anlaegget"),
    ("SEARCH",         0xE151, "search",           "soeg efter inverter"),
    ("ROTATE",         0xE148, "rotate-ccw",       "nulstil"),
    ("TRASH",          0xE18E, "trash-2",          "slet"),
    ("BACKSPACE",      0xE0AE, "delete",           "slet et tegn"),
]

HEADER_TOP = '''/*
 * zScreen - ikonnavne.
 *
 * Ikonerne kommer fra Lucide, den samme pakke som Zbox-webfladen
 * bruger. Saa ser en advarselstrekant ens ud paa skaermen og i
 * browseren, og vi behoever ikke tegne noget selv.
 *
 * Lucide laegger hvert ikon paa et kodepunkt i Unicodes private
 * omraade. Her staar de som UTF-8-strenge, saa de kan saettes direkte
 * ind i en LVGL-etiket:
 *
 *     lv_label_set_text(lbl, ZS_ICON_SUN);
 *     lv_obj_set_style_text_font(lbl, &zs_icons_20, 0);
 *
 * FILEN ER GENERERET. Ret ikke i den i haanden.
 * Listen staar i tools/icons.py, og den samme liste bestemmer ogsaa
 * hvilke ikoner der kommer med i skrifttypen. Skriv du et nyt ikon
 * ind her i stedet, ville navnet findes uden at tegningen fulgte med,
 * og feltet ville staa tomt paa skaermen uden en eneste fejl i loggen.
 *
 * Byg igen med:  ./tools/build-assets.sh --force
 */

#ifndef ZS_ICONS_H
#define ZS_ICONS_H

#include "lvgl.h"

LV_FONT_DECLARE(zs_icons_20)
LV_FONT_DECLARE(zs_icons_28)
'''


def utf8_escape(cp: int) -> str:
    return "".join("\\x%02X" % b for b in chr(cp).encode("utf-8"))


def check_unique():
    """To ikoner paa samme kode ville betyde at det ene aldrig blev vist."""
    seen = {}
    for name, cp, lucide, _ in ICONS:
        if cp in seen:
            print(f"FEJL: {name} og {seen[cp]} har begge kode U+{cp:04X}",
                  file=sys.stderr)
            return False
        seen[cp] = name
    names = [n for n, _, _, _ in ICONS]
    if len(set(names)) != len(names):
        print("FEJL: der er to ikoner med samme navn", file=sys.stderr)
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--range", action="store_true",
                    help="skriv kodepunkterne som lv_font_conv vil have dem")
    ap.add_argument("--header", metavar="FIL", help="skriv C-headeren")
    args = ap.parse_args()

    if not check_unique():
        return 1

    if args.range:
        print(",".join("0x%04x" % cp for _, cp, _, _ in ICONS))
        return 0

    if args.header:
        w = max(len(n) for n, _, _, _ in ICONS)
        out = [HEADER_TOP]
        for name, cp, lucide, desc in ICONS:
            out.append("/* %-16s U+%04X  %s */" % (lucide, cp, desc))
            out.append("#define ZS_ICON_%-*s \"%s\"" % (w, name, utf8_escape(cp)))
        out.append("")
        out.append("#endif /* ZS_ICONS_H */")
        with open(args.header, "w") as f:
            f.write("\n".join(out) + "\n")
        print(f"  zs_icons.h             {len(ICONS)} ikoner")
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
