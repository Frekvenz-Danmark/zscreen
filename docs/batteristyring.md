# Batteristyring over SunSpec model 124

**INTET HERUNDER ER EFTERPROEVET AF OS.** Det er Jakobs noter over hvad der
virker i dag, givet 9. oktober 2026, og han sagde selv udtrykkeligt at det
ikke er sikkert det er rigtigt. Det staar her som UDGANGSPUNKT for den dag
vi staar ved et anlaeg og maaler det, ikke som noget vi bygger paa.

Firmwarens Fronius-lag er skrivebeskyttet i dag. Det bliver det indtil
registrene er efterproevet paa rigtig hardware.

## Tilstandene, som de er oplyst

Tallene er raa registervaerdier. `rate` er den oenskede effekt.

| Tilstand | StorCtl_Mod | OutWRte | InWRte | MinRsvPct |
|---|---|---|---|---|
| 1. Oplad | 3 | -rate (gulv) | 10000 | 9900 |
| 2. Aflad | 3 | 10000 | -rate (gulv) | 500 |
| 3. Normal, goer intet | 0 | 10000 | 10000 | 500 |
| 5. Laas batteriet | 3 | 0 | 10000 | 500 |

Som det er forklaret:

- **Oplad:** gulvet paa `OutWRte` er det der tvinger ladning, og
  `MinRsvPct` paa 99 % skubber inverteren mod at lade
- **Aflad:** spejlvendt, gulvet ligger paa `InWRte`
- **Normal:** `StorCtl_Mod = 0` giver inverteren tilbage til sin egen
  automatik. De tre andre nulstilles, saa et gulv fra en tidligere
  kommando ikke bliver staaende
- **Laas:** `OutWRte = 0` blokerer AL afladning, baade til installationen
  og til nettet. `MinRsvPct` paa 5 % betyder "tving ikke opladning", saa
  den koeber ikke fra nettet. `InWRte = 10000` er intet ladeloft, saa solen
  maa lade frit

## Tilstand 4, zero export

Oplyst 9. oktober 2026: zero export er bare at lade med 100 %. Altsaa
samme registre som tilstand 1, med fuld rate.

Det er praktisk og det bruger det vi allerede har. MEN det er ikke
zero export i streng forstand, og det skal siges hoejt: **er batteriet
fuldt, gaar overskuddet ud paa nettet alligevel.** Vil man have en haard
graense paa hvad der sendes ud, uanset batteriets stand, er det en anden
sag i et andet register, og den skal findes for sig.

Det skal maales paa stedet hvad der faktisk sker naar batteriet rammer
100 % mens tilstanden er sat.

## Maalt paa et rigtigt anlaeg 17. oktober 2026

Fronius Gen24 med batteri, Emiliedalen 157, serienr. 36170267, laest med
`tools/proev-anlaeg.py` paa 192.168.0.43. **Punkt 1, 2 og 4 nedenfor er
dermed besvaret af enheden selv og ikke af os.**

Model 124 ligger paa unit 1, er 24 registre lang, og dataene begynder
paa 40345. Hele blokken:

| Felt | Adresse | Offset | Vaerdi |
|---|---|---|---|
| `WChaMax` | 40345 | 0 | 7270 |
| `StorCtl_Mod` | 40348 | 3 | 0 |
| `MinRsvPct` | 40350 | 5 | 100 |
| `ChaState` | 40351 | 6 | 180 |
| `ChaSt` | 40354 | 9 | 4 = aflader |
| **`OutWRte`** | **40355** | **10** | 10000 |
| **`InWRte`** | **40356** | **11** | 10000 |
| `InOutWRte_WinTms` | 40357 | 12 | 65535 |
| **`InOutWRte_RvrtTms`** | **40358** | **13** | 0 |
| `MinRsvPct_SF` | 40364 | 19 | 10^-2 |
| `ChaState_SF` | 40365 | 20 | 10^-2 |
| **`InOutWRte_SF`** | **40368** | **23** | **10^-2** |

- **Punkt 1 er besvaret.** `OutWRte` og `InWRte` ligger paa offset 10 og
  11, altsaa praecis hvor SunSpec-specen siger. Og alle syv offsets
  firmwaren bruger i forvejen passer med enheden.
- **Punkt 2 er besvaret.** `InOutWRte_SF` er 10^-2. Tabellen ovenfor er
  altsaa regnet paa den rigtige skala: 10000 er 100,00 % og 500 er
  5,00 %. Det var et gaet, og nu er det maalt.
- **Punkt 4 er besvaret.** Nulstillingstimeren findes:
  `InOutWRte_RvrtTms` paa 40358. Den staar paa 0, altsaa slaaet fra.

Unit 1 havde 8 modeller: 1, 103, 120, 121, 122, 123, 160 og 124. Unit
200 havde 1 og 203, men maalermodellens `W_SF` stod paa "ikke
implementeret", saa nettets fortegn kunne ikke efterproeves derfra -
det er stadig punkt 1 i `test-plan.md` afsnit 3.

**Punkt 3 og 5 staar stadig aabne**, og de kan ikke besvares ved at
laese: de kraever en SKRIVNING til en kundes batteri.

### Og en ting mere, som kun et rigtigt anlaeg viser

