#!/usr/bin/env python3
"""
zScreen - tjek at skaermens maal ikke staar skrevet ind i fladen.

HVORFOR DEN FINDES.

Geometrien staar ÉT sted, i ui/zs_layout.h, og den regnes ud af
skaermbredden og hoejden. Det er med vilje: skal vi en dag paa en stoerre
skaerm, skal det vaere ét tal der aendres.

Men fire steder i fladen stod maalene alligevel skrevet ind:

    zs_statusbar.c    tandhjulet paa 430, statusikonet paa 400
    zs_flow.c         midterlinjen paa 240
    zs_price_page.c   hele den lodrette stak, 14 til 380
    zs_screen_setup.c teksten paa ventesiden paa 160

Alle fire var rigtige paa den skaerm vi har, og alle fire ville gaa i
stykker paa en stoerre: tandhjulet ville sidde midt paa skaermen, grafen
ville efterlade et tomt felt nederst. Og det var ikke til at se uden at
lede efter det.

REGLEN HER ER SMAL MED VILJE.

Vi forbyder ikke alle tal. En skrifttype paa 20 px er 20 px uanset
skaermen, og en finger bliver ikke stoerre. Vi forbyder kun de tal der ER
skaermens maal: bredden, hoejden, sidens hoejde, og halvdelen af hver. De
tal kan IKKE vaere rigtige som faste tal, for de betyder noget andet paa
en anden skaerm.

Saa faa falske fund, og det der fanges er altid rigtigt.

Koeres af tools/check-alle.sh, saa den ikke kan glemmes.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
UI = ROD / "firmware/main/ui"
LAYOUT = UI / "zs_layout.h"

fejl = []


def tal_fra_layout():
    """Henter skaermens maal ud af zs_layout.h, saa listen foelger koden."""
    t = LAYOUT.read_text(encoding="utf-8")

    def find(navn):
        m = re.search(r"#define\s+" + navn + r"\s+(\d+)", t)
        return int(m.group(1)) if m else None

    bredde = find("ZS_G_SCR_WIDTH")
    hoejde = find("ZS_G_SCR_HEIGHT")
    bar = find("ZS_G_BAR_HEIGHT")
    dots = find("ZS_G_DOTS_HEIGHT")
    if None in (bredde, hoejde, bar, dots):
        fejl.append("kunne ikke laese skaermens maal ud af zs_layout.h. "
                    "Er konstanterne doebt om?")
        return {}

    side = hoejde - bar - dots
    # Navn -> tal. Navnet staar i fejlbeskeden, saa man kan se HVAD tallet
    # er, og ikke bare at det er forbudt.
    return {
        "skaermens bredde": bredde,
        "skaermens hoejde": hoejde,
        "sidens hoejde": side,
        "halve bredde (midten)": bredde // 2,
    }
    # Halvdelen af SIDEHOEJDEN staar med vilje IKKE paa listen.
    #
    # Den var med i foerste forsoeg, og den gav ét falsk fund: zs_flow.c
    # havde "Y_SIDE_VALUE 204", og 204 er tilfaeldigvis halvdelen af 408.
    # Men tallet betyder ikke "midt paa siden", det er en plads i en
    # lodret stak regnet ud af skrifttyperne. Et falsk fund er dyrere end
    # et manglende: en vagt der raaber forkert bliver slaaet fra.
    #
    # Halvdelen af BREDDEN staar der til gengaeld. Den betyder altid
    # midterlinjen, og den er der ingen anden grund til at skrive ind.


def main():
    forbudt = tal_fra_layout()
    if fejl:
        for f in fejl:
            print(f"  \033[1;31mFEJL\033[0m {f}")
        return 1

    print("Tjekker at skærmens mål ikke står skrevet ind i fladen ...")
    print("  forbudte tal: " + ", ".join(
        f"{v} ({k})" for k, v in sorted(forbudt.items(), key=lambda x: -x[1])))

    filer = sorted(list(UI.glob("*.c")) + list(UI.glob("widgets/*.c")))
    for f in filer:
        i_blok = False
        for nr, linje in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
            # Kommentarer maa godt naevne tallene: det er netop dér man
            # forklarer hvor de kommer fra.
            #
            # Foerste forsoeg delte bare paa "/*", og saa blev HVER linje i
            # en blokkommentar laest som kode. Det gav fjorten falske fund
            # og ét rigtigt, og en vagt med fjorten falske fund bliver
            # slaaet fra. Derfor foelges blokken nu linje for linje.
            kode, rest, i_blok = "", linje, i_blok
            while rest:
                if i_blok:
                    slut = rest.find("*/")
                    if slut < 0:
                        rest = ""
                    else:
                        rest = rest[slut + 2:]
                        i_blok = False
                else:
                    start = rest.find("/*")
                    enkelt = rest.find("//")
                    if enkelt >= 0 and (start < 0 or enkelt < start):
                        kode += rest[:enkelt]
                        rest = ""
                    elif start < 0:
                        kode += rest
                        rest = ""
                    else:
                        kode += rest[:start]
                        rest = rest[start + 2:]
                        i_blok = True
            if not kode.strip():
                continue
            for navn, tal in forbudt.items():
                if re.search(r"(?<![\w.])" + str(tal) + r"(?![\w.])", kode):
                    fejl.append(
                        f"{f.relative_to(ROD)}:{nr} har {tal} skrevet ind, "
                        f"og det er {navn}.\n"
                        f"        {kode.strip()[:90]}\n"
                        f"        Tag det fra ui/zs_layout.h i stedet, "
                        f"saa det flytter med paa en anden skaerm.")

    if fejl:
        print()
        for f in fejl:
            print(f"  \033[1;31mFEJL\033[0m {f}")
        print(f"\n  {len(fejl)} faste mål i fladen.")
        return 1

    print(f"  \033[1;32m OK\033[0m {len(filer)} filer, ingen af skærmens mål "
          f"står skrevet ind.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
