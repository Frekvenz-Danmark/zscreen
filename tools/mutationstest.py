#!/usr/bin/env python3
"""
zScreen - mutationstest: oedelaeg koden med vilje og se om proeverne opdager det.

HVORFOR DEN FINDES.

"941 proever, alle bestaaet" siger intet om hvor meget af koden der
faktisk bliver holdt fast. En proeve der ikke kan fejle, er ingen proeve.
Den eneste maade at maale det paa er at saette en fejl ind med vilje og se
om nogen siger fra.

Og det virker. Den har fundet rigtige fejl, ikke kun manglende proever:

  * ombytningen af store bogstaver i maerke-soegningen havde en oevre
    graense der aldrig blev ramt, fordi det eneste maerke i tabellen
    hedder "Fronius" og ikke indeholder et Z. Et maerke som Zeversolar
    eller ZCS, der begge findes, kunne ikke findes af koden.
  * vagten "ud != NULL && len > 0" stod skrevet forfra fire steder, og
    ingen proeve gav NULL med. && kunne laves til || uden at nogen sagde
    fra, og saa skriver vi i NULL.
  * errno-vurderingen i send og modtag kunne vendes om, saa HVER fejl
    blev meldt som "svarede ikke i tide". Kunden ville lede efter et
    langsomt netvaerk i stedet for efter en inverter der har lukket.

SAADAN LAESES RESULTATET.

En mutant der DOER er godt: proeverne fangede den. En der OVERLEVER er
ét af tre:

  1. et hul i proeverne. Skriv en proeve.
  2. en aendring der ikke kan ses udefra. Saa skal beslutningen trækkes
     ud som en ren funktion der KAN proeves, se zs_mb_errno_art og
     zs_mb_maal_ok i zs_modbus_tcp.h. Det er ikke snyd: det er at gøre
     en beslutning synlig.
  3. en aendring der ikke betyder noget, fx EAGAIN mod EWOULDBLOCK som er
     samme tal paa baade macOS og Linux. Skriv den paa listen UDEN_BETYDNING
     nedenfor med en begrundelse, saa den ikke skal vurderes forfra.

KOERES I HAANDEN, ikke af tests/run-all.sh: hver mutant koster en hel
oversaettelse og en hel proevekoersel, saa det tager minutter, ikke
sekunder.

    python3 tools/mutationstest.py              alle moduler
    python3 tools/mutationstest.py zs_tekst     kun ét modul
    python3 tools/mutationstest.py --vis        vis hvad der ville blive proevet
"""

import io
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

KILDE = pathlib.Path(__file__).resolve().parent.parent

# Der bliver arbejdet paa en KOPI, ikke i arbejdstraeet.
#
# Vaerktoejet skriver en fejl ind i en kildefil, koerer proeverne, og
# skriver filen tilbage. Gaar noget galt midt i det, eller bliver
# vaerktoejet afbrudt, saa staar der en fejl tilbage i koden. Og vaerre:
# bygger man firmwaren samtidig, bygger man en mutant uden at vide det.
# Vi har én gang foer faaet en rigtig fil skrevet over af et vaerktoej,
# saa koden bliver kopieret til en midlertidig mappe og alt sker derinde.
# Arbejdstraeet kan ikke roeres.
ROOT = None     # saettes af forbered()


def forbered():
    """Laver en kopi af det der skal til for at koere proeverne."""
    global ROOT
    mappe = pathlib.Path(tempfile.mkdtemp(prefix="zs-mutation-"))
    for del_ in ("firmware/main", "tests", "tools"):
        kilde = KILDE / del_
        maal = mappe / del_
        maal.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(kilde, maal, symlinks=True,
                        ignore=shutil.ignore_patterns("build", "*.dSYM", "ud"))
    ROOT = mappe
    return mappe

# Modulerne der koerer i tests/host, altsaa dem en mutant kan maales paa.
# De rene beslutninger, ikke hardware. Rækkefoelgen er den de staar i
# run.sh, saa det er let at se om der mangler et.
MODULER = [
    "firmware/main/util/zs_tekst.c",
    "firmware/main/net/zs_modbus_tcp.c",
    "firmware/main/net/zs_sunspec.c",
    "firmware/main/net/zs_maerker.c",
    "firmware/main/net/zs_energi.c",
    "firmware/main/net/zs_locate.c",
    "firmware/main/net/zs_fleet_msg.c",
    "firmware/main/net/zs_price_now.c",
    "firmware/main/net/zs_version.c",
    "firmware/main/app/zs_format.c",
    "firmware/main/ui/zs_tilegrid.c",
]

