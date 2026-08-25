# Testplan

Tre lag: enhedstest på en Mac uden hardware, hele vejen igennem mod en
simuleret Fronius, og til sidst på et rigtigt anlæg.

---

## 1. Enhedstest, ingen hardware

```bash
./tests/host/run.sh
```

218 tjek. Kører med address-sanitizer og undefined-behavior-sanitizer,
og med alle advarsler som fejl. Tager under ti sekunder.

Dækker:

| Område | Hvad |
|---|---|
| Modbus-rammer | forvanskede pakker, forkert transaktions-ID, forkert unit, løgnagtig bytetæller, exception, 125 registre |
| SunSpec | skalafaktorer, alle fire slags "ikke implementeret", float32 big-endian, strenge med polstring og rå bytes |
| Kanalnavne | STDISCHA før STCHA, mellemrum og understreger, MPPT, DC_STRING, PV |
| Model-vandring | begge startadresser, længde 0, afbrudt læsning, for mange modeller, slutmarkør på kanten |
| Fronius | de tre husforbrugs-scenarier, spøgelses-sol, reserveløsning bagfra, heltal mod flydende tal |
| Tal på dansk | komma, grænsen ved 999,5 W, afrunding af halve værdier |
| Headere | navnesammenstød mellem konstanter og include-guards |

---

## 2. Hele vejen igennem, mod simulatoren

Start den simulerede Fronius på din Mac:

```bash
cd tools/fronius-sim
sudo python3 serve.py                    # port 502
```

`sudo` er nødvendigt fordi porte under 1024 er beskyttede. Skal skærmens
netværksscanning kunne finde simulatoren, **skal** den ligge på 502.

Uden hardware kan hele datavejen prøves med `zs-probe`, som kører
firmwarens egen kode:

```bash
./tools/zs-probe/build.sh
./tools/zs-probe/zs-probe 127.0.0.1 --watch
```

### Profiler der skal køres igennem

| Profil | Hvad den prøver | Forventet |
|---|---|---|
| `battery` | almindeligt anlæg | alle fire kort viser tal |
| `nobattery` | uden batteri | BATTERI siger "Intet batteri" |
| `nometer` | uden elmåler | FORBRUG og NETTET siger "Ingen elmåler" |
| `nolabels` | kanaler uden navne | sol og batteri stadig rigtige, via reserveløsningen |
| `float` | model 113 og 213 | samme tal som `battery` |
| `onestring` | én solstreng | hele produktionen på ét kort |

```bash
sudo python3 serve.py --profile nometer
```

### Tjekliste på selve skærmen

Sæt skærmen i USB, flash, og gå hele vejen igennem:

- [ ] Velkomstsiden viser Frekvenz-logoet skarpt, ikke udtværet
- [ ] "Kom i gang" går til listen over netværk
- [ ] Netværkene står sorteret, stærkeste signal øverst
- [ ] Et mesh-netværk står kun **én** gang
- [ ] Hængelås på netværk med kodeord, "Åbent" på dem uden
- [ ] Tastaturet har æ, ø og å
- [ ] Store bogstaver slår fra igen efter det første tegn
- [ ] Slet-tasten fjerner et helt æ, ikke en halv byte
- [ ] Øjet viser og skjuler kodeordet
- [ ] Forkert kodeord siger "Kodeordet passer ikke", ikke en fejlkode
- [ ] "Prøv igen" går tilbage til tastaturet med feltet tomt
- [ ] Søgningen efter inverter når 100 % på under ti sekunder
- [ ] Simulatoren står på listen med model og serienummer
- [ ] Valget fører til hovedskærmen
- [ ] De fire tal stemmer med det simulatoren skriver i sit vindue
- [ ] Tallene skifter farve til orange når der sker noget
- [ ] Klokkeslættet står øverst til højre
- [ ] Wifi-ikonet passer til signalstyrken

### Når noget går galt

- [ ] Stop simulatoren: tallene bliver dæmpede inden for ti sekunder,
      og statusikonet bliver til en gul advarselstrekant
- [ ] Tallene **bliver stående** dæmpede, kortene bliver ikke tomme
- [ ] Start simulatoren igen: tallene bliver friske igen af sig selv
- [ ] Sluk for wifi på routeren: ikonet bliver rødt
- [ ] Tænd igen: skærmen kommer på uden at man rører den
- [ ] Træk strømmen og sæt den i: skærmen går direkte til hovedskærmen
      uden at spørge om noget

### Indstillinger

- [ ] Tandhjulet øverst til højre åbner Indstillinger
- [ ] Lysstyrke-skyderen virker med det samme, og hopper **ikke**
      tilbage under fingeren
- [ ] Natdæmpning kan slås til og fra
- [ ] "Byt køb og salg" vender fortegnet på NETTET
- [ ] Detaljer viser model, serienummer, SunSpec-modeller og tællere
- [ ] Genstart genstarter
- [ ] Nulstil spørger først, og fortryd gør ingenting
- [ ] Nulstil og bekræft: skærmen starter forfra på velkomstsiden

---

## 3. På et rigtigt anlæg

Kan først køres når der er en Fronius med Modbus TCP slået til.

- [ ] Skærmen finder inverteren i søgningen, uden at nogen taster en IP
- [ ] Fabrikat, model og serienummer stemmer med inverterens webside
- [ ] Solproduktionen stemmer med SolarWeb, inden for et par hundrede W
- [ ] Ladetilstanden i procent stemmer med SolarWeb
- [ ] Batteriet siger "Lader" når SolarWeb siger det samme
- [ ] **NETTETS fortegn:** tving en kendt eksport, og se om skærmen
      siger "Sælger". Gør den ikke det, så slå "Byt køb og salg" til
      under Indstillinger, og skriv det i `ÆNDRINGER.md`
- [ ] Forbruget er positivt hele dagen. Detaljer-siden må ikke tælle
      negativt forbrug
- [ ] Om natten uden sol: FORBRUG skal stadig vise noget
- [ ] Lad skærmen køre et døgn. Tællerne på Detaljer må gerne vise et
      par genforbindelser, men ikke hundredvis

### Med en Zbox på samme inverter

Fronius tåler omkring to samtidige Modbus-klienter.

- [ ] Begge kører i en time uden at hakke
- [ ] Zbox' egne aflæsninger bliver ikke langsommere
- [ ] Genstart Zbox: skærmen kommer sig selv
- [ ] Genstart skærmen: Zbox mærker ingenting
