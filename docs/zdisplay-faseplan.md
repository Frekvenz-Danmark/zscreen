# ZDisplay — vejen i mål

Hvad der skal gøres, før en ZDisplay kan hænge på en kundes væg, og i
hvilken rækkefølge.

**Der står ikke et eneste tal her, som ikke er målt.** Hver fase har tre
afsnit: det der er **målt**, det der kun er **læst** i andres kode eller
papirer, og det der er **ikke afprøvet**. Står noget under "ikke
afprøvet", så ved vi det ikke — heller ikke selvom det lyder oplagt.

Der står heller ingen tidsestimater. Vi ved hvad der blokerer hvad, og
det er det vi kan sige.

Skrevet 14. oktober 2026, samme dag som enheden blev tændt første gang.

Faserne herunder er **ZDisplays**. De fem faser A til E i
`faseplan.md` er flådestyringens, og de er noget andet.

---

## Oversigt

| | Fase | Status | Blokeret af |
|---|---|---|---|
| 1 | Skærmen virker | **næsten** — mangler et menneske | — |
| 2 | Radioen holder | **spærret** | C6'erens firmware |
| 3 | Den viser rigtige tal | ikke begyndt | fase 2 |
| 4 | Den ser rigtig ud på 8 tommer | ikke begyndt | — (kan køre sideløbende) |
| 5 | Den kan passes på afstand | ikke begyndt | fase 2 |
| 6 | Den står på et rigtigt anlæg | ikke begyndt | fase 3 |
| 7 | Resten af hardwaren | ikke begyndt | — |

Fase 4 og 7 hænger ikke på de andre. Fase 2 er den der spærrer mest.

---

## Fase 1 — Skærmen virker

Enheden kører vores firmware og tegner vores flade.

### Målt 14. oktober 2026

Enheden er flashet og booter. Fra opstartsloggen:

```
App version:      0.27.0
esp_psram:        Found 32MB PSRAM device, Speed: 200MHz
spi_flash:        detected chip: winbond
lvgl_port:        skaerm klar, 800 x 1280, to buffere paa 187 KB
tilmeld:          tilmeldingssiden bruger 370 af 1224 px, QR 232 px
ui:               brugerfladen er klar
baglys:           baglys klar, 80 %
zdisplay:         intern 178 KB ledig, PSRAM 30.386 KB ledig
```

* **Chippen:** ESP32-P4 revision v1.3, MAC `e8:f6:0a:e3:f0:f9`, CPU 360 MHz
* **Flashen:** 32 MB, og hele partitionstabellen kan nås. Det krævede
  `CONFIG_SPI_FLASH_SUPPORT_WINBOND_CHIP=y` — uden den skar ESP-IDF
  flashen ned til 16 MB, og to tredjedele af `storage` ville have været
  uden for rækkevidde
* **Panelet:** JD9365, `LCD ID: 93 65 04`, 800 x 1280
* **Touch:** kommer op. Seeeds driver skriver `"startup_chip failed"`
  hver gang, også når det lykkes — beskeden printes før tjekket, og
  `5a,5a,5a,5a` er netop den værdi der betyder succes
* **Hukommelse:** 30 MB PSRAM fri efter at fladen er bygget
* **Fabriksflashen** er sikret: `~/esp/d1001-fabrik/`, 32 MB, SHA256 i
  `SHA256.txt`. Enheden kan sættes tilbage som den kom
* Begge firmwarer bygger og signeres med samme nøgle. ZDisplay
  1.904.640 bytes, zScreen 1.642.496. 1819 enhedstjek, 89 ende til ende,
  8 fuzz-regler, 11 vagter, og CI grøn på begge produkter

### Læst, ikke målt

* Fabrikkens egen partitionstabel havde **ingen OTA-plads**: nvs,
  phy_init, én app på 9 MB og 4 MB storage, altså 13 af 32 MB. Læst ud
  af den sikkerhedskopi vi tog. En enhed med Seeeds firmware kan derfor
  ikke opdateres over netværket

### Ikke afprøvet

