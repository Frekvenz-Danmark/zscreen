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
# LISTEN ER TOM, og det er den rigtige tilstand.
#
# Her stod "zdisplay-" med den begrundelse at release.yml kun byggede
# zScreen. Den mangel er lukket 14. okt 2026: release.yml har nu to job,
# ét per ESP-IDF, og de laegger op i samme udgivelse.
#
# Listen bliver staaende tom, fordi den er stedet hvor et produkt
# MIDLERTIDIGT kan vaere uden udgivelse - med en grund, skrevet ned, et
# sted der skal roeres med vilje. Ikke i en note nogen laeser.
IKKE_UDGIVET_ENDNU = {}


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
    # HER STOD EN LOEKKE DER SPURGTE "staar praefikset nogen steder i
    # release.yml". Den kunne ikke fejle: "zscreen-" staar i
    # secure/zscreen-signing.pem, saa den var tilfreds uanset om der
    # blev udgivet en fil med det navn.
    #
    # Det rigtige spoergsmaal - hedder filen i "files:" det samme som
    # skaermen leder efter - bliver stillet ÉT sted, i
    # tjek_filnavnene. To funktioner der svarer paa det samme, hvor den
    # ene ikke kan fejle, er vaerre end én: en fremtidig aendring bliver
    # meldt af den ene og vinket igennem af den anden.
    return ud


# ------------------------------------------------------------------
# RAEKKEFOELGEN I UDGIVELSEN
# ------------------------------------------------------------------
#
# Skaerme paa v0.27.0 og aeldre kender ikke praefikset. De tager den
# FOERSTE .bin i udgivelsen - se noten ved zs_version_asset_passer. Saa
# zScreens fil skal ligge op foerst, og det maa ikke afhaenge af hvilket
# job GitHub tilfaeldigvis koerer hurtigst.
#
# Reglen: det job der udgiver zScreen, venter ikke paa noget. Hvert
# ANDET job der udgiver en .bin, SKAL vente paa det.
JOB = re.compile(r"^  ([a-z0-9][a-z0-9-]*):\s*$", re.M)
FOERST = "zscreen-"


def jobbene(tekst):
    """Deler release.yml op i job. Tekst, ikke YAML - stdlib kun.

    KUN DET DER STAAR UNDER "jobs:". Moensteret rammer en noegle med to
    mellemrum foran, og det goer "push:" under "on:" ogsaa. Foerste
    udgave gav derfor et spoegelsesjob "push", hvis krop var filens
    forord. Det gik godt alene fordi det forord ikke indeholder en
    "files:"-linje - altsaa et held, ikke en regel.
    """
    ud = {}
    m = re.search(r"^jobs:\s*$", tekst, re.M)
    if m is None:
        return ud
    tekst = tekst[m.end():]
    traef = list(JOB.finditer(tekst))
    for i, m in enumerate(traef):
        slut = traef[i + 1].start() if i + 1 < len(traef) else len(tekst)
        ud[m.group(1)] = tekst[m.end():slut]
    return ud


def tjek_raekkefoelgen(tekst):
    job = jobbene(tekst)
    if not job:
        return ["fandt ingen job i release.yml. Er filen skrevet om? "
                "Saa er raekkefoelgen ikke laengere tjekket"]
    ud = []
    foerst_job = None
    andre = []
    for navn, krop in job.items():
        # Det er FILNAVNET der afgoer hvem der udgiver hvad, og
        # ikke om strengen staar i jobbet. Foerste forsoeg
        # ledte efter "zscreen-" hvor som helst i kroppen, og
        # saa pegede den paa ZDisplay-jobbet - fordi
        # signeringsnoeglen hedder secure/zscreen-signing.pem.
        # Vagten sagde fra paa noeglens navn.
        m = re.search(r"^\s*files:\s*(\S+)", krop, re.M)
        if m is None:
            continue                      # bygger ikke, udgiver ikke
        if m.group(1).startswith(FOERST):
            foerst_job = navn
        else:
            andre.append((navn, krop))

    if foerst_job is None:
        return [f"intet job i release.yml udgiver {FOERST}*.bin. Enten er "
                f"zScreen holdt op med at blive udgivet, eller filnavnet "
                f"er aendret - og saa henter hver zScreen i marken "
                f"ingenting"]

    if re.search(r"^\s*needs:", job[foerst_job], re.M):
        ud.append(f"jobbet {foerst_job} udgiver {FOERST}*.bin og venter "
                  f"paa et andet job. Saa er zScreens fil ikke laengere "
                  f"den foerste i udgivelsen, og gamle skaerme henter "
                  f"den forkerte firmware")

    for navn, krop in andre:
        m = re.search(r"^\s*needs:\s*(\S+)", krop, re.M)
        if m is None:
            ud.append(f"jobbet {navn} udgiver en fil uden at vente paa "
                      f"{foerst_job}. Saa afgoer tilfaeldet hvilken .bin "
                      f"der ligger foerst i udgivelsen, og skaerme paa "
                      f"v0.27.0 og aeldre tager den foerste")
        elif m.group(1) != foerst_job:
            ud.append(f"jobbet {navn} venter paa {m.group(1)} og ikke paa "
                      f"{foerst_job}, som udgiver {FOERST}*.bin")
    return ud


