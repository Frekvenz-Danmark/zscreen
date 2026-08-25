# Ændringer i zScreen

Hver gang der er lavet noget, står det her med dato, klokkeslæt, hvad der
blev lavet, og hvorfor det løser noget. Nyeste øverst.

Formatet er med vilje kedeligt: en overskrift med tidspunkt, en kort
forklaring på almindeligt dansk, og hvor det er relevant en note om
hvilken fejl der blev fanget og hvordan.

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
