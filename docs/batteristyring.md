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
