#!/usr/bin/env python3
"""
Maaler et RIGTIGT anlaeg, og efterproever firmwarens registerkort mod det.

SYSTEMETS PYTHON. Homebrews er i stykker paa den her maskine - se
find-port.py for hvorfor. Scriptet bruger kun stdlib, saa der er
ingenting at hente.

HVORFOR FILEN FINDES.

docs/faseplan.md siger det selv, nederst: "Skaermen har ALDRIG staaet
paa et rigtigt anlaeg", og det kalder den et stoerre hul end noget i
Fase E. Og Fase E har fem punkter under "Det der SKAL maales paa et
anlaeg foerst", hvoraf ingen er gjort:

  1. hvilke adresser OutWRte og InWRte faktisk ligger paa. Vores
     offsets er spec minus to, og de er aldrig fundet
  2. hvad skalafaktoren er. 10000 og 500 LIGNER hundrededele procent
  3. om negative vaerdier kan skrives, og hvordan de kodes
  4. nulstillingstimeren - den skal findes i registrene
  5. om inverteren kvitterer. Fronius svarer uden fejl ogsaa naar en
     skrivning bliver afvist

Punkt 1, 2 og 4 er ren LAESNING. Punkt 5 kan afgoeres ved at skrive til
et register der ikke kan skrives, og se hvad der kommer tilbage - det
aendrer ingenting paa anlaegget. Punkt 3 kan kun afgoeres ved at skrive
en vaerdi der betyder noget, og det staar bag --skriv-rate.

HVAD DEN IKKE GOER AF SIG SELV.

Den aabner ikke et styrevindue paa en kundes batteri. Uden flag skriver
den KUN til et register der er maalt skrivebeskyttet, og aldrig en
rate, en tilstand eller en reserve. Et batteri i et hjem er ikke et
proevebord.

    ./tools/proev-anlaeg.py                      finder selv anlaegget
    ./tools/proev-anlaeg.py --vaert 192.168.0.25
    ./tools/proev-anlaeg.py --skriv-rate         AENDRER ANLAEGGET
"""
import argparse
import socket
import struct
import sys

G, R, Y, B, S = "\033[1;32m", "\033[1;31m", "\033[1;33m", "\033[1m", "\033[0m"
FEJL = 0
MAALT = {}


def ok(t):
    print(f"  {G} ok {S} {t}")


def nej(t):
    global FEJL
    FEJL += 1
    print(f"  {R}FEJL{S} {t}")


def info(t):
    print(f"  {Y} ?  {S} {t}")


def suite(t):
    print(f"\n{B}{t}{S}")


