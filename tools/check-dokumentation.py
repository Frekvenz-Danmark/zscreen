#!/usr/bin/env python3
"""
zScreen - tjekker at dokumentationen passer med koden.

HVORFOR DEN FINDES.

Dokumentation er det eneste i repoet som intet proever af. Koden har
tests, farverne har en vagt, layoutet har en, men en tabel i en md-fil
kan staa forkert i et aar uden at nogen opdager det.

Og netop registertabellen i docs/fronius-modbus.md er farlig at tage
fejl af: den er det man slaar op i naar man staar foran et rigtigt
anlaeg. Et forkert offset dér sender en paa vildspor i timevis.

Den tjekker tre ting, og kun ting der KAN tjekkes:

  1. Hver raekke i registertabellen mod konstanterne i zs_sunspec.h
  2. Hver ZS_-konstant der naevnes findes faktisk
  3. Hver filhenvisning peger paa en fil der findes
  4. Hver raekke i batteritabellen mod TABEL i zs_ladeplan.c

Punkt 4 kom til 9. oktober 2026. Tjekket her var haardkodet til
fronius-modbus.md, saa batteritabellen i docs/batteristyring.md kunne
drive fra koden uden at noget sagde fra - og den tabel er hele grunden
til at zs_ladeplan.c findes: "tallene skal kunne sammenlignes med noterne
linje for linje den dag de maales". En note der ikke passer med koden er
vaerre end ingen note, for man tror man ved hvad skaermen sender.

Den kan IKKE tjekke om teksten er sand. Det maa et menneske laese.
"""

import re
import sys
from pathlib import Path

ROD = Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# Dokumentationens navne for felterne, oversat til vores konstanter.
NAVNE = {
    "Mn": "MN", "Md": "MD", "Vr": "VR", "SN": "SN",
    "W": "W", "W_SF": "W_SF", "WHRtg": "WHRTG",
    "ChaState": "CHA_STATE", "ChaSt": "CHA_ST",
    "ChaState_SF": "CHA_STATE_SF",
    "DCW_SF": "DCW_SF", "N": "N",
}

# Filer der ligger ved siden af, ikke i vores repo.
#
# anlaeg.py, felter.py og tilmeld.py hoerer i zScreen-server. De maa
# naevnes - faseplanen og batteristyringen peger paa dem, fordi det er
# DER beslutningerne staar - men de kan ikke eftersees herfra: paa en
# CI-maskine er serverrepoet slet ikke hentet, og saa ville vagten falde
# paa noget der er helt i orden.
UDENFOR = ("DESIGN.txt", "modbus_controller.py",
           # zScreen-server. Faseplanen, batteristyringen og
           # hardware.md peger paa dem, fordi det er DER
           # beslutningerne og papirerne er.
           "anlaeg.py", "felter.py", "tilmeld.py", "kunder.py",
           "servicebrugere.py", "proev-frekvenz.py",
           "ca-ny-enhed.sh", "enhed-nvs.py", "servercert-ny.sh",
           "online.py", "papirer.py",
           # ESP-IDF's egne vaerktoejer.
           "nvs_tool.py", "nvs_partition_gen.py", "esptool.py",
           # ESP-IDF's egne headere. docs/zdisplay.md naevner dem fordi
           # de er grunden til at noget skulle laves anderledes paa
           # ESP32-P4: esp_phy_init.h findes slet ikke for den chip, og
           # esp_timer.h skal tvinges ind i Seeeds BSP.
           "esp_timer.h", "esp_phy_init.h",
           # ESP-IDF's eget opsaetningsscript, som README og
           # docs/zdisplay.md beder folk om at hente.
           "export.sh",
           # ESP-IDF's egne kilder. docs/zdisplay.md peger paa dem fordi
           # det er DER beviset staar for hvad en chip kan: soc_caps.h
           # siger om der er wifi i chippen, og mac_addr.c viser at hele
           # ESP_MAC_WIFI_STA-grenen ligger bag netop den.
           "soc_caps.h", "mac_addr.c",
           # Sikkerhedskopien af enhedens fabriksflash. Den ligger i
           # ~/esp/d1001-fabrik/ og IKKE i repoet: den er 32 MB, og det
           # er Seeeds firmware og ikke vores. Se zdisplay-faseplan.md.
           # Signeringsnoeglen. secure/ staar i .gitignore, saa filen
           # ligger paa den maskine der udgiver og ALDRIG i et klon.
           # Uden undtagelsen bestod vagten paa Mac'en, hvor noeglen
           # er, og faldt i CI, hvor den ikke er. En vagt der svarer
           # forskelligt alt efter hvilken maskine den koerer paa, er
           # vaerre end ingen vagt: den groenne koersel beviser intet.
           "zscreen-signing.pem",
           "SHA256.txt")


