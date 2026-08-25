# Ændringer

Nyeste øverst. Dato, hvad der blev lavet, og hvilke fejl der blev
fanget undervejs.

---

## 2026-08-25 20:37

Side 2 på hovedskærmen: energiflow med linjer, pile og farve efter
retning. Swipe mellem siderne, prikker nederst viser hvor man er. Side 2
får præcis samme data som side 1, så de to ikke kan sige hver sit om
samme måling.

Demo-tilstand under "Kom i gang". Hovedskærmen med opdigtede tal fra
samme model som simulatoren, et døgn på tre minutter. Gemmes aldrig, så
en enhed hos en kunde ikke kan starte op i demo. `ZS_DEMO_ENABLED` i
`zs_config.h` fjerner den helt fra bygget.

Elmålerens fortegn stod som ikke efterprøvet. Det er det nu, mod evcc's
template til Fronius GEN24, som kører på tusindvis af anlæg. Batteri er
`-160:3:DCW + 160:4:DCW`, altså aflad minus lad. Elmåleren læses råt som
`20x:W` uden negering, positiv ved køb. Målerens Modbus-enhed er 200,
flere målere får 201 og 202. Alt sammen som vores. evcc PR 18386 om
Fronius handler om at skrive til model 124, og vi skriver ikke.

Fem fejl rettet:

Tastaturet skiftede tast under fingeren. Tre årsager, alle bekræftet i
LVGL's kilde. `CLICK_TRIG` manglede, så tasten blev sendt når fingeren
ramte og igen for hver tast den gled over. `NO_REPEAT` manglede, så en
holdt tast gentog sig. Og layoutskiftet skete inde fra tryk-håndteringen,
hvor `lv_btnmatrix_set_map()` bygger gitteret om mens LVGL står midt i
trykket. Nu sendes tasten først når fingeren slippes, og layoutskift
udskydes med `lv_async_call`.

En afbrudt inverter-søgning rev brugeren tilbage. Trykkede man tilbage
under søgningen, viste den alligevel resultatet et halvt sekund senere.

Prikkernes fingerflader overlappede, 207..251 og 228..272, så et tryk til
højre for første prik ramte den anden. Afstanden er nu 30 px og fladen
38.

`probe_batch` i netværksscanningen havde to betingelser i samme løkke.
Stoppede den anden tidligt, blev resten af `fds[]` og `alive[]` læst uden
at være skrevet. Kan ikke ske i dag, men ville ske hvis nogen ændrede den
ene grænse.

`zs_fr_t` lå på stakken hvert sekund i demoens detaljeside, 850 bytes af
opgavens 8 KB. Nu static.

Ny E2E-test i `tests/e2e/run.py`. Den starter en simuleret Fronius, lader
firmwarens egen kode tale med den over en rigtig TCP-forbindelse, og
sammenligner de fire tal med det simulatoren siger den har. Alle seks
anlægstyper, base 40000 og 40001, og fire fejltilstande: lukket port,
åben port uden Modbus, rent skrald som svar, og en inverter der
forsvinder midt i. 24 tjek.

`tests/run-all.sh` kører det hele: headere, tegnsæt, 218 enhedstest og
24 E2E.

---

## 2026-08-25 17:20

Første flashning på hardware. Enheden stod stille lige efter at baglyset
blev tændt, uden en eneste fejl i loggen.

`lv_port_sem_take()` brugte en almindelig FreeRTOS-mutex, som ikke kan
tages to gange af samme opgave. `app_main` tog låsen og kaldte
`zs_ui_init()`, som til sidst kaldte `zs_ui_show()`, der tager den igen.
Rettet to steder: `zs_ui_init` bruger nu en låsefri udgave, og mutexen er
gjort rekursiv så det ikke kan låse skærmen fast igen.

`tools/check-text.py` læser den faktiske tegndækning ud af de byggede
skrifttypefiler og fanger tegn vi skriver som skrifttypen ikke har. LVGL
tegner ingenting i det tilfælde, uden advarsel hverken ved byg eller
kørsel. Den fandt to: midterprik og punkttegn, som begge stod som huller.

Skærmen huskede ikke wifi efter en genstart hvis der ikke også var valgt
en inverter. Nu gemmes netværket så snart det virker, og "Kom i gang"
bruger det hvis det er der.

---

## 2026-08-25 16:43

Hele opsætningsflowet: vælg netværk, kodeord på dansk touchtastatur, find
inverteren automatisk på netværket, vælg den. Ingen telefon og ingen
computer. Plus Indstillinger og Detaljer om anlægget.

Netværksscanningen prøver 12 adresser ad gangen med et fjerdedels sekunds
tålmodighed, så hele undernettet er gennemgået på under ti sekunder. En
ad gangen ville tage over et minut.

Arbejdsdelingen er stram: én opgave laver alt det der tager tid,
brugerfladen tegner. Trykker man på en knap mens en wifi-søgning kører,
lægges beskeden i en kø og skærmen kører videre. Alle funktioner i
`zs_ui` tager selv LVGL-låsen, så ingen kalder skal huske det.