# ── modbus, kun det vi skal bruge ────────────────────────────────────
#
# Rammerne bygges i haanden og ikke med et bibliotek. Det er PRAECIS de
# samme felter firmwaren bygger i zs_modbus_tcp.c, saa en forskel
# mellem proeven og firmwaren ikke kan skjule sig i et lag.
class MB:
    def __init__(self, vaert, port=502, tid=3.0):
        self.vaert, self.port, self.tid, self.s, self.tid_nr = vaert, port, tid, None, 0

    def aaben(self):
        self.s = socket.create_connection((self.vaert, self.port), self.tid)
        self.s.settimeout(self.tid)

    def luk(self):
        if self.s:
            try:
                self.s.close()
            finally:
                self.s = None

    def _byt(self, fk, krop, unit):
        self.tid_nr = (self.tid_nr + 1) & 0xFFFF
        tid_nr = self.tid_nr
        ramme = struct.pack(">HHHBB", tid_nr, 0, len(krop) + 2, unit, fk) + krop
        self.s.sendall(ramme)
        hoved = self._laes(8)
        t, p, laengde, u, f = struct.unpack(">HHHBB", hoved)
        if t != tid_nr:
            raise IOError(f"transaktions-id {t}, ventede {tid_nr}")
        rest = self._laes(laengde - 2)
        if f & 0x80:
            return None, rest[0]
        return rest, None

    def _laes(self, n):
        ud = b""
        while len(ud) < n:
            d = self.s.recv(n - len(ud))
            if not d:
                raise IOError("forbindelsen lukkede midt i et svar")
            ud += d
        return ud

    def laes(self, adr, antal, unit=1):
        """Holding-registre. Giver en liste af raa uint16, eller rejser."""
        krop, und = self._byt(3, struct.pack(">HH", adr, antal), unit)
        if und is not None:
            raise IOError(f"modbus-undtagelse {und}")
        n = krop[0]
        return list(struct.unpack(">" + "H" * (n // 2), krop[1:1 + n]))

    # DER ER INGEN skriv() HER, OG DET ER EN BESLUTNING.
    #
    # Vaerktoejet maaler et anlaeg der staar i et hjem, med et
    # batteri. En klient der KAN skrive, kan skrive ved en fejl - et
    # forkert flag, en linje flyttet et sted hen hvor den koeres uden
    # at nogen ser paa. Saa findes muligheden ikke.
    #
    # Fase E's punkt 5, om inverteren kvitterer, kraever en skrivning.
    # Den hoerer i et eget vaerktoej som et menneske starter.


def u16(v):
    return v


def i16(v):
    return v - 0x10000 if v >= 0x8000 else v


def skala(sf):
    """SunSpecs skalafaktor: en int16 tierpotens. 0x8000 = ikke sat."""
    if sf == 0x8000:
        return None
    return 10 ** i16(sf)


# ── SunSpec-kortet ───────────────────────────────────────────────────
#
# Basen proeves paa BEGGE de to adresser firmwaren proever, i samme
# raekkefoelge. zs_sunspec.c: SUNS_TRY = { 40000, 40001 }, fordi
# producenterne ikke er enige om 40001 i papiret er ledningsadresse
# 40000 eller 40001.
SUNS_TRY = (40000, 40001)


def find_kort(mb, unit):
    """(base, [(id, laengde, adresse_paa_data)]) eller (None, [])."""
    base = None
    for b in SUNS_TRY:
        try:
            if struct.pack(">HH", *mb.laes(b, 2, unit)) == b"SunS":
                base = b
                break
        except IOError:
            continue
    if base is None:
        return None, []
    modeller, adr = [], base + 2
    for _ in range(64):
        try:
            mid, laengde = mb.laes(adr, 2, unit)
        except IOError:
            break
        if mid == 0xFFFF:
            break
        if laengde == 0:
            break
        modeller.append((mid, laengde, adr + 2))
        adr += 2 + laengde
    return base, modeller


# Model 124, Storage, som SunSpec selv skriver den. Offset er fra
# DATAENS begyndelse, altsaa efter ID og laengde - og det er netop den
# "spec minus to" faseplanen taler om.
M124 = [
    (0, "WChaMax", "u16"), (1, "WChaGra", "u16"), (2, "WDisChaGra", "u16"),
    (3, "StorCtl_Mod", "bit"), (4, "VAChaMax", "u16"), (5, "MinRsvPct", "u16"),
    (6, "ChaState", "u16"), (7, "StorAval", "u16"), (8, "InBatV", "u16"),
    (9, "ChaSt", "enum"), (10, "OutWRte", "i16"), (11, "InWRte", "i16"),
    (12, "InOutWRte_WinTms", "u16"), (13, "InOutWRte_RvrtTms", "u16"),
    (14, "InOutWRte_RmpTms", "u16"), (15, "ChaGriSet", "enum"),
    (16, "WChaMax_SF", "sf"), (17, "WChaDisChaGra_SF", "sf"),
    (18, "VAChaMax_SF", "sf"), (19, "MinRsvPct_SF", "sf"),
    (20, "ChaState_SF", "sf"), (21, "StorAval_SF", "sf"),
    (22, "InBatV_SF", "sf"), (23, "InOutWRte_SF", "sf"),
]

# Det firmwaren TROR. zs_sunspec.h, model 124.
FW_124 = {
    "WChaMax": 0, "StorCtl_Mod": 3, "MinRsvPct": 5, "ChaState": 6,
    "ChaSt": 9, "WChaMax_SF": 16, "ChaState_SF": 20,
}
FW_124_MIN_LEN = 21

CHA_ST = {1: "FRA", 2: "TOM", 3: "OPLADER", 4: "AFLADER", 5: "FULD",
          6: "HOLDER", 7: "PROEVER"}


def find_anlaeg(net_hint=None, port=502):
    """Leder paa undernettet efter noget der lytter paa 502, som firmwaren."""
    import concurrent.futures as cf
    if net_hint is None:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 53))
        egen = s.getsockname()[0]
        s.close()
        net_hint = egen.rsplit(".", 1)[0]
    print(f"  leder paa {net_hint}.1-254, port {port} ...")

    def et(i):
        ip = f"{net_hint}.{i}"
        try:
            socket.create_connection((ip, port), 0.4).close()
            return ip
        except OSError:
            return None
    with cf.ThreadPoolExecutor(64) as ex:
        return [i for i in ex.map(et, range(1, 255)) if i]


def proev(vaert, port=502):
    suite(f"Forbindelsen til {vaert}")
    mb = MB(vaert, port)
    try:
        mb.aaben()
    except OSError as e:
        nej(f"kan ikke forbinde paa {port}: {e}")
        return
    ok(f"port {port} svarer")

    # ── kortet, for hver unit firmwaren taler med ──
    #
    # zs_modbus_tcp.h: "Vi skal tale med unit 1 (inverter) og unit
    # 200/201/240/241 (elmaaler)". Alle proeves, saa vi ved hvad der
    # ER der - og ikke kun hvad vi plejer at spoerge om.
    suite("SunSpec-kortet, unit for unit")
    kort = {}
    for unit in (1, 200, 201, 240, 241):
        base, modeller = find_kort(mb, unit)
        if base is None:
            info(f"unit {unit:3d}: ingen SunS-markoer")
            continue
        kort[unit] = (base, modeller)
        ider = ", ".join(str(m[0]) for m in modeller)
        ok(f"unit {unit:3d}: base {base}, {len(modeller)} modeller: {ider}")
    if not kort:
        nej("ingen unit svarede SunSpec - er Modbus TCP slaaet til?")
        mb.luk()
        return
    MAALT["kort"] = {u: v[1] for u, v in kort.items()}

    # Basen: firmwaren proever 40000 FOER 40001. Rammer den forkert,
    # ligger hele kortet to registre ved siden af.
    for unit, (base, _) in kort.items():
        if base == SUNS_TRY[0]:
            ok(f"unit {unit}: basen er {base}, altsaa den firmwaren proever foerst")
        else:
            info(f"unit {unit}: basen er {base} og ikke {SUNS_TRY[0]}")

    # ── model 124, hele blokken ──
    suite("Batteriet: model 124 register for register")
    m124 = None
    for unit, (_, modeller) in kort.items():
        for mid, laengde, adr in modeller:
            if mid == 124:
                m124 = (unit, laengde, adr)
                break
    if m124 is None:
        nej("model 124 findes ikke paa anlaegget. Er der et batteri paa "
            "Modbus, og er SunSpec-modellen slaaet til i inverteren?")
    else:
        unit, laengde, adr = m124
        ok(f"model 124 paa unit {unit}, laengde {laengde}, data fra {adr}")
        if laengde >= FW_124_MIN_LEN:
            ok(f"laengden er mindst ZS_M124_MIN_LEN ({FW_124_MIN_LEN})")
        else:
            nej(f"laengden {laengde} er UNDER ZS_M124_MIN_LEN "
                f"({FW_124_MIN_LEN}) - firmwaren laeser uden for modellen")
        raa = mb.laes(adr, laengde, unit)
        MAALT["124"] = {"unit": unit, "adr": adr, "laengde": laengde, "raa": raa}

        sf = {}
        for off, navn, slags in M124:
            if slags == "sf" and off < laengde:
                sf[navn] = raa[off]

        print()
        for off, navn, slags in M124:
            if off >= laengde:
                continue
            v = raa[off]
            abs_adr = adr + off
            if slags == "sf":
                s = skala(v)
                vis = "ikke sat" if s is None else f"10^{i16(v)} = {s}"
            elif slags == "i16":
                vis = str(i16(v))
            elif slags == "bit":
                vis = f"0b{v:016b} ({v})"
            elif slags == "enum":
                vis = f"{v} = {CHA_ST.get(v, '?')}" if navn == "ChaSt" else str(v)
            else:
                vis = str(v)
            if v == 0x8000 and slags != "sf":
                vis += "  (SunSpecs \"ikke implementeret\")"
            print(f"      {abs_adr:6d}  off {off:2d}  {navn:20s} {vis}")
        print()

        # ── punkt 1: ligger de fire navne hvor vi tror ──
        for navn, vent in FW_124.items():
            har = next(o for o, n, _ in M124 if n == navn)
            if har == vent:
                ok(f"{navn}: offset {vent}, som firmwaren tror")
            else:
                nej(f"{navn}: firmwaren bruger {vent}, specen siger {har}")

        # ── punkt 1 og 4: de to vi ALDRIG har fundet, plus timeren ──
        for navn in ("OutWRte", "InWRte", "InOutWRte_RvrtTms"):
            off = next(o for o, n, _ in M124 if n == navn)
            if off >= laengde:
                nej(f"{navn} ligger paa offset {off}, men modellen er kun "
                    f"{laengde} lang - feltet FINDES IKKE paa det her anlaeg")
                continue
            v = raa[off]
            if v == 0x8000:
                info(f"{navn}: adresse {adr + off} (offset {off}), staar paa "
                     f"\"ikke implementeret\" - findes, men er ikke i brug")
            else:
                ok(f"{navn}: adresse {adr + off} (offset {off}), "
                   f"vaerdi {i16(v) if navn == 'OutWRte' or navn == 'InWRte' else v}")

        # ── punkt 2: skalafaktoren, laest og ikke antaget ──
        s = skala(sf.get("InOutWRte_SF", 0x8000))
        if s is None:
            info("InOutWRte_SF er ikke sat. Saa er 10000 og 500 ikke "
                 "hundrededele procent FORDI inverteren siger det")
        elif s == 0.01:
            ok("InOutWRte_SF = 10^-2, altsaa hundrededele procent. "
               "10000 = 100,00 % og 500 = 5,00 % - som noterne gaettede")
        else:
            nej(f"InOutWRte_SF giver {s}, IKKE hundrededele procent. "
                f"Tabellen i batteristyring.md er regnet paa en forkert skala")
        s = skala(sf.get("MinRsvPct_SF", 0x8000))
        if s is not None:
            ok(f"MinRsvPct_SF = {s} (9900 bliver {9900 * s}, 500 bliver {500 * s})")
        s = skala(sf.get("ChaState_SF", 0x8000))
        if s is not None and "ChaState" in [n for _, n, _ in M124]:
            soc = raa[6] * s
            ok(f"ChaState: batteriet staar paa {soc:g} %")
    mb.luk()


# ── Punkt 5 er IKKE bygget her, og det er med vilje ──────────────────
#
# "Om inverteren kvitterer" er det vigtigste ubesvarede spoergsmaal i
# Fase E: zs_mb_write_verified laeser tilbage FORDI noterne siger at
# Fronius svarer uden fejl naar en skrivning afvises. Er det rigtigt,
# er tilbagelaesningen det eneste der holder.
#
# Det kan kun afgoeres ved at SKRIVE til en rigtig inverter. Anlaegget
# her er en kundes, med et batteri i et hjem, og det er ikke et
# proevebord. Den proeve skal et menneske starte, se paa, og kunne
# stoppe - ikke et vaerktoej der koerer af sig selv.
#
# Naar den bygges, er den sikreste form et MAALT felt, fx ChaState:
# inverteren regner det selv ud, ingen styring hviler paa det, og en
# skrivning dertil kan kun afvises eller ignoreres. Svarer den "ok" og
# staar vaerdien uaendret bagefter, er noterne bekraeftet.


def proev_fortegn(vaert, port=502):
    """test-plan.md afsnit 3: nettets fortegn. Ren laesning."""
    suite("Nettet og forbruget (test-plan afsnit 3, aldrig maalt foer)")
    mb = MB(vaert, port)
    try:
        mb.aaben()
    except OSError as e:
        info(f"kan ikke forbinde: {e}")
        return
    try:
        fandt = False
        for unit, modeller in MAALT.get("kort", {}).items():
            for mid, laengde, adr in modeller:
                # De tre-fasede maalermodeller. W staar paa offset 18 og
                # skalafaktoren paa 19 i dem alle.
                if mid not in (201, 202, 203, 204, 211, 212, 213, 214):
                    continue
                if laengde < 20:
                    info(f"unit {unit} model {mid}: kun {laengde} registre")
                    continue
                raa = mb.laes(adr, 20, unit)
                w, sfw = i16(raa[18]), skala(raa[19])
                if sfw is None:
                    info(f"unit {unit} model {mid}: W_SF er ikke sat")
                    continue
                watt = w * sfw
                retning = ("IMPORT fra nettet" if watt > 0
                           else "EKSPORT til nettet" if watt < 0 else "nul")
                ok(f"unit {unit} model {mid}: {watt:g} W = {retning}")
                fandt = True
        if not fandt:
            info("ingen maalermodel fundet. Uden den kan nettets fortegn "
                 "ikke efterproeves, og det er punkt 1 i test-plan afsnit 3")
    except IOError as e:
        info(f"kunne ikke laese maaleren: {e}")
    finally:
        mb.luk()


def serienumre(vaerter, port=502):
    """FLERE ADRESSER KAN VAERE ÉN INVERTER.

    Maalt 17. oktober 2026: samme Fronius svarede paa .25 og .43 med
    samme serienummer. Soegningen i firmwaren sammenligner kun IP'er,
    saa den viser den samme inverter to gange i listen.
    """
    suite("Er de flere adresser den SAMME inverter?")
    set_ = {}
    for v in vaerter:
        mb = MB(v, port)
        try:
            mb.aaben()
            base, modeller = find_kort(mb, 1)
            if base is None:
                continue
            for mid, laengde, adr in modeller:
                if mid != 1 or laengde < 66:
                    continue
                raa = mb.laes(adr, 66, unit := 1)
                # Serienummeret ligger paa offset 48, ikke 32.
                # Offset 32 er Opt og 40 er Vr - se ZS_M1_SN i
                # zs_sunspec.h. Foerste udgave laeste variant og
                # firmwareversion og kaldte det et serienummer, og
                # simulatoren svarede derfor med en tom streng.
                sn = struct.pack(">" + "H" * 16, *raa[48:64]).split(b"\0")[0]
                set_.setdefault(sn.decode("ascii", "replace"), []).append(v)
        except (OSError, IOError):
            pass
        finally:
            mb.luk()
    for sn, adresser in set_.items():
        if len(adresser) > 1:
            nej(f"serienummer {sn} svarer paa {len(adresser)} adresser: "
                f"{', '.join(adresser)}. Soegningen viser den samme "
                f"inverter flere gange, for den sammenligner kun IP'er")
        else:
            ok(f"serienummer {sn} paa {adresser[0]}")


def main():
    p = argparse.ArgumentParser(
        description="Maaler et rigtigt anlaeg mod firmwarens registerkort.")
    p.add_argument("--vaert", help="inverterens adresse. Uden den ledes der selv")
    p.add_argument("--net", help="undernet at lede paa, fx 192.168.0")
    # Firmwaren har ogsaa en valgfri port, s_cfg.inverter_port, hvor nul
    # betyder 502. Og simulatoren kan koere paa en hoej port uden sudo,
    # saa vaerktoejet kan proeves af uden et anlaeg.
    p.add_argument("--port", type=int, default=502)
    a = p.parse_args()

    print(f"{B}Maaler et rigtigt anlaeg. Kun laesning.{S}")
    # HER STOD "svarer paa 502" OGSAA MED --vaert, hvor ingen havde
    # spurgt. En udskrift der paastaar en maaling der ikke er gjort, er
    # vaerre end ingen udskrift.
    if a.vaert:
        vaerter = [a.vaert]
        print(f"  adresse givet: {a.vaert}")
    else:
        vaerter = find_anlaeg(a.net, a.port)
        if not vaerter:
            print(f"  {R}fandt ingen der lytter paa {a.port}{S}")
            return 1
        print(f"  svarer paa {a.port}: {', '.join(vaerter)}")

    for v in vaerter:
        proev(v, a.port)
    serienumre(vaerter, a.port)
    proev_fortegn(vaerter[0], a.port)

    print()
    print("─" * 40)
    if FEJL:
        print(f"{R}{FEJL} ting passer ikke med firmwarens kort{S}")
    else:
        print(f"{G}Alt passer med firmwarens kort{S}")
    print("Punkt 5, om inverteren kvitterer, kraever en SKRIVNING til et "
          "rigtigt\nanlaeg og staar derfor ikke her. Se noten i filen.")
    return 1 if FEJL else 0


if __name__ == "__main__":
    sys.exit(main())
