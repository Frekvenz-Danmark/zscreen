# ZDisplay — den store skærm

Internt navn for en energiskærm på **Seeed reTerminal D1001**. Samme
opgave som zScreen: hænge på væggen og vise hvad solcellerne, huset,
batteriet og elnettet laver. Større skærm, og plads til mere.

Den her fil er forarbejdet: hvad enheden er, hvad der er anderledes fra
zScreen, hvad der blev besluttet og hvorfor, og hvad der endnu ikke er
målt. Datoer er måledatoer. Står der at noget er målt, er det målt.

Skrevet 14. oktober 2026, før enheden har været tændt.

---

## Enheden

Tallene er fra Seeeds wiki og deres eget SDK, ikke fra vores egen
måling.

| | reTerminal D1001 (ZDisplay) | SenseCAP Indicator D1 (zScreen) |
|---|---|---|
| Hovedchip | ESP32-P4, to RISC-V-kerner, 400 MHz | ESP32-S3, to Xtensa-kerner, 240 MHz |
| Anden chip | ESP32-C6 — **radioen** | RP2040 — sensorer og summer |
| Skærm | 8", 800 x 1280, MIPI-DSI, JD9365 | 4", 480 x 480, RGB, ST7701 |
| Touch | GSL3670 | FT5x06 |
| PSRAM | 32 MB | 8 MB |
| Flash | 32 MB opgivet, **ikke målt** | 8 MB |
| Wifi | Wi-Fi 6, 2,4 GHz, gennem C6'eren | Wi-Fi 4, 2,4 GHz, i chippen selv |
| Lyd | ES8311-kodek, 2 W højttaler | passiv summer på RP2040'eren |
| Ur | PCF8563 med batteri | intet |
| Strøm | USB-C og 2500 mAh batteri | USB-C |
| Kort | microSD | intet |
| Plads til mere | mPCIe til 4G, LoRa-ben, kamera, IMU, RGB-lysdiode | intet |
| Pris | 84,90 USD | ca. 50 USD |

Enheden tændes med et **langt tryk over 3 sekunder** på den grønne
knap. Det er ikke som zScreen, der bare starter når der kommer strøm.
Det skal stå i vejledningen til kunden, og det skal afprøves om den
starter af sig selv når strømmen har været væk — en energiskærm der
kræver et tryk efter et strømsvigt, er en fejl.

### Flashen er ikke afklaret

Seeeds wiki siger 32 MB. Deres egen `sdkconfig.defaults` til
fabriksfirmwaren siger `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`. De to
passer ikke sammen, og partitionstabellen kan ikke laves rigtigt før
det er afgjort.

Måles med:

    esptool.py --port /dev/cu.usbmodemXXXX flash_id

---

## Radioen sidder i en anden chip

Det er den største forskel, og den rører alt.

ESP32-P4 **har ingen radio**. Wifi kommer fra en ESP32-C6 ved siden af,
og de to taler SDIO sammen. På vores side ser det ud som almindelig
`esp_wifi`, fordi Espressifs `esp_wifi_remote` lægger sig ind og sender
hvert kald videre til C6'eren. Alle de kald firmwaren bruger i dag
findes i den vej:

    esp_wifi_init           esp_wifi_scan_start        esp_wifi_connect
    esp_wifi_set_mode       esp_wifi_scan_get_ap_num   esp_wifi_disconnect
    esp_wifi_set_config     esp_wifi_scan_get_ap_records
    esp_wifi_set_storage    esp_wifi_sta_get_ap_info
    esp_wifi_start          esp_wifi_clear_ap_list     esp_wifi_restore

Det betyder **ikke** at `zs_wifi.c` kan flyttes uden videre. To ting
skal afgøres på enheden:

**1. C6'eren skal have ny firmware, og det er ikke valgfrit.** Chippen
kommer med Seeeds egen fra 11. september 2025. Den indeholder en udgave
af `esp_hosted`, og i de gamle udgaver dør wifi efter nogle minutter:
et andet P4-board har målt at en v2.3.0-slave mister forbindelsen efter
cirka fire minutter, og at der skal v2.9.4 eller nyere til i **begge**
ender for at SDIO-forbindelsen genoprettes af sig selv. Nyeste er 3.0.9.