def konstanter():
    """Alle ZS_-konstanter med et tal, fra hele firmwaren."""
    ud = {}
    for f in (ROD / "firmware/kerne").rglob("*.h"):
        if "assets" in str(f):
            continue
        for m in re.finditer(r"#define\s+(ZS_\w+)\s+\(?\s*(-?\d+)\s*\)?",
                             f.read_text(encoding="utf-8")):
            ud[m.group(1)] = int(m.group(2))
    return ud


def tabelraekker(tekst):
    """| 103 | W effekt | 12 | int16 |  ->  (103, 'W', 12)"""
    ud = []
    for nr, l in enumerate(tekst.split("\n"), 1):
        m = re.match(r"\s*\|\s*(\d+)\s*\|\s*\**([A-Za-z_]+)[^|]*\|"
                     r"\s*\**(\d+)\**\s*\|", l)
        if m:
            ud.append((nr, int(m.group(1)), m.group(2), int(m.group(3))))
    return ud


def batteritabellen():
    """Noternes tilstandstabel mod TABEL i zs_ladeplan.c.

    Doc:   | 1. Oplad | 3 | -rate (gulv) | 10000 | 9900 |
    Kode:  { ZS_LADE_OPLAD, 3, RESERVE_HOEJ, true, GULV, FAST(...), "oplad" }

    Begge sider laeses, oversaettes til de samme fire tal, og sammenlignes.
    Tilstand 4 staar IKKE i tabellen - den er beskrevet i prosa - saa den
    kraeves kun at findes i koden, ikke i tabellen.
    """
    dok = ROD / "docs/batteristyring.md"
    kode = ROD / "firmware/kerne/net/zs_ladeplan.c"
    hdr = ROD / "firmware/kerne/net/zs_ladeplan.h"
    fejl = []
    if not (dok.exists() and kode.exists() and hdr.exists()):
        return fejl, 0

    kt, ht = kode.read_text(encoding="utf-8"), hdr.read_text(encoding="utf-8")

    # Tallene bag navnene, saa en aendret skala ikke slipper igennem.
    tal = {}
    for m in re.finditer(r"#define\s+(\w+)\s+\(?\s*(-?\d+)\s*\)?", kt + ht):
        tal[m.group(1)] = int(m.group(2))

    # Tilstandsnumrene fra enummen: ZS_LADE_OPLAD = 1
    numre = {m.group(1): int(m.group(2)) for m in
             re.finditer(r"(ZS_LADE_[A-Z_]+)\s*=\s*(\d+)", ht)}

    def felt(tekst):
        """GULV eller FAST(x) -> 'gulv' eller et tal."""
        tekst = tekst.strip()
        if tekst == "GULV":
            return "gulv"
        m = re.match(r"FAST\(\s*([A-Za-z_0-9]+)\s*\)$", tekst)
        if not m:
            return None
        v = m.group(1)
        return tal.get(v, v if not v.lstrip("-").isdigit() else int(v))

    # Kodens raekker. Hele filen paa én linje, saa en ombrudt raekke
    # stadig kan laeses.
    flad = " ".join(kt.split())
    kode_raekker = {}
    for m in re.finditer(
            r"\{\s*(ZS_LADE_[A-Z_]+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,"
            r"\s*(true|false)\s*,\s*(GULV|FAST\([^)]*\))\s*,"
            r"\s*(GULV|FAST\([^)]*\))\s*,\s*\"([^\"]*)\"\s*\}", flad):
        nr = numre.get(m.group(1))
        if nr is None:
            fejl.append(f"zs_ladeplan.c: {m.group(1)} staar i TABEL men "
                        f"ikke i enummen i zs_ladeplan.h")
            continue
        rsv = tal.get(m.group(3))
        if rsv is None:
            fejl.append(f"zs_ladeplan.c: reserven {m.group(3)} i raekke "
                        f"{m.group(1)} er ikke et tal vi kan slaa op")
            continue
        kode_raekker[nr] = (int(m.group(2)), felt(m.group(5)),
                            felt(m.group(6)), rsv)

    if not kode_raekker:
        fejl.append("zs_ladeplan.c: TABEL kunne slet ikke laeses. "
                    "Er formen aendret, skal det her tjek foelge med")
        return fejl, 0

    # Noternes raekker: | 1. Oplad | 3 | -rate (gulv) | 10000 | 9900 |
    def celle(s):
        s = s.strip().replace("`", "")
        if "rate" in s:
            return "gulv"
        return int(s) if s.lstrip("-").isdigit() else s

    sete = set()
    for nr, l in enumerate(dok.read_text(encoding="utf-8").split("\n"), 1):
        m = re.match(r"\s*\|\s*(\d+)\.\s*([^|]+)\|([^|]*)\|([^|]*)\|"
                     r"([^|]*)\|([^|]*)\|", l)
        if not m:
            continue
        t_nr = int(m.group(1))
        sete.add(t_nr)
        vil = (celle(m.group(3)), celle(m.group(4)),
               celle(m.group(5)), celle(m.group(6)))
        har = kode_raekker.get(t_nr)
        if har is None:
            fejl.append(f"docs/batteristyring.md:{nr}: tabellen har tilstand "
                        f"{t_nr}, men TABEL i zs_ladeplan.c har den ikke")
            continue
        for navn, a, b in zip(("StorCtl_Mod", "OutWRte", "InWRte", "MinRsvPct"),
                              vil, har):
            if a != b:
                fejl.append(f"docs/batteristyring.md:{nr}: tilstand {t_nr} "
                            f"{navn}: noterne siger {a}, koden har {b}")

    # En raekke i koden som noterne ikke kender. Tilstand 4 er beskrevet i
    # prosa og derfor undtaget - men den SKAL findes i koden.
    for t_nr in sorted(kode_raekker):
        if t_nr not in sete and t_nr != 4:
            fejl.append(f"zs_ladeplan.c: TABEL har tilstand {t_nr}, som "
                        f"ikke staar i docs/batteristyring.md. En tilstand "
                        f"skaermen kan saette, skal staa i noterne")
    if 4 not in kode_raekker:
        fejl.append("zs_ladeplan.c: tilstand 4, zero export, mangler i TABEL")

    return fejl, len(kode_raekker)


