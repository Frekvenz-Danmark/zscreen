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

Port 502 kraever sudo paa macOS og Linux. Skal netvaerksscanningen i
skaermen kunne finde simulatoren, SKAL den ligge paa 502, for det er
den port der scannes efter.
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

            if fc != FC_READ_HOLDING:
                # zScreen sender kun funktionskode 3. Alt andet er enten
                # en fejl eller nogen der proever at skrive, og begge
                # dele skal afvises tydeligt.
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
            values = []
            for i in range(count):
                a = addr + i
                if a not in regs:
                    return self._exception(tid, unit, fc, EXC_ILLEGAL_ADDRESS)
                values.append(regs[a] & 0xFFFF)

            data = b"".join(struct.pack(">H", v) for v in values)
            pdu = struct.pack(">BB", fc, len(data)) + data
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

def update_registers(inv_dev, meter_dev, p: Plant, has_meter: bool,
                     float_models: bool):
    """Skriver anlaeggets nuvaerende tilstand ind i registerkortet."""

    # --- inverter ---
    if float_models:
        m = inv_dev.find(113)
        m.f32(20, p.inverter_ac_w)          # W
        m.f32(22, 50.0)                     # Hz
        m.f32(30, p.total_wh)               # WH
        m.enum16(46, 4)                     # St = MPPT
    else:
        m = inv_dev.find(103)
        m.i16(12, round(p.inverter_ac_w))   # W, W_SF = 0
        m.u16(14, 5000)                     # Hz, Hz_SF = -2 -> 50,00
        m.acc32(22, int(p.total_wh))        # WH
        m.enum16(36, 4)                     # St = MPPT

    # --- batteri ---
    m124 = inv_dev.find(124)
    if m124 is not None:
        m124.u16(6, int(round(p.soc_pct * 100)))   # ChaState, SF = -2
        m124.enum16(9, p.charge_status)

    # --- DC-kanaler ---
    m160 = inv_dev.find(160)
    if m160 is not None:
        n_ch = m160.data[6]
        ACTIVE, SLEEPING = 4, 2
        for i in range(n_ch):
            base = 8 + i * 20
            if i < len(p.string_w):
                w = p.string_w[i]
                m160.u16(base + 11, int(round(max(0.0, w))))
                m160.enum16(base + 17, ACTIVE if w > 5.0 else SLEEPING)
            elif i == len(p.string_w):
                # ladekanal
                charge = -p.battery_w if p.battery_w < 0 else 0.0
                m160.u16(base + 11, int(round(charge)))
                m160.enum16(base + 17, ACTIVE if charge > 5.0 else SLEEPING)
            else:
                # afladekanal
                dis = p.battery_w if p.battery_w > 0 else 0.0
                m160.u16(base + 11, int(round(dis)))
                m160.enum16(base + 17, ACTIVE if dis > 5.0 else SLEEPING)

    # --- elmaaler ---
    if has_meter and meter_dev is not None:
        if float_models:
            mm = meter_dev.find(213)
            mm.f32(26, p.grid_w)
        else:
            mm = meter_dev.find(203)
            mm.i16(16, round(p.grid_w))     # W, positiv = koeb


def ticker(inv_dev, meter_dev, p: Plant, args, inv_unit: int, meter_unit: int):
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
                             args.profile == "float")
            STATE["units"][inv_unit] = inv_dev.build_registers()
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
                    help="Modbus-port. 502 kraever sudo, men er den eneste "
                         "port skaermens scanning leder efter")
    ap.add_argument("--bind", default="0.0.0.0")
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
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    STATE["verbose"] = args.verbose

    has_battery = args.profile not in ("nobattery",)
    has_meter = args.profile not in ("nometer",)
    labels = args.profile != "nolabels"
    floats = args.profile == "float"
    strings = 1 if args.profile == "onestring" else 2

    sunspec.SUNSPEC_BASE = args.base
    inv_dev = sunspec.make_inverter(has_battery=has_battery,
                                    pv_strings=strings,
                                    label_channels=labels,
                                    float_models=floats)
    inv_dev.base = args.base
    meter_dev = None
    if has_meter:
        meter_dev = sunspec.make_meter(float_models=floats)
        meter_dev.base = args.base

    p = Plant(has_battery=has_battery, pv_strings=strings)

    with STATE["lock"]:
        update_registers(inv_dev, meter_dev, p, has_meter, floats)
        STATE["units"][args.inverter_unit] = inv_dev.build_registers()
        if meter_dev is not None:
            STATE["units"][args.meter_unit] = meter_dev.build_registers()
        STATE["plant"] = p

    t = threading.Thread(target=ticker,
                         args=(inv_dev, meter_dev, p, args,
                               args.inverter_unit, args.meter_unit),
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
    if args.realtime:
        print("  Tid             foelger maskinens ur")
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
