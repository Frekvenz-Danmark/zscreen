# Ændringer i zScreen

Hver gang der er lavet noget, står det her med dato, klokkeslæt, hvad der
blev lavet, og hvorfor det løser noget. Nyeste øverst.

Formatet er med vilje kedeligt: en overskrift med tidspunkt, en kort
forklaring på almindeligt dansk, og hvor det er relevant en note om
hvilken fejl der blev fanget og hvordan.

## 2026-08-25 16:15 · Firmware, designsystem og hovedskærm

**Hvad der blev lavet**

Firmwaren bygger nu og kan flashes. 606 KB, 81 % af partitionen ledig.

| Fil | Hvad den laver |
|---|---|
| `firmware/CMakeLists.txt` | Byg mod Seeed-SDK'et som pinnet submodul |
| `firmware/partitions.csv` | To app-pladser fra starten, så OTA kan tilføjes senere |
| `firmware/main/ui/zs_theme.c` | Farver, skrifttyper, mål, knapper og kort |
| `firmware/main/ui/zs_display.c` | Lysstyrke med PWM og natdæmpning |
| `firmware/main/ui/widgets/zs_tile.c` | Ét af de fire kort |
| `firmware/main/ui/widgets/zs_statusbar.c` | Statuslinjen foroven |
| `firmware/main/ui/zs_screen_home.c` | Hovedskærmen |
| `tools/build-assets.sh` | Skrifttyper, ikoner og logoer til LVGL |
| `tools/png2lvgl.py` | PNG til LVGL uden at installere noget |
| `tools/icons.py` | Ikonlisten, ét sted |
| `tools/check-headers.sh` | Værn mod navnesammenstød |

**Designet**

Brandfarverne fra `brand/`-mappen, i mørk udgave: baggrund #0E2A29,
kort #16403E, accent #FBAC18. Skrifttype Funnel Sans, samme som
frekvenz.nu og Zbox-webfladen. Ikoner fra Lucide, samme pakke som
Zbox-webfladen, så en advarselstrekant ser ens ud begge steder.

Hele layoutet er regnet ud, ikke skudt efter:

```
Vandret:  12 + 222 + 12 + 222 + 12 = 480
Lodret:   44 + 12 + 200 + 12 + 200 + 12 = 480
```

Inde i hvert kort: overskrift på y=0, stort tal på y=60, undertekst på
y=154, som slutter præcis på kortets indvendige underkant, 172.

Enheden "kW" står på samme grundlinje som tallet. De to skrifttyper har
forskellig størrelse, så forskellen regnes ud af LVGL's egne mål
(54-9=45 mod 31-5=26, altså 19 pixels) i stedet for at blive skrevet
ind som et tal der holder op med at passe.

Alt man kan trykke på er mindst 44x44 pixels, som er cirka 8 mm på
denne skærm. Tandhjulet er derfor en 44x44 knap med et 20 px ikon i.

**Fejl der blev fanget**

1. **Navnesammenstød der slog en hel header fra.**
   `zs_config.h` havde `#define ZS_STATUSBAR_H 44` som statuslinjens
   højde. Det er præcis samme navn som include-guarden i
   `zs_statusbar.h`. Fordi `zs_config.h` blev læst først, troede
   præprocessoren at headeren allerede var med, og sprang hele filen
   over. Ingen advarsel. Fejlen dukkede op et helt andet sted som
   "unknown type name".
   Rettet ved roden: målene står nu kun ét sted, i `zs_theme.h`, og
   ingen konstant slutter på `_H`. `tools/check-headers.sh` håndhæver
   det og køres af testene, så det ikke kan glemmes.

2. **Forkert funktionstype i afbrydelsesstien.**
   `bsp_lcd_set_cb()` forventer `bool (*)(void *)`, men Seeeds eksempel
   giver den `bool (*)(void)`. Det virker i praksis på Xtensa, men er
   udefineret opførsel, og det er ikke noget man vil have stående i en
   skærm der skal køre i årevis. Rettet.