En skærm på en væg skal køre i årevis uden at nogen rører den. Derfor:
vi bygger C6-firmwaren selv, af samme udgave som den vores egen side
bruger, og vi flasher den. P4'eren kan selv: der går en UART til
C6'eren (P4 GPIO 48 og 47, og dens BOOT-ben på GPIO 12), og der findes
`esp_hosted_slave_ota()` over SDIO.

**2. Så er der to firmwarer at holde styr på.** Vores
underskriftsordning dækker P4-programmet. C6-billedet er uden for den.
Planen er at lægge C6-billedet **ind i** P4-programmet, så det arver
underskriften, og lade P4'eren flashe C6'eren når versionerne ikke
passer. Det koster cirka 1,2 MB flash, og det giver én fil at udgive og
én underskrift at passe på. Det er ikke bygget endnu.

Wifi er stadig **kun 2,4 GHz**. C6'eren er enkeltbånds, så årsag 201 er
stadig næsten altid et 5 GHz-net. Den fejl flytter med.

**Og én ting fulgte ikke med gratis:** `esp_read_mac(…, ESP_MAC_WIFI_STA)`
virker ikke, fordi chippen ikke har en wifi-MAC. Det kostede nær at hver
enkelt ZDisplay meldte sig på flåden under samme navn. Se afsnittet "Og
det gennemgangen fandt bagefter".

---

## Hvad Seeed selv leverer

`https://github.com/Seeed-Studio/reTerminal-D1001`, Apache 2.0, hentet
som pinnet submodul i `firmware/vendor/reTerminal_D1001`, samme mønster
som SenseCAP-SDK'et. Pinnet på `5074d3b`, 17. april 2026.

Det vi bruger:

| Komponent | Hvad |
|---|---|
| `esp_lcd_jd9365_8` | panelet over MIPI-DSI |
| `esp_lcd_touch_gsl3670` | touch |
| `esp_io_expander_pca9535` | benudvideren, som holder reset, baglys, batteri og 4G-ben |
| `esp32_p4_re_terminal_d1001` | hele BSP'en: stroem, panel, touch, batteri, ur, IMU |

Det vi **ikke** bruger, og hvorfor:

* **`esp-brookesia`** er en hel telefonlignende brugerflade. Vi har vores
  egen flade, og den er prøvet.
* **`esp_cam_sensor`, `esp_video`, `esp_h264`, `esp-sr`** er kamera, video
  og taleopfattelse. Ikke på en energiskærm.
* **Deres BSP-glue til LVGL.** Se beslutningen nedenfor.

### Deres kode, vores afhængighedsliste

Deres `bsp_display_start()` returnerer en `lv_display_t *`. Den type
findes kun i **LVGL 9**. Vores flade er 33.000 linjer skrevet til LVGL
8.3, og de to kan ikke være i samme byg.

Men hele LVGL-delen af deres kilde ligger bag
`#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)`, og `display.h` og `touch.h`
findes netop for dem der ikke vil bruge LVGL. Problemet er ikke koden,
det er deres `idf_component.yml`: den beder om `esp_lvgl_port ^2`
uanset, og den kræver LVGL 9. Så selv med glue-delen slået fra ville
BSP'en trække LVGL 9 ind i bygget — ved siden af vores egen LVGL 8.3.

Løsningen er at bruge deres **kilder** og ikke deres manifest.
`firmware/boards/zdisplay/components/zd_bsp/CMakeLists.txt` registrerer
deres fire kildefiler, uændrede, direkte fra submodulet, med en
afhængighedsliste vi selv skriver. Så får vi hele deres opstart, og
hverken LVGL 9, esp-brookesia, kamera eller taleopfattelse kommer med.