Fejl fanget: seks steder blev en etiket hentet med
`lv_obj_get_child(row, 1)`, hvor rækkefølgen af børn afhænger af om der
er et ikon og en værdi. `zs_row_create` afleverer nu sine dele i en
struct. Indstillingssiden blev fyldt fem gange i sekundet, også mens
brugeren trak i lysstyrke-skyderen, så den ville hoppe tilbage under
fingeren. Detaljesiden blev bygget helt om lige så tit. Fire steder kunne
en tekst løbe over sin buffer, fanget af oversætteren fordi vores egen
kode bygger med alle advarsler som fejl. Og `%u` blev brugt på en
`uint32_t`, som på ESP32 er en `unsigned long`.

---

## 2026-08-25 16:15

Firmwaren bygger og kan flashes. Designsystem, hovedskærm med fire
kasser, lysstyrke med natdæmpning, og skrifttyper, ikoner og logoer
bygget fra Funnel Sans, Lucide og brand-mappen.

Hele layoutet er regnet ud: vandret 12 + 222 + 12 + 222 + 12 = 480,
lodret 44 + 12 + 186 + 12 + 186 + 12 + 28 = 480. Enheden "kW" står på
samme grundlinje som tallet, regnet ud af LVGL's egne skriftmål i stedet
for at blive skrevet ind som et tal der holder op med at passe. Alt man
kan trykke på er mindst 44x44, som er cirka 8 mm på denne skærm.

Fejl fanget: `zs_config.h` havde `#define ZS_STATUSBAR_H 44` som
statuslinjens højde, præcis samme navn som include-guarden i
`zs_statusbar.h`. Fordi `zs_config.h` blev læst først, sprang
præprocessoren hele headeren over. Ingen advarsel, kun en fejl et helt
andet sted om en type der ikke fandtes. Målene står nu kun ét sted, og
`tools/check-headers.sh` håndhæver at ingen konstant slutter på `_H`.

`bsp_lcd_set_cb()` forventer `bool (*)(void *)` men fik `bool (*)(void)`.
Det virker i praksis på Xtensa, men er udefineret opførsel i
afbrydelsesstien. Ikonlisten stod to steder, så et ikon kunne findes i
koden uden at tegningen fulgte med. Og touch-læsningen var mærket
`IRAM_ATTR` uden grund.

Seeeds SDK oversætter ikke rent med ESP-IDF v5.1.7, fordi det er skrevet
til en ældre udgave hvor loggens tidsstempel var en `int`. Løst ved at
slå netop de advarsler fra for deres komponenter alene. Vores egen kode
bygger med nul advarsler.

---

## 2026-08-25 15:43

Datalaget: Modbus TCP-klient med kun funktionskode 3, SunSpec-vandring og
afkodning, og udregningen af sol, forbrug, batteri og net. Plus en
simuleret Fronius og et kommandolinjeværktøj der kører firmwarens egen
kode på en Mac.

Alle registeradresser er slået op i SunSpecs officielle modelfiler, ikke
skrevet efter hukommelsen. Simulatoren og C-testene er bygget uafhængigt
af hinanden og er enige om hvor modellerne ligger: begge siger at model
160 starter på adresse 40178.

Fejl fanget: N i model 160 ligger på offset 6, ikke 5. Feltet lige før,
Evt, er en bitfield32 og fylder både offset 4 og 5. Læser man antallet af
DC-kanaler på offset 5, får man den nederste halvdel af Evt, som næsten
altid er nul, og så forsvinder hele kanalgenkendelsen lydløst. Den samme
fejl står i Zbox Raspberry i dag, i `app/modbus_controller.py`.

Testen for om en model bruger flydende tal var `id >= 111`, hvilket er
rigtigt for inverteren og forkert for alle fire målermodeller, fordi 201
til 204 også er større end 111. Skærmen viste 0 W på nettet mens måleren
meldte 5 kW eksport. Fanget af `zs-probe` mod simulatoren, ikke af
enhedstestene, fordi testene var skrevet med den samme forkerte
antagelse. Nu står de otte modelnumre skrevet ud, låst med 18 tests.

Reserveløsningen for unavngivne kanaler talte forfra. Fronius' manual
siger at batteriets to kanaler lægges i enden, så et anlæg med én
solstreng har batteriet på kanal 2 og 3, ikke 3 og 4.

En model med længde 0 kunne få vandringen til at gå i ring.
`printf("%.1f")` runder halve værdier til nærmeste lige ciffer, så 4250 W
blev til 4,2 kW mens 4350 W blev til 4,4. Og simulatorens batteri ændrede
aldrig ladetilstand på grund af en indrykningsfejl.

---

## 2026-08-25 15:12

Værktøjskæden. `tools/setup-toolchain.sh` henter ESP-IDF v5.1.7,
`tools/env.sh` sætter miljøet op.

Både `python@3.12` og `python@3.14` fra Homebrew på denne maskine har et
`pyexpat` der loader systemets `libexpat` i stedet for Homebrews, så alt
Python-værktøj der rører XML fejler. Fejlen ser ud som
`ensurepip returned non-zero exit status 1`. Rettes med
`brew reinstall expat python@3.12`. Indtil da bruges Apples egen
`/usr/bin/python3`. Scriptet prøver hver kandidat af i praksis ved at
bygge et rigtigt venv med pip i, i stedet for at se på versionsnummeret.

ESP-IDF leverer ikke `cmake` og `ninja` på macOS. De installeres nu
automatisk.