Anlaegget var **helt vaek fra nettet** i flere timer midt paa dagen -
port 502 og 80 lukkede paa baade .25 og .43, ingen svar paa ping, mens
resten af nettet svarede. Kort foer meldte Solar-API'et
`InverterState: Sleeping` med `ErrorCode: 1175`. Det kom igen af sig
selv om aftenen.

Og **to adresser er ÉN inverter**: .25 og .43 svarer begge, med samme
serienummer. Soegningen i firmwaren sammenligner kun IP'er, saa den
viser den samme inverter to gange i listen.

## Hvad vi SKAL maale paa stedet, foer noget bygges

1. Hvilke registeradresser de fire navne faktisk ligger paa. Vi laeser i
   forvejen `StorCtl_Mod`, `MinRsvPct`, `ChaState` og `ChaSt`, men vi har
   aldrig fundet `OutWRte` og `InWRte`, og vores offsets er spec minus to
   paa grund af ID- og laengdehovedet
2. Hvad skalafaktoren er. 10000 og 500 ser ud som hundrededele procent,
   altsaa 100,00 % og 5,00 %, men det skal laeses af inverteren og ikke
   antages
3. Om negative vaerdier overhovedet kan skrives i registrene, og hvordan
   de kodes
4. **Nulstillingstimeren.** SunSpec har et felt til at en kommando
   udloeber af sig selv. Besluttet 8. oktober 2026: den SKAL saettes paa
   hver plan, saa batteriet gaar tilbage til normal hvis Platformen tier.
   Vi skal finde den og maale at den virker, foer vi sender en eneste
   kommando i drift
5. Om inverteren kvitterer. En skrivning der tavst ignoreres er den
   vaerste udgave, og vi har allerede en tilbagelaesning i Modbus-laget
   der kan fange det

## Hvad der ER bevist, uden en inverter

Oversaettelsen. `firmware/kerne/net/zs_ladeplan.c` laver fire
registervaerdier af en tilstand og en rate, og tabellen ovenfor er
proevet raekke for raekke i `tests/host/test_ladeplan.c` - med tallene
skrevet ud, ikke laant fra koden, saa en aendret skala ikke kan bestaa
sin egen proeve. `tools/check-dokumentation.py` sammenligner desuden
tabellen HER med tabellen i koden, begge veje, saa de ikke kan drive fra
hinanden.

Modulet SENDER stadig ingenting. Men vejen ud er laengere bygget end
foerst skrevet her - efterset 10. oktober 2026:

- `zs_modbus_tcp` **kan** skrive. `zs_mb_write_verified` skriver og
  laeser tilbage, netop fordi Fronius svarer uden fejl ogsaa naar en
  skrivning bliver afvist. Det er proevet i `test_modbus_skriv`
- `zs_fronius` er skrivebeskyttet og kalder ingen af dem
- **ingen** kalder `zs_mb_write_verified` - nul steder i firmwaren

Vejen findes altsaa, men den er ikke forbundet. Og
`zs_mb_write_verified` er praecis den tilbagelaesning
`zs_ladeplan_kvitter` doemmer paa.

**To ting blev maalt paa selve oversaettelsen, og de var ikke til at se:**

- **Rate 0 er ikke "ingenting".** `oplad` med rate 0 gav `OutWRte = 0` og
  `MinRsvPct = 9900`: al afladning blokeret, og 99 % reserve - og
  forklaringen ovenfor siger selv at 99 % skubber inverteren mod at lade,
  altsaa KOEBE fra nettet. Den der bad om nul effekt, fik en plan der
  handler stroem. `aflad` med 0 gav `InWRte = 0`, altsaa al LADNING
  blokeret. Begge afvises nu. Vil man ingenting, er det tilstand 3.
- **Doedmandsknappen havde kun et gulv.** 65535 sekunder - 18 timer og 12
  minutter - blev godtaget. Der er nu et loft paa 900 sekunder, ét
  kvarter, samme rytme som skaermens egne tal. **Det er vores beslutning,
  ikke en maaling**: skal den vaere laengere, skal nogen sige hvorfor.
  Der er med vilje ingen nedre graense ud over nul - en timer paa ét
  sekund ville faa batteriet til at skifte mellem plan og normal hvert
  sekund, men vi har ikke maalt registret, og et tal vi finder paa er et
  gaet. Det hoerer paa listen ovenfor.

## Paa serveren mangler der et sted at skrive timeren

Maalt 9. oktober 2026 paa vores egen OpenRemote: `ElectricityBatteryAsset`
har 29 standardattributter, og **ingen af dem er en timer, en udloebstid
eller en gyldighed**. Beslutningen om at nulstillingstimeren skal med paa
hver plan, har altsaa ikke et felt at staa i endnu. Platformen kan ikke
sende den, og skaermen kan ikke melde tilbage hvad den satte.

Samtidig: `powerSetpoint` er ét fortegnet tal i **kilowatt**, mens
ladeplanen her regner i raa registertal uden fortegn plus en tilstand. De
to ender passer ikke sammen endnu, og **ingen kode oversaetter imellem**.
`powerSetpoint` tog ogsaa imod 99999 og -99999 uden at blinke, og
`powerExportMax`/`powerImportMax` paa batteriet staar tomme, saa der er
ingen graense nogen steder. Det skal besluttes foer Fase E kan bygges
videre - det er ikke noget vi kan maale os frem til.