**Det er grunden til at vi ikke skrev opstarten selv.** Deres
`bsp_power_init()` gør mere end navnet siger: den sætter en
PMU-spændingsindstilling, holder VDD_3V3 via et ben på benudvideren,
tænder skærm-, kamera- og 4G-strøm, starter batterimålingen, uret,
IMU'en og den grønne knap — og den **sætter drivestyrken ned på de seks
SDIO-ben** ud til C6'eren for at dæmpe overshoot. Det sidste er ikke
noget man gætter, og det er præcis den slags der giver et wifi der
næsten virker.

To ting følger af at listen står to steder:

* `tools/check-bsp-zdisplay.py` sammenligner vores kildeliste med deres
  egen og siger fra hvis Seeed lægger en fil til. Vagten er prøvet ved
  at fjerne en fil og se den falde.
* Deres `esp32_p4_re_terminal_d1001.c` kalder `esp_timer_get_time()`
  uden at hente `esp_timer.h`. Det bygger hos dem, fordi
  `esp_lvgl_port`s headere trækker den med. Hos os er det en fejl, og
  den løses med `-include esp_timer.h` på netop den komponent — ikke med
  en rettelse i submodulet, som ville være væk ved næste opdatering.

### Panelets størrelse er betinget hos dem

Seeeds `display.h` vælger mellem 1024 x 600 og 800 x 1280 på
`CONFIG_BSP_LCD_TYPE_1024_600`. Den konstant findes **ikke** i deres
Kconfig i dag, så vi får 800 x 1280. Men tilføjer de valget i en
opdatering, flytter panelet sig under os: fladen ville regne i 800 x
1280, mens touchen blev skaleret til 1024 x 600, og hvert tryk ville
ramme et andet sted end der blev trykket. Intet af det ville være en
byggefejl.

Der står nu to `_Static_assert` i `firmware/boards/zdisplay/main/lv_port.c`
der binder vores mål til deres. Flytter tallene sig, falder byggeriet.

### Deres touch-opsætning ser forkert ud, og er det ikke

`bsp_touch_new()` sætter `rst_gpio_num = 12`, og GPIO 12 på det her
board er C6'erens BOOT-ben. Det ser ud som om en touch-reset trækker i
radiochippens boot-ben.

Det gør den ikke. Driveren bruger tallet som et **benudvider-nummer**,
ikke som et chip-GPIO: `esp_lcd_touch_gsl3670.c` kalder
`esp_io_expander_set_level(io_expander, 1 << rst_gpio_num, …)`, og
`1 << 12` er præcis `BSP_LCD_TOUCH_RST`. Chippens eget GPIO 12 bliver
aldrig rørt.

Det er også derfor Seeed har kommenteret reset-koden ud i deres egen
`bsp_touch_new()`: driveren gør det selv.

Det står her fordi det så galt ud, og fordi den næste der læser de linjer
vil tro det samme. Her er svaret, så ingen går og leder i den forkerte
chip.

---

## De fem beslutninger

Enheden er ikke tændt endnu, så det her er valg og ikke målinger. De
står her for at kunne laves om med åbne øjne.

### 1. To produkter, ét repo, én motor

Begge skærme laver det samme: læser Modbus fra inverteren, regner fire
tal ud, melder ind på flåden, hører efter en ladeplan, opdaterer sig
selv. Det er **motoren**, og den skal findes ét sted. Kun skærmen,
lyden, uret og opstarten er forskellige.

Kommer der et firmwarehul i Modbus-afkodningen, skal det rettes én gang.
To repoer betyder at det bliver rettet én gang og glemt det andet sted.

### 2. LVGL 8.3 bliver

At flytte til LVGL 9 ville koste en omskrivning af hele fladen, nye
skrifttyper, og nye målinger på et produkt hvis udseende er låst med
fire vagtscripts. Det er ikke arbejde der giver kunden noget.

### 3. ESP-IDF 5.4.2 på P4, og 5.1.7 bliver på S3 indtil videre

ESP32-P4 kræver mindst v5.3. Seeed bygger og prøver deres SDK med
**v5.4.2**, så det er den vi tager.

