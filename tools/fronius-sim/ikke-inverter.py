#!/usr/bin/env python3
"""
En enhed der taler Modbus TCP, men IKKE er en inverter.

Et netvaerk hos en kunde har ofte andet paa port 502: en varmepumpe, en
PLC, en energimaaler. De svarer paent paa FC3, men der staar ikke "SunS"
i registrene. Soegningen maa ikke kalde dem invertere og vise dem paa
listen, for saa vaelger kunden en varmepumpe og undrer sig over at der
ikke kommer tal.

    python3 ikke-inverter.py --port 15030 [--svar nuller|skrald|tavs]
"""
import argparse, socket, socketserver, struct, sys

SVAR = "nuller"

class H(socketserver.BaseRequestHandler):
    def handle(self):
        while True:
            try:
                h = self.request.recv(7)
            except OSError:
                return
            if len(h) < 7:
                return
            tid, _, laengde, unit = struct.unpack(">HHHB", h)
            krop = self.request.recv(max(0, laengde - 1))
            if len(krop) < 1:
                return
            fc = krop[0]
            if fc != 3:
                return
            antal = struct.unpack(">H", krop[3:5])[0] if len(krop) >= 5 else 1
            antal = max(1, min(antal, 125))

            if SVAR == "tavs":
                continue                      # tager imod, svarer aldrig
            if SVAR == "skrald":
                data = bytes([0xAA]) * (antal * 2)
            else:
                data = bytes(antal * 2)       # lutter nuller

            pdu = bytes([3, len(data)]) + data
            self.request.sendall(struct.pack(">HHHB", tid, 0, len(pdu) + 1, unit) + pdu)

class S(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=15030)
    p.add_argument("--bind", default="127.0.0.1")
    p.add_argument("--svar", choices=["nuller", "skrald", "tavs"], default="nuller")
    a = p.parse_args()
    SVAR = a.svar
    print(f"  taler Modbus paa {a.bind}:{a.port}, svarer med {a.svar}", flush=True)
    S((a.bind, a.port), H).serve_forever()