# Hvad vi bytter om. Kun aendringer der stadig kan oversaettes, og kun
# saadanne hvor en rigtig fejl ville se praecis saadan ud.
BYT = [
    (r"(?<![<>=!])>=(?!=)", ">"),
    (r"(?<![<>=!])<=(?!=)", "<"),
    (r"(?<![<>=!])==(?!=)", "!="),
    (r"(?<![<>=!])!=(?!=)", "=="),
    (r"&&", "||"),
    (r"\|\|", "&&"),
]

# Mutanter der overlever med vilje, med begrundelse. Teksten skal vaere
# den FOERSTE linje som den staar i filen, saa den er til at finde igen.
UDEN_BETYDNING = {
    "errno == EAGAIN || errno == EWOULDBLOCK":
        "EAGAIN og EWOULDBLOCK er samme tal paa baade macOS (35) og Linux "
        "(11), saa de to grene er den samme gren",
}


def linjer(fil):
    """Linjerne vi maa mutere: ingen kommentarer, ingen tomme."""
    tekst = io.open(ROOT / fil, encoding="utf-8").read()
    ud = []
    i_blok = False
    for nr, linje in enumerate(tekst.split("\n"), 1):
        strippet = linje.strip()
        if i_blok:
            if "*/" in strippet:
                i_blok = False
            continue
        if strippet.startswith("/*"):
            if "*/" not in strippet:
                i_blok = True
            continue
        if strippet.startswith("//") or strippet.startswith("*") or not strippet:
            continue
        # Teksten i anfoerselstegn roerer vi ikke: en aendring der kun
        # rammer en dansk besked er ikke en fejl i logikken.
        uden_tekst = re.sub(r'"(\\.|[^"\\])*"', '""', linje)
        ud.append((nr, linje, uden_tekst))
    return tekst, ud


def mutanter(fil):
    tekst, ls = linjer(fil)
    ud = []
    for nr, linje, uden_tekst in ls:
        for moenster, til in BYT:
            for m in re.finditer(moenster, uden_tekst):
                fra_tegn = uden_tekst[m.start():m.end()]
                ny = linje[:m.start()] + til + linje[m.end():]
                if ny == linje:
                    continue
                ud.append((nr, linje, ny, fra_tegn, til))
    return tekst, ud


def byg_probe():
    """Bygger zs-probe i kopien, BEGGE udgaver.

    Den sanitiserede er den vigtige: ende til ende koerer mod
    zs-probe-san, ikke mod zs-probe. Foerste udgave af det her vaerktoej
    byggede kun den almindelige, og saa proevede ende til ende en binaer
    der aldrig blev roert af mutationen. Den kunne altsaa ikke fange
    noget som helst, og vaerktoejet ville melde huller der ikke var der.
    """
    miljoe = dict(os.environ, ZS_SANITIZE="1")
    for env in (os.environ, miljoe):
        try:
            r = subprocess.run(["./build.sh"], cwd=ROOT / "tools/zs-probe",
                               capture_output=True, text=True,
                               env=dict(env), timeout=300)
        except subprocess.TimeoutExpired:
            return False, "oversaettelsen haengte"
        if r.returncode != 0:
            return False, r.stdout + r.stderr
    return True, ""