zScreen bliver på 5.1.7 for nu. Ikke fordi det er pænt — to
værktøjskæder er noget skrammel at passe — men fordi en opdatering af
S3-firmwaren er en ændring af et produkt der virker, og den skal laves
for sig med sine egne målinger. Arbejdet er halvt gjort i forvejen:
grenen `idf-5.3` byggede grønt i CI med 5.3.6. Den er 99 commits bagud
og skal laves om, ikke flettes.

De to IDF'er kan stå ved siden af hinanden. Det er `source export.sh`
der afgør hvilken der bruges i en terminal, ikke noget i repoet.

**Men 5.4.2 sætter et loft.** `esp_hosted` 3.0 og nyere kræver ESP-IDF
5.5, så vi får 2.13.0 — nyeste i 2-serien. Det er godt nok: rettelsen
der genopretter SDIO-forbindelsen af sig selv kom i 2.9.4, og det er
netop den der afgør om wifi holder i mere end nogle minutter. Men
3-serien er der hvor arbejdet foregår nu, og den kræver 5.5.

**Målet er én IDF til begge.** Det rigtige tidspunkt er lige efter at
ZDisplay bygger, for så er begge sider kendte. Det er den eneste
beslutning her der har en udløbsdato.

### 4. Skærmen står på højkant, 800 x 1280

Panelet er 800 x 1280 i hardwaren. Vil man have det på tværs, skal LVGL
dreje hver ramme, og det kan ikke kombineres med
`BSP_DISPLAY_LVGL_AVOID_TEAR` og `DIRECT_MODE`, som er netop det der
gør at billedet ikke flimrer på en 8-tommer.

Og højkant er i forvejen den prøvede vej: `ui/zs_layout.h` regner alt ud
fra `ZS_G_SCR_WIDTH` og `ZS_G_SCR_HEIGHT`, og `tests/host/test_layout_800.c`
prøver præcis 800 x 1280. Det blev lagt ind da fladen blev skrevet, med
den her enhed nævnt ved navn.

Fire kort på 800 x 1280 bliver meget store. Det er ikke forkert — en
skærm man skal kunne læse fra en stue, må godt have store tal — men der
er plads til en række mere. Det er en designopgave, ikke en teknisk, og
den venter til skærmen kan tænde.

### 5. Udgivelserne skal have navn på produktet

Som koden står nu, tager `zs_ota.c` **den første fil i udgivelsen der
slutter på `.bin`**. Med to produkter i samme repo ville en zScreen
kunne hente en ZDisplay-firmware. ESP-IDF ville afvise den, fordi
chip-id'et i billedets hoved er forkert, så den ville ikke ødelægge
noget — men skærmen ville hente 2 MB over en kundes forbindelse, afvise
den, og prøve igen tredive minutter senere. For evigt.

Rettes nu: filerne hedder `zscreen-<version>.bin` og
`zdisplay-<version>.bin`, og hver skærm tager kun sit eget navn.
Versionsmærket er fælles, så de to produkter følges.

---

## Motoren: hvad er fælles

Tallene er linjer kode, målt 14. oktober 2026.

| Mappe | Linjer | Fælles? |
|---|---|---|
| `net/` — Modbus, SunSpec, Fronius, flåde, pris, OTA, tilmelding | 11.270 | **ja, alt** |
| `app/` — status, fejlvagt, formatering | 4.474 | **ja, alt** |
| `util/` — tid og tekst | 318 | **ja** |
| `storage/` — NVS | 604 | **ja** |
| `ui/` — flade, kort, tastatur, skrifttyper | 33.543 | ja, men skrifttyperne skal laves om i større grad |

Det er efterprøvet at motoren ikke rører hardwaren: der er **ingen**
`bsp_`-kald, ingen `driver/gpio`, ingen `driver/i2c` og ingen LVGL i
`net/`, `app/`, `util/` eller `storage/`. Det er derfor den samme kode
kan køre i `tests/host` på en Mac. Den egenskab er hele grunden til at
det her kan gøres på en aften i stedet for en måned, og den skal
beskyttes: `tools/check-headers.sh` skal udvides til at sige fra hvis
der kommer et hardware-kald ned i motoren.

Det der er pr. board:

| Fil | zScreen | ZDisplay |
|---|---|---|
| `main.c` | `bsp_board_init()`, egen rækkefølge | LDO, benudvider, DSI, egen rækkefølge |
| `lv_port.c` | RGB-panel, to buffere | MIPI-DSI, DPI-buffere |
| `ui/zs_display.c` | baglys på eget ben | baglys gennem benudvideren |
| `ui/zs_lyd.c` | pakke over UART til RP2040 | ES8311-kodek, rigtig højttaler |
| `util/zs_tid.c` | kun NTP | NTP og PCF8563 når nettet er væk |
| `zs_config.h` | fælles, med et afsnit pr. board | samme |
| partitionstabel | 8 MB | 16 eller 32 MB, se ovenfor |
| skrifttyper | 13–64 px | større, skal genereres |

Lyden er det pæneste eksempel på hvorfor det skal skilles ordentligt.
På zScreen sidder summeren på den anden chip og kan kun nås med en
COBS-pakke over en seriel ledning, og fabriksfirmwaren i den chip svarer
ikke på bip. På ZDisplay er der en rigtig højttaler på en rigtig kodek.
Det er to vidt forskellige ting bag det samme `zs_lyd_bip()`.

---

## Det der skal måles på enheden først

Intet herunder er afprøvet. Rækkefølgen er den rækkefølge det blokerer i.

1. **Flashens størrelse.** `esptool.py flash_id`. Partitionstabellen kan
   ikke lægges fast før.
2. **Starter den af sig selv efter et strømsvigt**, eller kræver den et
   langt tryk på knappen.
3. **Holder wifi i mere end et døgn.** Det er den fejl der er målt på
   andre P4-boards, og den ses ikke på fem minutter.
4. **Hvad C6'eren kommer med fra fabrikken.** Versionen af `esp_hosted`
   afgør om punkt 3 overhovedet kan lykkes.
5. **Om touch rammer rigtigt** på 800 x 1280 i højkant.
6. **Om billedet flimrer** med `AVOID_TEAR` og `DIRECT_MODE`, og hvad det
   koster i PSRAM: to rammer i 800 x 1280 x 16 bit er 4 MB.
7. **Hvad skærmen trækker**, og hvor længe batteriet holder uden stik.
8. **Om RTC'en overlever strømsvigt**, og om den stiller uret rigtigt
   når nettet er væk. Dag og time skal komme fra samme ur, se
   `zs_tid_dagnr`.

---

## Hvad der er plads til senere

Hardwaren kan mere end den første firmware bruger. Det er grunden til at
enheden blev valgt, så det står her for ikke at blive glemt:

* **4G.** mPCIe-stikket fører USB 2.0, og Seeed lægger
  `iot_usbh_modem` med i deres SDK. Benene til at tænde, nulstille og
  flyvetilstand ligger på benudvideren (`BSP_LTE_PWR_EN`,
  `BSP_LTE_RESET`, `BSP_LTE_AIRPLANE_MODE`, `BSP_LTE_DTR`), og
  fabriksfirmwaren slår PPP og NAPT til i lwIP. En skærm med 4G kan stå
  på et anlæg uden kundens wifi — og det fjerner hele
  opsætningsvejen med tastatur og netværksvalg.
* **LoRa.** Der er SPI-ben til et Wio-SX1262-modul.
* **microSD.** Gemte målinger til en dagsgraf. `storage`-partitionen på
  zScreen blev lagt ind til netop det og står stadig tom.
* **Kamera og mikrofoner.** Ingen plan. Nævnt her for at sige at det
  **ikke** er en plan: et kamera i en kundes stue er ikke noget vi
  tilføjer uden en god grund.
* **Batteriet** betyder at skærmen kan vise at strømmen er gået, i
  stedet for at gå i sort sammen med huset.

---

## Status

Lavet 14. oktober 2026. **Firmwaren bygger.**