# ---------------------------------------------------------------------
# NVS-ADRESSEN I HARDWARE.MD
#
# docs/hardware.md fortaeller hvordan papirerne - certifikat, noegle, CA
# og serveradresse - skrives ind i en ny enhed med esptool. Adressen er
# IKKE den samme paa de to produkter:
#
#     zScreen   nvs paa 0x9000
#     ZDisplay  nvs paa 0xB000
#
# Skriver man 0x9000 paa en ZDisplay, rammer man bootloaderen - den
# ligger paa 0x2000 og partitionstabellen paa 0xA000. Kortet starter
# ikke op bagefter, og det er den slags en forkert linje i et dokument
# ikke maa kunne koste.
#
# Derfor laeses tallene ud af partitions.csv og holdes op mod tabellen i
# dokumentet.
# ---------------------------------------------------------------------

NVS_RAEKKE = re.compile(
    r"^\|\s*(zScreen|ZDisplay)\s*\|\s*`(esp32\w+)`\s*\|\s*`0x([0-9A-Fa-f]+)`",
    re.M)
BOARD_AF = {"zScreen": "zscreen", "ZDisplay": "zdisplay"}
CHIP_AF = {"zscreen": "esp32s3", "zdisplay": "esp32p4"}


def nvs_fra_csv(board):
    """nvs-partitionens adresse, som den staar i partitions.csv."""
    sti = ROD / f"firmware/boards/{board}/partitions.csv"
    if not sti.exists():
        return None
    for l in sti.read_text(encoding="utf-8").split("\n"):
        if l.strip().startswith("nvs,"):
            dele = [d.strip() for d in l.split(",")]
            if len(dele) >= 4 and dele[3].lower().startswith("0x"):
                return int(dele[3], 16)
    return None