def tjek_container_vaerktoej(tekst):
    """Et job i en container maa ikke bruge runnerens vaerktoej.

    HVORFOR DEN FINDES.

    Kontrollen af udgivelsens raekkefoelge laa foerst som sidste trin i
    release-zdisplay, som koerer i container: espressif/idf:v5.4.2. Den
    kaldte gh.

    Et container-job faar sit vaerktoej fra BILLEDET og ikke fra
    runneren. gh, jq og aws ligger paa runneren. Trinnet ville derfor
    doe paa "gh: command not found" ved HVER udgivelse - efter at begge
    binaere var lagt op - og operatoeren ville se en shell-fejl i
    stedet for den besked der var skrevet til ham. Vagten der skulle
    bevise raekkefoelgen, maalte ingenting.

    Det er ikke en byggefejl, og intet i repoet sagde fra. Det her
    goer.
    """
    ud = []
    # Vaerktoej der ligger paa runneren og IKKE i espressif/idf.
    RUNNER_VAERKTOEJ = ("gh", "jq", "aws", "docker")
    for navn, krop in jobbene(tekst).items():
        # KOMMENTARER TAELLER IKKE MED. Foerste udgave af vagten faldt
        # paa noten der forklarer hvorfor gh ikke maa staa i en
        # container - ordet "gh" stod i prosaen. Og en kommentarblok
        # FOER et job tilregnes det forrige job, saa noten til det ene
        # job landede i kroppen af det andet.
        krop = "\n".join(l for l in krop.splitlines()
                         if not l.lstrip().startswith("#"))
        if not re.search(r"^\s+container:", krop, re.M):
            continue
        for v in RUNNER_VAERKTOEJ:
            # Kun som kommando i starten af et ord, saa "github" og
            # "jquery" ikke taeller med.
            if re.search(rf"(^|[|&;(\s]){v}\s", krop):
                ud.append(
                    f"jobbet {navn} koerer i en container OG bruger "
                    f"{v}. Det vaerktoej ligger paa runneren, ikke i "
                    f"billedet, saa trinnet doer paa "
                    f"\"{v}: command not found\". Laeg trinnet i et "
                    f"eget job uden container, eller installer {v} i "
                    f"trinnet")
    return ud


def tjek_filnavnene(tekst, praefikser):
    """Hedder filen i 'files:' det samme som skaermen leder efter."""
    ud = []
    udgivet = set(re.findall(r"^\s*files:\s*(\S+)", tekst, re.M))
    for pre in praefikser:
        if pre in IKKE_UDGIVET_ENDNU:
            # Noten stod foer i tjek_praefikser. Den foelger med hertil,
            # fordi det er HER spoergsmaalet bliver stillet.
            print(f"  {GROEN}note{SLUT} {pre}* bliver ikke udgivet endnu: "
                  f"{IKKE_UDGIVET_ENDNU[pre]}")
            continue
        if not any(f.startswith(pre) for f in udgivet):
            ud.append(f"release.yml udgiver {sorted(udgivet)}, og ingen af "
                      f"dem begynder med {pre} - som er praefikset en "
                      f"skaerm leder efter. Et filnavn der er taet paa, er "
                      f"ikke godt nok: matchningen er strncmp")
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

    rel = (ROD / ".github/workflows/release.yml").read_text(encoding="utf-8")
    fejl = tjek_praefikser(rel)
    fejl += tjek_raekkefoelgen(rel)
    fejl += tjek_container_vaerktoej(rel)
    fejl += tjek_filnavnene(
        rel, PRAEFIKS.findall(
            (ROD / "firmware/kerne/zs_config.h").read_text(encoding="utf-8")))
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
          f"{', '.join(sorted(fundet['release.yml']))}, alle er faste, "
          f"begge produkter bliver udgivet, og zScreens fil ligger op "
          f"foerst.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
