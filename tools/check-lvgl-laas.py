#!/usr/bin/env python3
"""
zScreen - tjekker at LVGL ikke bliver roert uden laasen.

HVORFOR DEN FINDES.

LVGL 8 er ikke bygget til at blive kaldt fra flere opgaver paa én gang.
Skaermopgaven tegner i sit eget tempo, og roerer en anden opgave de samme
objekter imens, kan listerne LVGL gaar igennem skifte under den. Det
viser sig som en skaerm der fryser eller et billede der gaar i stykker,
og det sker en gang om ugen uden moenster. Den slags fejl kan ikke
fejlsoeges bagefter, den skal forhindres.

Reglen i huset er at ALT i firmware/kerne/ui selv tager laasen, saa ingen
kalder skal huske det. Den regel stod kun som en kommentar, og en
kommentar holder ingen i haanden.

Det her script holder den fast: en funktion i ui/ som kaldes fra app,
net eller main, og som roerer LVGL, SKAL tage laasen.

Den fangede én: zs_theme_set_mode roerte LVGL tolv steder uden laasen.
Det gik godt indtil nu, men kun fordi den ene kalder tilfaeldigvis koerer
foer brugerfladen findes. Et sammentraef, ikke en garanti.

GRAENSER, saa ingen tror den kan mere end den kan. Den laeser teksten,
ikke programmet. Kaldes en funktion gennem en pegepind, eller bygges
navnet sammen af stumper, ser den det ikke. Den er en bund, ikke et
bevis.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
UI = ROD / "firmware/kerne/ui"
UDEFRA = [ROD / "firmware/kerne/app", ROD / "firmware/kerne/net"]
# BEGGE produkters opstart OG deres LVGL-porte.
#
# Her stod kun zScreens main.c. Da ZDisplay kom til, var dens opstart og
# dens lv_port.c ikke tjekket af nogen - og de gOEr praecis det her
# handler om: tager laasen om zs_ui_init(), og kalder lv_disp_flush_ready
# fra en afbrydelse. En vagt der kun daekker det ene produkt, er ikke en
# vagt paa reglen; den er en vagt paa ét board.
MAIN = [ROD / f"firmware/boards/{b}/main/{f}"
        for b in ("zscreen", "zdisplay")
        for f in ("main.c", "lv_port.c")]

GROEN = "\033[1;32m"
ROED = "\033[1;31m"
SLUT = "\033[0m"

LAAS = "lv_port_sem_take"
# lv_port_sem_* er selve laasen, ikke noget der skal laases om.
LV_KALD = re.compile(r"\blv_(?!port_sem)\w+\s*\(")
# En definition: noget der ikke er static, med en krop bagefter.
DEFINITION = re.compile(r"^(?!static)(?:\w[\w \*]*?)\b(\w+)\s*\([^;{]*?\)\s*\{", re.M)
NOEGLEORD = {"if", "for", "while", "switch", "return", "sizeof", "else"}


def uden_kommentarer(s):
    """Fjerner kommentarer UDEN at flytte linjenumrene.

    HER STOD DEN SAMME FEJL SOM I check-flade.py, og den blev rettet
    DER men ikke her. Den gamle udgave slettede blokkommentarer med
    linjeskift og alt, og saa rykkede alt nedenunder op.

    Maalt 17. oktober 2026 paa de filer reglerne nedenfor skanner:

        zs_app.c              2631 linjer -> 1573, afvigelse 1058
        zs_fleet.c            1830 linjer -> 1188, afvigelse  642
        zs_screen_settings.c  1139 linjer ->  894, afvigelse  245

    Foer de to nye regler kom til, udskrev den her fil ingen
    linjenumre, saa fejlen var harmloes. Nu melder motoren-maa-ikke-
    kalde-lv_*-reglen "{fil}:{nr}", og en overtraeedelse naer slutningen
    af zs_app.c ville blive meldt tusind linjer forkert. En vagt der
    sender dig det forkerte sted hen, koster mere end den sparer.
    """
    def nyline(m):
        return "\n" * m.group(0).count("\n")
    s = re.sub(r"/\*.*?\*/", nyline, s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def krop_af(s, start):
    """Fra den foerste { til den der lukker den."""
    i = s.index("{", start)
    dybde = 0
    for j in range(i, len(s)):
        if s[j] == "{":
            dybde += 1
        elif s[j] == "}":
            dybde -= 1
            if dybde == 0:
                return s[i:j + 1]
    return s[i:]


def main():
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI} findes ikke")
        return 1

    print("Tjekker at LVGL ikke roeres uden laasen ...")

    # Hvad findes der i brugerfladen, og hvad gør hver funktion.
    funktioner = {}
    for f in sorted(UI.glob("*.c")):
        tekst = f.read_text(encoding="utf-8")
        for m in DEFINITION.finditer(tekst):
            navn = m.group(1)
            if navn in NOEGLEORD:
                continue
            krop = krop_af(tekst, m.start())
            funktioner[navn] = {
                "fil": f.relative_to(ROD),
                "lv": len(LV_KALD.findall(krop)),
                "laas": LAAS in krop,
            }

    # Hvem bliver kaldt fra udenfor brugerfladen.
    # Hver af de fire board-filer SKAL findes. Mangler en, er et
    # produkt flyttet, og saa tjekker vagten ikke det den lover.
    kilder = []
    for m in MAIN:
        if m.exists():
            kilder.append(m)
        else:
            print(f"  \033[1;31mFEJL\033[0m {m.relative_to(ROD)} findes ikke. "
                  f"Er et board flyttet? Se MAIN i check-lvgl-laas.py")
            return 1
    for d in UDEFRA:
        kilder += sorted(d.glob("*.c"))

    kaldt = {}
    for f in kilder:
        tekst = uden_kommentarer(f.read_text(encoding="utf-8"))
        for navn in funktioner:
            if re.search(r"\b" + re.escape(navn) + r"\s*\(", tekst):
                kaldt.setdefault(navn, []).append(f.relative_to(ROD).name)

    fejl = []
    for navn, d in sorted(funktioner.items()):
        if navn in kaldt and d["lv"] > 0 and not d["laas"]:
            fejl.append(f"{d['fil']}: {navn}() roerer LVGL {d['lv']} "
                        f"sted(er) uden at tage laasen, og kaldes fra "
                        f"{', '.join(kaldt[navn])}")

    # ── OG MOTOREN ROERER SLET IKKE LVGL ────────────────────────────
    #
    # Reglen ovenfor gaelder funktioner i ui/ der kaldes udefra. Men
    # app/, net/, storage/ og hal/ kan jo ogsaa bare kalde lv_* selv -
    # og saa er der ingen ui-funktion at stille krav til. Den vej var
    # ikke lukket.
    #
    # Den er tom i dag, maalt 15. oktober 2026: nul direkte lv_*-kald i
    # de fire mapper. Netop derfor koster det ingenting at laase den
    # fast nu. Den dag nogen skriver det foerste, skal det vaere et
    # VALG og ikke noget der bare sker.
    #
    # Fladen naas gennem zs_ui_*, som selv tager laasen.
    for mappe in ("app", "net", "storage", "hal"):
        d = ROD / "firmware/kerne" / mappe
        if not d.is_dir():
            continue
        for f in sorted(d.rglob("*.c")):
            tekst = uden_kommentarer(f.read_text(encoding="utf-8"))
            for nr, linje in enumerate(tekst.splitlines(), 1):
                m = LV_KALD.search(linje)
                if m is None:
                    continue
                fejl.append(
                    f"{f.relative_to(ROD)}:{nr} kalder {m.group(0)[:-1]} "
                    f"direkte. Motoren maa ikke roere LVGL - den koerer "
                    f"i sin egen opgave, og saa er der ingen laas. Gaa "
                    f"gennem zs_ui_*, som selv tager den")

    # ── OG FLADEN LAESER IKKE HARDWARE ──────────────────────────────
    #
    # Spejlet af reglen ovenfor. Motoren maa ikke roere LVGL; fladen
    # maa ikke LAESE hardwaren.
    #
    # Hvorfor netop LAESE: zs_details_update kaldte zs_stroem_laes fra
    # LVGL-opgaven, mens BSP'ens batteriopgave skrev de samme tal. To
    # opgaver om det samme felt, og en side hvis indhold ingen kalder
    # kunne give den - altsaa heller ikke proeve. Alt andet paa siden
    # kom som argument. Nu kommer stroemmen ogsaa.
    #
    # AT SKRIVE ER EN ANDEN SAG, og baglyset staar derfor paa listen
    # over undtagelser: lysstyrkeskyderens hele formaal ER at saette
    # baglyset, det er en kommando og ikke en tilstand nogen anden
    # opgave skriver i. Der er ingen at kappes med.
    SKRIV_OK = {
        "zs_baglys_init": "lysstyrken saettes af skyderen selv",
        "zs_baglys_set":  "samme",
        "zs_lyd_bip":     "et bip er en kommando, ikke en tilstand",
        "zs_lyd_init":    "samme",
        "zs_lyd_pakke":   "samme",
        "zs_lyd_findes":  "svarer paa boardet, ikke paa hardwaren",
    }
    hal = ROD / "firmware/kerne/hal"
    if hal.is_dir():
        navne = set()
        for h in sorted(hal.glob("*.h")):
            navne |= set(re.findall(r"\bzs_[a-z0-9_]+(?=\s*\()",
                                    h.read_text(encoding="utf-8")))
        for f in sorted((ROD / "firmware/kerne/ui").rglob("*.c")):
            if "/assets/" in f.as_posix():
                continue
            kode = uden_kommentarer(f.read_text(encoding="utf-8"))
            for navn in sorted(navne):
                if navn in SKRIV_OK:
                    continue
                for nr, linje in enumerate(kode.splitlines(), 1):
                    if re.search(rf"\b{navn}\s*\(", linje):
                        fejl.append(
                            f"{f.relative_to(ROD)}:{nr} kalder {navn} "
                            f"direkte. Fladen koerer i LVGL-opgaven, og "
                            f"hardwarens tal skrives af en anden - saa "
                            f"det er et kaploeb, og siden kan ikke "
                            f"proeves. Tag vaerdien ind som argument, "
                            f"saadan som zs_details_update faar "
                            f"zs_stroem_t")

    if fejl:
        for linje in fejl:
            print(f"  {ROED}FEJL{SLUT} {linje}")
        print()
        print("  Laeg lv_port_sem_take() og lv_port_sem_give() om den del")
        print("  der roerer LVGL. Mutexen er rekursiv, og lv_port_sem_take")
        print("  goer ingenting naar den kaldes fra skaermopgaven selv, saa")
        print("  det er trygt ogsaa naar en kalder allerede holder den.")
        return 1

    roerer = sum(1 for d in funktioner.values() if d["lv"] > 0)
    print(f"  {GROEN} OK{SLUT} {len(funktioner)} funktioner i ui/, {roerer} "
          f"roerer LVGL, {len(kaldt)} kaldes udefra, alle tager laasen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
