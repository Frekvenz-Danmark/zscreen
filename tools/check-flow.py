#!/usr/bin/env python3
"""
zScreen - tjekker at ingen side er en blindgyde.

HVORFOR DEN FINDES.

Skaermen haenger paa en vaeg og har kun én slags input: en finger. Lander
en kunde paa en side uden vej tilbage, er der ingen tast at trykke paa,
ingen mus og ingen menu. Saa er skaermen i praksis gaaet i staa, og den
eneste udvej er at tage stroemmen.

Den fangede én: tilbage fra netvaerkslisten foerte til velkomstsiden, hvor
den eneste knap er "Kom i gang", som foerer tilbage til netvaerkslisten.
En kunde der trykkede paa Netvaerk i indstillingerne og fortrod, kunne
derfor ikke komme hjem igen uden at gaa hele opsaetningen igennem,
inklusive en ny soegning efter inverteren paa op til fire minutter.

REGLEN: hver side der laves med zs_page_create SKAL have en tilbage-knap.
Undtagelserne staar herunder, med en grund hver.

REGEL 2: tilbage i opsaetningen maa ikke foere TIL FORSIDEN.

Den regel kom til 14. okt 2026, og den kom fordi den foerste regel ikke
var nok. Tilmeldingssiden HAVDE en tilbage-knap - den var altsaa i
orden efter regel 1 - men knappen foerte til forsiden, og tilmeldingen
er sidste trin i opsaetningen. Ét tryk paa pilen sendte derfor
montoeren videre med et anlaeg der aldrig blev tilmeldt. Tavst: ingen
besked, ingen bekraeftelse, og siden har ingen anden udgang.

Alle de andre sider i opsaetningen gaar tilbage GENNEM flowet. Den ene
der sprang ud af det, var den der betoed noget.

GRAENSER. Den laeser teksten, ikke programmet. Den kan se at der ER en
tilbage-knap og hvad opsaetningen saetter som maal, ikke at ethvert maal
er fornuftigt. Den er en bund, ikke et bevis.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
UI = ROD / "firmware/kerne/ui"

GROEN = "\033[1;32m"
ROED = "\033[1;31m"
SLUT = "\033[0m"

# Sider der med vilje ikke har en tilbage-knap, og hvorfor.
UNDTAGELSER = {
    "": "velkomstsiden: der er ingenting foer den, og den har "
        "\"Kom i gang\" fremad",
    "Ingen forbindelse":
        "netvaerkssiden: der er ikke noget at gaa tilbage TIL - siden "
        "vises netop fordi hovedsiden ikke har noget at vise. Den har "
        "\"Fix fejl\" fremad til netvaerkstrinnet, hvorfra tilbage "
        "sender SETUP_CANCEL, og app'en viser hovedsiden af sig selv i "
        "samme sekund som nettet er tilbage. Skaermen kan altsaa ikke "
        "gaa i staa paa den",
}

# zs_page_create(&var, "Titel", tilbage_cb, ...)
SIDE = re.compile(
    r'zs_page_create\(\s*&(\w+)\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,')


# Hvor opsaetningen slutter, og hvad den IKKE maa pege paa.
APP = ROD / "firmware/kerne/app/zs_app.c"
SLUT_FUNK = "slut_opsaetningen"


def krop_af(tekst, navn):
    """Kroppen af en funktion, fra dens foerste { til den der lukker."""
    m = re.search(r"\b" + re.escape(navn) + r"\s*\([^;{]*\)\s*\{", tekst)
    if m is None:
        return None
    i = tekst.index("{", m.start())
    dybde = 0
    for j in range(i, len(tekst)):
        if tekst[j] == "{":
            dybde += 1
        elif tekst[j] == "}":
            dybde -= 1
            if dybde == 0:
                return tekst[i:j + 1]
    return tekst[i:]


def tjek_opsaetningens_slutning():
    """Regel 2: tilbage fra sidste trin maa ikke foere til forsiden.

    Maalet saettes med zs_tilmeld_page_set_return i slut_opsaetningen.
    Peger det paa ZS_SCREEN_HOME, betyder tilbage-knappen "spring
    tilmeldingen over" - og det er ikke noget en pil skal kunne.
    """
    if not APP.exists():
        return [f"{APP.relative_to(ROD)} findes ikke. Er motoren flyttet? "
                f"Saa tjekker vagten ikke det den lover."]
    krop = krop_af(APP.read_text(encoding="utf-8"), SLUT_FUNK)
    if krop is None:
        return [f"fandt ikke {SLUT_FUNK}() i "
                f"{APP.relative_to(ROD)}. Er den doebt om? Saa er regel 2 "
                f"ikke laengere tjekket - se hovedet i check-flow.py."]
    m = re.search(r"zs_tilmeld_page_set_return\s*\(\s*(\w+)\s*\)", krop)
    if m is None:
        return [f"{SLUT_FUNK}() saetter ikke hvor tilbage foerer hen. "
                f"Saa staar der det den forrige side satte, og det kan "
                f"vaere forsiden."]
    if m.group(1) == "ZS_SCREEN_HOME":
        return [f"{SLUT_FUNK}() lader tilbage foere til forsiden. Saa "
                f"betyder tilbage-knappen paa tilmeldingssiden \"spring "
                f"tilmeldingen over\", og anlaegget bliver aldrig "
                f"tilmeldt. Peg paa trinnet FOER i stedet."]
    return []


def main():
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI} findes ikke")
        return 1

    print("Tjekker at ingen side er en blindgyde ...")

    sider = []
    for f in sorted(UI.glob("zs_screen_*.c")):
        tekst = f.read_text(encoding="utf-8")
        for m in SIDE.finditer(tekst):
            nr = tekst[:m.start()].count("\n") + 1
            sider.append({
                "fil": f.relative_to(ROD),
                "linje": nr,
                "var": m.group(1),
                "titel": m.group(2),
                "tilbage": m.group(3),
            })

    if not sider:
        print(f"  {ROED}FEJL{SLUT} fandt ingen sider. Er zs_page_create "
              f"blevet omdoebt?")
        return 1

    regel2 = tjek_opsaetningens_slutning()
    fejl = list(regel2)
    undtaget = 0
    for s in sider:
        if s["tilbage"] != "NULL":
            continue
        if s["titel"] in UNDTAGELSER:
            undtaget += 1
            continue
        fejl.append(f"{s['fil']}:{s['linje']}: siden \"{s['titel']}\" har "
                    f"ingen tilbage-knap, og staar ikke paa listen over "
                    f"undtagelser")

    if fejl:
        for linje in fejl:
            print(f"  {ROED}FEJL{SLUT} {linje}")
        print()
        # Raadet skal passe til fejlen. Stod regel 1's raad
        # under en regel 2-fejl, sendte det laeseren efter en
        # tilbage-knap der var der hele tiden.
        if len(fejl) > len(regel2):
            print("  Giv siden en tilbage-knap, eller skriv den paa "
                  "UNDTAGELSER")
            print("  i det her script MED en grund. En side uden vej "
                  "tilbage er")
            print("  en skaerm der er gaaet i staa for den der staar "
                  "foran den.")
        if regel2:
            print("  Tilbage skal foere TILBAGE. Peg paa trinnet foer i")
            print("  stedet for paa forsiden - saa kan en pil ikke komme")
            print("  til at betyde \"spring over\". Se regel 2 i hovedet.")
        return 1

    print(f"  {GROEN} OK{SLUT} {len(sider)} sider, {len(sider) - undtaget} "
          f"med tilbage-knap, {undtaget} undtaget med en grund, og "
          f"tilbage fra opsaetningens sidste trin foerer ikke til "
          f"forsiden.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
