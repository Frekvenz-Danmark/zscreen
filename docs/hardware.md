# Hardware og flashning

## Enheden

**Seeed SenseCAP Indicator D1**

| | |
|---|---|
| Hovedchip | ESP32-S3, to kerner, 240 MHz |
| Flash | 8 MB, QIO |
| PSRAM | 8 MB, oktal, 120 MHz |
| Anden chip | RP2040, styrer summeren — se nedenfor |
| Skærm | 4,0", 480 x 480, ST7701, 16-bit parallel RGB |
| Touch | kapacitiv over I2C |
| Netværk | wifi 2,4 GHz og Bluetooth. Vi bruger kun wifi |
| Porte | 2 x USB-C, 2 x Grove |
| Baglys | GPIO 45, styret med PWM |

**D1 er grundmodellen.** Den har hverken CO2-sensor, tVOC-sensor eller
LoRa. Det er D1S, D1L og D1Pro der har det. Til en energiskærm er
grundmodellen den rigtige: vi skal kun bruge skærmen og wifi.

### RP2040-chippen

Den sidder på boardet og styrer sensorer og SD-kort på de større
modeller. På en D1 er der ingen sensorer, så den passer kun én ting vi
bruger: **summeren.**

Summeren sidder på RP2040'ens GPIO 19 og er passiv — den skal have en
PWM-tone, ikke bare spænding. ESP32'en kan ikke nå den. Det eneste der
går mellem de to chips er en seriel ledning:

| ESP32-S3 | | RP2040 |
|---|---|---|
| GPIO 19 (TX) | → | GPIO 17 (RX) |
| GPIO 20 (RX) | ← | GPIO 16 (TX) |

115200 baud, 8N1, pakker i COBS. Et bip er altså en pakke vi sender til
den anden chip.

**De to ben er ikke USB.** På en almindelig ESP32-S3 er GPIO 19 og 20
chippens egne USB-ben, og ESP-IDF sætter som standard en konsol på dem
oveni UART0. Her går de til RP2040'en, og der er intet USB-stik på dem.
Derfor står `CONFIG_ESP_CONSOLE_SECONDARY_NONE=y` i
`sdkconfig.defaults`: ellers holder USB-blokken fast på benene, og en
pakke kommer aldrig ud. Skærmen flashes over UART0, som før — Seeed
skriver selv at ESP32'en melder sig som `USB-SERIAL CH340`.

Lyden kræver **to** ting: `ZS_LYD_ENABLED` i `zs_config.h`, og at
RP2040'en har vores egen firmware. Firmwaren fra fabrikken svarer kun på
sluk-kommandoen — den kender bip-kommandoen, men kalder den aldrig. Se
`tools/rp2040/HVORDAN.md`.

Som standard er lyden **slået fra**, netop fordi den anden chip skal
flashes først. Blinket og baglys-pulsen virker uanset.

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

## Papirerne ind i en ny enhed

Firmwaren læser sit certifikat, sin nøgle, serverens CA og serveradressen
ud af NVS — rummet `zsfleet`, se `net/zs_fleet.c`. Uden dem springer den
flådestyringen over, **tavst**.

Der var ingen vej til at få dem ind. Det blev fundet 11. oktober 2026,
da spørgsmålet var om vi kunne flashe en skærm og prøve forløbet på
rigtigt jern. Svaret var nej — ikke fordi noget var i stykker, men fordi
`ca-ny-enhed.sh` lavede PEM-filer og intet forbandt dem med enheden.

```bash
# i zScreen-server
./scripts/ca-ny-enhed.sh <id>                  papirerne, hvis de mangler
python3 scripts/enhed-nvs.py <id> --vaert 10.1.3.7
```

Den skriver en `nvs.bin` med fire nøgler og **efterser den bagefter** med
IDF's eget `nvs_tool.py`: rummet, de fire nøgler, og at de er **strenge**
— firmwaren læser dem med `nvs_get_str`, som kræver en NUL-afslutning.

Den siger også fra hvis servercertifikatet ikke dækker den adresse du
giver. Gør det ikke, afviser enheden serveren med en TLS-fejl der ser ud
som *"EOF occurred in violation of protocol"* og ikke peger nogen steder.

```bash
esptool.py --chip esp32s3 -p /dev/cu.usbmodem* write_flash 0x9000 \
    firmware-filer/nvs/zscreen-<id>-nvs.bin
```

**AT FLASHE NVS SLETTER ALT ANDET I NVS:** wifi-navn, kodeord, valgt
inverter og tema. Det er rigtigt på en ny enhed og forkert på en der står
og kører.

**Og `--vaert` er det vigtige ved en prøve.** Firmwarens indbyggede
`ZS_FLEET_HOST` er `fleet.frekvenz.nu`, og der er ingen server på det navn
i dag — det peger på domænets webhosting. Derfor sættes adressen per
enhed, så den samme firmware kan pege på en prøveserver og på den
rigtige.

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