1. **Hvad der står på skærmen.** Loggen siger at fladen er bygget og at
   baglyset lyser 80 procent. Ingen har set den. Det er det eneste der
   mangler i den her fase
2. **Om et tryk virker.** Touch-kredsen svarer, men ingen har trykket
3. **Om den starter af sig selv efter et strømsvigt.** Enheden har en
   blød tændknap og et batteri, og den tændes med et langt tryk på over
   3 sekunder. En energiskærm der kræver et tryk efter et strømsvigt er
   en fejl, og vi ved ikke hvad den gør

### Sådan ved vi at fase 1 er færdig

Et menneske har set de fire kort på skærmen, trykket på tandhjulet og
set indstillingerne åbne, og taget strømmen og sat den til igen og set
skærmen komme op af sig selv.

---

## Fase 2 — Radioen holder

Den her fase er grunden til at resten venter.

### Målt 14. oktober 2026

Wifi kommer op. SDIO-forbindelsen til radiochippen virker:

```
sdio_wrapper:  SDIO master: Slot 1, Data-Lines: 4-bit Freq 40000 KHz
H_SDIO_DRV:    Card init success
transport:     Identified slave [esp32c6], capabilities 0xd (WLAN)
wifi:          wifi klar
RPC_WRAP:      ESP Event: wifi station started
```

Og så den besked der spærrer fasen:

```
W transport: Version mismatch: Host [2.13.0] > Co-proc [2.3.0]
             ==> Upgrade co-proc to avoid RPC timeouts
W transport: Co-processor fw did not report transport size,
             and may not be compatible with host.
W transport: Please update the co-processor firmware.
```

C6'eren kom fra fabrikken med `esp_hosted` **2.3.0**. Vores side er
**2.13.0**. ESP-IDF siger selv at den skal opgraderes.

**Og C6-firmwaren kan bygges her.** Målt: `network_adapter.bin`,
**1.176.496 bytes**, bygget af `esp_hosted` 2.13.0 — altså præcis samme
udgave som vores egen side — med ESP-IDF 5.4.2 og målet `esp32c6`.

### Læst, ikke målt

* Et andet ESP32-P4-board har målt at en 2.3.0-slave **mister wifi efter
  cirka fire minutter**, og at der skal 2.9.4 eller nyere til i begge
  ender, før SDIO-forbindelsen genoprettes af sig selv. Vi har ikke selv
  målt det på vores enhed
* Espressifs egen vejledning siger at C6'eren kan opdateres **fra
  P4'eren over SDIO**, uden kabel og uden ekstra ledninger. Der ligger
  et eksempel, `host_performs_slave_ota`, i `esp_hosted`-komponenten
