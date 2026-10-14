#!/usr/bin/env python3
"""
zScreen - tjekker at vi SENDER det vi har TESTET.

HVORFOR DEN FINDES.

Vi har to arbejdsgange: test.yml proever hver aendring, og release.yml
bygger den fil skaermene henter. Bygger de to med hver sin ESP-IDF, saa
tester vi med én oversaetter og sender en anden ud til kunderne.

Det er ikke teoretisk. Da ESP-IDF blev loeftet fra 5.1 til 5.3 blev
test.yml rettet og release.yml glemt. Forskellen mellem de to
oversaettere er maalt til naesten tredive kilobyte i den faerdige fil, og
GCC 13 fanger fejl GCC 12 lader ligge. Saa den fil kunderne ville have
faaet, var bygget af en oversaetter ingen test havde set.

Scriptet tjekker ogsaa at det IKKE er et flydende maerke som "latest"
eller "release-v5.3": en udgivelse skal kunne bygges igen om et aar og
give det samme.

TO PRODUKTER, OG DERFOR TO OVERSAETTERE.

Reglen var foer "begge filer skal bruge PRAECIS ét billede, og det
samme". Den holdt da der var én skaerm. Nu er der to: zScreen bygges med
5.1.7, og ZDisplay SKAL have mindst 5.3, fordi ESP32-P4 ikke findes i
5.1.

Reglen er derfor vendt om til den paastand der faktisk betyder noget:
HVER oversaetter der bruges til en udgivelse, skal ogsaa vaere brugt i
test. Altsaa release.yml's billeder er en delmaengde af test.yml's.

Den fanger stadig den fejl den blev skrevet for - rettes test.yml til en
ny IDF og glemmes release.yml, saa bygger release med et billede ingen
test har set - og den tillader det vi faktisk har: at test.yml proever
MERE end der bliver sendt ud. Det er den rigtige vej at vaere skaev.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

BILLEDE = re.compile(r'^\s*container:\s*(\S+)\s*$', re.M)
# v5.3.6 er fast. v5.3, release-v5.3 og latest flytter sig.
FAST = re.compile(r'^espressif/idf:v\d+\.\d+\.\d+$')

# Det navn hver skaerm leder efter i en udgivelse.
PRAEFIKS = re.compile(r'#define\s+ZS_OTA_ASSET_PREFIX\s+"([^"]+)"')

# Produkter der endnu IKKE bliver udgivet, og hvorfor.
#
# HVORFOR LISTEN FINDES, OG HVORFOR DEN IKKE ER EN KOMMENTAR.
#
# En skaerm leder efter sit eget navn i udgivelsen (se
# ZS_OTA_ASSET_PREFIX). Er filen ikke der, henter skaermen ingenting og
# proever igen hver halve time - for evigt, uden at nogen kan se hvorfor.
#
# ZDisplay staar her fordi release.yml kun bygger zScreen. Det er en
# bevidst mangel: de to produkter bygger med hver sin ESP-IDF, saa de
# skal vaere to job, og de to job skal laegge op i SAMME udgivelse. Det
# arbejde kan stoppe en udgivelse, og det skal laves naar nogen sidder og
# ser paa det - ikke klemmes ind.
#
# Men det maa ikke blive glemt, og det maa ikke kunne ske for et
# PRODUKT MERE uden at nogen siger ja. Derfor staar manglen her, hvor
# den skal fjernes med vilje, i stedet for i en note nogen laeser.
IKKE_UDGIVET_ENDNU = {
    "zdisplay-": "release.yml bygger kun zScreen. Se docs/zdisplay.md, "
                 "afsnittet 'Det naeste'. Og HUSK: zScreen skal laegges "
                 "op FOERST, saa skaerme paa v0.27.0 og aeldre - som tager "
                 "den foerste .bin i udgivelsen - stadig rammer deres egen.",
}


def tjek_praefikser(release_tekst):
    """Leder hver skaerm efter et navn udgivelsen faktisk laegger op?"""
    sti = ROD / "firmware/kerne/zs_config.h"
    praefikser = PRAEFIKS.findall(sti.read_text(encoding="utf-8"))
    ud = []
    if not praefikser:
        return ["fandt ingen ZS_OTA_ASSET_PREFIX i zs_config.h. "
                "Er den doebt om? Uden den ved ingen skaerm hvilken fil "
                "der er dens"]
    if len(praefikser) != len(set(praefikser)):
        ud.append(f"to produkter leder efter det SAMME navn: "
                  f"{sorted(praefikser)}. Saa kan en skaerm hente den "
                  f"andens firmware")
    for pre in praefikser:
        if pre in release_tekst:
            continue
        if pre in IKKE_UDGIVET_ENDNU:
            print(f"  {GROEN}note{SLUT} {pre}* bliver ikke udgivet endnu: "
                  f"{IKKE_UDGIVET_ENDNU[pre]}")
            continue
        ud.append(f"en skaerm leder efter {pre}*.bin, men release.yml "
                  f"laegger ikke en fil op med det navn. Saa henter den "
                  f"ingenting og proever igen hver halve time, for evigt")
    return ud


def main():
    print("Tjekker at vi sender det vi har testet ...")
    fundet = {}
    for navn in ("test.yml", "release.yml"):
        sti = ROD / ".github/workflows" / navn
        if not sti.exists():
            print(f"  {ROED}FEJL{SLUT} {navn} findes ikke")
            return 1
        billeder = set(BILLEDE.findall(sti.read_text(encoding="utf-8")))
        if not billeder:
            print(f"  {ROED}FEJL{SLUT} {navn} bygger ikke i nogen container")
            return 1
        fundet[navn] = billeder

    fejl = tjek_praefikser(
        (ROD / ".github/workflows/release.yml").read_text(encoding="utf-8"))
    # Hver oversaetter der sender firmware ud, skal ogsaa vaere proevet.
    for b in sorted(fundet["release.yml"] - fundet["test.yml"]):
        fejl.append(f"release.yml bygger med {b}, som test.yml ikke "
                    f"bruger. Saa bliver den fil kunderne faar, bygget af "
                    f"en oversaetter ingen test har set")
    for navn, billeder in fundet.items():
        for b in sorted(billeder):
            if not FAST.match(b):
                fejl.append(f"{navn} bruger {b}, som kan flytte sig. "
                            f"Skriv en fast version som espressif/idf:v5.3.6, "
                            f"saa en udgivelse kan bygges igen om et aar")

    if fejl:
        for f in fejl:
            print(f"  {ROED}FEJL{SLUT} {f}")
        return 1

    print(f"  {GROEN} OK{SLUT} test.yml bygger med "
          f"{', '.join(sorted(fundet['test.yml']))}, udgivelsen med "
          f"{', '.join(sorted(fundet['release.yml']))}, og alle er faste.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
