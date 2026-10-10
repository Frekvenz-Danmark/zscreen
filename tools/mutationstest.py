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
import signal
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

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

# HVOR FREMDRIFTEN SKRIVES.
#
# En hel koersel tager timer: der er 374 mutanter, og hver af dem der IKKE
# bliver fanget af enhedstestene skal igennem en bygning og hele datavejen.
# Foer kunne man ikke se hvor langt den var, og en koersel man ikke kan se
# ind i er ikke til at skelne fra en der haenger. Vi har spildt tid paa
# netop det spoergsmaal: arbejder den, eller venter vi paa ingenting?
#
# Nu skrives hver linje OGSAA til en fil, med flush hver gang, saa man kan
# foelge med:    tail -f <stien der skrives foeroven>
LOG = None

# Et fast sted, saa man kan finde den uden at kende mappenavnet, og saa den
# bliver staaende naar kopien er ryddet op.
LOGSTI = pathlib.Path(tempfile.gettempdir()) / "zs-mutation-fremdrift.log"


def sig(linje=""):
    """Skriver baade paa skaermen og i loggen, og tvinger den ud med det
    samme. Uden flush staar der ingenting i filen foer bufferen er fuld,
    og det er netop en lang koersel man vil kigge paa undervejs."""
    print(linje, flush=True)
    if LOG is not None:
        LOG.write(linje + "\n")
        LOG.flush()


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
# De rene beslutninger, ikke hardware. Raekkefoelgen er den de staar i
# run.sh, saa det er let at se om der mangler et.
#
# "Let at se" var ikke nok. Tre moduler - zs_tid, zs_kvarter og
# zs_tilmeld - blev skrevet med proever og stod i run.sh, men ingen
# huskede at skrive dem her. De slap altsaa helt uden om mutationstesten,
# og det var ikke til at se paa noget: testen sagde "alle mutanter doer"
# om de moduler den kendte. Derfor staar der nu en vagt nedenfor der
# laeser run.sh og siger fra. Se tjek_listen.
MODULER = [
    "firmware/main/util/zs_tekst.c",
    "firmware/main/util/zs_tid.c",
    "firmware/main/net/zs_tilmeld.c",
    "firmware/main/net/zs_ladeplan.c",
    "firmware/main/net/zs_ladeordre.c",
    "firmware/main/net/zs_modbus_tcp.c",
    "firmware/main/net/zs_sunspec.c",
    "firmware/main/net/zs_maerker.c",
    "firmware/main/net/zs_energi.c",
    "firmware/main/net/zs_kvarter.c",
    "firmware/main/app/zs_format.c",
    "firmware/main/net/zs_version.c",
    "firmware/main/ui/zs_tilegrid.c",
    "firmware/main/ui/zs_qr.c",
    "firmware/main/net/zs_price_now.c",
    "firmware/main/net/zs_fleet_msg.c",
    "firmware/main/net/zs_locate.c",
]

# De moduler i run.sh der med vilje IKKE muteres, og hvorfor. Staar et
# modul hverken her eller i MODULER, falder tjek_listen.
UDEN_MUTATION = {
    "firmware/main/app/zs_demo.c":
        "grunden er ikke skrevet ned. Skal ses paa: den har proever i "
        "test_demo.c, saa den kan sandsynligvis muteres",
    "firmware/main/net/zs_discovery.c":
        "aabner sockets og scanner netvaerket, saa en mutant maales paa "
        "hardware og timing og ikke paa en beslutning",
}


def tjek_listen():
    """Siger fra hvis et modul i run.sh hverken muteres eller er undtaget.

    Den gamle aftale var at MODULER stod i samme raekkefoelge som run.sh,
    "saa det er let at se om der mangler et". Det holdt ikke: tre moduler
    blev glemt, og intet sagde fra. Nu laeses run.sh.
    """
    sti = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "tests", "host", "run.sh")
    with open(sti, encoding="utf-8") as f:
        tekst = f.read()
    i_run = re.findall(r"\.\./\.\./(firmware/main/[a-z]+/zs_[a-z_0-9]+\.c)",
                       tekst)
    glemt = [m for m in i_run
             if m not in MODULER and m not in UDEN_MUTATION]
    doede = [m for m in MODULER if m not in i_run]
    if glemt:
        sig("  DE HER MODULER KOERER I TESTENE MEN MUTERES IKKE:")
        for m in glemt:
            sig(f"    {m}")
        sig("  Skriv dem i MODULER, eller i UDEN_MUTATION med en grund.")
    if doede:
        sig("  DE HER STAAR I MODULER MEN IKKE I run.sh LAENGERE:")
        for m in doede:
            sig(f"    {m}")
    return not (glemt or doede)

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
# NOEGLEN MAA IKKE INDEHOLDE ET VARIABELNAVN.
#
# Her stod "errno == EAGAIN || errno == EWOULDBLOCK". Da beslutningen blev
# trukket ud i zs_mb_errno_art, kom den til at hede fejlnr i stedet for
# errno, og saa passede noeglen ikke mere. Undtagelsen holdt TAVST op med
# at virke, og mutanten dukkede op som overlevende i en koersel maaneder
# senere, hvor ingen kunne huske hvorfor den var undtaget.
#
# Noeglen skal derfor vaere den del der ikke aendrer sig naar en variabel
# bliver omdoebt. Og vagten nedenfor, tjek_undtagelser, naegter at koere
# hvis en undtagelse ikke passer paa ÉN eneste linje mere. Saa kan den
# ikke blive tavs igen.
UDEN_BETYDNING = {
    "EAGAIN || ":
        "EAGAIN og EWOULDBLOCK er SAMME tal: 35 paa macOS, og i ESP-IDF's "
        "newlib staar der ligefrem '#define EWOULDBLOCK EAGAIN'. Efterset "
        "i begge, ikke antaget. De to grene er altsaa den samme gren, og "
        "ingen proeve kan skelne dem. Begge staar der alligevel, fordi "
        "POSIX TILLADER at de er forskellige",
}