* Der går også en UART fra P4'eren til C6'eren (P4 GPIO 48 og 47, og
  C6'ens boot-ben på GPIO 12). Det er den anden vej, hvis SDIO-vejen
  ikke kan bruges

### Ikke afprøvet

1. **Om 2.3.0-slaven overhovedet tager imod en OTA.** Vejen findes i
   2.13.0. Om den gamle firmware i C6'eren forstår den, ved vi ikke
2. **Hvor lang tid wifi holder** — hverken før eller efter opgraderingen
3. **Hvad der sker hvis en OTA af C6'eren går i stykker midtvejs.**
   Enheden har så en radio der ikke svarer, og vejen tilbage er UART'en

### Rækkefølge

1. Skub `network_adapter.bin` til C6'eren over SDIO
2. Se at `Version mismatch` er væk i opstartsloggen
3. **Lad den stå et døgn på kundens slags net** og se om den taber
   forbindelsen. Det er det egentlige mål for fasen — fejlen ses ikke på
   fem minutter

### Sådan ved vi at fase 2 er færdig

Opstartsloggen melder samme version i begge ender, og enheden har haft
wifi i 24 timer i træk uden at miste det.

---

## Fase 3 — Den viser rigtige tal

Modbus fra enheden til en inverter, og de fire tal på skærmen.

### Målt

Intet. Hele datavejen er prøvet 89 gange, men **på en Mac** mod
simulatoren — ikke fra ZDisplay.

### Læst, ikke målt

Motoren er den samme kode som zScreens, og den er den samme binære vej:
`net/zs_modbus_tcp.c`, `zs_sunspec.c` og `zs_fronius.c` ligger i
`firmware/kerne/` og bygges ind i begge produkter fra samme liste. Der
er derfor god grund til at tro at den virker. **Men det er en slutning,
ikke en måling**, og de to største fejl i projektet blev netop fundet
fordi noget var prøvet et andet sted end der hvor det skulle køre.

### Ikke afprøvet

1. Om enheden kan finde en inverter på nettet. Søgningen åbner tolv
   sockets på én gang, og det er netop den slags en marginal radio
   falder igennem på — se fase 2
2. Om de fire tal står rigtigt på skærmen
3. Om nettets fortegn er rigtigt, og om forbruget stadig viser noget om
   natten

### Sådan ved vi at fase 3 er færdig

ZDisplay står på samme net som `tools/fronius-sim`, finder den selv, og
viser fire tal der passer med det simulatoren sender. Derefter det
samme mod en rigtig inverter, hvilket er fase 6.

---

## Fase 4 — Den ser rigtig ud på 8 tommer

Den her fase hænger ikke på nogen af de andre og kan laves når som helst.

### Målt

Fladen regner rigtigt på den nye størrelse — den er bygget til det fra
starten, og `tests/host/test_layout_800.c` har prøvet 800 x 1280 siden
den blev skrevet. Men kortene bliver meget større:

| | 480 x 480 (zScreen) | 800 x 1280 (ZDisplay) |
|---|---|---|
| Siden | 408 px høj | **1208** px høj |
| Indhold | 456 px bredt | **776** px bredt |
| Ét kort | 222 x 186 | **382 x 586** |

Kortet er altså **3,1 gange højere** og 1,7 gange bredere. Skrifttyperne
er de samme som på 4 tommer: 13, 16, 20, 28 og 64 px, plus 24 til
tastaturet og ikoner i 20 og 28.

Tallet i et kort står i 64 px i et kort der er 586 px højt.

### Ikke afprøvet

1. **Hvordan det ser ud.** Tallene er for små i forhold til kortene, det
   siger regnestykket — men hvor store de skal være, og om der skal
   være en række kort mere når der er 1208 px at give af, er en
   designbeslutning. Ikke noget der kan regnes
2. Om der er plads til mere end de fire tal. Der er 800 px i bredden

### Sådan ved vi at fase 4 er færdig

Et menneske har set fladen på enheden og sagt god for den, og
`check-maal`, `check-colors` og `check-flow` er stadig grønne — de
tjekker at ingen af skærmens mål bliver skrevet ind i koden, at
kontrasten holder, og at ingen side er en blindgyde.

---

## Fase 5 — Den kan passes på afstand

Opdatering over netværket og indmelding på flåden.

### Målt

* **Udgivelsen laver ikke en ZDisplay-fil.** `release.yml` bygger kun
  zScreen. Manglen er håndhævet og ikke kun skrevet ned:
  `tools/check-udgivelse.py` kræver at udgivelsen lægger en fil op med
  det navn hver skærm leder efter, og ZDisplay står i
  `IKKE_UDGIVET_ENDNU` med en grund
* **Versionsnummeret virker.** Enheden melder `0.27.0`, ikke
  `v0.27.0-dirty`. Begge produkter læser samme `firmware/version.txt`,
  og bygningen falder hvis det ikke er tre tal med punktummer
* **Enheden har intet certifikat:** `fleet: intet certifikat paa
  enheden, floedestyring er ikke sat op`
* Navnet på flåden bliver `zdisplay-` plus MAC'en, og MAC-typen er
  `ESP_MAC_BASE`, fordi `ESP_MAC_WIFI_STA` ikke findes på en chip uden
  radio. Uden den rettelse ville **hver** enhed have heddet
  `zscreen-ukendt`

### Ikke afprøvet

1. Om en ZDisplay kan hente og installere en ny version af sig selv
2. Om serveren kan kende forskel på de to produkter
3. Om tilmeldingen med QR-kode virker på 800 x 1280 i praksis

### Det der skal besluttes, ikke måles

**C6-firmwaren skal med inde i P4-programmet**, så den arver
underskriften. Vores ordning signerer P4-programmet; C6-billedet er
uden for den. Planen er at lægge de 1.176.496 bytes ind i vores eget
program og lade P4'eren flashe C6'eren når versionerne ikke passer. Det
koster plads, og der er 4 MB pr. app-plads — men det giver **én** fil at
udgive og **én** underskrift at passe på.

Og når udgivelsen skal bygge begge: **zScreen skal lægges op først.** En
skærm der kører v0.27.0 eller ældre tager den første `.bin` den finder i
udgivelsen, uanset hvad den heder.

### Sådan ved vi at fase 5 er færdig

En udgivelse indeholder begge filer, en ZDisplay har hentet og
installeret en ny version af sig selv over wifi, og den står på flåden
med sit eget navn og sender målinger.

---

## Fase 6 — Den står på et rigtigt anlæg

### Målt

Intet, og det gælder **begge** produkter. `faseplan.md` siger det
rent ud: skærmen har aldrig stået på et rigtigt anlæg.
`test-plan.md` afsnit 3 har 13 punkter der kun kan gøres der, og ingen
af dem er gjort.

Det er et større hul end noget andet i den her plan, og det bliver ikke
mindre af at der nu er to produkter.

### Sådan ved vi at fase 6 er færdig

De 13 punkter i `test-plan.md` afsnit 3 er gjort, på et anlæg, med en
rigtig Fronius.

---

## Fase 7 — Resten af hardwaren

Tre ting enheden kan, som firmwaren ikke bruger endnu. Ingen af dem
blokerer noget.

### Højttaleren

**Målt:** ingen lyd. `zs_lyd_d1001.c` svarer `false` fra
`zs_lyd_init()`, og fladen er bygget til det — en skærm uden lyd
pulserer stadig i baglyset.

**Læst:** 2 W på 8 ohm gennem en ES8311-kodek og en NS4150B-forstærker,
og strømmen til forstærkeren tændes af Seeeds `bsp_power_init()`.

**Ikke afprøvet:** alt. Hvor høj en tone skal være i en stue, og om
kodekken knalder når den tændes, er noget man **hører**.

### Batteriet

**Læst:** 2500 mAh, og Seeeds BSP har `bsp_battery_percent_read()` og
`bsp_battery_charge_status_read()`.

**Ikke afprøvet:** hvor længe den holder uden stik, og hvad skærmen
trækker. Det interessante er at en skærm med batteri kan **vise at
strømmen er gået**, i stedet for at gå i sort sammen med huset.

### Uret

**Læst:** en PCF8563 med eget batteri, som Seeeds BSP sætter op.

**Ikke afprøvet:** om den overlever et strømsvigt, og om den stiller
uret rigtigt når nettet er væk. Dag og time skal komme fra samme ur —
se `zs_tid_dagnr`.

---

## To ting i loggen som vi har besluttet at leve med

Begge kommer fra Seeeds `bsp_power_init()`, altså kode vi bruger
uændret med vilje:

```
E sdmmc_common: sdmmc_init_ocr: send_op_cond (1) returned 0x107
E vfs_fat_sdmmc: sdmmc_card_init failed (0x107)
```

Der sidder ikke noget microSD-kort i. Deres opstart monterer et kort
uanset, og vi bruger det ikke. At fjerne de to linjer ville kræve en
rettelse i submodulet, og den ville være væk næste gang det opdateres.

```
W ldo: The voltage value 0 is out of the recommended range [500, 2700]
W i2c.master: Please check pull-up resistances whether be connected properly
E gpio: gpio_install_isr_service(503): GPIO isr service already installed
```

Samme sted, samme grund. De står skrevet her, så den næste der læser en
opstartslog ved at de er kendte og ikke nye.
