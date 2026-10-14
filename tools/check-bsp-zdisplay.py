#!/usr/bin/env python3
"""
zScreen - tjek at vi bygger HELE Seeeds BSP til ZDisplay.

HVORFOR DEN FINDES.

ZDisplay bruger Seeeds egen BSP til reTerminal D1001, men ikke deres
idf_component.yml: den beder om esp_lvgl_port ^2, som kraever LVGL 9, og
vores flade er 33.000 linjer skrevet til LVGL 8.3. Derfor registrerer
firmware/boards/zdisplay/components/zd_bsp/CMakeLists.txt deres KILDER -
uaendrede, fra submodulet - med vores egen afhaengighedsliste.

Prisen er at kildelisten staar to steder: hos dem og hos os. Den dag
Seeed laegger en fil til, kommer den ikke med af sig selv, og det ville
ikke vaere en byggefejl. Det ville vaere en funktion der mangler, eller
en der linker fordi noget ANDET tilfaeldigvis definerer det samme navn.

Derfor sammenlignes de to lister her. Koeres af tools/check-alle.sh.

Siger den fra, er der to ting at goere:

  * kom der en fil til, saa skriv den ind i vores CMakeLists - og laes
    hvad den laver foerst. Vi bygger ikke kode vi ikke ved hvad er.
  * forsvandt en fil, saa tag den ud. Vores cmake falder i forvejen med
    en FATAL_ERROR, saa den fanges ogsaa af et byg - men her staar det
    med en forklaring i stedet for som en sti der ikke findes.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
DERES = ROD / ("firmware/vendor/reTerminal_D1001/components/"
               "esp32_p4_re_terminal_d1001/CMakeLists.txt")
VORES = ROD / "firmware/boards/zdisplay/components/zd_bsp/CMakeLists.txt"


def kilder(fil, hvem):
    if not fil.exists():
        print(f"  \033[1;31mFEJL\033[0m {hvem}: {fil.relative_to(ROD)} findes ikke. "
              f"Mangler submodulet? 'git submodule update --init --depth 1'")
        return None
    tekst = fil.read_text(encoding="utf-8")
    # Kun filnavnet. Deres staar som "src/x.c", vores med hele stien
    # foran, og det er den SAMME fil.
    return {m.rsplit("/", 1)[-1] for m in re.findall(r'"([^"]+\.c)"', tekst)}


def main():
    print("Tjekker at ZDisplay bygger hele Seeeds BSP ...")

    deres = kilder(DERES, "Seeeds BSP")
    vores = kilder(VORES, "vores zd_bsp")
    if deres is None or vores is None:
        return 1
    if not deres:
        print("  \033[1;31mFEJL\033[0m fandt ingen kilder i Seeeds CMakeLists. "
              "Er den skrevet om?")
        return 1

    mangler = sorted(deres - vores)
    for_meget = sorted(vores - deres)

    if mangler:
        for f in mangler:
            print(f"  \033[1;31mFEJL\033[0m {f} er i Seeeds BSP, men ikke i vores. "
                  f"Den bliver ikke bygget til ZDisplay.")
    if for_meget:
        for f in for_meget:
            print(f"  \033[1;31mFEJL\033[0m {f} staar i vores liste, men findes "
                  f"ikke i Seeeds BSP laengere.")

    if mangler or for_meget:
        print(f"\n  {len(mangler) + len(for_meget)} ting at rette i "
              f"{VORES.relative_to(ROD)}.")
        return 1

    print(f"  \033[1;32m OK\033[0m {len(deres)} kildefiler, samme liste som Seeeds egen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