3. **Ikonlisten stod to steder.**
   Én liste bestemte hvilke ikoner der kom med i skrifttypen, en anden
   hvilke navne C-koden kendte. Tilføjede man et ikon ét sted og ikke
   det andet, blev feltet tomt på skærmen uden en eneste fejl i loggen.
   Nu står listen i `tools/icons.py` og bruges begge steder.

4. **IRAM-mærke uden grund.**
   Touch-læsningen var mærket `IRAM_ATTR`, men kaldes fra LVGL's egen
   opgave og læser I2C, som alligevel ikke ligger i IRAM. Den optog
   bare plads i den knappe interne hukommelse.

5. **Seeeds kode oversætter ikke rent med ESP-IDF v5.1.7.**
   Deres SDK er skrevet til en ældre udgave hvor loggens tidsstempel var
   en `int`. ESP-IDF slår `-Werror=all` til for alt der bygges, også
   kode vi ikke ejer, så byggeriet stoppede.
   Løst ved at slå netop de advarsler fra for deres komponenter alene.
   Vores egen kode i `main/` har stadig `-Werror=all`, og bygger med
   nul advarsler.
   Bemærk: deres `gt1151`-driver har en rigtig fejl hvor x og y kan
   bruges uinitialiserede. Vi bruger ikke den driver på D1.

**Sådan prøver du det**

```
source tools/env.sh
cd firmware && idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## 2026-08-25 16:43 · Hele flowet: wifi, opsætning, indstillinger og detaljer

**Hvad der blev lavet**

Skærmen kan nu sættes op fra ende til anden på sig selv. 1,17 MB
firmware, 63 % af partitionen ledig, nul advarsler i vores egen kode.

| Fil | Hvad den laver |
|---|---|
| `firmware/main/storage/zs_nvs.c` | Gemte indstillinger |
| `firmware/main/net/zs_wifi.c` | Wifi: søg, forbind, fejlbeskeder på dansk |
| `firmware/main/net/zs_discovery.c` | Finder inverteren på netværket selv |
| `firmware/main/app/zs_app.c` | Den ene opgave der styrer det hele |
| `firmware/main/ui/zs_ui.c` | Skifter mellem skærme, tager LVGL-låsen |
| `firmware/main/ui/zs_screen_setup.c` | De seks opsætningstrin |
| `firmware/main/ui/zs_screen_settings.c` | Indstillinger og Detaljer |
| `firmware/main/ui/widgets/zs_keyboard.c` | Dansk touchtastatur med æ, ø og å |
| `docs/` | Registerkort, designsystem, hardware, testplan |

**Hvorfor det virker**

Opsætningen: velkomst, vælg netværk, kodeord, forbinder, søger efter
inverter, vælg inverter. Ingen telefon, ingen computer, ingen QR-kode.

Netværksscanningen prøver 12 adresser ad gangen med et fjerdedels
sekunds tålmodighed, så hele undernettet er gennemgået på under ti
sekunder. En ad gangen ville tage over et minut.

Arbejdsdelingen er stram: én opgave laver alt det der tager tid, og
brugerfladen tegner. Trykker man på en knap mens en wifi-søgning kører,
lægges beskeden i en kø og skærmen kører videre. Alle funktioner i
`zs_ui` tager selv LVGL-låsen, så ingen kalder skal huske det.

**Fejl der blev fanget**

1. **Skrøbelige opslag på børn efter indeks.**
   Seks steder blev en etiket hentet med `lv_obj_get_child(row, 1)`.
   Rækkefølgen af børn afhænger af om der er et ikon og en værdi, så et
   fast indeks peger på noget forskelligt fra række til række.
   Tilføjede man en dag noget nyt til rækken, ville alle de indekser
   stille og roligt begynde at pege forkert, uden en eneste advarsel.
   `zs_row_create` afleverer nu sine dele i en struct.

2. **Lysstyrke-skyderen ville kæmpe mod fingeren.**
   Indstillingssiden blev fyldt fem gange i sekundet, også mens
   brugeren trak i skyderen, så den ville hoppe tilbage. Nu fyldes den
   én gang, når man kommer ind på siden.

3. **Detaljesiden blev bygget helt om fem gange i sekundet.**
   Tredive etiketter slettet og lavet igen, hver 200 ms. Det ville både
   flimre og spilde tid på noget ingen når at læse. Nu én gang i
   sekundet.

4. **Fire steder hvor en tekst kunne løbe over sin buffer.**
   Fanget af oversætteren, fordi vores egen kode bygger med alle
   advarsler som fejl. Modelnavn plus fabrikat er 67 tegn i en buffer
   på 64. Rettet med præcision i formatet (`%.32s`), som både er en
   tydelig grænse for den der læser koden og bevislig for oversætteren.

5. **`%u` på en `uint32_t`.**
   På ESP32 er `uint32_t` en `unsigned long`, ikke en `unsigned int`.
   Fem steder på Detaljer-siden. Rettet med eksplicit cast.

6. **Redundans:** `make_column` stod to steder, og `zs_app_get_settings`
   var både ubrugt og trådusikker. Begge fjernet.

**Gennemgået for evige løkker**

Der er tre løkker uden fast øvre grænse i hele koden. Alle tre er
opgaveløkker der blokerer på noget indeni:

- `main.c`: venter 10 sekunder ad gangen, har ikke andet at lave
- `lv_port.c`: LVGL's egen tegneløkke, fra Seeeds SDK
- `zs_app.c`: venter på beskedkøen, højst 200 ms ad gangen

Alt andet er tælleløkker med en fast grænse. Model-vandringen stopper
efter 64 skridt uanset hvad, netværksscanningen efter 254 adresser, og
genforsøg fordobler ventetiden op til et minut i stedet for at hamre
løs.

**Sådan prøver du det**

```
./tests/host/run.sh                                218 tests
cd tools/fronius-sim && sudo python3 serve.py      simuleret Fronius
cd firmware && idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`docs/test-plan.md` har hele tjeklisten.

