# Hardware og flashning

## Enheden

**Seeed SenseCAP Indicator D1**

| | |
|---|---|
| Hovedchip | ESP32-S3, to kerner, 240 MHz |
| Flash | 8 MB, QIO |
| PSRAM | 8 MB, oktal, 120 MHz |
| Anden chip | RP2040, bruges ikke af zScreen |
| Skærm | 4,0", 480 x 480, ST7701, 16-bit parallel RGB |
| Touch | kapacitiv over I2C |
| Netværk | wifi 2,4 GHz og Bluetooth. Vi bruger kun wifi |
| Porte | 2 x USB-C, 2 x Grove |
| Baglys | GPIO 45, styret med PWM |

**D1 er grundmodellen.** Den har hverken CO2-sensor, tVOC-sensor eller
LoRa. Det er D1S, D1L og D1Pro der har det. Til en energiskærm er
grundmodellen den rigtige: vi skal kun bruge skærmen og wifi.

RP2040-chippen bruges ikke. Den sidder på boardet og styrer sensorer og
SD-kort på de større modeller. Vi rører den ikke.

---

## Værktøjskæde

```bash
./tools/setup-toolchain.sh      # én gang
source tools/env.sh             # i hver ny terminal
```

Scriptet henter **ESP-IDF v5.1.7**. Versionen er ikke tilfældig: Seeeds
SDK kræver v5.1.x. På v5.0 mangler det RGB-panel-API deres BSP bruger,
og fra v5.2 er LCD-drivernes signaturer ændret så deres kode ikke
oversætter.

`cmake` og `ninja` installeres via Homebrew, for ESP-IDF leverer dem
ikke selv på macOS.

### Hvis Python driller

ESP-IDF v5.1 er testet til og med Python 3.12 og bygger sit eget
virtuelle miljø. Det kan gå galt af to grunde:

1. **For ny Python.** Homebrews `python3` er i dag 3.14
2. **Ødelagt Homebrew-Python.** Både `python@3.12` og `python@3.14` kan
   have et `pyexpat` der loader systemets `libexpat` i stedet for
   Homebrews. Alt der rører XML fejler så

Begge ser ud som `ensurepip returned non-zero exit status 1`, hvilket
ikke afslører noget. Scriptet prøver derfor hver kandidat af i praksis
ved at bygge et rigtigt venv med pip i, og lægger vinderen i en
shim-mappe forrest i PATH.

Er dine Homebrew-pythons i stykker:

```bash
brew reinstall expat python@3.12
```

---

## Byg og flash

```bash
source tools/env.sh
cd firmware
idf.py build
ls /dev/cu.usbmodem*                      # find porten
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Sæt USB-C i porten mærket **USB** på bagsiden. Kommer der ingen
`/dev/cu.usbmodem*` frem, så prøv den anden port eller et andet kabel:
mange USB-C-kabler kan kun strøm.

Ctrl-] afslutter `monitor`.

### Kommer den ikke i programmeringstilstand

Hold **BOOT**-knappen nede, tryk kort på **RESET**, slip **BOOT**.
Derefter virker `idf.py flash`.

---

## Hvis skærmen ser forkert ud

D1 findes med to panelvarianter, GX og DX. Vi bygger til **GX**, som er
standard.

Er billedet forskudt, spejlvendt eller har forkerte farver, så skift i
`firmware/sdkconfig.defaults`:

```
CONFIG_SENSECAP_INDICATOR_SCREEN_GX=y      # ← denne ud
CONFIG_SENSECAP_INDICATOR_SCREEN_DX=y      # ← denne ind
```

Derefter `rm -rf build && idf.py build`.

Rammer touch ved siden af, er det den samme indstilling.

---

## Partitionstabellen

8 MB flash, delt sådan her:

| Navn | Fra | Størrelse |
|---|---|---|
| nvs | 0x009000 | 24 KB |
| otadata | 0x00F000 | 8 KB |
| phy_init | 0x011000 | 4 KB |
| ota_0 | 0x020000 | 3 MB |
| ota_1 | 0x320000 | 3 MB |
| storage | 0x620000 | 1856 KB |

I alt 7,94 MB af 8.

**To app-pladser fra starten, selvom vi ikke opdaterer over netværket
endnu.** Partitionstabellen ligger fast på en enhed der er sendt ud til
en kunde. Skal skærmen senere kunne opdateres uden at blive hentet hjem
og sat i USB, skal der være plads til to udgaver af programmet allerede
nu. Det koster ingenting at have pladsen stående tom, og det er umuligt
at tilføje bagefter.

`storage` står tom indtil videre. Den er tænkt til gemte målinger hvis
vi vil vise en dagsgraf.

---

## Sikkerhed før serieproduktion

zScreen er skrivebeskyttet mod inverteren og har ingen cloud, ingen
konto og ingen åbne porte. Der er én ting tilbage før enheder sendes ud
til kunder:

**Wifi-kodeordet ligger i klartekst i flash.** Sådan er det på praktisk
talt alt IoT-udstyr fra hylden, men enhver der kan skille kabinettet ad
og sætte en programmer på, kan læse det.

Det løses med flash-kryptering og sikker opstart:

```
idf.py menuconfig
  Security features
    [*] Enable flash encryption on boot
        Release mode          ← ikke Development
    [*] Enable hardware Secure Boot in bootloader
```

**Det brænder sikringer i chippen som ikke kan brændes tilbage.**
Efter det kan boardet ikke længere flashes frit, og det kan ikke bruges
til at prøve ting af på. Derfor gør vi det ikke i udviklingsfasen.

Fremgangsmåden skal skrives ind i produktionsflowet sammen med
nøglehåndteringen, før den første enhed sælges.

---

## Strøm

Skærmen kører på USB-C, 5 V. Ved fuld lysstyrke trækker den omkring
0,5 A, altså 2,5 W. En almindelig telefonoplader er rigeligt.

Der er intet batteri i enheden. Går strømmen, går skærmen ud og starter
igen af sig selv når den kommer tilbage. Indstillingerne ligger i NVS
og overlever.
