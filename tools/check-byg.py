#!/usr/bin/env python3
"""
zScreen - tjek at hver bygning kan finde de headere dens egne filer beder om.

HVORFOR DEN FINDES.

Koden bliver bygget fire steder, og hver af dem har sin egen liste over
kilder og sine egne include-stier:

    firmware/boards/zscreen/main/CMakeLists.txt    zScreen, altsaa ESP32-S3
    firmware/boards/zdisplay/...     ZDisplay, altsaa ESP32-P4
    tests/host/run.sh               enhedstestene paa en almindelig maskine
    tools/zs-probe/build.sh         fejlsoegningsvaerktoejet
    tests/fuzz/koer.sh              fuzzerne

De to firmwarer deler kildelisten i firmware/kerne/kilder.cmake og laegger hver
sine tre-fire board-filer oveni. Begge bliver tjekket, og det er ikke
for symmetriens skyld: en header kan findes paa det ene board og mangle
paa det andet. Det ER sket - storage/zs_nvs.c bad om esp_phy_init.h, som
slet ikke findes for esp32p4, fordi radioen sidder i en anden chip.

Da util/zs_tekst.c blev lagt ind, blev den foejet til de to foerste og
glemt i den tredje. Enhedstestene var groenne, og saa faldt bygningen af
vaerktoejet med "zs_tekst.h file not found".

Det var til at se. Det der IKKE var til at se var det naeste: testkoerslen
gik videre, og ende til ende koerte mod den GAMLE binaer der stadig laa
der. Den sagde "70 tjek, alle bestaaet", og den loeg. Foerst da bygningen
virkede, kom de rigtige fejl frem, og saa havde de ikke noget at goere med
det jeg havde lavet.

Vi kraever IKKE at de fire har de samme stier. Vaerktoejet oversaetter
ingen skaermkode og skal ikke kende ui-mappen. Vi kraever det der faktisk
betyder noget: at hver bygning kan finde HVER header som dens egne filer
beder om, ogsaa de headere en header selv tager med.

Og dertil én regel mere: hver .c under firmware/kerne skal staa i
CMakeLists.txt. Ellers bliver den slet ikke oversat til skaermen, uden en
advarsel nogen steder.

Koeres af tests/run-all.sh, saa den ikke kan glemmes.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
SRC = ROD / "firmware/kerne"

# Produkterne. Hver mappe under firmware/boards/ er ét board.
#
# HER STOD DER FOER EN LISTE VED NAVN KUN_ZSCREEN.
#
# Dengang laa den faelles kode inde i zScreens egen main-mappe, saa
# vagten kunne ikke se forskel paa "faelles" og "kun zScreen" ud fra
# hvor filen laa. Den maatte have en liste over de fire undtagelser, og
# en femte board-fil skulle skrives ind i den for at blive bygget.
#
# Efter at kerne/ og boards/ blev skilt, siger STRUKTUREN det:
# ligger filen i firmware/kerne/, skal den bygges af ALLE; ligger den i
# firmware/boards/<navn>/main/, skal den bygges af DET board. Der er
# ingen undtagelser at huske laengere, og et tredje produkt tilfoejes
# ved at laegge en mappe her.
BOARDS = ("zscreen", "zdisplay")

# Skrifttyperne ligger i ét saet pr. skaermbredde, ui/assets/<bredde>/,
# og hvert board vaelger sit med ZS_ASSET_SAET. En skrift i LVGL er et
# bitmap og kan ikke skaleres paa skaermen, saa de KAN ikke vaere
# faelles - se tools/build-assets.sh.
#
# Derfor er assets undtaget fra regel 1 nedenfor og har deres egen
# regel: hver fil i et saet skal staa i listen, og hvert board skal
# vaelge et saet der findes.
ASSET_ROD = "ui/assets/"

# Motoren: de mapper hvor koden skal kunne koere paa BEGGE chips, og i
# tests/host paa en Mac uden ESP-IDF.
MOTOR_DIRS = ("net/", "app/", "util/", "storage/")

# Hvad motoren IKKE maa hente.
#
# HVORFOR REGLEN STAAR HER OG IKKE KUN I EN KOMMENTAR.
#
# kerne.cmake lovede at "tools/check-headers.sh siger fra" hvis der kom
# et hardware-kald ned i motoren. Det passede ikke: check-headers.sh
# tjekker kun at ingen konstant kolliderer med en include-guard. Hele
# opdelingen mellem de to produkter hviler paa at motoren er fri af
# hardware - og den paastand var uhaandhaevet.
#
# Det er ikke en teoretisk regel. Er den overholdt, kan 16.000 linjer
# deles mellem to vidt forskellige chips og samtidig proeves paa en Mac.
# Brydes den ét sted, holder ingen af de tre ting laengere.
FORBUDT_I_MOTOREN = (
    (re.compile(r"^driver/"), "en driver fra ESP-IDF"),
    (re.compile(r"^(soc|hal)/"), "chippens egne registre"),
    (re.compile(r"^esp_lcd"), "en skaermdriver"),
    (re.compile(r"^(lvgl|lv_conf|lv_port)"), "LVGL"),
    (re.compile(r"bsp", re.I), "en board-support-pakke"),
    (re.compile(r"^(display|touch)\.h$"), "en board-header"),
    (re.compile(r"re_terminal|sensecap|indicator", re.I), "et bestemt board"),
)
# Og kald, ikke kun headere: et bsp_-navn kan naas gennem en header der
# ikke selv heder noget med bsp.
FORBUDT_KALD = re.compile(r"\bbsp_[a-z][a-z0-9_]*\s*\(")


def tjek_motoren():
    """Er motoren fri af hardware?"""
    for c in sorted(SRC.rglob("*.[ch]")):
        rel = c.relative_to(SRC).as_posix()
        if not any(rel.startswith(d) for d in MOTOR_DIRS):
            continue
        tekst = c.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', tekst, re.M):
            navn = m.group(1)
            if navn.startswith("zs_") or navn.startswith("../zs_"):
                continue
            for moenster, hvad in FORBUDT_I_MOTOREN:
                if moenster.search(navn):
                    fejl.append(f"firmware/kerne/{rel} henter {navn}, altsaa "
                                f"{hvad}. Motoren skal kunne oversaettes til "
                                f"to forskellige chips OG paa en Mac uden "
                                f"ESP-IDF. Hoerer det til hardwaren, saa laeg "
                                f"det i boardets egne filer og lad motoren "
                                f"kalde en funktion i stedet - som "
                                f"ui/zs_baglys.h.")
                    break
        m = FORBUDT_KALD.search(tekst)
        if m:
            fejl.append(f"firmware/kerne/{rel} kalder {m.group(0)[:-1].strip()}"
                        f"(), altsaa direkte ind i en board-support-pakke. "
                        f"Se forklaringen ovenfor.")

# VORES EGNE headere, og kun dem.
#
# Det er en liste over hvad vi HAR og ikke en liste over alt det fremmede.
# Den anden vej rundt skulle den vedligeholdes hver gang ESP-IDF eller
# LVGL fik en ny mappe, og en glemt linje der ville give en falsk fejl som
# nogen saa ville slaa testen fra for.
VORES = re.compile(r"^(zs_[a-z0-9_]+|lv_port)\.h$")

fejl = []


def includes(fil):
    """Hvilke "egne" headere beder filen om? Kun dem i gaaseoejne."""
    try:
        tekst = fil.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    ud = []
    for m in re.finditer(r'^\s*#\s*include\s+"([^"]+)"', tekst, re.M):
        navn = m.group(1)
        if not VORES.match(navn.rsplit("/", 1)[-1]):
            continue
        ud.append(navn)
    return ud


def find(navn, fra, stier):
    """Kan include'en findes? Foerst ved siden af filen selv, som C goer,
    saa i hver af bygningens stier."""
    if navn.startswith("../") or "/" in navn:
        # En sti med mappe i: proev den som den staar, relativt til filen.
        p = (fra.parent / navn).resolve()
        if p.exists():
            return p
    if (fra.parent / navn).exists():
        return (fra.parent / navn)
    for s in stier:
        p = s / navn
        if p.exists():
            return p
    return None


def tjek_bygning(navn, kilder, stier):
    """Kan hver header findes, ogsaa de headere en header selv tager med?"""
    set_ = set()
    koe = list(kilder)
    while koe:
        fil = koe.pop()
        if fil in set_:
            continue
        set_.add(fil)
        for inc in includes(fil):
            fundet = find(inc, fil, stier)
            if fundet is None:
                rel = fil.relative_to(ROD).as_posix()
                fejl.append(f'{navn}: {rel} beder om "{inc}", og den kan ikke findes. '
                            f"Mangler der en -I eller en kilde?")
            else:
                koe.append(fundet)


def kilder_og_stier_fra_shell(sti_til_script):
    """Henter -I-stierne og .c-filerne ud af et byggescript."""
    tekst = sti_til_script.read_text(encoding="utf-8")
    base = sti_til_script.parent
    stier = [(base / m).resolve() for m in re.findall(r"-I(\S+)", tekst)]
    kilder = []
    for m in re.findall(r"(\S*\.\./\.\./firmware/kerne/\S+\.c)", tekst):
        p = (base / m).resolve()
        if p.exists():
            kilder.append(p)
    return kilder, stier


def kilder_og_stier_fra_fuzz():
    """Fuzzerne skriver det anderledes, saa de har deres egen laesning.

    De staar med stier regnet fra projektets rod, og med et mellemrum
    efter -I. Da util/ kom til, var det den FJERDE bygning der faldt, og
    den faldt efter at baade enhedstestene og ende til ende var groenne.
    Saa den skal med her.
    """
    sti = ROD / "tests/fuzz/koer.sh"
    tekst = sti.read_text(encoding="utf-8")
    stier = [(ROD / m).resolve()
             for m in re.findall(r"-I\s+(\S+)", tekst)]
    kilder = []
    for m in re.findall(r"((?:tests|firmware)/\S+\.c)", tekst):
        q = (ROD / m).resolve()
        if q.exists():
            kilder.append(q)
    return kilder, stier


def udfold(tekst):
    """Erstatter de to cmake-variabler kilder.cmake bruger i stierne."""
    return (tekst.replace("${ZS_KERNE_DIR}", (ROD / "firmware/kerne").as_posix())
                 .replace("${ZS_VENDOR_DIR}", (ROD / "firmware/vendor").as_posix()))


def blok_efter(tekst, navn):
    """Indholdet af et set(NAVN ... ) i en cmake-fil."""
    m = re.search(r"set\(" + re.escape(navn) + r"\b(.*?)\)", tekst, re.S)
    return m.group(1) if m else ""


def kerne():
    """Kilder og include-stier fra firmware/kerne/kilder.cmake.

    Den fil findes fordi to produkter skal bygge af SAMME liste. Stod
    listen to steder, ville en ny fil i net/ komme med i den ene skaerm
    og ikke i den anden - og det ville ikke vaere en byggefejl, men en
    manglende funktion hos en kunde.
    """
    tekst = udfold((ROD / "firmware/kerne/kilder.cmake").read_text(encoding="utf-8"))
    kilder = []
    for navn in ("ZS_MOTOR_SRCS", "ZS_FLADE_SRCS", "ZS_ASSET_SRCS"):
        for m in re.findall(r'"([^"]+\.c)"', blok_efter(tekst, navn)):
            p = pathlib.Path(m)
            if p.exists():
                kilder.append(p.resolve())
    stier = [pathlib.Path(m).resolve()
             for m in re.findall(r'"([^"]+)"', blok_efter(tekst, "ZS_KERNE_INCLUDE_DIRS"))]
    return kilder, stier


def tjek_board(navn, cmake, kerne_kilder, kerne_stier):
    """Ét produkts bygning: dens egne filer plus motoren."""
    tekst = cmake.read_text(encoding="utf-8")
    base = cmake.parent

    egne = []
    for m in re.findall(r'"([^"$]+\.c)"', tekst):
        p = (base / m).resolve()
        if p.exists():
            egne.append(p)
        else:
            rel = cmake.relative_to(ROD).as_posix()
            fejl.append(f"{rel}: SRCS naevner {m}, men filen findes ikke.")

    stier = list(kerne_stier)
    blok = tekst.split("INCLUDE_DIRS", 1)
    if len(blok) == 2:
        for m in re.findall(r'"([^"$]+)"', blok[1].split("REQUIRES", 1)[0]):
            stier.append((base / m).resolve())

    tjek_bygning(navn, egne + kerne_kilder, stier)
    return egne


def tjek_assets():
    """Hvert saet af skrifttyper skal staa helt i listen, og hvert board
    skal vaelge et saet der findes."""
    tekst = (ROD / "firmware/kerne/kilder.cmake").read_text(encoding="utf-8")
    navne = re.findall(r'"\$\{ZS_ASSET_DIR\}/([^"]+\.c)"', tekst)
    if not navne:
        fejl.append("firmware/kerne/kilder.cmake: fandt ingen filer under "
                    "${ZS_ASSET_DIR}. Er listen over skrifttyper skrevet om?")
        return

    rod = ROD / "firmware/kerne" / ASSET_ROD
    saet = sorted(d.name for d in rod.iterdir() if d.is_dir())
    if not saet:
        fejl.append(f"der er ingen saet i firmware/kerne/{ASSET_ROD}. "
                    f"Byg dem med ./tools/build-assets.sh")
        return

    for s in saet:
        forventet = {(rod / s / n).resolve() for n in navne}
        faktisk = {c.resolve() for c in (rod / s).glob("*.c")}
        for mangler in sorted(forventet - faktisk):
            fejl.append(f"{mangler.relative_to(ROD)} staar i listen, men "
                        f"filen findes ikke. Byg saettene med "
                        f"./tools/build-assets.sh")
        for ekstra in sorted(faktisk - forventet):
            fejl.append(f"{ekstra.relative_to(ROD)} findes, men staar ikke i "
                        f"listen under ${{ZS_ASSET_DIR}} i kilder.cmake. "
                        f"Saa bliver den aldrig oversat.")

    # Og at hvert board vaelger et saet der er der.
    for board in BOARDS:
        cm = ROD / f"firmware/boards/{board}/main/CMakeLists.txt"
        if not cm.exists():
            continue            # sagt fra om i tjek_cmake()
        m = re.search(r"set\(ZS_ASSET_SAET\s+(\w+)\)",
                      cm.read_text(encoding="utf-8"))
        if not m:
            fejl.append(f"{board} saetter ikke ZS_ASSET_SAET, saa den ved "
                        f"ikke hvilke skrifttyper den skal bruge")
        elif m.group(1) not in saet:
            fejl.append(f"{board} vaelger saettet '{m.group(1)}', og det "
                        f"findes ikke. Der er: {', '.join(saet)}")


def tjek_cmake():
    kerne_kilder, kerne_stier = kerne()
    if not kerne_kilder:
        fejl.append("firmware/kerne/kilder.cmake: fandt ingen kilder. "
                    "Er listen skrevet om?")
        return
    i_kerne = set(kerne_kilder)

    # Regel 1: ALT i firmware/kerne skal staa i kilder.cmake.
    #
    # Ellers bliver filen slet ikke oversat - eller, vaerre, kun af det
    # ene produkt. Det ville ikke vaere en byggefejl, men en funktion
    # der mangler hos en kunde paa det ene produkt.
    for c in sorted(SRC.rglob("*.c")):
        rel = c.relative_to(SRC).as_posix()
        if rel.startswith(ASSET_ROD):
            continue            # egen regel, se tjek_assets()
        if c.resolve() not in i_kerne:
            fejl.append(f"firmware/kerne/{rel} findes, men staar ikke i "
                        f"firmware/kerne/kilder.cmake. Saa bliver den ikke "
                        f"oversat til nogen af skaermene. Hoerer den kun til "
                        f"paa ét board, saa laeg den i "
                        f"firmware/boards/<navn>/main/ i stedet.")

    # Regel 2: hvert board bygger sine EGNE filer, og kan finde de
    # headere baade dens egne og motorens beder om.
    for board in BOARDS:
        cmake = ROD / f"firmware/boards/{board}/main/CMakeLists.txt"
        if not cmake.exists():
            # En vagt der ikke kan koere, skal sige fra. Stod der
            # "if exists" her, ville en flyttet fil betyde at det ene
            # produkt holdt op med at blive tjekket, mens udskriften
            # stadig sagde at alt var i orden.
            fejl.append(f"{cmake.relative_to(ROD)} findes ikke. Er {board} "
                        f"flyttet eller doebt om? Saa tjekker vagten ikke det "
                        f"den lover. Er produktet vaek, skal det ud af "
                        f"BOARDS i check-byg.py.")
            continue

        egne = tjek_board(board, cmake, kerne_kilder, kerne_stier)

        i_board = set(egne) | i_kerne
        for c in sorted(cmake.parent.rglob("*.c")):
            if c.resolve() not in i_board:
                rel = c.relative_to(ROD).as_posix()
                fejl.append(f"{rel} findes, men staar ikke i SRCS. Den bliver "
                            f"aldrig oversat til {board}.")


def main():
    print("Tjekker at hver bygning kan finde sine headere ...")
    tjek_cmake()
    tjek_assets()
    tjek_motoren()
    for navn, sti in (("enhedstestene", ROD / "tests/host/run.sh"),
                      ("vaerktoejet", ROD / "tools/zs-probe/build.sh")):
        kilder, stier = kilder_og_stier_fra_shell(sti)
        if not kilder:
            fejl.append(f"{navn}: fandt ingen kildefiler i {sti.name}. "
                        f"Er listen skrevet om?")
            continue
        tjek_bygning(navn, kilder, stier)

    kilder, stier = kilder_og_stier_fra_fuzz()
    if not kilder:
        fejl.append("fuzzerne: fandt ingen kildefiler i tests/fuzz/koer.sh. "
                    "Er listen skrevet om?")
    else:
        tjek_bygning("fuzzerne", kilder, stier)

    if fejl:
        for f in fejl:
            print(f"  \033[1;31mFEJL\033[0m {f}")
        print(f"\n  {len(fejl)} ting at rette.")
        return 1

    print(f"  alle kilder er med, motoren er fri af hardware, og "
          f"{len(BOARDS) + 3} bygninger kan finde det de beder om")
    return 0


if __name__ == "__main__":
    sys.exit(main())