## 2026-08-25 20:37 · Side 2, demo, tastaturfejl og fejljagt

Side 2 med energiflow, swipe og prikker. Demo-tilstand. Fem fejl
rettet. Ny E2E-test af hele datavejen: 24 tjek.

**Fronius efterprøvet mod produktionskode**

Elmålerens fortegn stod som ikke efterprøvet. Det er det nu, mod evcc's
template til netop Fronius GEN24, som kører på tusindvis af anlæg:

- Batteri: `-160:3:DCW + 160:4:DCW`, altså aflad minus lad, positiv =
  aflader. Samme fortegn som vores
- Elmåler: `20x:W` læses råt uden negering, og evcc's grid-måler er
  positiv ved køb. Vores standard er rigtigt
- Målerens Modbus-enhed er 200, flere målere får 201, 202. Vores
  søgerækkefølge passer
- PV er `160:1:DCW + 160:2:DCW`, batteri er kanal 3 og 4. Vores
  navnebaserede løsning er en generalisering af det samme

evcc PR 18386 om Fronius handler om at skrive til model 124. Vi
skriver ikke, så den rører os ikke.

**Fejl fundet og rettet**

1. Tastaturet skiftede tast under fingeren. Tre årsager, alle bekræftet
   i LVGL's kilde: `CLICK_TRIG` manglede, så tasten blev sendt når
   fingeren ramte og igen for hver tast den gled over. `NO_REPEAT`
   manglede, så en holdt tast gentog sig. Og layoutskiftet skete inde
   fra tryk-håndteringen, hvor `lv_btnmatrix_set_map()` bygger gitteret
   om mens LVGL står midt i trykket. Nu sendes tasten først når fingeren
   slippes, og layoutskift udskydes med `lv_async_call`.

