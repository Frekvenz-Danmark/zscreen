#!/usr/bin/env python3
"""
zScreen - test af hele datavejen.

Enhedstestene i tests/host tjekker de enkelte dele hver for sig. Det
her er den anden slags: en simuleret Fronius startes, firmwarens EGEN
kode taler med den over en rigtig TCP-forbindelse, og vi sammenligner
de fire tal skaermen ville vise med det simulatoren siger den har.

Det fanger det enhedstestene ikke kan: at to dele hver for sig er
rigtige, men er uenige om hvad de sender til hinanden. Det var
praecis saadan fejlen med elmaalerens modelnumre kom igennem: baade
koden og testen gik ud fra det samme forkerte.

    python3 tests/e2e/run.py            alle profiler
    python3 tests/e2e/run.py -v         med al udskrift
"""

import os
import atexit
import re
import signal
import socket
import struct
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SIM = os.path.join(ROOT, "tools", "fronius-sim", "serve.py")
# Hvilken zs-probe der proeves. Testene koerer mod den sanitiserede
# udgave, se tools/zs-probe/build.sh for hvorfor. ZS_PROBE kan pege et
# andet sted hen, fx naar man fejlsoeger med den almindelige.
PROBE = os.environ.get(
    "ZS_PROBE", os.path.join(ROOT, "tools", "zs-probe", "zs-probe-san"))
def _porten_svarer(port, vaert="127.0.0.1", timeout=0.3):
    """Er der noget der lytter? Bruges baade foer og efter en hjaelpeproces."""
    try:
        socket.create_connection((vaert, port), timeout=timeout).close()
        return True
    except OSError:
        return False


