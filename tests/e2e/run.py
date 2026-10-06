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
PROBE = os.path.join(ROOT, "tools", "zs-probe", "zs-probe")
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
        cmd = [PROBE, "--genfind", "127.0.0.0"]
        if serial:
            cmd.append(serial)
            if sidste:
                cmd.append(sidste)
        cmd += ["--port", str(MODBUS_PORT)]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if VERBOSE:
            print(r.stdout)
        return r.returncode, r.stdout

    try:
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

    print("\n" + "─" * 40)
    if fejl == 0:
        print(f"\033[1;32m{tjek} tjek, alle bestået\033[0m\n")
        return 0
    print(f"\033[1;31m{tjek} tjek, {fejl} fejlede\033[0m\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
