#!/usr/bin/env python3
"""
zScreen - tjek at partitionstabellerne kan bruges af den kode der skal
laese dem.

HVORFOR DEN FINDES.

En partitionstabel kan ikke aendres paa en enhed der er sendt ud til en
kunde. Den skal altsaa vaere rigtig FOERSTE gang, og "rigtig" er mere end
at tallene gaar op.

ZDisplays storage blev lagt paa 23,9 MB, fordi flashen er maalt til 32
MB. Tabellen byggede, enheden bootede, og alt saa godt ud. Men SPIFFS
kan IKKE adressere saa meget med standardsiden paa 256 bytes: sidetallet
er en u16_t, saa loftet er 65535 x 256 = 16 MB. Det ville foerst have
vist sig den dag nogen kaldte esp_vfs_spiffs_register() - med
ESP_ERR_INVALID_ARG, i en tabel der ikke kunne rettes.

Scriptet regner derfor de fire graenser fra ESP-IDF's egen
spiffs/esp_spiffs.c efter for hver spiffs-partition, med den
sidestoerrelse boardet faktisk bygger med.

Og dertil det almindelige: at tabellen passer i flashen, at app-pladser
ligger paa 64 KB-graenser, og at de to OTA-pladser er lige store.

Koeres af tools/check-alle.sh.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# Alle fire typer i SPIFFS er u16_t. Efterset i ESP-IDF v5.4.2:
# components/spiffs/include/spiffs_config.h linje 310-321.
U16 = 65535
# g_rom_flashchip.sector_size paa alle ESP32'er.
SLETTESEKTOR = 4096

FLASH = {  # CONFIG_ESPTOOLPY_FLASHSIZE_<x> -> bytes
    "1MB": 1 << 20, "2MB": 2 << 20, "4MB": 4 << 20, "8MB": 8 << 20,
    "16MB": 16 << 20, "32MB": 32 << 20, "64MB": 64 << 20,
}

fejl = []


def tal(s):
    s = s.strip()
    if s.lower().startswith("0x"):
        return int(s, 16)
    if s.upper().endswith("K"):
        return int(s[:-1]) * 1024
    if s.upper().endswith("M"):
        return int(s[:-1]) * 1024 * 1024
    return int(s)


def laes_tabel(sti):
    """Navn, type, undertype, offset og stoerrelse. Tomme offsets udfyldes
    som ESP-IDF selv gor: lige efter den forrige."""
    ud = []
    naeste = None
    for linje in sti.read_text(encoding="utf-8").splitlines():
        linje = linje.split("#")[0].strip()
        if not linje:
            continue
        d = [x.strip() for x in linje.split(",")]
        if len(d) < 5:
            continue
        navn, typ, undertyp, off, stoer = d[0], d[1], d[2], d[3], d[4]
        stoer = tal(stoer)
        off = tal(off) if off else naeste
        if off is None:
            fejl.append(f"{sti.name}: {navn} har intet offset, og der er "
                        f"ingen partition foer den")
            continue
        # App-pladser skal ligge paa 64 KB. ESP-IDF kraever det, men
        # fejlen kommer sent og er svaer at laese.
        if typ == "app" and off % 0x10000:
            fejl.append(f"{sti.name}: {navn} ligger paa 0x{off:x}, og "
                        f"app-pladser skal ligge paa en 64 KB-graense")
        ud.append((navn, typ, undertyp, off, stoer))
        naeste = off + stoer
    return ud


def sidestoerrelse(projekt):
    m = re.search(r"^CONFIG_SPIFFS_PAGE_SIZE=(\d+)",
                  (projekt / "sdkconfig.defaults").read_text(encoding="utf-8"),
                  re.M)
    return int(m.group(1)) if m else 256   # ESP-IDF's egen standard


def flashstoerrelse(projekt):
    t = (projekt / "sdkconfig.defaults").read_text(encoding="utf-8")
    m = re.search(r"^CONFIG_ESPTOOLPY_FLASHSIZE_(\w+)=y", t, re.M)
    if not m:
        fejl.append(f"{projekt.name}: ingen CONFIG_ESPTOOLPY_FLASHSIZE_*=y. "
                    f"Saa ved vi ikke hvor stor flashen er")
        return None
    navn = m.group(1)
    if navn not in FLASH:
        fejl.append(f"{projekt.name}: kender ikke flashstoerrelsen {navn}")
        return None
    return FLASH[navn]


def tjek_spiffs(navn, stoer, side, hvem):
    """De fire graenser fra ESP-IDF's spiffs/esp_spiffs.c linje 186-203."""
    graenser = (
        ("blokke",   stoer // SLETTESEKTOR,            "spiffs_block_ix"),
        ("sider",    stoer // side,                    "spiffs_page_ix"),
        ("objekter", 2 + 2 * (stoer // (2 * side)),    "spiffs_obj_id"),
        ("spaend",   stoer // side - 1,                "spiffs_span_ix"),
    )
    for hvad, vaerdi, typ in graenser:
        if vaerdi > U16:
            fejl.append(
                f"{hvem}: {navn} er {stoer / 1048576:.2f} MB, og det giver "
                f"{vaerdi} {hvad} med en side paa {side} bytes. {typ} er en "
                f"u16_t, saa loftet er {U16}. esp_vfs_spiffs_register() "
                f"ville svare ESP_ERR_INVALID_ARG, og partitionen kan ikke "
                f"bruges. Saet CONFIG_SPIFFS_PAGE_SIZE op, eller goer "
                f"partitionen mindre - men husk at tabellen ikke kan "
                f"aendres paa en enhed der er sendt ud")


def main():
    print("Tjekker partitionstabellerne ...")
    boards = sorted((ROD / "firmware/boards").iterdir())
    if not boards:
        print(f"  {ROED}FEJL{SLUT} fandt ingen boards i firmware/boards")
        return 1

    antal = 0
    for projekt in boards:
        if not (projekt / "partitions.csv").exists():
            continue
        hvem = projekt.name
        tabel = laes_tabel(projekt / "partitions.csv")
        side = sidestoerrelse(projekt)
        flash = flashstoerrelse(projekt)
        apps = []

        for navn, typ, undertyp, off, stoer in tabel:
            antal += 1
            if flash and off + stoer > flash:
                fejl.append(f"{hvem}: {navn} slutter paa 0x{off + stoer:x}, "
                            f"men flashen er 0x{flash:x}")
            if typ == "data" and undertyp == "spiffs":
                tjek_spiffs(navn, stoer, side, hvem)
            if typ == "app" and undertyp.startswith("ota_"):
                apps.append((navn, stoer))

        # To OTA-pladser skal vaere lige store, ellers kan den ene ikke
        # rumme hvad den anden kan.
        if len(apps) == 2 and apps[0][1] != apps[1][1]:
            fejl.append(f"{hvem}: {apps[0][0]} er 0x{apps[0][1]:x} og "
                        f"{apps[1][0]} er 0x{apps[1][1]:x}. De SKAL vaere "
                        f"lige store")
        if len(apps) == 1:
            fejl.append(f"{hvem}: der er kun én OTA-plads. Saa kan enheden "
                        f"ikke opdateres over netvaerket")

    if fejl:
        for f in fejl:
            print(f"  {ROED}FEJL{SLUT} {f}")
        print(f"\n  {len(fejl)} ting at rette.")
        return 1

    print(f"  {GROEN} OK{SLUT} {len(boards)} boards, {antal} partitioner, "
          f"SPIFFS kan adressere dem, og OTA-pladserne er lige store.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