```
firmware/
  version.txt                  ét versionsnummer, begge produkter
  kerne/
    kilder.cmake               listen BEGGE produkter bygger af
    net/ app/ util/ storage/   motoren, fri af hardware
    ui/                        fladen, LVGL 8.3
    hal/                       de TRE kontrakter et board opfylder:
                               lv_port.h, zs_baglys.h, zs_lyd.h
  boards/
    zscreen/                   ESP32-S3, 480 x 480
      main/ main.c lv_port.c zs_baglys_d1.c zs_lyd.c
    zdisplay/                  ESP32-P4, 800 x 1280
      components/zd_bsp/       Seeeds BSP med vores afhængigheder
      main/ main.c lv_port.c zs_baglys_d1001.c zs_lyd_d1001.c
```

**Et nyt produkt er fire filer.** `main.c` plus én for hver kontrakt i
`kerne/hal/`. Ingen af de to boards rækker ind i den anden, og ingen af
dem er 'hovedproduktet'.

`tools/check-byg.py` håndhæver det: alt i `kerne/` skal stå i
`kilder.cmake`, og alt i et boards `main/` skal stå i dét boards SRCS.
Da den fælles kode lå inde i zScreens egen mappe, måtte vagten have en
liste over fire undtagelser ved navn `KUN_ZSCREEN`. Den er væk — nu
siger strukturen det.

Målt, ikke antaget:

| | |
|---|---|
| ZDisplay, program | 1.904.640 bytes, 55 % plads tilbage i en 4 MB plads |
| ZDisplay, bootloader | 0x5b40, 29 % luft efter at tabellen blev flyttet til 0xA000 |
| zScreen, program | 1.642.496 bytes, uændret efter opsplitningen |
| Enhedstest | 1819 tjek, alle bestået. 19 af dem er nye |
| Vagter | alle 11 tilfredse, inklusive den nye |
| `esp_hosted` | 2.13.0, og `esp_wifi_remote` 1.6.5, begge pinnet præcist |

Tre ting blev fundet ved at bygge, og ingen af dem stod i noget datablad:

1. **`esp_phy_init.h` findes slet ikke for esp32p4.** ESP-IDF registrerer
   `esp_phy` som en tom komponent på den chip. `storage/zs_nvs.c` kaldte
   `esp_phy_erase_cal_data_in_nvs()` i fabriksnulstillingen, og det
   stoppede bygningen. Det er ikke en manglende afhængighed — funktionen
   er der ikke, fordi radioen ikke er der. Nu står forskellen som
   `ZS_HAR_LOKAL_RADIO` i `zs_config.h`, og **en fabriksnulstilling på
   ZDisplay rydder ikke radioens kalibrering.** Det kostede en halv dag
   på zScreen at finde ud af at en dårlig kalibrering ser ud som en
   søgning der ikke virker. Sker det her, er det C6'eren der skal ses på.
2. **Bootloaderen havde 5 % luft.** P4'erens bootloader starter på
   0x2000 mod S3'erens 0x1000, så med partitionstabellen på 0x8000 var
   der 1216 bytes fri. Tabellen kan ikke flyttes på en enhed der er
   sendt ud, så den ligger nu på 0xA000 fra starten.
3. **Seeeds BSP mangler en include.** Se afsnittet ovenfor.

### Og det gennemgangen fandt bagefter

Ti ting, i to runder, og den værste ville ikke have været en byggefejl.

**Hver ZDisplay ville have meldt sig på flåden med samme navn.**
`zs_fleet_unique_id()` byggede navnet af `esp_read_mac(…,
ESP_MAC_WIFI_STA)`. Den MAC-type findes ikke på ESP32-P4: chippen har
ingen radio, så `soc_caps.h` sætter ikke `SOC_WIFI_SUPPORTED`, og hele
grenen i ESP-IDF's `mac_addr.c` ligger bag netop den. Kaldet svarer
`ESP_ERR_NOT_SUPPORTED`, og koden faldt tilbage til `"zscreen-ukendt"` —
**for hver enkelt enhed**. To skærme med samme klient-id slår hinanden
af, og en død MQTT-session lever halvandet minut, så det ville have set
ud som et dårligt netværk hos kunden.