def tjek_nvs_adresser():
    sti = ROD / "docs/hardware.md"
    if not sti.exists():
        return ["docs/hardware.md findes ikke"]
    tekst = sti.read_text(encoding="utf-8")
    fundet = {}
    for m in NVS_RAEKKE.finditer(tekst):
        fundet[BOARD_AF[m.group(1)]] = (m.group(2), int(m.group(3), 16))

    ud = []
    for board in ("zscreen", "zdisplay"):
        rigtig = nvs_fra_csv(board)
        if rigtig is None:
            ud.append(f"kunne ikke laese nvs-adressen i "
                      f"firmware/boards/{board}/partitions.csv")
            continue
        if board not in fundet:
            ud.append(f"docs/hardware.md har ingen raekke med {board}s "
                      f"nvs-adresse. Uden den staar der kun ét tal, og "
                      f"det ene er forkert paa det andet produkt")
            continue
        chip, adr = fundet[board]
        if adr != rigtig:
            ud.append(f"docs/hardware.md siger at {board}s nvs ligger paa "
                      f"0x{adr:X}, men partitions.csv siger 0x{rigtig:X}. "
                      f"Skriver man det forkerte tal, rammer man noget "
                      f"andet end nvs")
        if chip != CHIP_AF[board]:
            ud.append(f"docs/hardware.md siger at {board} er {chip}, men "
                      f"den er {CHIP_AF[board]}")

    # Og at kommandoerne i dokumentet bruger de samme tal.
    for board in ("zscreen", "zdisplay"):
        rigtig = nvs_fra_csv(board)
        if rigtig is None:
            continue
        kommando = f"--chip {CHIP_AF[board]}"
        # HER STOD EN LOEKKE HVIS KROP VAR ET BART "continue".
        #
        # Den loeb hver linje i dokumentet igennem og gjorde
        # ingenting. Den LIGNEDE et tjek af at kommandoen og boardet
        # hoerte sammen, og den efterproevede ikke noget som helst -
        # og det er vaerre end ingen linje, for den der laeser filen
        # taeller den med.
        #
        # Og "import re as _re" hentede modulet der allerede er
        # importeret oeverst, under et andet navn, brugt to steder. To
        # navne for det samme er to steder en rettelse kan glemme det
        # ene.
        #
        # Findes der en write_flash-linje for chippen, skal adressen
        # staa i den eller paa linjen efter. Det er tjekket, og nu er
        # det det eneste der staar her.
        for m in re.finditer(
                re.escape(kommando) + r"[^\n]*(?:\\\n[^\n]*)?", tekst):
            blok = m.group(0)
            if "write_flash" not in blok:
                continue
            if f"0x{rigtig:X}" not in blok and f"0x{rigtig:x}" not in blok:
                ud.append(f"kommandoen med --chip {CHIP_AF[board]} i "
                          f"docs/hardware.md skriver ikke til "
                          f"0x{rigtig:X}, som er hvor {board}s nvs ligger")
    return ud


def main():
    print("Tjekker at dokumentationen passer med koden ...")
    kon = konstanter()
    fejl = []

    # 1. Registertabellen
    sti = ROD / "docs/fronius-modbus.md"
    raekker = 0
    if sti.exists():
        for nr, mdl, felt, off in tabelraekker(sti.read_text(encoding="utf-8")):
            navn = NAVNE.get(felt)
            if navn is None:
                continue          # en raekke vi ikke kan slaa op, fx "første kanal"
            key = f"ZS_M{mdl}_{navn}"
            raekker += 1
            if key not in kon:
                fejl.append(f"docs/fronius-modbus.md:{nr}: {key} findes ikke "
                            f"i koden, men tabellen naevner model {mdl} {felt}")
            elif kon[key] != off:
                fejl.append(f"docs/fronius-modbus.md:{nr}: tabellen siger "
                            f"model {mdl} {felt} = {off}, men {key} er "
                            f"{kon[key]}")

    # 2. og 3. Konstanter og filer der naevnes
    doks = [ROD / "README.md"] + sorted((ROD / "docs").glob("*.md"))
    nk = nf = 0
    for d in doks:
        for nr, l in enumerate(d.read_text(encoding="utf-8").split("\n"), 1):
            for m in re.finditer(r"`(ZS_[A-Z0-9_]+)`", l):
                nk += 1
                navn = m.group(1)
                if navn not in kon and not any(
                        (ROD / "firmware/kerne").rglob("*.h")
                        and navn in f.read_text(encoding="utf-8")
                        for f in (ROD / "firmware/kerne").rglob("*.h")):
                    fejl.append(f"{d.name}:{nr}: {navn} findes ikke i koden")
            for m in re.finditer(
                    r"`([a-zA-Z0-9_./-]+\.(?:c|h|py|sh|yml|csv|txt|json|pem))`", l):
                p = m.group(1)
                nf += 1
                if Path(p).name in UDENFOR:
                    continue
                if any((ROD / k).exists() for k in
                       (p, f"firmware/kerne/{p}", f"firmware/{p}",
                        f"tools/{p}", f"tests/{p}", f"docs/{p}")):
                    continue
                if list(ROD.rglob(Path(p).name)):
                    continue
                fejl.append(f"{d.name}:{nr}: filen {p} findes ikke")

    # 4. Batteritabellen i noterne mod TABEL i koden
    bfejl, braekker = batteritabellen()
    fejl += bfejl
    fejl += tjek_nvs_adresser()

    if fejl:
        for f in fejl:
            print(f"  {ROED}FEJL{SLUT} {f}")
        return 1
    print(f"  {GROEN} OK{SLUT} {raekker} registerrækker, {braekker} "
          f"batteritilstande, {nk} konstanter, {nf} filhenvisninger og "
          f"begge nvs-adresser passer med koden.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
