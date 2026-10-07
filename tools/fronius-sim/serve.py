#!/usr/bin/env python3
"""
zScreen - Fronius-simulator.

Svarer som en Fronius Gen24 med batteri og Smart Meter over Modbus TCP,
saa hele skaermen kan proeves af uden at have et anlaeg i naerheden:
netvaerksscanningen, valget af inverter, de fire tal og alle de
tilstande hvor noget mangler.

    sudo python3 serve.py                      almindelig Gen24 med batteri
    sudo python3 serve.py --profile nobattery  anlaeg uden batteri
    sudo python3 serve.py --profile nometer    anlaeg uden elmaaler
    sudo python3 serve.py --profile nolabels   kanaler uden navne
    sudo python3 serve.py --profile float      inverter i float-tilstand
    python3 serve.py --port 5020               uden sudo, paa hoej port

Port 502 kraever sudo paa macOS og Linux. Skaermen leder paa 502 som
standard, men porten kan saettes i indstillingerne, og zs-probe --scan
tager ogsaa --port. Saa en hoej port er nok til at proeve soegningen af:

    python3 serve.py --port 5502 --bind 127.0.0.1
    ../zs-probe/zs-probe --scan 127.0.0.0 --port 5502
"""

import argparse
import socket
import socketserver
import struct
import sys
import threading
import time

sys.path.insert(0, __file__.rsplit("/", 1)[0])

import sunspec
from plant import Plant

# Modbus exception-koder
EXC_ILLEGAL_FUNCTION = 0x01
EXC_ILLEGAL_ADDRESS = 0x02
EXC_ILLEGAL_VALUE = 0x03
EXC_GATEWAY_NO_RESPONSE = 0x0B

FC_READ_HOLDING = 0x03
FC_WRITE_MULTI  = 0x10

STATE = {
    "lock": threading.Lock(),
    "units": {},        # unit_id -> {addr: value}
    "plant": None,
    "verbose": False,
    "requests": 0,
}


# ----------------------------------------------------------------------
# Modbus TCP
# ----------------------------------------------------------------------