def tjek_undtagelser(alle):
    """Passer hver undtagelse stadig paa noget?

    En undtagelse der ikke passer paa noget, er ikke uskadelig. Den er en
    vagt der er faldet af uden at nogen saa det, og saa staar der en mutant
    og overlever med en begrundelse ingen kan finde.
    """
    brugt = set()
    for _, _, _, gammel, _, _, _ in alle:
        for noegle in UDEN_BETYDNING:
            if noegle in gammel:
                brugt.add(noegle)
    tabt = [n for n in UDEN_BETYDNING if n not in brugt]
    if not tabt:
        return True
    print("\n\033[1;31mEn undtagelse passer ikke paa noget mere:\033[0m")
    for n in tabt:
        print(f"    {n!r}")
    print("  Koden er sandsynligvis skrevet om. Ret noeglen, eller tag")
    print(f"  undtagelsen ud af UDEN_BETYDNING i {__file__}.")
    print("  Vi koerer ikke videre: en undtagelse der ikke passer, gemmer")
    print("  en mutant for os uden at sige det.")
    return False


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
                               capture_output=True, text=True, errors="replace",
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
                           capture_output=True, text=True, errors="replace", timeout=180)
    except subprocess.TimeoutExpired:
        return True, "host (den haengte)"
    ud = r.stdout + r.stderr
    if r.returncode != 0 or "FEJL" in ud or "error:" in ud:
        return True, "host"

    byggede, log = byg_probe()
    if not byggede:
        return True, "oversaettelsen af vaerktoejet"
    try:
        # 420 sekunder og ikke 900.
        #
        # Hele datavejen tager omkring 150 sekunder naar alt er normalt.
        # 420 er naesten tre gange saa laenge, rigeligt til en langsom
        # maskine, og kort nok til at en mutant der faar proeverne til at
        # HAENGE bliver opdaget i stedet for at koste et kvarter. Med
        # flere haengende mutanter var forskellen timer.
        r = subprocess.run([sys.executable, "run.py"], cwd=ROOT / "tests/e2e",
                           capture_output=True, text=True, errors="replace", timeout=420)
    except subprocess.TimeoutExpired:
        return True, "ende til ende (den haengte)"
    ud = r.stdout + r.stderr
    if r.returncode != 0 or "FEJL" in ud:
        return True, "ende til ende"
    return False, None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    bare_vis = "--vis" in sys.argv

    # Foerst: koerer der et modul i testene som INGEN muterer? Det skal
    # siges foer noget andet, for ellers lyder "alle mutanter doede" som
    # om hele koden var daekket.
    if not tjek_listen():
        return 1

    global ROOT
    if bare_vis:
        ROOT = KILDE          # der skrives ikke noget, saa en kopi er spild
    else:
        mappe = forbered()
        global LOG
        # Loggen ligger UDEN FOR kopien.
        #
        # Foerste forsoeg lagde den inde i kopien. Den blev saa ryddet op
        # sammen med kopien naar koerslen sluttede, og saa var der ingen
        # log tilbage at se paa bagefter. Den er netop til at kigge paa
        # BAGEFTER, naar man vil vide hvad der overlevede.
        LOG = io.open(LOGSTI, "w", encoding="utf-8")
        print(f"\n  arbejder paa en kopi i {mappe}")
        print(f"  arbejdstraeet bliver ikke roert")
        print(f"\n  FOELG MED:  tail -f {LOGSTI}")

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

    # Foer der bruges timer paa at oversaette: passer undtagelserne stadig?
    #
    # Og de maales mod ALLE moduler, ikke mod dem der er valgt paa
    # kommandolinjen. Gjorde vi det sidste, ville "mutationstest.py
    # zs_tekst" altid falde: undtagelsen for EAGAIN staar i Modbus-laget,
    # og den passer naturligvis ikke paa et modul om tekst. Saa var den
    # dokumenterede enkeltmodul-koersel umulig, og den fejl lignede en
    # foraeldet undtagelse.
    #
    # Det koster kun at laese filerne igennem, ikke at oversaette.
    til_tjek = alle
    if valgte is not MODULER and len(valgte) != len(MODULER):
        til_tjek = []
        for fil in MODULER:
            tekst, ms = mutanter(fil)
            for nr, gammel, ny, fra_tegn, til in ms:
                til_tjek.append((fil, tekst, nr, gammel, ny, fra_tegn, til))
    if not tjek_undtagelser(til_tjek):
        return 2

    sig(f"\n\033[1mMutationstest\033[0m")
    sig(f"  {len(valgte)} moduler, {len(alle)} mutanter")
    if not bare_vis:
        # Et skoen, saa ingen sidder og venter uden at vide paa hvad.
        #
        # En mutant enhedstestene fanger koster omkring ti sekunder. En der
        # slipper forbi dem skal igennem en bygning og hele datavejen, og
        # det er omkring tre minutter. Erfaringen er at omkring én af ti
        # slipper forbi.
        hurtige = len(alle) * 0.9 * 10
        langsomme = len(alle) * 0.1 * 180
        timer = (hurtige + langsomme) / 3600.0
        sig(f"  skoennet tid: omkring {timer:.1f} timer. Den skriver hver "
            f"mutant undervejs,")
        sig(f"  saa den er til at foelge, og den kan afbrydes uden at "
            f"efterlade noget.")
    sig("")

    if bare_vis:
        for fil, _, nr, gammel, _, fra_tegn, til in alle:
            print(f"  {pathlib.Path(fil).name:20s}:{nr:4d}  "
                  f"{fra_tegn:3s} -> {til}")
        return 0

    overlevede = []
    sprunget = 0
    begyndt = time.time()
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

        # Hvor langt er vi, og hvor laenge er der igen. Det staar paa hver
        # linje med vilje: saadan kan man se paa ÉN linje om den rykker sig,
        # uden at skulle regne.
        gaaet = time.time() - begyndt
        igen = (gaaet / i) * (len(alle) - i)
        naar = f"{gaaet/60:.0f}m gaaet, ~{igen/60:.0f}m igen"

        if doed:
            sig(f"  \033[1;32mdød\033[0m    [{i}/{len(alle)}] {navn}  "
                f"{fra_tegn} -> {til}   (fanget af {hvem})   {naar}")
        elif undtagelse is not None:
            sig(f"  \033[1;33mok\033[0m     [{i}/{len(alle)}] {navn}  "
                f"{fra_tegn} -> {til}   uden betydning: {undtagelse}   {naar}")
        else:
            sig(f"  \033[1;31mLEVER\033[0m  [{i}/{len(alle)}] {navn}  "
                f"{fra_tegn} -> {til}   {naar}")
            sig(f"              {gammel.strip()}")
            overlevede.append((navn, gammel.strip(), fra_tegn, til))

    sig("\n" + "─" * 40)
    if sprunget:
        sig(f"  {sprunget} sprunget over (linjen ser ikke ud som ventet)")
    if not overlevede:
        sig(f"\033[1;32malle {len(alle)} mutanter blev fanget\033[0m\n")
        return 0
    sig(f"\033[1;31m{len(overlevede)} af {len(alle)} mutanter overlevede\033[0m")
    sig("  Hver af dem er enten et hul i proeverne, en beslutning der skal")
    sig("  traekkes ud saa den kan proeves, eller noget uden betydning der")
    sig("  skal paa listen UDEN_BETYDNING i denne fil. Se noten foeroven.\n")
    for navn, gammel, fra_tegn, til in overlevede:
        sig(f"    {navn}  {fra_tegn} -> {til}")
        sig(f"      {gammel}")
    return 1


def ryd_op():
    if ROOT is not None and ROOT != KILDE:
        shutil.rmtree(ROOT, ignore_errors=True)
    global LOG
    if LOG is not None:
        try:
            LOG.close()
        except Exception:
            pass
        LOG = None


def afbrudt(signalnr, _ramme):
    """Bliver vi stoppet udefra, skal kopien RYDDES OP.

    Foer laa der en mappe tilbage hver gang en koersel blev afbrudt, og den
    blev ikke fundet igen. Et finally daekker ikke et signal, saa det skal
    haandteres for sig."""
    ryd_op()
    sys.exit(130 if signalnr == signal.SIGINT else 143)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, afbrudt)
    signal.signal(signal.SIGTERM, afbrudt)
    try:
        kode = main()
    finally:
        ryd_op()
    sys.exit(kode)
