"""
Byg et SunSpec-registerkort der ser ud som en rigtig inverter.

Registerkortet er bygget efter de officielle smdx-filer fra
https://github.com/sunspec/models, ikke efter hukommelsen. Hvis
simulatoren og firmwaren er enige om noget forkert, har vi ikke
testet noget som helst, saa offsets staar med kildehenvisning.

Alt er 16-bit registre. Modbus er big-endian paa ledningen, men her
holder vi dem som almindelige Python-heltal og pakker foerst naar de
skal sendes.
"""

import math
import struct

# 0-baseret ledningsadresse. Fronius-dokumentationen taeller fra 40001,
# hvilket er den samme celle.
SUNSPEC_BASE = 40000

NA_U16 = 0xFFFF
NA_I16 = -32768
NA_SF = -32768


def _u16(v: int) -> int:
    return v & 0xFFFF


def _i16_to_u16(v: int) -> int:
    if v < 0:
        v += 65536
    return v & 0xFFFF


class Model:
    """En SunSpec-model: et ID, en laengde og en raekke dataregistre."""

    def __init__(self, model_id: int, length: int):
        self.id = model_id
        self.length = length
        self.data = [0] * length

    # -- skrivehjaelpere, offset er talt fra foerste DATAregister --

    def u16(self, off: int, value: int):
        self.data[off] = _u16(value)

    def i16(self, off: int, value: int):
        self.data[off] = _i16_to_u16(int(value))

    def sf(self, off: int, value: int):
        self.data[off] = _i16_to_u16(int(value))

    def enum16(self, off: int, value: int):
        self.data[off] = _u16(value)

    def acc32(self, off: int, value: int):
        v = int(value) & 0xFFFFFFFF
        self.data[off] = (v >> 16) & 0xFFFF
        self.data[off + 1] = v & 0xFFFF

    def f32(self, off: int, value: float):
        hi, lo = struct.unpack(">HH", struct.pack(">f", float(value)))
        self.data[off] = hi
        self.data[off + 1] = lo

    def string(self, off: int, n_regs: int, text: str):
        """SunSpec-strenge: to tegn pr. register, hoejeste byte foerst,
        polstret med nul-bytes."""
        raw = text.encode("ascii", errors="replace")[: n_regs * 2]
        raw = raw + b"\x00" * (n_regs * 2 - len(raw))
        for i in range(n_regs):
            self.data[off + i] = (raw[i * 2] << 8) | raw[i * 2 + 1]


class Device:
    """En Modbus-enhed: en kaede af SunSpec-modeller paa ét unit-ID."""

    def __init__(self, base: int = SUNSPEC_BASE):
        self.base = base
        self.models: list[Model] = []

    def add(self, m: Model) -> Model:
        self.models.append(m)
        return m

    def find(self, model_id: int):
        for m in self.models:
            if m.id == model_id:
                return m
        return None

    def build_registers(self) -> dict[int, int]:
        """Laegger kaeden ud som adresse -> vaerdi."""
        regs: dict[int, int] = {}
        addr = self.base
        regs[addr] = 0x5375   # "Su"
        addr += 1
        regs[addr] = 0x6E53   # "nS"
        addr += 1
        for m in self.models:
            regs[addr] = m.id
            addr += 1
            regs[addr] = m.length
            addr += 1
            for i, v in enumerate(m.data):
                regs[addr + i] = v
            addr += m.length
        # Slutblok: ID 0xFFFF fulgt af laengde 0, som i standarden.
        regs[addr] = 0xFFFF
        addr += 1
        regs[addr] = 0x0000
        return regs

    def model_data_addr(self, model_id: int) -> int:
        """Adressen paa foerste dataregister i en model."""
        addr = self.base + 2
        for m in self.models:
            if m.id == model_id:
                return addr + 2
            addr += 2 + m.length
        raise KeyError(model_id)


# ----------------------------------------------------------------------
# Faerdige enheder
# ----------------------------------------------------------------------