class ModbusHandler(socketserver.BaseRequestHandler):

    def _recv_exact(self, n: int) -> bytes:
        """TCP er en stroem uden rammer. Et svar kan sagtens komme i to
        stumper, og en server der ikke haandterer det, virker fint paa
        et hurtigt netvaerk og gaar i stykker paa et langsomt."""
        buf = b""
        while len(buf) < n:
            chunk = self.request.recv(n - len(buf))
            if not chunk:
                return b""
            buf += chunk
        return buf

    def handle(self):
        peer = self.client_address[0]
        self.request.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.request.settimeout(60.0)
        if STATE["verbose"]:
            print(f"  [{peer}] forbundet")
        try:
            while True:
                head = self._recv_exact(6)
                if not head:
                    break
                tid, pid, length = struct.unpack(">HHH", head)
                if pid != 0 or length < 2 or length > 253:
                    break
                body = self._recv_exact(length)
                if len(body) != length:
                    break

                unit = body[0]
                fc = body[1]

                # --tavs N: svar paa de foerste N og TI saa stille, med
                # forbindelsen aaben.
                #
                # Det er ikke en opfundet situation. En inverter der har
                # travlt, fx fordi den samtidig serverer sin egen
                # hjemmeside eller sender til Solar.web, kan holde op med
                # at svare uden at lukke. For skaermen ser det ud som om
                # alt er fint, lige indtil timeouten loeber ud. Uden den
                # her kunne den vej slet ikke proeves.
                tavs_efter = STATE.get("tavs_efter")
                if tavs_efter is not None:
                    STATE["svar_taeller"] = STATE.get("svar_taeller", 0) + 1
                    if STATE["svar_taeller"] > tavs_efter:
                        if STATE["verbose"]:
                            print(f"  [{peer}] tier stille, forbindelsen "
                                  f"holdes aaben")
                        continue

                resp = self._dispatch(tid, unit, fc, body[2:])
                if resp:
                    self.request.sendall(resp)
        except (socket.timeout, ConnectionResetError, BrokenPipeError, OSError):
            pass
        finally:
            if STATE["verbose"]:
                print(f"  [{peer}] lukket")

    def _dispatch(self, tid: int, unit: int, fc: int, payload: bytes) -> bytes:
        with STATE["lock"]:
            STATE["requests"] += 1
            units = STATE["units"]

            if fc == FC_WRITE_MULTI:
                return self._skriv(tid, unit, fc, payload, units)

            if fc != FC_READ_HOLDING:
                return self._exception(tid, unit, fc, EXC_ILLEGAL_FUNCTION)

            if unit not in units:
                # Ukendt unit. En rigtig Fronius svarer med en gateway-fejl
                # naar man spoerger efter en elmaaler der ikke findes, og
                # det er praecis den vej skaermens maaler-soegning gaar.
                return self._exception(tid, unit, fc, EXC_GATEWAY_NO_RESPONSE)

            if len(payload) < 4:
                return self._exception(tid, unit, fc, EXC_ILLEGAL_VALUE)
            addr, count = struct.unpack(">HH", payload[:4])

            if count < 1 or count > 125:
                return self._exception(tid, unit, fc, EXC_ILLEGAL_VALUE)

            regs = units[unit]

            # --naegt: en model staar i kaeden, men kan ikke laeses.
            #
            # Det sker i virkeligheden. En inverter melder fx model 160 i
            # sin modelliste og svarer saa med en adressefejl naar man
            # henter den, typisk efter en firmwareopdatering hvor listen
            # og indholdet ikke foelges ad. Skaermen skal klare det uden
            # at miste resten af anlaegget.
            # Kun paa INVERTERENS unit. Adresserne er forskellige per
            # unit, og uden det her ramte afvisningen ogsaa elmaaleren.
            naegtet = (STATE.get("naegt") or set()) if unit == STATE.get("inv_unit") else set()
            if naegtet:
                kun_data = STATE.get("naegt_kun_data") or set()
                for mid, (start, slut, dstart, dslut) in (STATE.get("model_omraader") or {}).items():
                    if mid not in naegtet:
                        continue
                    # Hele modellen, eller kun dataene og ikke hovedet.
                    a0, a1 = (dstart, dslut) if mid in kun_data else (start, slut)
                    if not (addr + count <= a0 or addr > a1):
                        return self._exception(tid, unit, fc, EXC_ILLEGAL_ADDRESS)

            values = []
            for i in range(count):
                a = addr + i
                if a not in regs:
                    return self._exception(tid, unit, fc, EXC_ILLEGAL_ADDRESS)
                values.append(regs[a] & 0xFFFF)

            data = b"".join(struct.pack(">H", v) for v in values)
            pdu = struct.pack(">BB", fc, len(data)) + data
            return struct.pack(">HHH", tid, 0, len(pdu) + 1) + bytes([unit]) + pdu

    def _skriv(self, tid, unit, fc, payload, units):
        """
        Funktionskode 16.

        Tre maader at opfoere sig paa, og den midterste er den vigtige:

          skriv   helt normalt, registrene aendrer sig
          tavs    svarer PAENT og aendrer INGENTING
          naegt   svarer med en rigtig exception

        Den tavse er ikke opfundet. Fronius skriver selv i sin manual:
        "If an attempt is made to write to such registers, the inverter
        does not return an exception code!" Det sker fx naar "Inverter
        control via Modbus" ikke er slaaet til paa inverterens webside.
        Uden tilbagelaesning kan den ikke skelnes fra en skrivning der
        lykkedes, og det er praecis det der skal kunne proeves af.
        """
        # payload begynder EFTER funktionskoden, praecis som i laesevejen.
        if len(payload) < 5:
            return self._exception(tid, unit, fc, EXC_ILLEGAL_VALUE)
        addr, antal = struct.unpack(">HH", payload[0:4])
        bc = payload[4]
        data = payload[5:5 + bc]
        if antal < 1 or antal > 123 or bc != antal * 2 or len(data) != bc:
            return self._exception(tid, unit, fc, EXC_ILLEGAL_VALUE)

        maade = STATE.get("skrivemaade", "skriv")
        if maade == "naegt":
            return self._exception(tid, unit, fc, EXC_ILLEGAL_ADDRESS)
        if maade != "tavs":
            regs = units[unit]
            skrevet = STATE.setdefault("skrevet", {})
            for i in range(antal):
                v = struct.unpack(">H", data[i * 2:i * 2 + 2])[0]
                regs[addr + i] = v
                if unit == STATE.get("inv_unit"):
                    skrevet[addr + i] = v

        # Svaret er et EKKO af adresse og antal, ogsaa naar vi intet gjorde.
        pdu = struct.pack(">BHH", fc, addr, antal)
        return struct.pack(">HHH", tid, 0, len(pdu) + 1) + bytes([unit]) + pdu

    @staticmethod
    def _exception(tid: int, unit: int, fc: int, code: int) -> bytes:
        pdu = bytes([fc | 0x80, code])
        return struct.pack(">HHH", tid, 0, len(pdu) + 1) + bytes([unit]) + pdu


class ThreadedServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


# ----------------------------------------------------------------------
# Simulering
# ----------------------------------------------------------------------

def parse_fejl(tekst: str) -> dict:
    """\"evt1=0x80,st=7\" bliver til {\"evt1\": 128, \"st\": 7}."""
    ud = {}
    for del_ in tekst.split(","):
        del_ = del_.strip()
        if not del_ or "=" not in del_:
            continue
        n, v = del_.split("=", 1)
        try:
            ud[n.strip().lower()] = int(v.strip(), 0)
        except ValueError:
            pass
    return ud


def update_registers(inv_dev, meter_dev, p: Plant, has_meter: bool,
                     float_models: bool, fejl: dict = None):
    """Skriver anlaeggets nuvaerende tilstand ind i registerkortet."""
    fejl = fejl or {}

    # --- inverter ---
    if float_models:
        m = inv_dev.find(113)
        m.f32(20, p.inverter_ac_w)          # W
        m.f32(22, 50.0)                     # Hz
        m.f32(30, p.wh_pv)                  # WH, stiger
        m.enum16(46, fejl.get("st", 4))     # St
        m.enum16(47, fejl.get("stvnd", 0))  # StVnd
        m.acc32(48, fejl.get("evt1", 0))
        m.acc32(50, fejl.get("evt2", 0))
        m.acc32(52, fejl.get("evtvnd1", 0))
        m.acc32(54, fejl.get("evtvnd2", 0))
        m.acc32(56, fejl.get("evtvnd3", 0))
        m.acc32(58, fejl.get("evtvnd4", 0))
    else:
        m = inv_dev.find(103)
        m.i16(12, round(p.inverter_ac_w))   # W, W_SF = 0
        m.u16(14, 5000)                     # Hz, Hz_SF = -2 -> 50,00
        m.acc32(22, int(p.wh_pv))           # WH, stiger
        m.enum16(36, fejl.get("st", 4))     # St, 4 = MPPT
        m.enum16(37, fejl.get("stvnd", 0))  # StVnd, Fronius' egen kode
        m.acc32(38, fejl.get("evt1", 0))    # Evt1
        m.acc32(40, fejl.get("evt2", 0))    # Evt2
        m.acc32(42, fejl.get("evtvnd1", 0))
        m.acc32(44, fejl.get("evtvnd2", 0))
        m.acc32(46, fejl.get("evtvnd3", 0))
        m.acc32(48, fejl.get("evtvnd4", 0))

    # --- batteri ---
    m124 = inv_dev.find(124)
    if m124 is not None:
        m124.u16(6, int(round(p.soc_pct * 100)))   # ChaState, SF = -2
        m124.enum16(9, p.charge_status)

    # --- DC-kanaler ---
    m160 = inv_dev.find(160)
    if m160 is not None:
        # Hvor mange kanaler der FAKTISK er plads til, ikke hvad N
        # paastaar. Med --loegn-kanaler lyver N med vilje, og saa maa
        # simulatoren ikke skrive uden for sin egen tabel.
        n_ch = min(m160.data[6], (len(m160.data) - 8) // 20)
        ACTIVE, SLEEPING = 4, 2
        for i in range(n_ch):
            base = 8 + i * 20
            # DCWH paa base+12 er kanalens EGEN livstaeller, acc32.
            #
            # Det er den der goer batteriets energi maalbar: paa en
            # Fronius ligger lade- og afladesiden som to ekstra kanaler,
            # og hver kanal har sin egen taeller. Saa batteriets ind og ud
            # er praecise tal og ikke noget der skal regnes af effekten.
            if i < len(p.string_w):
                w = p.string_w[i]
                m160.u16(base + 11, int(round(max(0.0, w))))
                m160.acc32(base + 12, int(p.wh_string[i]))
                m160.enum16(base + 17, ACTIVE if w > 5.0 else SLEEPING)
            elif i == len(p.string_w):
                # ladekanal
                charge = -p.battery_w if p.battery_w < 0 else 0.0
                m160.u16(base + 11, int(round(charge)))
                m160.acc32(base + 12, int(p.wh_bat_ind))
                m160.enum16(base + 17, ACTIVE if charge > 5.0 else SLEEPING)
            else:
                # afladekanal
                dis = p.battery_w if p.battery_w > 0 else 0.0
                m160.u16(base + 11, int(round(dis)))
                m160.acc32(base + 12, int(p.wh_bat_ud))
                m160.enum16(base + 17, ACTIVE if dis > 5.0 else SLEEPING)

    # --- elmaaler ---
    if has_meter and meter_dev is not None:
        if float_models:
            mm = meter_dev.find(213)
            mm.f32(26, p.grid_w)
            # Flydende tal baerer selv deres stoerrelse, saa ingen
            # skalafaktor. Offsets efterset mod model_213.json.
            mm.f32(58, p.wh_exp)            # TotWhExp
            mm.f32(66, p.wh_imp)            # TotWhImp
        else:
            mm = meter_dev.find(203)
            mm.i16(16, round(p.grid_w))     # W, positiv = koeb
            # Taellerne, efterset mod model_203.json: Exp paa 36, Imp paa
            # 44 og skalafaktoren paa 52, alt regnet fra datablokkens
            # start som resten af filen.
            mm.acc32(36, int(p.wh_exp))     # TotWhExp
            mm.acc32(44, int(p.wh_imp))     # TotWhImp


def ticker(inv_dev, meter_dev, p: Plant, args, inv_unit: int, meter_unit: int,
           fejl: dict = None):
    """Driver uret og opdaterer registrene."""
    sim_seconds = args.start_hour * 3600.0
    last = time.monotonic()
    last_print = 0.0

    while True:
        now = time.monotonic()
        real_dt = now - last
        last = now

        if args.realtime:
            lt = time.localtime()
            sim_seconds = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec
            dt = max(0.001, real_dt)
        else:
            dt = real_dt * args.speed
            sim_seconds = (sim_seconds + dt) % 86400.0

        p.step(sim_seconds / 86400.0, dt)

        with STATE["lock"]:
            update_registers(inv_dev, meter_dev, p, meter_dev is not None,
                             args.profile == "float", fejl)
            nye = inv_dev.build_registers()
            # Behold hvad der er SKREVET udefra.
            #
            # Tickeren bygger registrene om hver runde ud af anlaeggets
            # tilstand. Uden det her blev en skrivning overskrevet et
            # oejeblik senere, og saa kunne man ikke se forskel paa en
            # skrivning der virkede og en der blev ignoreret. Det er
            # praecis den forskel det hele handler om.
            for adr in STATE.get("skrevet", ()):
                nye[adr] = STATE["skrevet"][adr]
            STATE["units"][inv_unit] = nye
            STATE["model_omraader"] = inv_dev.model_omraader()
            STATE["inv_unit"] = inv_unit
            if meter_dev is not None:
                STATE["units"][meter_unit] = meter_dev.build_registers()

        if now - last_print >= args.print_every:
            last_print = now
            h = int(sim_seconds // 3600)
            mi = int((sim_seconds % 3600) // 60)
            with STATE["lock"]:
                n = STATE["requests"]
            print(f"  {h:02d}:{mi:02d}  {p.summary()}   [{n} forespoergsler]",
                  flush=True)

        time.sleep(0.25)


# ----------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="Simuleret Fronius Gen24 paa Modbus TCP",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    ap.add_argument("--profile", default="battery",
                    choices=["battery", "nobattery", "nometer", "nolabels",
                             "float", "onestring"],
                    help="hvilket slags anlaeg der simuleres")
    ap.add_argument("--port", type=int, default=502,
                    help="Modbus-port. 502 kraever sudo. Skaermen leder "
                         "paa 502 som standard, men baade den og "
                         "zs-probe --scan kan saettes til en anden")
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--skrivemaade", choices=["skriv", "tavs", "naegt"],
                    default="skriv", dest="skrivemaade",
                    help="hvordan simulatoren tager imod en skrivning. tavs "
                         "svarer paent og aendrer ingenting, praecis som en "
                         "Fronius hvor styring ikke er slaaet til")
    ap.add_argument("--loegn-kanaler", type=int, default=0,
                    dest="loegn_kanaler",
                    help="lad model 160 paastaa flere kanaler end den er "
                         "lang til, fx 8")
    ap.add_argument("--maaler-i-kaeden", action="store_true",
                    dest="maaler_i_kaeden",
                    help="elmaaleren i inverterens egen kaede i stedet for "
                         "paa sin egen unit")
    ap.add_argument("--naegt-kun-data", default="",
                    help="modeller hvor kun DATAENE naegtes, ikke hovedet. "
                         "Saa kan kaeden laeses videre")
    ap.add_argument("--tavs-efter", type=int, default=None,
                    help="svar paa de foerste N forespoergsler og ti saa "
                         "stille, med forbindelsen aaben. Som en inverter "
                         "der haenger eller har for travlt")
    ap.add_argument("--naegt", default="",
                    help="modelnumre der staar i kaeden men ikke kan laeses, "
                         "fx 160 eller 124,160")
    ap.add_argument("--inverter-unit", type=int, default=1)
    ap.add_argument("--meter-unit", type=int, default=200)
    ap.add_argument("--base", type=int, default=40000, choices=[40000, 40001],
                    help="hvor SunSpec-markoeren ligger")
    ap.add_argument("--speed", type=float, default=600.0,
                    help="simulerede sekunder pr. virkeligt sekund. "
                         "600 giver et doegn paa knap 2,5 minut")
    ap.add_argument("--realtime", action="store_true",
                    help="foelg maskinens rigtige ur i stedet")
    ap.add_argument("--start-hour", type=float, default=11.0)
    ap.add_argument("--print-every", type=float, default=5.0)
    ap.add_argument("--fejl", default="",
                    help="meld fejl fra inverteren, fx "
                         "\"evt1=0x80,evtvnd2=0x80,st=7\". Bruges til at "
                         "proeve fejlsiden af uden et rigtigt anlaeg")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    STATE["verbose"] = args.verbose
    STATE["naegt"] = {int(x) for x in args.naegt.split(",") if x.strip()}
    STATE["tavs_efter"] = args.tavs_efter
    STATE["svar_taeller"] = 0
    STATE["skrivemaade"] = args.skrivemaade
    STATE["naegt_kun_data"] = {int(x) for x in args.naegt_kun_data.split(",") if x.strip()}
    STATE["naegt"] |= STATE["naegt_kun_data"]

    has_battery = args.profile not in ("nobattery",)
    has_meter = args.profile not in ("nometer",)
    labels = args.profile != "nolabels"
    floats = args.profile == "float"
    strings = 1 if args.profile == "onestring" else 2

    sunspec.SUNSPEC_BASE = args.base
    inv_dev = sunspec.make_inverter(has_battery=has_battery,
                                    pv_strings=strings,
                                    label_channels=labels,
                                    float_models=floats,
                                      loegn_kanaler=args.loegn_kanaler)
    inv_dev.base = args.base
    meter_dev = None
    if has_meter:
        meter_dev = sunspec.make_meter(float_models=floats)
        meter_dev.base = args.base

    p = Plant(has_battery=has_battery, pv_strings=strings)

    fejl = parse_fejl(args.fejl)
    with STATE["lock"]:
        update_registers(inv_dev, meter_dev, p, has_meter, floats, fejl)
        if args.maaler_i_kaeden and meter_dev is not None:
            # Elmaaleren i INVERTERENS egen kaede i stedet for paa sin
            # egen unit.
            #
            # Det findes i virkeligheden, og firmwaren har en egen
            # kodevej til det (meter_in_inverter_chain). Den var aldrig
            # proevet af: simulatoren lagde altid maaleren paa unit 200.
            # Vi haenger maalerens modeller bagerst i inverterens liste,
            # og saa er der kun ÉN unit.
            for m in meter_dev.models:
                if m.id != 1:
                    inv_dev.models.append(m)
            STATE["units"][args.inverter_unit] = inv_dev.build_registers()
        else:
            STATE["units"][args.inverter_unit] = inv_dev.build_registers()
            if meter_dev is not None:
                STATE["units"][args.meter_unit] = meter_dev.build_registers()
        STATE["plant"] = p

    t = threading.Thread(target=ticker,
                         args=(inv_dev, meter_dev, p, args,
                               args.inverter_unit, args.meter_unit, fejl),
                         daemon=True)
    t.start()

    try:
        server = ThreadedServer((args.bind, args.port), ModbusHandler)
    except PermissionError:
        print(f"\nKan ikke binde port {args.port}. Port under 1024 kraever sudo.\n"
              f"Proev:   sudo python3 {sys.argv[0]} --profile {args.profile}\n"
              f"Eller:   python3 {sys.argv[0]} --port 5020\n", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"\nKan ikke binde {args.bind}:{args.port}: {e}\n", file=sys.stderr)
        return 1

    ips = local_ips()
    print()
    print("  Fronius-simulator kører")
    print("  ----------------------------------------------------------")
    print(f"  Profil          {args.profile}")
    print(f"  Lytter paa      {args.bind}:{args.port}")
    for ip in ips:
        print(f"  Find den paa    {ip}:{args.port}")
    print(f"  Inverter        unit {args.inverter_unit}, "
          f"{'model 113 (float)' if floats else 'model 103 (heltal)'}")
    if meter_dev is not None:
        print(f"  Elmaaler        unit {args.meter_unit}, "
              f"{'model 213' if floats else 'model 203'}")
    else:
        print("  Elmaaler        ingen")
    print(f"  Batteri         {'10,24 kWh' if has_battery else 'ingen'}")
    print(f"  Kanalnavne      {'ja' if labels else 'nej'}")
    print(f"  SunSpec-base    {args.base}")
    if fejl:
        print(f"  Melder fejl     {fejl}")
    if args.realtime:
        print("  Tid             foelger maskinens ur")
    elif args.speed <= 0:
        # Staar stille. Bruges af ende til ende-testene naar de
        # sammenligner tal: ellers naar solen at flytte sig mellem at
        # simulatoren siger hvad den har, og skaermen naar at laese det,
        # og saa fejler en test der er helt i orden.
        print("  Tid             staar stille")
    else:
        print(f"  Tid             {args.speed:.0f}x, et doegn paa "
              f"{86400 / args.speed / 60:.1f} minutter")
    print("  ----------------------------------------------------------")
    print("  Ctrl-C for at stoppe")
    print(flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n  Stoppet.\n")
    return 0


def local_ips() -> list[str]:
    """De IP-adresser skaermen kan naa os paa."""
    out = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 53))
        out.append(s.getsockname()[0])
        s.close()
    except OSError:
        pass
    return out


if __name__ == "__main__":
    sys.exit(main())