def koer_proever():
    """Host-proever foerst, og KUN hvis de bestaar, ende til ende.

    Raekkefoelgen er vigtig for tiden det tager. Host-proeverne tager et
    par sekunder, ende til ende tager over et minut. De fleste mutanter
    doer paa host, og saa skal der ikke koeres mere.
    """
    #
    # MED TIDSGRAENSE. En mutant kan faa en loekke til at koere for evigt,
    # fx hvis "i < n" bliver "i != n" og i springer over n. Uden en
    # graense her ville vaerktoejet staa stille for evigt paa den ene
    # mutant, og det er praecis sket: 28 mutanter var en halv time inde
    # uden at vaere faerdige.
    #
    # Og en mutant der haenger ER fanget: opfoerslen er aendret paa en
    # maade proeverne kan se. Vi skriver bare hvad der skete, saa man kan
    # skelne en haengende mutant fra en der fejlede paent.
    #
    try:
        r = subprocess.run(["./run.sh"], cwd=ROOT / "tests/host",
                           capture_output=True, text=True, timeout=180)
    except subprocess.TimeoutExpired:
        return True, "host (den haengte)"
    ud = r.stdout + r.stderr
    if r.returncode != 0 or "FEJL" in ud or "error:" in ud:
        return True, "host"

    byggede, log = byg_probe()
    if not byggede:
        return True, "oversaettelsen af vaerktoejet"
    try:
        r = subprocess.run([sys.executable, "run.py"], cwd=ROOT / "tests/e2e",
                           capture_output=True, text=True, timeout=900)
    except subprocess.TimeoutExpired:
        return True, "ende til ende (den haengte)"
    ud = r.stdout + r.stderr
    if r.returncode != 0 or "FEJL" in ud:
        return True, "ende til ende"
    return False, None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    bare_vis = "--vis" in sys.argv

    global ROOT
    if bare_vis:
        ROOT = KILDE          # der skrives ikke noget, saa en kopi er spild
    else:
        mappe = forbered()
        print(f"\n  arbejder paa en kopi i {mappe}")
        print(f"  arbejdstraeet bliver ikke roert")

    valgte = MODULER
    if args:
        valgte = [m for m in MODULER if any(a in m for a in args)]
        if not valgte:
            print(f"ingen af modulerne passer paa {args}")
            print("de der findes:")
            for m in MODULER:
                print(f"  {pathlib.Path(m).stem}")
            return 1

    alle = []
    for fil in valgte:
        tekst, ms = mutanter(fil)
        for nr, gammel, ny, fra_tegn, til in ms:
            alle.append((fil, tekst, nr, gammel, ny, fra_tegn, til))

    print(f"\n\033[1mMutationstest\033[0m")
    print(f"  {len(valgte)} moduler, {len(alle)} mutanter\n")

    if bare_vis:
        for fil, _, nr, gammel, _, fra_tegn, til in alle:
            print(f"  {pathlib.Path(fil).name:20s}:{nr:4d}  "
                  f"{fra_tegn:3s} -> {til}")
        return 0

    overlevede = []
    sprunget = 0
    for i, (fil, tekst, nr, gammel, ny, fra_tegn, til) in enumerate(alle, 1):
        navn = f"{pathlib.Path(fil).name}:{nr}"

        undtagelse = None
        for noegle, hvorfor in UDEN_BETYDNING.items():
            if noegle in gammel:
                undtagelse = hvorfor
                break

        p = ROOT / fil
        org = io.open(p, encoding="utf-8").read()
        linjevis = org.split("\n")
        if linjevis[nr - 1] != gammel:
            sprunget += 1
            continue
        linjevis[nr - 1] = ny
        try:
            io.open(p, "w", encoding="utf-8").write("\n".join(linjevis))
            doed, hvem = koer_proever()
        finally:
            io.open(p, "w", encoding="utf-8").write(org)

        if doed:
            print(f"  \033[1;32mdød\033[0m    [{i}/{len(alle)}] {navn}  "
                  f"{fra_tegn} -> {til}   (fanget af {hvem})")
        elif undtagelse is not None:
            print(f"  \033[1;33mok\033[0m     [{i}/{len(alle)}] {navn}  "
                  f"{fra_tegn} -> {til}   uden betydning: {undtagelse}")
        else:
            print(f"  \033[1;31mLEVER\033[0m  [{i}/{len(alle)}] {navn}  "
                  f"{fra_tegn} -> {til}")
            print(f"              {gammel.strip()}")
            overlevede.append((navn, gammel.strip(), fra_tegn, til))

    print("\n" + "─" * 40)
    if sprunget:
        print(f"  {sprunget} sprunget over (linjen ser ikke ud som ventet)")
    if not overlevede:
        print(f"\033[1;32malle {len(alle)} mutanter blev fanget\033[0m\n")
        return 0
    print(f"\033[1;31m{len(overlevede)} af {len(alle)} mutanter overlevede\033[0m")
    print("  Hver af dem er enten et hul i proeverne, en beslutning der skal")
    print("  traekkes ud saa den kan proeves, eller noget uden betydning der")
    print("  skal paa listen UDEN_BETYDNING i denne fil. Se noten foeroven.\n")
    for navn, gammel, fra_tegn, til in overlevede:
        print(f"    {navn}  {fra_tegn} -> {til}")
        print(f"      {gammel}")
    return 1


if __name__ == "__main__":
    try:
        kode = main()
    finally:
        if ROOT is not None and ROOT != KILDE:
            shutil.rmtree(ROOT, ignore_errors=True)
    sys.exit(kode)