def make_inverter(has_battery: bool = True,
                  pv_strings: int = 2,
                  label_channels: bool = True,
                  float_models: bool = False,
                  model_name: str = "Symo GEN24 10.0",
                  serial: str = "31234567") -> Device:
    """Fronius Gen24 som den ser ud paa unit 1."""
    d = Device()

    # --- Model 1, Common (smdx_00001) ---
    m1 = d.add(Model(1, 66))
    m1.string(0, 16, "Fronius")          # Mn
    m1.string(16, 16, model_name)        # Md
    m1.string(32, 8, "")                 # Opt
    m1.string(40, 8, "1.36.5-1")         # Vr
    m1.string(48, 16, serial)            # SN
    m1.u16(64, 1)                        # DA
    m1.u16(65, 0)                        # Pad

    # --- Inverter ---
    if float_models:
        # Model 113 (smdx_00113): alt er float32, ingen skalafaktorer
        inv = d.add(Model(113, 60))
    else:
        # Model 103 (smdx_00103)
        inv = d.add(Model(103, 50))
        inv.sf(4, 0)     # A_SF
        inv.sf(11, 0)    # V_SF
        inv.sf(13, 0)    # W_SF
        inv.sf(15, -2)   # Hz_SF
        inv.sf(17, 0)    # VA_SF
        inv.sf(19, 0)    # VAr_SF
        inv.sf(21, -3)   # PF_SF
        inv.sf(24, 0)    # WH_SF
        inv.sf(26, -2)   # DCA_SF
        inv.sf(28, -1)   # DCV_SF
        inv.sf(30, 0)    # DCW_SF
        inv.sf(36, 0)    # Tmp_SF
        inv.enum16(37, 4)  # St = MPPT

    # --- Model 120, Nameplate (smdx_00120) ---
    m120 = d.add(Model(120, 26))
    m120.enum16(0, 4)      # DERTyp = PV
    m120.u16(1, 10000)     # WRtg = 10 kW
    m120.sf(2, 0)          # WRtg_SF
    m120.u16(3, 10000)     # VARtg
    m120.sf(4, 0)
    m120.sf(9, 0)
    m120.u16(10, 16)       # ARtg
    m120.sf(11, 0)
    m120.sf(16, 0)
    if has_battery:
        m120.u16(17, 10240)   # WHRtg = 10,24 kWh batteri
        m120.sf(18, 0)
    else:
        m120.u16(17, NA_U16)
        m120.sf(18, NA_SF)
    m120.sf(20, 0)
    m120.sf(22, 0)
    m120.sf(24, 0)

    # --- Model 124, Storage (smdx_00124) ---
    if has_battery:
        m124 = d.add(Model(124, 24))
        m124.u16(0, 5000)     # WChaMax
        m124.u16(3, 0)        # StorCtl_Mod
        m124.u16(5, 500)      # MinRsvPct, 5,00 %
        m124.u16(6, 5000)     # ChaState, saettes af simuleringen
        m124.enum16(9, 6)     # ChaSt = HOLDING
        m124.sf(16, 0)        # WChaMax_SF
        m124.sf(19, -2)       # MinRsvPct_SF
        m124.sf(20, -2)       # ChaState_SF -> 5000 * 10^-2 = 50,00 %
        m124.sf(21, 0)        # StorAval_SF
        m124.sf(22, -1)       # InBatV_SF
        m124.sf(23, -2)       # InOutWRte_SF

    # --- Model 160, MPPT (smdx_00160) ---
    n_ch = pv_strings + (2 if has_battery else 0)
    m160 = d.add(Model(160, 8 + n_ch * 20))
    m160.sf(0, -2)   # DCA_SF
    m160.sf(1, -1)   # DCV_SF
    m160.sf(2, 0)    # DCW_SF
    m160.sf(3, 0)    # DCWH_SF
    m160.acc32(4, 0)  # Evt, fylder BAADE offset 4 og 5
    m160.u16(6, n_ch)  # N ligger paa offset 6, IKKE 5
    m160.u16(7, 0)     # TmsPer

    labels = []
    for i in range(pv_strings):
        labels.append(f"MPPT {i + 1}" if label_channels else "")
    if has_battery:
        labels.append("STCHA" if label_channels else "")
        labels.append("STDISCHA" if label_channels else "")

    for i, label in enumerate(labels):
        base = 8 + i * 20
        m160.u16(base + 0, i + 1)              # ID
        m160.string(base + 1, 8, label)        # IDStr
        m160.u16(base + 9, 0)                  # DCA
        m160.u16(base + 10, 0)                 # DCV
        m160.u16(base + 11, 0)                 # DCW
        m160.acc32(base + 12, 0)               # DCWH
        m160.acc32(base + 14, 0)               # Tms
        m160.i16(base + 16, 25)                # Tmp
        m160.enum16(base + 17, 2)              # DCSt = SLEEPING
        m160.acc32(base + 18, 0)               # DCEvt

    return d


def make_meter(float_models: bool = False, serial: str = "19123456") -> Device:
    """Fronius Smart Meter, broet ud paa sit eget unit-ID."""
    d = Device()

    m1 = d.add(Model(1, 66))
    m1.string(0, 16, "Fronius")
    m1.string(16, 16, "Smart Meter 63A-3")
    m1.string(32, 8, "")
    m1.string(40, 8, "1.7.1")
    m1.string(48, 16, serial)
    m1.u16(64, 200)
    m1.u16(65, 0)

    if float_models:
        d.add(Model(213, 124))
    else:
        # Model 203 (smdx_00203), trefaset stjerne
        m = d.add(Model(203, 105))
        m.sf(4, -2)     # A_SF
        m.sf(13, -1)    # V_SF
        m.sf(15, -2)    # Hz_SF
        m.sf(20, 0)     # W_SF
        m.sf(25, 0)     # VA_SF
        m.sf(30, 0)     # VAR_SF
        m.sf(35, -3)    # PF_SF
        m.sf(40, 0)     # TotWh_SF
        m.enum16(104, 0)
    return d
