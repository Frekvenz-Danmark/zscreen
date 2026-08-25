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

    def __init__(self, profil, ekstra=None):
        self.profil = profil
        self.args = ["python3", SIM, "--port", str(PORT), "--profile", profil,
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
                s = socket.create_connection(("127.0.0.1", PORT), timeout=0.3)
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
    with Sim("battery") as sim:
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
    test_naar_det_gaar_galt()

    print("\n" + "─" * 40)
    if fejl == 0:
        print(f"\033[1;32m{tjek} tjek, alle bestået\033[0m\n")
        return 0
    print(f"\033[1;31m{tjek} tjek, {fejl} fejlede\033[0m\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