2. En afbrudt inverter-søgning rev brugeren tilbage. Trykkede man
   tilbage under søgningen, viste den alligevel resultatet et halvt
   sekund senere og skiftede side.

3. Prikkernes fingerflader overlappede: 207..251 og 228..272, så et tryk
   til højre for første prik ramte den anden. Afstanden er nu 30 px og
   fladen 38, altså 201..239 og 241..279.

4. Uinitialiserede felter i netværksscanningen. `probe_batch` havde to
   betingelser i samme løkke; stoppede den anden tidligt, blev resten af
   `fds[]` og `alive[]` læst uden at være skrevet. Kan ikke ske i dag,
   men ville ske hvis nogen ændrede den ene grænse.

5. `zs_fr_t` på stakken hvert sekund i demoens detaljeside. 850 bytes af
   opgavens 8 KB. Nu static.

**Ny E2E-test**

`tests/e2e/run.py` starter en simuleret Fronius, lader firmwarens egen
kode tale med den over en rigtig TCP-forbindelse, og sammenligner de
fire tal med det simulatoren siger den har. Alle seks anlægstyper, base
40000 og 40001, og fire fejltilstande: lukket port, åben port uden
Modbus, rent skrald som svar, og en inverter der forsvinder midt i.

Det fanger det enhedstestene ikke kan: at to dele hver for sig er
rigtige men uenige om hvad de sender til hinanden. Præcis sådan fejlen
med elmålerens modelnumre kom igennem.

`tests/run-all.sh` kører det hele: 22 guards, 5 skrifttyper, 218
enhedstest, 24 E2E.

---

## 2026-08-25 15:43 · Datalaget står færdigt og er testet

**Hvad der blev lavet**

Hele vejen fra inverterens registre til de fire tal skærmen skal vise.
Fem nye stykker kode, alt sammen almindelig C som både kan køre på
ESP32'en og oversættes på en Mac:

| Fil | Hvad den laver |
|---|---|
| `firmware/main/net/zs_modbus_tcp.c` | Modbus TCP-klient. Kun funktionskode 3, altså kun læsning |
| `firmware/main/net/zs_sunspec.c` | Vandrer SunSpec-modelkæden og afkoder værdier |
| `firmware/main/net/zs_fronius.c` | Laver registre om til sol, forbrug, batteri og net |
| `firmware/main/app/zs_format.c` | Tal på dansk: komma, W under 1000, kW derover |
| `tools/fronius-sim/` | Simuleret Fronius Gen24, så alt kan prøves uden anlæg |
| `tools/zs-probe/` | Kommandolinjeværktøj der kører firmwarens egen kode |

**Hvorfor det virker**

Alle registeradresser er slået op i SunSpecs officielle modelfiler
(smdx\_00001, 00103, 00113, 00120, 00124, 00160, 00203, 00213) den
25. august 2026, ikke skrevet efter hukommelsen. Simulatoren er bygget
efter de samme filer, og de to er enige om hvor tingene ligger:
C-testen forventer at model 160 starter på adresse 40178, og
simulatoren lægger den samme sted, uden at de har set hinandens kode.

217 enhedstest kører på en Mac uden hardware, med address-sanitizer og
undefined-behavior-sanitizer slået til og alle advarsler som fejl.

**Fejl der blev fanget undervejs**

1. **N i model 160 ligger på offset 6, ikke 5.**
   Feltet lige før, Evt, er en bitfield32 og fylder både offset 4 og 5.
   Læser man antallet af DC-kanaler på offset 5, får man den nederste
   halvdel af Evt, som næsten altid er nul. Så bliver svaret "nul
   kanaler", og hele genkendelsen af sol- og batterikanaler forsvinder
   lydløst uden en eneste fejl i loggen.
   Verificeret mod smdx\_00160.xml.
   **Den samme fejl står i Zbox Raspberry i dag** (`app/modbus_controller.py`,
   `M160_N = 5`). Vi har ikke rørt Zbox, men den bør rettes der.

