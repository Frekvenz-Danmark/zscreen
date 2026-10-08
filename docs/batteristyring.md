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

## Tilstand 4, zero export, mangler

Den staar ikke paa listen, og det passer med min egen mistanke: de fire
andre handler om BATTERIET, mens zero export handler om hvad INVERTEREN
sender ud paa nettet. Det er sandsynligvis et andet sted i SunSpec end
model 124. Det skal findes og maales for sig.

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