Navnet og MAC-typen er nu `ZS_FLEET_NAVN_PREFIX` og `ZS_MAC_TYPE` pr.
board. ZDisplay bruger `ESP_MAC_BASE`, fabrikkens MAC fra eFuse, som
findes på alle chips. zScreen bliver på `ESP_MAC_WIFI_STA`: på ESP32-S3
er de to præcis de samme seks bytes, men en skærms navn på flåden er
ikke noget man ændrer på en slutning.

**Låsefilen kan ikke committes.** Den blev lagt i git for at binde de
transitive versioner, og det var forkert: komponentstyringen skriver
lokale komponenter ud med en absolut sti fra den maskine der byggede,
så et byg på en anden maskine — CI først — dør i selve
afhængighedsopslaget. I stedet er **hver** hentet komponent pinnet
præcist i vores egne `idf_component.yml`. Ti styks. Et byg fra bunden
med slettet låsefil og `managed_components` giver de samme versioner.

**Reglen hele opdelingen hviler på var ikke håndhævet.** `kerne.cmake`
lovede at `tools/check-headers.sh` sagde fra hvis der kom et
hardware-kald ned i motoren. Det gør den ikke; den tjekker kun
include-guards. Reglen står nu i `tools/check-byg.py` som
`FORBUDT_I_MOTOREN`: ingen `driver/`, ingen `soc/`, ingen `esp_lcd`,
ingen LVGL, intet `bsp`-navn og intet board-navn i `net/`, `app/`,
`util/` eller `storage/`.

**Vagten på kildelisten var for slap.** Den krævede kun at en fil blev
bygget af mindst ét produkt, så en ny fil i `net/` kunne stå i zScreens
egen liste alene og aldrig blive oversat til ZDisplay. Det er præcis det
hul `kerne.cmake` findes for at lukke. Nu skal alt under `firmware/kerne`
stå i `kerne.cmake`, undtagen de fire filer i `KUN_ZSCREEN`.

**Og to ting der kostede hukommelse og en forkert note:** den anden
billedbuffer blev aldrig tegnet i — 2 MB PSRAM for ingenting, se
`sdkconfig.defaults` — og min note om at Seeeds touch-reset trak i
radiochippens boot-ben var gal. Se afsnittet ovenfor.

Ikke lavet: en skærm der har været tændt. Alt herunder er stadig uprøvet
på hardware, og listen i afsnittet "Det der skal måles" står urørt.

### Det næste, i rækkefølge

**Hele vejen i mål står i `zdisplay-faseplan.md`**, delt i syv faser med
hvad der er målt, hvad der kun er læst, og hvad der ikke er afprøvet.
Herunder kun det korte.

1. **Tænd den.** Flash, se om panelet kommer op, og om touch rammer.
2. **Byg C6-firmwaren** af `esp_hosted` 2.13.0's slave og flash den over
   UART. Derefter: lad wifi stå i et døgn.
3. **Mål flashen** og lås partitionstabellen.
4. **Udgivelsen.** `.github/workflows/release.yml` bygger kun zScreen.
   Manglen er nu **håndhævet** og ikke kun skrevet ned: en skærm leder
   efter sit eget navn i udgivelsen, og `tools/check-udgivelse.py`
   kræver at release.yml lægger en fil op med det navn. ZDisplay står i
   `IKKE_UDGIVET_ENDNU` med en grund, og den linje skal fjernes med
   vilje. Et produkt **mere** kan ikke komme til at mangle i stilhed.
   Den skal bygge begge og lægge `zdisplay-<version>.bin` op ved siden
   af `zscreen-<version>.bin`. Firmwaren er klar til det — den vælger
   selv sin egen fil — men arbejdsgangen er med vilje ikke rørt endnu:
   en fejl der kan stoppe en zScreen-udgivelse skal laves når nogen
   sidder og ser på den.
   **Og zScreen skal uploades FØRST.** En skærm der kører v0.27.0 eller
   ældre tager den første `.bin` i udgivelsen, uanset hvad den heder.
5. **Skrifttyperne** skal genereres i større grad, og så skal fladen på
   800 x 1280 ses igennem af et menneske.
6. **Højttaleren**, når der er en skærm at høre den på.
7. **Én ESP-IDF til begge**, se beslutning 3.