def hold_en_fri_port():
    """Giver (socket, port) hvor der GARANTERET ikke lytter noget.

    Vi beholder socket'en, saa ingen anden kan tage porten imens, og vi
    kalder med vilje IKKE listen(): et forsoeg paa at forbinde bliver
    afvist praecis som naar der slet ikke er noget. Det er forskellen paa
    en proeve der altid proever det samme, og en der haaber at ingen havde
    taget den port vi gættede paa.

    Kalderen lukker socket'en naar den er faerdig.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    return s, s.getsockname()[1]


# PORTEN DEN HJAELPEPROCES DER KOERER NU LYTTER PAA.
#
# HVORFOR DEN IKKE STAAR FAST, OG HVORFOR VI IKKE LEDER EFTER EN LEDIG.
#
# Foer stod porten fast paa 15020. Det faldt fra hinanden den dag der
# koerte to ting paa maskinen samtidig: mutationstesten koerer ogsaa hele
# vejen igennem, saa to koersler saa 15020 som fri i det SAMME oejeblik og
# tog den begge to. To simulatorer paa én port, og proeverne laeste paa
# den forkerte. Fem og tyve proever fejlede med tal fra et helt andet
# anlaeg, og fejlen pegede paa firmwaren. Den var ikke der.
#
# Saa blev der ledt efter en ledig port i stedet. Det er bedre, men det er
# stadig et gaet: mellem at vi ser porten fri og at simulatoren binder
# den, kan en anden have taget den. Et vindue paa et par millisekunder er
# stadig et vindue, og den slags fejl kommer igen naar man mindst har tid
# til den.
#
# Nu bliver simulatoren startet med --port 0. Saa vaelger STYRESYSTEMET,
# og det er den eneste der kan garantere at ingen anden har porten.
# Simulatoren skriver den den fik, og vi laeser den. To koersler kan ikke
# laengere komme til at tage den samme, uanset hvor mange der koerer.
#
# ZS_PORT kan stadig laase den fast naar man fejlsoeger og vil kunne se
# med udefra.
PORT = int(os.environ["ZS_PORT"]) if os.environ.get("ZS_PORT") else None

VERBOSE = "-v" in sys.argv

fejl = 0
tjek = 0


def ok(hvad):
    global tjek
    tjek += 1
    print(f"  \033[1;32m ok \033[0m {hvad}")


def fail(hvad, detalje=""):
    global tjek, fejl
    tjek += 1
    fejl += 1
    print(f"  \033[1;31mFEJL\033[0m {hvad}")
    if detalje:
        for line in str(detalje).splitlines():
            print(f"         {line}")


def suite(navn):
    print(f"\n\033[1m{navn}\033[0m")


# ----------------------------------------------------------------------

# Alle hjaelpeprocesser vi har startet, saa de kan lukkes ned selv om
# proeverne bliver afbrudt.
#
# HVORFOR. Bliver run.py stoppet midt i, fx med Ctrl-C eller fordi en
# ydre tidsgraense loeb ud, saa lever simulatoren og vaerktoejet videre
# som foraeldreloese processer og bliver ved at banke paa porten. Naeste
# koersel starter sin egen simulator, den gamle scanning aeder
# forbindelserne, og en proeve falder med en besked der intet har at goere
# med sagen. Det er sket to gange, og begge gange blev der ledt efter en
# fejl i koden der ikke var der.
_boern = []


def _ryd_op(*a):
    for p in list(_boern):
        try:
            if p.poll() is None:
                os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except OSError:
            pass
    _boern.clear()


def _afbrudt(signalnr, ramme):
    _ryd_op()
    print(f"\n\033[1;33mAfbrudt\033[0m, hjaelpeprocesserne er lukket ned\n")
    sys.exit(130)


atexit.register(_ryd_op)
for _s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
    try:
        signal.signal(_s, _afbrudt)
    except (OSError, ValueError):
        pass


def luk_ned(p, port, navn):
    """Lukker en hjaelpeproces HELT ned, og siger fra hvis porten ikke bliver fri.

    HVORFOR DEN FINDES, OG HVORFOR DEN ER SAA GRUNDIG.

    Baade Sim og FalskEnhed havde deres egen nedlukning, og begge
    lignede hinanden: et SIGTERM, et wait paa tre sekunder, og et
    "except Exception: pass". Doede processen ikke inden for de tre
    sekunder, gav oprydningen TAVST op, og processen levede videre paa
    porten.

    Det er den slags fejl der koster mest, fordi den ikke falder her. Den
    falder i en senere proeve, som den der tjekker at skaermen siger fra
    naar inverteren forsvinder: inverteren var ikke forsvundet, den gamle
    simulator svarede stadig. Saa leder man efter en fejl i firmwaren der
    ikke findes. Det er praecis sket, og det kostede en halv time.

    Derfor: foerst paent, saa haardt, og til sidst et BEVIS for at porten
    er fri. Er den ikke det, raaber vi op her, hvor fejlen er opstaaet.
    """
    if p is not None:
        if p in _boern:
            _boern.remove(p)
        try:
            gruppe = os.getpgid(p.pid)
        except OSError:
            gruppe = None

        for signal_nr in (signal.SIGTERM, signal.SIGKILL):
            if gruppe is not None:
                try:
                    os.killpg(gruppe, signal_nr)
                except OSError:
                    pass
            try:
                p.wait(timeout=3)
                break
            except subprocess.TimeoutExpired:
                continue

    # Beviset. Et par forsoeg, for kernen er et oejeblik om at lukke en
    # lyttende socket ned.
    for _ in range(20):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.3).close()
            time.sleep(0.1)
        except OSError:
            return
    raise RuntimeError(
        f"{navn} svarer stadig paa port {port} efter nedlukning. Der er "
        f"noget andet der lytter, eller en tidligere koersel blev afbrudt "
        f"og lever videre. Ryd op med:  pkill -f fronius-sim")


class Sim:
    """Starter simulatoren og lukker den igen, ogsaa hvis noget gaar galt."""

    def __init__(self, profil, ekstra=None, port=None):
        self.profil = profil
        # None betyder "lad styresystemet vaelge", se noten ved PORT.
        self.port = port if port is not None else PORT
        # sys.executable og IKKE "python3".
        #
        # "python3" er hvad der tilfaeldigvis staar foerst i PATH. Paa
        # denne maskine er det en anden udgave end den proeverne selv kan
        # vaere startet med, og saa koerer simulatoren paa én Python og
        # proeverne paa en anden. Det er to ting at holde styr paa i
        # stedet for én, og et sted hvor en forskel kan vise sig som en
        # fejl i firmwaren. Simulatoren skal koere under PRAECIS den samme
        # udgave som proeverne.
        # --bind 127.0.0.1 staar HER og ikke ude i hver proeve.
        #
        # Nogle proever gav den med, andre gjorde ikke, og de sidste fik
        # saa en simulator paa 0.0.0.0, altsaa ude paa netvaerket. To ting
        # fulgte af det. Dels laa der en aaben Modbus-server paa nettet
        # mens proeverne koerte. Dels kunne _ledig_port ikke se den: man
        # kan godt binde 127.0.0.1:P mens en anden proces lytter paa
        # 0.0.0.0:P, saa porten saa fri ud og var det ikke.
        self.args = [sys.executable, SIM,
                     "--port", str(self.port if self.port else 0),
                     "--profile", profil,
                     "--bind", "127.0.0.1",
                     "--start-hour", "13", "--speed", "1", "--print-every", "1"]
        if ekstra:
            self.args += ekstra
        self.p = None
        self.log = []

    def __enter__(self):
        """
        Starter simulatoren, og er SIKKER paa at det er vores egen der
        svarer.

        Foer stod der bare "vent til porten svarer". Men svarede porten
        fordi der laa en simulator tilbage fra en tidligere proeve, saa
        koerte proeven videre mod DEN, og tallene kom fra et helt andet
        anlaeg. Det maa ikke kunne ske i tavshed.

        start_new_session og ikke preexec_fn=os.setsid: de to er maalt
        til at give samme resultat her, men preexec_fn koerer kode i
        barnet mellem fork og exec, og det er ikke sikkert naar der er
        traade i spil. Gik det nogensinde fejl, ville barnet havne i
        VORES egen procesgruppe, og saa sendte nedlukningen SIGTERM til
        hele testkoerslen i stedet for til simulatoren. start_new_session
        er den dokumenterede maade at bede om det samme uden den risiko.
        """
        # Er porten laast fast med ZS_PORT, skal den vaere fri FOER vi
        # starter. Vaelger styresystemet selv, kan den ikke vaere i brug,
        # og saa er der ingenting at tjekke.
        if self.port and _porten_svarer(self.port):
            raise RuntimeError(
                f"port {self.port} var i brug FOER simulatoren "
                f"({self.profil}) blev startet. Der ligger noget tilbage "
                f"fra en tidligere koersel. Ryd op med:  pkill -f fronius-sim")

        self.p = subprocess.Popen(self.args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  bufsize=1, start_new_session=True)
        _boern.append(self.p)

        self.port = self._laes_porten()

        # Og saa bekraeftes det udefra: svarer den paa den port den sagde?
        # Den siger porten FOER den begynder at tage imod, saa der kan gaa
        # et oejeblik.
        for _ in range(50):
            self._lever()
            if _porten_svarer(self.port):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError(
                f"simulatoren ({self.profil}) sagde port {self.port}, "
                f"men svarer ikke paa den")

        # Og saa ved resten af proeverne hvor den er, se noten ved PORT.
        global PORT
        PORT = self.port

        time.sleep(0.8)   # lad den naa at regne en tilstand ud
        return self

    def _lever(self):
        """Doede den med det samme? Saa skal der staa HVORFOR, ikke bare at
        den ikke kom op. Et ukendt argument er den typiske grund."""
        if self.p.poll() is None:
            return
        ud = ""
        try:
            ud = self.p.stdout.read() or ""
        except Exception:
            pass
        raise RuntimeError(
            f"simulatoren ({self.profil}) stoppede med det samme, "
            f"kode {self.p.returncode}:\n{ud.strip()[:600]}")

    def _laes_porten(self):
        """Laeser linjen "PORT=<n>" som simulatoren skriver foerst.

        Det er det eneste vi kan stole paa: med --port 0 er det
        styresystemet der har valgt, og kun simulatoren ved hvad den fik.
        """
        slut = time.time() + 10
        while time.time() < slut:
            self._lever()
            linje = self.p.stdout.readline()
            if not linje:
                time.sleep(0.05)
                continue
            self.log.append(linje)
            if linje.startswith("PORT="):
                return int(linje.strip().split("=", 1)[1])
        raise RuntimeError(
            f"simulatoren ({self.profil}) sagde aldrig hvilken port den "
            f"fik:\n" + "".join(self.log[-10:]))

    def __exit__(self, *a):
        luk_ned(self.p, self.port, f"simulatoren ({self.profil})")
        return False

    def tilstand(self):
        """Simulatorens EGEN opfattelse, laest af dens statuslinje."""
        # "  13:00  sol 7000 W  forbrug 300 W  batteri -0 W (48.0 %) net -6700 W"
        line = None
        deadline = time.time() + 4
        while time.time() < deadline:
            l = self.p.stdout.readline()
            if not l:
                break
            self.log.append(l.rstrip())
            if re.match(r"\s+\d{2}:\d{2}\s+sol", l):
                line = l
                break
        if line is None:
            return None
        m = re.search(r"sol\s+(-?\d+) W\s+forbrug\s+(-?\d+) W\s+"
                      r"batteri\s+([-+]?\d+) W\s+\(\s*([\d.]+) %\)\s+"
                      r"net\s+([-+]?\d+) W", line)
        if not m:
            return None
        return {
            "sol": float(m.group(1)),
            "forbrug": float(m.group(2)),
            "batteri": float(m.group(3)),
            "soc": float(m.group(4)),
            "net": float(m.group(5)),
        }


def probe(args=None, forvent_fejl=False):
    """Koerer zs-probe. Giver (udskrift, fejl).

    BEGGE kanaler er med i udskriften. Loggen gaar paa standardfejl, og
    foer blev den smidt vaek naar man forventede en fejl. Saa kunne en
    proeve ikke se HVORFOR det gik galt, kun AT det gik galt, og det var
    netop forskellen vi ville efterproeve.
    """
    cmd = [PROBE, "127.0.0.1", str(PORT)] + (args or [])
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    alt = (r.stdout or "") + (r.stderr or "")
    if VERBOSE:
        print(alt)
    if not forvent_fejl and r.returncode != 0:
        return None, alt
    return alt, None


def tal(ud, felt):
    """Traekker et af de fire tal ud af zs-probes udskrift."""
    m = re.search(r"^\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$",
                  ud.split("SOLCELLER")[1].split("\n")[1] if "SOLCELLER" in ud else "",
                  re.M)
    return m


def parse_kort(ud):
    """De fire tal som watt, eller None hvis der staar en streg."""
    if "SOLCELLER" not in ud:
        return None
    blok = ud.split("SOLCELLER")[1].splitlines()
    if len(blok) < 2:
        return None
    felter = blok[1].split()
    if len(felter) < 4:
        return None

    def til_watt(vaerdi, enhed):
        if vaerdi == "-":
            return None
        v = float(vaerdi.replace(",", "."))
        return v * 1000.0 if enhed == "kW" else v

    # Felterne staar som "7,0 kW  490 W  2,9 kW  3,6 kW"
    toks = blok[1].split()
    ud_liste = []
    i = 0
    while i < len(toks) and len(ud_liste) < 4:
        if toks[i] == "-":
            ud_liste.append(None)
            i += 1
        else:
            ud_liste.append(til_watt(toks[i], toks[i + 1]))
            i += 2
    if len(ud_liste) < 4:
        return None
    return dict(zip(("sol", "forbrug", "batteri", "net"), ud_liste))


def taet_paa(a, b, tolerance):
    return a is not None and b is not None and abs(a - b) <= tolerance


# ----------------------------------------------------------------------
# Testene
# ----------------------------------------------------------------------

def test_almindeligt_anlaeg():
    suite("Almindeligt anlæg: stemmer tallene med simulatoren")
    # Simulatoren staar STILLE her. Vi sammenligner dens egne tal med
    # dem skaermen laeser, og loeb tiden imens, naaede solen at flytte
    # sig mellem de to aflaesninger. Det gav en test der fejlede en gang
    # imellem uden at der var noget galt, og saadan en er vaerre end
    # ingen test: man holder op med at tro paa den.
    with Sim("battery", ["--speed", "0"]) as sim:
        st = sim.tilstand()
        ud, err = probe()
        if ud is None:
            fail("zs-probe kunne læse fra simulatoren", err)
            return
        ok("zs-probe kunne læse fra simulatoren")

        if "Fronius Symo GEN24 10.0" in ud:
            ok("fabrikat og model læst rigtigt")
        else:
            fail("fabrikat og model læst rigtigt", ud[:400])

        if "model 203 paa unit 200" in ud:
            ok("elmåleren fundet på unit 200, model 203")
        else:
            fail("elmåleren fundet på unit 200, model 203")

        if "Batteri         ja" in ud:
            ok("batteriet fundet")
        else:
            fail("batteriet fundet")

        kort = parse_kort(ud)
        if kort is None or st is None:
            fail("de fire tal kunne aflæses", ud[:400])
            return

        # Simulatoren og skaermen laeser ikke i praecis samme oejeblik,
        # saa vi tillader et par hundrede watt.
        TOL = 400.0
        for navn, sim_v in (("sol", st["sol"]), ("forbrug", st["forbrug"])):
            if taet_paa(kort[navn], sim_v, TOL):
                ok(f"{navn} stemmer ({kort[navn]:.0f} W mod {sim_v:.0f} W)")
            else:
                fail(f"{navn} stemmer", f"skærm {kort[navn]}, simulator {sim_v}")

        for navn, sim_v in (("batteri", abs(st["batteri"])), ("net", abs(st["net"]))):
            if taet_paa(kort[navn], sim_v, TOL):
                ok(f"{navn} stemmer i størrelse ({kort[navn]:.0f} W mod {sim_v:.0f} W)")
            else:
                fail(f"{navn} stemmer i størrelse",
                     f"skærm {kort[navn]}, simulator {sim_v}")

        # Retningen siges med ord, ikke med fortegn.
        if st["net"] < -25 and "saelger" in ud:
            ok("nettet siger sælger når simulatoren eksporterer")
        elif st["net"] > 25 and "koeber" in ud:
            ok("nettet siger køber når simulatoren importerer")
        elif abs(st["net"]) <= 25 and "balance" in ud:
            ok("nettet siger i balance")
        else:
            fail("nettets retning", f"simulator {st['net']} W, udskrift mangler ordet")


def test_manglende_dele():
    suite("Anlæg hvor noget mangler: siges der fra, eller opfindes der et nul")

    with Sim("nobattery"):
        ud, err = probe()
        if ud is None:
            fail("uden batteri: kunne læses", err)
        else:
            k = parse_kort(ud)
            if k and k["batteri"] is None:
                ok("uden batteri: batteriet står som en streg, ikke som 0 W")
            else:
                fail("uden batteri: batteriet står som en streg", str(k))
            if "Batteri         nej" in ud:
                ok("uden batteri: det står i oplysningerne")
            else:
                fail("uden batteri: det står i oplysningerne")

    with Sim("nometer"):
        ud, err = probe()
        if ud is None:
            fail("uden elmåler: kunne læses", err)
        else:
            k = parse_kort(ud)
            if k and k["forbrug"] is None and k["net"] is None:
                ok("uden elmåler: forbrug og net står som streger")
            else:
                fail("uden elmåler: forbrug og net står som streger", str(k))
            if k and k["sol"] is not None:
                ok("uden elmåler: solen vises stadig")
            else:
                fail("uden elmåler: solen vises stadig")
            if "Elmaaler        ingen fundet" in ud:
                ok("uden elmåler: det står i oplysningerne")
            else:
                fail("uden elmåler: det står i oplysningerne")

    with Sim("nolabels"):
        ud, err = probe()
        if ud is None:
            fail("uden kanalnavne: kunne læses", err)
        else:
            k = parse_kort(ud)
            if k and k["sol"] is not None and k["batteri"] is not None:
                ok("uden kanalnavne: reserveløsningen finder sol og batteri")
            else:
                fail("uden kanalnavne: reserveløsningen virker", str(k))

    with Sim("float"):
        ud, err = probe()
        if ud is None:
            fail("flydende tal: kunne læses", err)
        else:
            if "113 (float)" in ud and "model 213" in ud:
                ok("flydende tal: model 113 og 213 genkendt")
            else:
                fail("flydende tal: model 113 og 213 genkendt")
            k = parse_kort(ud)
            if k and all(v is not None for v in k.values()):
                ok("flydende tal: alle fire tal læst")
            else:
                fail("flydende tal: alle fire tal læst", str(k))

    with Sim("onestring"):
        ud, err = probe()
        if ud is None:
            fail("én solstreng: kunne læses", err)
        else:
            k = parse_kort(ud)
            if k and k["sol"] is not None and k["batteri"] is not None:
                ok("én solstreng: hele produktionen på ét kort")
            else:
                fail("én solstreng: virker", str(k))


def test_sunspec_base_40001():
    suite("SunSpec der starter på 40001 i stedet for 40000")
    with Sim("battery", ["--base", "40001"]):
        ud, err = probe()
        if ud is None:
            fail("base 40001 kunne læses", err)
        elif "base 40001" in ud:
            ok("base 40001 findes og bruges")
        else:
            fail("base 40001 findes og bruges", ud[:300])


def test_fejlkoder():
    suite("Fejlkoder fra inverteren")

    # Kendte koder skal oversaettes til dansk
    with Sim("battery", ["--fejl", "st=5,stvnd=307,evtvnd2=0x80,evt1=0x0080"]):
        ud, err = probe()
        if ud is None:
            fail("kendte fejlkoder kunne læses", err)
        else:
            for hvad, tekst in (
                ("driftstilstand 5 oversættes", "Produktionen er begrænset"),
                ("SunSpec Evt1 bit 7 oversættes", "For høj temperatur"),
                ("Fronius EvtVnd2 bit 7 oversættes", "Effekt sænket på grund af varme"),
                ("Fronius-tilstand vises til opslag", "Fronius-tilstand 307"),
            ):
                if tekst in ud:
                    ok(hvad)
                else:
                    fail(hvad, f"fandt ikke \"{tekst}\"")
            if "Der er en fejl" in ud:
                ok("sammenfatningen siger at der er en fejl")
            else:
                fail("sammenfatningen siger at der er en fejl")

    # Ukendte koder skal give telefonnummeret, ikke et gæt
    with Sim("battery", ["--fejl",
                         "st=7,evtvnd3=0x00100000,evtvnd4=0xDEADBEEF,evt2=0x40"]):
        ud, err = probe()
        if ud is None:
            fail("ukendte fejlkoder kunne læses", err)
        else:
            if "7060 3676" in ud:
                ok("ukendt kode: telefonnummeret står der")
            else:
                fail("ukendt kode: telefonnummeret står der")
            if "ZOL Energi" in ud:
                ok("ukendt kode: hvem man skal ringe til står der")
            else:
                fail("ukendt kode: hvem man skal ringe til står der")
            if "0xDEADBEEF" in ud:
                ok("ukendt kode: den rå værdi vises så den kan slås op")
            else:
                fail("ukendt kode: den rå værdi vises")
            if "EvtVnd3 bit 20" in ud:
                ok("ukendt bit: feltet og bitnummeret vises")
            else:
                fail("ukendt bit: feltet og bitnummeret vises")
            # Det vigtigste: en ukendt fejl maa ALDRIG blive til "alt virker"
            if "Alt virker" not in ud:
                ok("ukendt kode bliver ikke til \"alt virker\"")
            else:
                fail("ukendt kode bliver ikke til \"alt virker\"")

    # Ingen fejl skal give ro
    with Sim("battery"):
        ud, err = probe()
        if ud is None:
            fail("uden fejl kunne læses", err)
        else:
            if "Ingen meldinger" in ud:
                ok("uden fejl: ingen meldinger")
            else:
                fail("uden fejl: ingen meldinger", ud[-600:])
            if "7060 3676" not in ud:
                ok("uden fejl: telefonnummeret står der ikke")
            else:
                fail("uden fejl: telefonnummeret står der ikke")


def test_naar_det_gaar_galt():
    suite("Når det går galt")

    # Ingen der lytter.
    #
    # Porten stod foer fast paa 15999. Den er sandsynligvis fri, men
    # "sandsynligvis" er ikke godt nok: var der noget, ville proeven fejle
    # af en grund der intet havde med sagen at goere. Nu holder vi en port
    # vi VED er tom, se hold_en_fri_port.
    tom_sok, tom_port = hold_en_fri_port()
    try:
        r = subprocess.run([PROBE, "127.0.0.1", str(tom_port)],
                           capture_output=True, text=True, timeout=20)
    finally:
        tom_sok.close()
    if r.returncode != 0 and "Kunne ikke" in (r.stdout + r.stderr):
        ok("lukket port: siges der fra, på dansk")
    else:
        fail("lukket port: siges der fra", r.stdout + r.stderr)

    # Noget lytter, men taler ikke Modbus
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(4)
    try:
        r = subprocess.run([PROBE, "127.0.0.1", str(srv.getsockname()[1])],
                           capture_output=True, text=True, timeout=25)
        if r.returncode != 0:
            ok("åben port uden Modbus: giver op i stedet for at hænge")
        else:
            fail("åben port uden Modbus: giver op", r.stdout)
    finally:
        srv.close()

    # Noget svarer med rent skrald
    class Skrald(socket.socket):
        pass

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(4)
    skrald_port = srv.getsockname()[1]

    import threading
    stop = threading.Event()

    def skraldeserver():
        srv.settimeout(0.5)
        while not stop.is_set():
            try:
                c, _ = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                c.recv(256)
                c.sendall(b"\xde\xad\xbe\xef" * 40)
            except OSError:
                pass
            finally:
                c.close()

    t = threading.Thread(target=skraldeserver, daemon=True)
    t.start()
    try:
        r = subprocess.run([PROBE, "127.0.0.1", str(skrald_port)],
                           capture_output=True, text=True, timeout=25)
        if r.returncode != 0:
            ok("skrald i stedet for svar: afvises, intet nedbrud")
        else:
            fail("skrald i stedet for svar: afvises", r.stdout)
    finally:
        stop.set()
        srv.close()
        t.join(timeout=2)

    # Simulatoren forsvinder midt i
    with Sim("battery") as sim:
        ud, err = probe()
        if ud is None:
            fail("kunne læse før nedlukning", err)
        else:
            ok("kunne læse før nedlukning")
    r = subprocess.run([PROBE, "127.0.0.1", str(PORT)],
                       capture_output=True, text=True, timeout=20)
    if r.returncode != 0:
        ok("efter nedlukning: siges der fra i stedet for at vise gamle tal")
    else:
        fail("efter nedlukning: siges der fra", r.stdout)


def test_genfind():
    suite("Inverteren har fået en ny IP-adresse")

    # Scanningen tager porten som argument, saa simulatoren kan ligge paa
    # den hoeje testport.
    #
    # Foerste forsoeg brugte 502, Modbus' egen port. Det virkede paa Mac
    # og faldt i CI: 502 er UNDER 1024, altsaa privilegeret, og den kan en
    # almindelig bruger ikke binde paa Linux. Og det afsloerede et rigtigt
    # hul: scanningen ledte kun paa 502, selv om kundens indstillinger har
    # et portfelt. En inverter paa en anden port kunne vaelges i haanden
    # men aldrig findes.
    MODBUS_PORT = PORT
    SN = "31234567"          # simulatorens serienummer, se profilen

    # Porten kendes foerst naar simulatoren har sagt hvilken den fik, se
    # noten ved PORT. Derfor en liste med én plads: den indre funktion
    # skal kunne laese den efter at with-blokken har sat den.
    MODBUS_PORT = [None]

    # Udfaldene som zs_locate.h definerer dem, brugt som exitkode.
    SAMME, NY, TAGET, INGEN, FLERE = 0, 1, 2, 3, 4

    def genfind(serial=None, sidste=None):
        # Vores EGEN adresse, ikke undernettet. Scanningen regner selv
        # raekkerne ud af den og netmasken, se zs_discovery_blokke.
        cmd = [PROBE, "--genfind", "127.0.0.1", "--praefiks", "24",
               "--port", str(MODBUS_PORT)]
        if serial:
            cmd += ["--serienr", serial]
            if sidste:
                cmd += ["--sidste", sidste]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if VERBOSE:
            print(r.stdout)
        return r.returncode, r.stdout

    try:
        # Bind til ÉN adresse.
        #
        # Simulatoren lytter som standard paa 0.0.0.0, og paa Linux svarer
        # HELE 127-omraadet paa loopback. Saa fandt scanningen den samme
        # simulator 254 gange og meldte "flere invertere". Paa Mac er kun
        # 127.0.0.1 oppe, saa det kunne ikke ses her. En rigtig inverter
        # har én adresse, og saadan skal proeven ogsaa vaere.
        with Sim("battery", port=MODBUS_PORT):
            # 1. En ny skaerm der ikke kender noget serienummer. Er der
            #    praecis én inverter, er det den.
            kode, ud = genfind()
            if kode == TAGET and SN in ud and "127.0.0.1" in ud:
                ok("ny skærm uden serienummer finder den eneste inverter")
            else:
                fail("ny skærm uden serienummer finder den eneste inverter", ud)

            # 2. DET DER VAR I STYKKER. Vi kender inverteren, men den stod
            #    paa en anden adresse sidst. Den skal findes paa sin nye.
            kode, ud = genfind(SN, "127.0.0.99")
            if kode == NY and "127.0.0.1" in ud:
                ok("kendt inverter på ny adresse bliver fundet igen")
            else:
                fail("kendt inverter på ny adresse bliver fundet igen", ud)

            # 3. DEN FARLIGE. Vores inverter er vaek, og paa praecis den
            #    adresse vi havde gemt svarer der en FREMMED inverter. Den
            #    maa ikke tages, uanset at den er den eneste der er.
            kode, ud = genfind("39999999", "127.0.0.1")
            if kode == INGEN:
                ok("en fremmed inverter på vores gamle adresse bliver afvist")
            else:
                fail("en fremmed inverter på vores gamle adresse bliver afvist", ud)

            # 4. Den sidder hvor den plejer. Saa skal der ikke meldes
            #    flytning, for saa ville adressen blive gemt hver gang.
            kode, ud = genfind(SN, "127.0.0.1")
            if kode == SAMME:
                ok("sidder den hvor den plejer, meldes der ikke flytning")
            else:
                fail("sidder den hvor den plejer, meldes der ikke flytning", ud)
    except RuntimeError as e:
        fail(f"simulatoren kunne ikke lytte på port {MODBUS_PORT}", str(e))


class FalskEnhed:
    """
    Noget andet der taler Modbus paa samme port.

    Et kundenetvaerk har ofte andet paa 502: en varmepumpe, en PLC, en
    energimaaler. De svarer paent paa FC3, men der staar ikke "SunS" i
    registrene.
    """

    def __init__(self, svar):
        # Samme Python som proeverne selv, se Sim.__init__.
        self.args = [sys.executable,
                     os.path.join(ROOT, "tools", "fronius-sim", "ikke-inverter.py"),
                     "--port", str(PORT), "--bind", "127.0.0.1", "--svar", svar]
        self.p = None

    def __enter__(self):
        """Samme tre regler som Sim.__enter__: fri port foer, egen proces
        i live undervejs, og haard fejl hvis noget af det ikke holder."""
        if _porten_svarer(PORT):
            raise RuntimeError(
                f"port {PORT} var i brug FOER den falske enhed blev "
                f"startet. Ryd op med:  pkill -f fronius-sim")

        self.p = subprocess.Popen(self.args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  start_new_session=True)
        _boern.append(self.p)
        for _ in range(50):
            if self.p.poll() is not None:
                ud = ""
                try:
                    ud = self.p.stdout.read() or ""
                except Exception:
                    pass
                raise RuntimeError(
                    f"den falske enhed stoppede med det samme, kode "
                    f"{self.p.returncode}:\n{ud.strip()[:600]}")
            if _porten_svarer(PORT):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("den falske enhed kom aldrig op")
        return self

    def __exit__(self, *a):
        luk_ned(self.p, PORT, "den falske enhed")
        return False


def test_ikke_en_inverter():
    suite("Noget andet taler Modbus på samme port")

    # Tre maader at vaere besvaerlig paa. Ingen af dem maa blive kaldt en
    # inverter: saa ville kunden vaelge sin varmepumpe fra listen og
    # undre sig over at der aldrig kommer tal.
    for svar, hvad in (("nuller", "svarer med lutter nuller"),
                       ("skrald", "svarer med skrald"),
                       ("tavs",   "tager imod men svarer aldrig")):
        with FalskEnhed(svar):
            ud, err = probe(forvent_fejl=True)
            if ud is not None and "SunSpec" in ud and "Kunne ikke" not in ud:
                fail(f"{hvad}: bliver IKKE kaldt en inverter", ud[:200])
            else:
                ok(f"{hvad}: bliver ikke kaldt en inverter")


def test_model_kan_ikke_laeses():
    suite("En model står i kæden men kan ikke læses")

    # Det sker efter en firmwareopdatering paa inverteren, hvor listen og
    # indholdet ikke foelges ad.
    with Sim("battery", ["--speed", "0", "--naegt", "160"]):
        ud, err = probe()
        if ud is None:
            fail("hele modellen nægtet: der kan stadig læses", err)
        else:
            k = parse_kort(ud)
            if k and k["sol"] is None and k["forbrug"] is not None \
                    and k["net"] is not None:
                ok("hele modellen nægtet: solen står som streg, resten læses")
            else:
                fail("hele modellen nægtet: solen står som streg", str(k))
            if "ufuldstaendig" in ud or "ufuldstændig" in ud:
                ok("og skærmen siger selv at listen ikke kunne læses færdig")
            else:
                fail("skærmen siger at listen er ufuldstændig", ud[:200])

    # Naegtes kun DATAENE, kan hovedet laeses, og saa skal resten af
    # kaeden stadig komme med. Ellers mistede vi batteri og solstrenge
    # fordi maerkeeffekten ikke kunne hentes.
    with Sim("battery", ["--speed", "0", "--naegt-kun-data", "120"]):
        ud, err = probe()
        if ud is None:
            fail("kun dataene nægtet: der kan stadig læses", err)
        else:
            k = parse_kort(ud)
            if k and all(v is not None for v in k.values()):
                ok("kun dataene nægtet: alle fire tal er der endnu")
            else:
                fail("kun dataene nægtet: alle fire tal", str(k))
            if "160" in ud and "124" in ud:
                ok("og kæden blev læst helt til ende")
            else:
                fail("kæden blev læst helt til ende", ud[:200])


def test_maaler_i_kaeden():
    suite("Elmåleren ligger i inverterens egen kæde")

    # Den kodevej fandtes i firmwaren men var aldrig proevet af:
    # simulatoren lagde altid maaleren paa sin egen unit.
    with Sim("battery", ["--speed", "0", "--maaler-i-kaeden"]) as sim:
        ud, err = probe()
        if ud is None:
            fail("der kan læses når måleren ligger i kæden", err)
            return
        if "i inverterens egen kaede" in ud:
            ok("skærmen ser at måleren ligger i inverterens egen kæde")
        else:
            fail("skærmen ser at måleren ligger i kæden", ud[:200])
        k = parse_kort(ud)
        if k and all(v is not None for v in k.values()):
            ok("og alle fire tal læses")
        else:
            fail("alle fire tal læses", str(k))
        t = sim.tilstand()
        # I STOERRELSE. Skaermen viser altid et positivt tal med
        # retningen som ord under, saa fortegnet kan ikke sammenlignes
        # direkte. Det er samme maade som i den foerste suite.
        if t and taet_paa(k["net"], abs(t.get("net", 0)),
                          max(80.0, abs(t.get("net", 0)) * 0.1)):
            ok("og nettet stemmer i størrelse med simulatoren")
        else:
            fail("nettet stemmer i størrelse med simulatoren",
                 f'skaerm {k["net"]}, sim {t}')


def test_loegn_om_kanaler():
    suite("Inverteren lyver om hvor mange DC-kanaler den har")

    # N-feltet i model 160 siger otte kanaler, men modellen er kun lang
    # nok til fire. Uden vagten i zs_fronius.c ville skaermen laese ud
    # over modellens data og vise gammelt indhold fra bufferen som en
    # rigtig solstreng, med et tal der ser helt plausibelt ud.
    with Sim("battery", ["--speed", "0", "--loegn-kanaler", "8"]):
        ud, err = probe()
        if ud is None:
            fail("der kan læses når N lyver", err)
            return
        linjer = [l for l in ud.splitlines()
                  if re.match(r"^\s+\d+\s+\S", l) and ("streng" in l or "batteri" in l)]
        if len(linjer) == 4:
            ok("der læses præcis de fire kanaler der er plads til")
        else:
            fail("der læses fire kanaler", f"fandt {len(linjer)}: {linjer}")

        k = parse_kort(ud)
        if k and k["sol"] is not None and k["sol"] > 1000:
            ok("og solen er stadig rigtig")
        else:
            fail("solen er stadig rigtig", str(k))


def test_inverter_paa_anden_unit():
    suite("Inverteren sidder ikke på unit 1")

    # Et anlaeg med flere invertere bag én Datamanager lægger dem paa
    # unit 1, 2, 3. Det her viser hvad vi kan og ikke kan: soegningen
    # finder kun unit 1, men vaelger man unitten i haanden, virker alt.
    with Sim("battery", ["--speed", "0",
                         "--inverter-unit", "2", "--meter-unit", "201"]):
        # Det der betyder noget er at der ikke kommer TAL. Selve
        # fejlbeskeden gaar til stderr, som probe ikke giver videre naar
        # man forventer en fejl.
        ud, err = probe(forvent_fejl=True)
        if ud is None or parse_kort(ud) is None:
            ok("med standard unit 1 kommer der ingen tal, og den opfinder ingen")
        else:
            fail("med unit 1 kommer der ingen tal", ud[:200])

        ud, err = probe(["--unit", "2"])
        if ud is None:
            fail("med unit 2 kan den læses", err)
            return
        k = parse_kort(ud)
        if k and all(v is not None for v in k.values()):
            ok("med unit 2 læses alle fire tal")
        else:
            fail("med unit 2 læses alle fire tal", str(k))
        if "201" in ud:
            ok("og elmåleren findes på sin egen unit ved siden af")
        else:
            fail("elmåleren findes", ud[:200])


def test_mange_fejl():
    suite("Inverteren melder flere fejl end der er plads til")

    # Flaget der siger at listen blev klippet, blev sat men ALDRIG laest.
    # Saa viste vaerktoejet de foerste fjorten og tav om resten. En
    # montoer kunne rette dem og gaa hjem mens aarsagen stod paa plads
    # femten. Modulets egen header advarer netop mod at tie om noget.
    with Sim("battery", ["--speed", "0", "--fejl",
                         "st=7,evtvnd1=0xFFFFFFFF,evtvnd2=0xFFFFFFFF,evt1=0xFFFF"]):
        ud, err = probe()
        if ud is None:
            fail("der kan læses når inverteren melder alt på én gang", err)
            return
        linjer = [l for l in ud.splitlines() if re.search(r"\[(FEJL|advarsel|info|ok)", l)]
        if len(linjer) > 5:
            ok(f"der vises en liste med fejl ({len(linjer)} linjer)")
        else:
            fail("der vises en liste med fejl", f"{len(linjer)} linjer")
        if "FLERE end der var plads til" in ud:
            ok("og der siges til om at listen er klippet")
        else:
            fail("der siges til om at listen er klippet", ud[-300:])

    # Og med ÉN fejl maa der ikke staa at listen er klippet.
    with Sim("battery", ["--speed", "0", "--fejl", "evt1=0x0001"]):
        ud, err = probe()
        if ud is not None and "FLERE end der var plads til" not in ud:
            ok("med én fejl står der ikke at listen er klippet")
        else:
            fail("med én fejl står der ikke at den er klippet",
                 (ud or "")[-200:])


def test_skriv_modbus():
    suite("At skrive på Modbus, og at opdage når det ikke virker")

    # FUNDAMENTET til at styre inverteren. Intet herinde roerer en rigtig
    # inverter, og firmwaren sender ingen kommandoer af sig selv.
    #
    # Det hele staar og falder med én saetning fra Fronius' egen manual:
    # "If an attempt is made to write to such registers, the inverter does
    # not return an exception code!" En afvist skrivning ser altsaa
    # PRAECIS ud som en der lykkedes. Derfor laeses der tilbage, og derfor
    # er den midterste proeve her den vigtigste af dem alle.
    ADR, VAERDI = 40100, 1234

    def skriv(maade):
        with Sim("battery", ["--speed", "0", "--skrivemaade", maade]):
            r = subprocess.run([PROBE, "127.0.0.1", str(PORT),
                                "--skriv", str(ADR), str(VAERDI)],
                               capture_output=True, text=True, timeout=60)
            if VERBOSE:
                print(r.stdout)
            return r.returncode, r.stdout

    kode, ud = skriv("skriv")
    if kode == 0 and "SKREVET OG BEKRAEFTET" in ud:
        ok("en skrivning der virker bliver bekræftet")
    else:
        fail("en skrivning der virker bliver bekræftet", ud[-200:])
    if f"Efter:    {VAERDI}" in ud:
        ok("og registret står med den nye værdi bagefter")
    else:
        fail("registret står med den nye værdi", ud[-200:])

    # DEN VIGTIGSTE. Inverteren svarer paent og gOEr ingenting.
    kode, ud = skriv("tavs")
    if kode != 0 and "tog ikke imod" in ud:
        ok("en skrivning der TAVST ignoreres bliver fanget af tilbagelæsningen")
    else:
        fail("en tavst ignoreret skrivning bliver fanget", ud[-200:])
    if "Inverter control via Modbus" in ud:
        ok("og teksten peger på inverterens egen indstilling")
    else:
        fail("teksten peger på inverterens indstilling", ud[-200:])

    kode, ud = skriv("naegt")
    if kode != 0 and "afviste" in ud:
        ok("en rigtig afvisning siges der også fra om")
    else:
        fail("en rigtig afvisning siges der fra om", ud[-200:])


def test_inverter_hopper():
    suite("Inverteren hopper af og på nettet")

    # En inverter der genstarter, en router der taber den, en kontakt der
    # bliver slaaet fra og til. Soegningen skal give samme svar hver gang
    # og ikke ende i en halv tilstand fordi den fandt den midt i et hop.
    SN = "31234567"
    MODBUS_PORT = PORT

    def genfind(serial=None, sidste=None):
        cmd = [PROBE, "--genfind", "127.0.0.1", "--praefiks", "24",
               "--port", str(MODBUS_PORT)]
        if serial:
            cmd += ["--serienr", serial]
            if sidste:
                cmd += ["--sidste", sidste]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if VERBOSE:
            print(r.stdout)
        return r.returncode, r.stdout

    # Tre gange af og paa. Hver gang den er vaek skal svaret vaere "ikke
    # fundet", og hver gang den er der skal den findes paa serienummeret.
    stabil = True
    detaljer = []
    for runde in range(3):
        kode, ud = genfind(SN, "127.0.0.1")
        if kode != 3:          # 3 = ZS_LOC_INGEN
            stabil = False
            detaljer.append(f"runde {runde}, vaek: kode {kode}")
        with Sim("battery", ["--speed", "0"]):
            kode, ud = genfind(SN, "127.0.0.1")
            if kode != 0 or SN not in ud:     # 0 = ZS_LOC_SAMME
                stabil = False
                detaljer.append(f"runde {runde}, paa: kode {kode}")

    if stabil:
        ok("tre gange af og på giver samme svar hver gang")
    else:
        fail("tre gange af og på giver samme svar", "; ".join(detaljer))

    # Og naar den er vaek, maa der ikke staa en adresse tilbage som om
    # den var fundet.
    kode, ud = genfind(SN, "127.0.0.1")
    if "Adresse:" not in ud:
        ok("når den er væk, står der ingen adresse tilbage")
    else:
        fail("når den er væk, står der ingen adresse", ud[-200:])


def test_scan_vaerktoej():
    suite("Fejlsøgningsværktøjet søger som skærmen, ikke på sin egen måde")

    # HVORFOR DEN HER PROEVE FINDES.
    #
    # zs-probe --scan havde sin EGEN loekke: én adresse ad gangen, med sin
    # egen taalmodighed paa 200 ms. Skaermen aabner tolv paa én gang,
    # venter 250 ms, og proever hver adresse to gange.
    #
    # Det ligner det samme og er det ikke. Melder en kunde at skaermen ikke
    # kan finde inverteren, og finder vaerktoejet den saa alligevel, har vi
    # ikke fundet fejlen. Vi har maalt to forskellige ting og draget den
    # forkerte slutning.
    #
    # Nu kalder --scan zs_discovery_scan, altsaa den samme kode skaermen
    # koerer. Proeven her holder det fast: skrider de fra hinanden igen,
    # falder den.
    SN = "31234567"

    def scan(port):
        r = subprocess.run([PROBE, "--scan", "127.0.0.0", "--port", str(port)],
                           capture_output=True, text=True, timeout=180)
        if VERBOSE:
            print(r.stdout)
        return r.returncode, r.stdout

    try:
        # Bind til én adresse. Paa Linux svarer hele 127-omraadet ellers,
        # se noten i test_genfind.
        with Sim("battery", port=PORT):
            kode, ud = scan(PORT)
            if kode == 0 and "127.0.0.1" in ud:
                ok("værktøjet finder inverteren med skærmens egen motor")
            else:
                fail("værktøjet finder inverteren", f"kode {kode}: {ud}")

            if SN in ud:
                ok("og læser serienummeret, så man kan se at det er den rigtige")
            else:
                fail("serienummeret vises", ud)

            # Porten skal virke. Foer blev --port ignoreret ved en
            # soegning, saa vaerktoejet ledte paa 502 uanset hvad der stod.
            kode, ud = scan(PORT + 1)
            if kode != 0 and "0 invertere" in ud:
                ok("og på en port hvor der ikke er noget, findes der ingen")
            else:
                fail("tom port giver ingen fund", f"kode {kode}: {ud}")
    except Exception as e:
        fail("scanningen kunne køres", str(e))


def test_inverter_tier_stille():
    suite("Inverteren holder op med at svare, men lukker ikke")

    # HVORFOR DEN FINDES.
    #
    # En inverter der har travlt, fx fordi den samtidig serverer sin egen
    # hjemmeside eller sender til Solar.web, kan holde op med at svare
    # UDEN at lukke forbindelsen. For skaermen ser alt fint ud, lige
    # indtil timeouten loeber ud.
    #
    # Den vej var helt uproevet. Mutationstest afsloerede det: aendrer man
    # "errno == EAGAIN" til "!=" i zs_modbus_tcp.c, saa en timeout
    # rapporteres som en lukket forbindelse, opdagede INGEN test det.
    try:
        with Sim("battery", ["--tavs-efter", "3"],
                 port=PORT):
            t0 = time.time()
            ud, fejl = probe(forvent_fejl=True)
            brugt = time.time() - t0

            # Det vigtigste: den maa ikke haenge for evigt.
            if brugt < 60:
                ok(f"den giver op i stedet for at hænge ({brugt:.1f}s)")
            else:
                fail("den giver op inden for et minut", f"brugte {brugt:.0f}s")

            tekst = (ud or "") + (fejl or "")

            # DEN VIGTIGE: den skal sige at der ikke kom et SVAR, ikke at
            # forbindelsen blev lukket. Forbindelsen staar jo aaben.
            #
            # Foerste udgave af den her proeve tjekkede kun at den gav op,
            # og saa bestod den ogsaa med fejlen indsat. Baade "timeout"
            # og "lukket" er at give op. Det er netop forskellen mellem de
            # to der afgoer om man leder efter et netvaerksproblem eller
            # efter en inverter der har travlt.
            if "svarede ikke i tide" in tekst:
                ok("og den siger at der ikke kom et svar, ikke at "
                   "forbindelsen blev lukket")
            elif "Forbindelsen blev lukket" in tekst:
                fail("den skelner mellem tavshed og en lukket forbindelse",
                     "den sagde at forbindelsen blev lukket, men den stod "
                     "aaben hele tiden")
            else:
                fail("den siger hvad der gik galt",
                     f"ingen af de to beskeder stod i udskriften: {tekst[:200]}")
    except Exception as e:
        fail("prøven kunne køres", str(e))


def taellere(ud):
    """De fem livstaellere i Wh, laest ud af zs-probes udskrift.

    Dem der ikke findes staar som None, saa en manglende taeller kan ses
    for sig og ikke bare bliver nul.
    """
    navne = {
        "Produceret": "prod", "Koebt fra net": "imp", "Solgt til net": "exp",
        "Ind i batteri": "bat_ind", "Ud af batteri": "bat_ud",
    }
    svar = {k: None for k in navne.values()}
    for linje in ud.splitlines():
        for tekst, noegle in navne.items():
            if linje.strip().startswith(tekst):
                m = re.search(r"([\d.]+)\s*Wh", linje)
                if m:
                    svar[noegle] = float(m.group(1))
    return svar


def test_energitaellere():
    suite("Inverterens livstællere, som timeenergien regnes af")

    # HVORFOR DEN FINDES.
    #
    # Timens energi i kWh er en FORSKEL mellem to taellerstande en time
    # fra hinanden. Gaar der noget galt i aflaesningen, kan man ikke se det
    # paa resultatet: et forkert tal ser ud som et rigtigt tal, og man
    # opdager det foerst naar en graf paa serveren ser underlig ud en uge
    # senere.
    #
    # Og hullet var reelt. Da taellerne blev lagt ind, blev batteriets to
    # kun sat i den gren der haandterer kanaler UDEN navn. Simulatoren
    # navngiver sine, saa de to taellere var tomme hele vejen. Det blev
    # fanget ved et tilfaelde, ikke af en proeve. Den her proeve er den
    # proeve.
    try:
        with Sim("battery"):
            ud, err = probe()
            if ud is None:
                fail("kunne læse anlægget", err)
                return
            t = taellere(ud)

            mangler = [k for k, v in t.items() if v is None]
            if mangler:
                # Og saa er der ikke mere at proeve: resten ville bestaa
                # paa ingenting. Et tjek der ikke kan fejle er ingen proeve.
                fail("alle fem livstællere læses",
                     f"disse mangler: {', '.join(mangler)}")
                return
            ok("alle fem livstællere læses")

            # De skal vaere positive. En livstaeller der er nul betyder
            # "har den ikke" i SunSpec, og en negativ kan slet ikke vaere
            # en acc32.
            nul = [k for k, v in t.items() if v <= 0]
            if not nul:
                ok("og ingen af dem står på nul")
            else:
                fail("ingen tæller står på nul", f"disse er nul: {nul}")

            # Produktionen er livets samlede, saa den skal vaere stoerst.
            # Det fanger at to taellere er byttet om, hvilket er let at
            # komme til naar de ligger i rækkefølge i samme blok.
            if t["prod"] > t["exp"]:
                ok("produktionen er større end det solgte, altså ikke byttet om")
            else:
                fail("produktionen er større end det solgte",
                     f"produceret {t['prod']}, solgt {t['exp']}")

            # Og de skal VOKSE. En taeller der staar stille ville give nul
            # kWh hver time, og det ser ud som en nat.
            time.sleep(2.5)
            ud2, err2 = probe()
            if ud2 is None:
                fail("kunne læse igen", err2)
                return
            t2 = taellere(ud2)
            if any(v is None for v in t2.values()):
                fail("tællerne kunne læses igen", f"anden læsning: {t2}")
                return
            voksede = [k for k in t if t2[k] > t[k]]
            if voksede:
                ok(f"tællerne vokser mellem to læsninger ({len(voksede)} af 5)")
            else:
                fail("tællerne vokser", f"før {t}, efter {t2}")

            # Ingen af dem maa falde. Falder en, bliver timens vaerdi
            # negativ, og saa springer skaermen feltet over for evigt.
            faldt = [k for k in t if t2[k] < t[k]]
            if not faldt:
                ok("og ingen af dem falder")
            else:
                fail("ingen tæller falder", f"disse faldt: {faldt}")
    except Exception as e:
        fail("prøven kunne køres", str(e))

    # Uden batteri skal de to batteritaellere IKKE staa der. Et nul ville
    # blive sendt til serveren som "nul kWh ind i batteriet", og det er en
    # paastand om et batteri der ikke findes.
    try:
        with Sim("nobattery"):
            ud, err = probe()
            if ud is None:
                fail("kunne læse anlæg uden batteri", err)
                return
            t = taellere(ud)
            if t["bat_ind"] is None and t["bat_ud"] is None:
                ok("uden batteri oplyses batteritællerne ikke")
            else:
                fail("uden batteri oplyses batteritællerne ikke",
                     f"ind {t['bat_ind']}, ud {t['bat_ud']}")
            if t["prod"] is not None:
                ok("men produktionen læses stadig")
            else:
                fail("produktionen læses stadig uden batteri", ud[:300])
    except Exception as e:
        fail("prøven uden batteri kunne køres", str(e))


def main():
    for sti, navn in ((SIM, "simulatoren"), (PROBE, "zs-probe")):
        if not os.path.exists(sti):
            print(f"\nFEJL: {navn} mangler: {sti}")
            if navn == "zs-probe":
                print("Byg den med: ./tools/zs-probe/build.sh\n")
            return 1

    print("\n\033[1mzScreen, hele datavejen\033[0m")
    test_almindeligt_anlaeg()
    test_manglende_dele()
    test_sunspec_base_40001()
    test_fejlkoder()
    test_naar_det_gaar_galt()
    test_genfind()
    test_ikke_en_inverter()
    test_model_kan_ikke_laeses()
    test_maaler_i_kaeden()
    test_loegn_om_kanaler()
    test_inverter_paa_anden_unit()
    test_mange_fejl()
    test_skriv_modbus()
    test_inverter_hopper()
    test_scan_vaerktoej()
    test_inverter_tier_stille()
    test_energitaellere()

    print("\n" + "─" * 40)
    if fejl == 0:
        print(f"\033[1;32m{tjek} tjek, alle bestået\033[0m\n")
        return 0
    print(f"\033[1;31m{tjek} tjek, {fejl} fejlede\033[0m\n")
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as e:
        # En port der ikke bliver fri poisoner alt bagefter, saa der er
        # ingen grund til at koere videre og vise falske resultater.
        print(f"\n\033[1;31mProeverne blev stoppet\033[0m\n  {e}\n")
        sys.exit(2)
