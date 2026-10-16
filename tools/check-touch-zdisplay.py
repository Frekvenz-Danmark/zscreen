#!/usr/bin/env python3
"""
zScreen - sig fra naar Seeed retter i den touch-driver vi har en rettet
kopi af.

HVORFOR DEN FINDES.

firmware/boards/zdisplay/components/zd_touch/esp_lcd_touch_gsl3670.c er
en kopi af Seeeds fil med tre bufferfejl rettet. Se hovedet af den fil.

En kopi er en gaeld. Retter Seeed noget i originalen - en rigtig
rettelse, en ny kreds, en ny registeradresse - saa faar vi det ikke, og
ingen opdager det. Derfor staar deres fils fingeraftryk her. Skifter
det, skal nogen laese deres aendringer igennem og afgoere:

  * er vores tre rettelser blevet unoedvendige, saa kopien kan smides
    vaek og deres komponent bruges direkte igen?
  * eller skal rettelserne laegges oven paa deres nye udgave?

Og saa skrives det nye fingeraftryk ind her.

Koeres af tools/check-alle.sh.
"""
import hashlib
import pathlib
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

DERES = ROD / ("firmware/vendor/reTerminal_D1001/components/"
               "esp_lcd_touch_gsl3670/esp_lcd_touch_gsl3670.c")
VORES = ROD / ("firmware/boards/zdisplay/components/zd_touch/"
               "esp_lcd_touch_gsl3670.c")

# Fingeraftrykket paa den udgave vi forkede fra, 14. oktober 2026.
FORKET_FRA = "c9e278bcec2cc59533092354b05020adcb723e9bad10e4b9a36d01c52acf2254"


def main():
    print("Tjekker den rettede touch-driver ...")

    if not DERES.exists():
        print(f"  {ROED}FEJL{SLUT} {DERES.relative_to(ROD)} findes ikke. "
              f"Mangler submodulet?")
        return 1
    if not VORES.exists():
        print(f"  {ROED}FEJL{SLUT} {VORES.relative_to(ROD)} findes ikke. "
              f"Er kopien slettet? Saa bygger ZDisplay med en driver der "
              f"skriver ud over tre buffere.")
        return 1

    nu = hashlib.sha256(DERES.read_bytes()).hexdigest()
    if nu != FORKET_FRA:
        print(f"  {ROED}FEJL{SLUT} Seeed har rettet i deres "
              f"esp_lcd_touch_gsl3670.c.")
        print(f"         forket fra: {FORKET_FRA}")
        print(f"         nu:         {nu}")
        print(f"       Laes deres aendringer igennem. Er de tre "
              f"bufferfejl rettet hos dem, saa smid kopien vaek og brug "
              f"deres komponent. Ellers laeg rettelserne oven paa den "
              f"nye udgave og skriv det nye fingeraftryk i "
              f"{pathlib.Path(__file__).name}.")
        return 1

    # Og at vores kopi faktisk ER rettet. En kopi uden rettelserne er
    # det daarligste af begge verdener: vi slaeber en fil rundt OG har
    # fejlen.
    vores = VORES.read_text(encoding="utf-8")
    mangler = [k for k, v in (
        ("den stoerre buffer",      "uint8_t touch_data[48]"),
        ("tjekket af laesefejlen",  "if (err != ESP_OK) {"),
        ("graensen paa ti fingre",  "if (Finger_num > 10) {"),
        ("graensen paa XY-feltet",  "if (Finger_num > MAX_FINGER_NUM) {"),
        # De tre her kom til 15. oktober 2026. Alle tre handler om det
        # samme: en fejl der blev tiet ned.
        ("at load_fw's fejl stopper opstarten",
         "ESP_RETURN_ON_ERROR(esp_lcd_touch_gsl3670_load_fw"),
        ("at init's svar bliver tjekket",
         "ESP_GOTO_ON_ERROR(esp_lcd_touch_gsl3670_init"),
        # Den vaerste af dem: err-labelen frigav tp'en og skrev
        # DEREFTER pegepinden ud i *out_touch. Kalderen fik altsaa en
        # pegepind til frigivet hukommelse sammen med et svar der sagde
        # fejl.
        ("at en frigjort pegepind ikke gaar ud af doeren",
         "*out_touch = NULL;"),
        # Og DEN RIGTIGE ROD bag den. Foerste rettelse nulstillede kun
        # kalderens kopi; den globale pegepind stod stadig og pegede
        # paa frigivet hukommelse resten af enhedens levetid.
        ("at ogsaa den globale pegepind nulstilles",
         "esp_lcd_touch_gsl3670 = NULL;"),
        ("at den globale ikke kan naas udefra",
         "static esp_lcd_touch_handle_t esp_lcd_touch_gsl3670;"),
        # En funktion der FAAR et haandtag, skal bruge det. De to
        # hjaelpere laeste den globale i stedet, saa de kunne roere
        # frigivet hukommelse selv om de havde et gyldigt tp i haanden.
        ("at hjaelperne bruger deres eget tp",
         "1 << tp->config.rst_gpio_num"),
    ) if v not in vores]
    if mangler:
        for m in mangler:
            print(f"  {ROED}FEJL{SLUT} kopien mangler {m}")
        return 1

    # ── OG VI BRUGER IKKE SEEEDS bsp_touch_new ─────────────────────
    #
    # Deres funktion laver et I2C-panel-io og giver det til driveren.
    # Fejler driveren, tager den ikke haandtaget - og saa er det
    # kalderens at slette, men kalderen har det ikke. Hvert mislykket
    # forsoeg efterlod altsaa en forladt i2c-enhed paa bussen, og
    # loekken i lv_port.c proever tre gange.
    #
    # Derfor laver lv_port.c det selv, ejer io'et og sletter det paa
    # fejlvejen. Se noten ved touch_ny. Kommer bsp_touch_new tilbage,
    # kommer laekagen med den.
    port = ROD / "firmware/boards/zdisplay/main/lv_port.c"
    if not port.exists():
        print(f"  {ROED}FEJL{SLUT} {port.name} findes ikke - flyt tjekket "
              f"med, eller tag det ud")
        return 1
    kode = "\n".join(l for l in port.read_text(encoding="utf-8").splitlines()
                      if not l.lstrip().startswith(("*", "/*", "//")))
    if "bsp_touch_new" in kode:
        print(f"  {ROED}FEJL{SLUT} lv_port.c kalder bsp_touch_new igen. "
              f"Den laekker et i2c-haandtag hver gang driveren fejler, "
              f"og loekken proever tre gange. Brug touch_ny, som ejer "
              f"haandtaget og sletter det paa fejlvejen")
        return 1
    for hvad, hvad_der_mangler in (
            ("laver panel-io'et selv", "esp_lcd_new_panel_io_i2c"),
            ("og sletter det igen",    "esp_lcd_panel_io_del")):
        if hvad_der_mangler not in kode:
            print(f"  {ROED}FEJL{SLUT} lv_port.c {hvad} ikke "
                  f"({hvad_der_mangler} mangler). Uden begge to er "
                  f"ejerskabet af i2c-haandtaget ikke paa plads")
            return 1

    print(f"  {GROEN} OK{SLUT} kopien er rettet paa ti punkter, Seeeds "
          f"original er uaendret siden vi forkede, og vi ejer selv "
          f"touchens i2c-haandtag.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