2. **Elmålerens modeller blev læst som flydende tal.**
   Testen for "bruger denne model flydende tal" var `id >= 111`, hvilket
   er rigtigt for inverteren og forkert for alle fire målermodeller,
   fordi 201 til 204 også er større end 111. Skærmen viste 0 W på NETTET
   mens måleren meldte 5 kW eksport, og FORBRUG blev dermed også forkert.
   Ingen fejlmeddelelse, bare et forkert tal.
   Fanget af `zs-probe` mod simulatoren, ikke af enhedstestene, fordi
   testene var skrevet med den samme forkerte antagelse.
   Nu står de otte modelnumre skrevet ud, så der ikke er en regning at
   regne forkert. Låst fast med 18 tests.

3. **Reserveløsningen for unavngivne kanaler talte forfra.**
   Fronius' manual siger at batteriets to kanaler lægges i enden, efter
   solstrengene. Koden gik ud fra fire kanaler og læste kanal 3 og 4 som
   batteri. På et anlæg med kun én solstreng har batteriet kanal 2 og 3,
   og så blev solstreng nummer to læst som ladeside. Nu tælles der
   bagfra.

4. **Model med længde 0 kunne få vandringen til at gå i ring.**
   Adressen ville stå stille og løkken snurre forgæves. Nu stopper den
   og markerer kortet som ufuldstændigt.

5. **Afrunding af halve værdier.**
   `printf("%.1f")` runder halve værdier til nærmeste lige ciffer, så
   4250 W blev til "4,2 kW" mens 4350 W blev til "4,4 kW". Samme måling,
   to forskellige regler, og tal der ikke stemmer med SolarWeb. Nu
   rundes der selv, altid væk fra nul.

6. **Simulatorens batteri ændrede aldrig ladetilstand.**
   En indrykningsfejl gjorde at SoC kun blev integreret i den gren hvor
   der ikke var noget batteri. Fanget ved at SoC stod på 48,0 % et helt
   døgn igennem.

**Sådan prøver du det**

```
./tests/host/run.sh                                    217 tests, ingen hardware
cd tools/fronius-sim && python3 serve.py --port 5020   simuleret anlæg
./tools/zs-probe/zs-probe 127.0.0.1 5020               se hvad skærmen ville vise
```

Profilerne `nobattery`, `nometer`, `nolabels`, `float` og `onestring`
dækker anlæg hvor noget mangler. Alle fem er kørt igennem.

---

## 2026-08-25 15:43 · Værktøjskæden sat op

**Hvad der blev lavet**

`tools/setup-toolchain.sh` henter ESP-IDF v5.1.7 og alt hvad der skal
til. `tools/env.sh` sætter miljøet op i en ny terminal.

**Fejl der blev fanget**

**Din Homebrew-Python er i stykker.** Både `python@3.12` og
`python@3.14` har et `pyexpat` der loader systemets `libexpat` i stedet
for Homebrews, og derfor mangler symbolet
`_XML_SetAllocTrackerActivationThreshold`. Alt Python-værktøj der rører
XML fejler, ikke kun ESP-IDF. Fejlen ser ud som
`ensurepip returned non-zero exit status 1`, hvilket ikke afslører noget.

Rettes med:

```
brew reinstall expat python@3.12
```

Indtil da bruger vi Apples egen `/usr/bin/python3` (3.9.6), som virker
fint og er understøttet af ESP-IDF. Scriptet prøver hver kandidat af i
praksis ved at bygge et rigtigt virtuelt miljø med pip i, i stedet for
bare at se på versionsnummeret, og lægger vinderen i en shim-mappe
forrest i PATH.

ESP-IDF leverer ikke `cmake` og `ninja` på macOS. De installeres nu
automatisk via Homebrew.
