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
PORT = 15020          # hoej port, saa der ikke skal bruges sudo

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

class Sim:
    """Starter simulatoren og lukker den igen, ogsaa hvis noget gaar galt."""

    def __init__(self, profil, ekstra=None, port=None):
        self.profil = profil
        self.port = port or PORT
        self.args = ["python3", SIM, "--port", str(self.port), "--profile", profil,
                     "--start-hour", "13", "--speed", "1", "--print-every", "1"]
        if ekstra:
            self.args += ekstra
        self.p = None
        self.log = []

    def __enter__(self):
        self.p = subprocess.Popen(self.args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  bufsize=1, preexec_fn=os.setsid)
        # Vent paa at porten svarer, hoejst fem sekunder.
        for _ in range(50):
            try:
                s = socket.create_connection(("127.0.0.1", self.port), timeout=0.3)
                s.close()
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError(f"simulatoren ({self.profil}) kom aldrig op")
        time.sleep(0.8)   # lad den naa at regne en tilstand ud
        return self

    def __exit__(self, *a):
        if self.p is not None:
            try:
                os.killpg(os.getpgid(self.p.pid), signal.SIGTERM)
                self.p.wait(timeout=3)
            except Exception:
                pass
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
    cmd = [PROBE, "127.0.0.1", str(PORT)] + (args or [])
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if VERBOSE:
        print(r.stdout)
    if not forvent_fejl and r.returncode != 0:
        return None, r.stdout + r.stderr
    return r.stdout, None


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

    # Ingen der lytter
    r = subprocess.run([PROBE, "127.0.0.1", "15999"], capture_output=True,
                       text=True, timeout=20)
    if r.returncode != 0 and "Kunne ikke" in (r.stdout + r.stderr):
        ok("lukket port: siges der fra, på dansk")
    else:
        fail("lukket port: siges der fra", r.stdout + r.stderr)

    # Noget lytter, men taler ikke Modbus
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", PORT + 1))
    srv.listen(4)
    try:
        r = subprocess.run([PROBE, "127.0.0.1", str(PORT + 1)],
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
    srv.bind(("127.0.0.1", PORT + 2))
    srv.listen(4)

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
        r = subprocess.run([PROBE, "127.0.0.1", str(PORT + 2)],
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
        with Sim("battery", ["--bind", "127.0.0.1"], port=MODBUS_PORT):
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
        self.args = ["python3",
                     os.path.join(ROOT, "tools", "fronius-sim", "ikke-inverter.py"),
                     "--port", str(PORT), "--bind", "127.0.0.1", "--svar", svar]
        self.p = None

    def __enter__(self):
        self.p = subprocess.Popen(self.args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  preexec_fn=os.setsid)
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.3).close()
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError("den falske enhed kom aldrig op")
        return self

    def __exit__(self, *a):
        try:
            os.killpg(os.getpgid(self.p.pid), signal.SIGTERM)
            self.p.wait(timeout=3)
        except Exception:
            pass
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
        with Sim("battery", ["--speed", "0", "--bind", "127.0.0.1"]):
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
        with Sim("battery", ["--bind", "127.0.0.1"], port=PORT):
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

    print("\n" + "─" * 40)
    if fejl == 0:
        print(f"\033[1;32m{tjek} tjek, alle bestået\033[0m\n")
        return 0
    print(f"\033[1;31m{tjek} tjek, {fejl} fejlede\033[0m\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
