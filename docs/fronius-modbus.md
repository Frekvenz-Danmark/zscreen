# Fronius over Modbus TCP

Hvordan zScreen læser fra inverteren, hvilke registre der bruges, og
hvilke fælder der er i dem.

Alle registeradresser her er slået op i SunSpecs officielle modelfiler
den 25. august 2026, ikke skrevet efter hukommelsen:
<https://github.com/sunspec/models>

---

## Sådan kommer man på

Modbus TCP skal slås til i inverteren. På en Fronius GEN24:

1. Åbn inverterens webside i en browser
2. Kommunikation → Modbus
3. Slå **Modbus TCP** til
4. Port **502**
5. SunSpec Model Type: **int + SF** eller **float**, begge virker

**Slå IKKE "Tillad styring" til.** zScreen skriver aldrig til
inverteren, så den har ikke brug for det, og et slukket flag er en
mindre måde at komme galt afsted på.

Fronius' egen manual siger at forespørgsler skal køre sekventielt og
højst to ad gangen. zScreen sender fire forespørgsler i træk hvert
andet sekund, aldrig parallelt.

Har kunden både en Zbox og en zScreen på samme inverter, er det to
klienter, altså lige på grænsen. Kommer der en tredje, for eksempel
Home Assistant, begynder det at hakke.

---

## Registerkortet

SunSpec ligger som en kæde man vandrer igennem, med start på 40001
(altså ledningsadresse 40000 eller 40001, begge prøves):

```
"SunS"        2 registre, 0x5375 0x6E53
model-ID      1 register
længde        1 register
data          længde registre
...
0xFFFF        slut
```

Modeller på en Fronius GEN24 med batteri:

| Model | Hvad | Bruges til |
|---|---|---|
| 1 | Common | Fabrikat, model, firmware, serienummer |
| 103 eller 113 | Inverter | AC-effekt, netfrekvens |
| 120 | Nameplate | Mærkeeffekt, batterikapacitet |
| 124 | Storage | Ladetilstand i procent, lader eller aflader |
| 160 | MPPT | Solstrenge og batteriets to kanaler |
| 203 eller 213 | Elmåler | Køb og salg mod nettet |

**Offsets tælles fra første dataregister**, altså efter ID og længde.
Det er samme talning som i SunSpecs egne smdx-filer.

| Model | Felt | Offset | Type |
|---|---|---|---|
| 1 | Mn fabrikat | 0 | streng, 16 reg |
| 1 | Md model | 16 | streng, 16 reg |
| 1 | Vr firmware | 40 | streng, 8 reg |
| 1 | SN serienummer | 48 | streng, 16 reg |
| 103 | W effekt | 12 | int16 |
| 103 | W_SF | 13 | sunssf |
| 113 | W effekt | 20 | float32 |
| 120 | WHRtg batteri | 17 | uint16 |
| 124 | ChaState SoC | 6 | uint16 |
| 124 | ChaSt tilstand | 9 | enum16 |
| 124 | ChaState_SF | 20 | sunssf |
| 160 | DCW_SF | 2 | sunssf |
| 160 | **N antal kanaler** | **6** | count |
| 160 | første kanal | 8 | 20 reg pr. kanal |
| 160 | kanal +IDStr | +1 | streng, 8 reg |
| 160 | kanal +DCW | +11 | uint16 |
| 160 | kanal +DCSt | +17 | enum16 |
| 203 | W effekt | 16 | int16 |
| 203 | W_SF | 20 | sunssf |
| 213 | W effekt | 26 | float32 |

---

## Fælder

### N i model 160 ligger på offset 6, ikke 5

Feltet lige før, `Evt`, er en bitfield32 og fylder **både offset 4 og
5**. Læser man antallet af DC-kanaler på offset 5, får man den nederste
halvdel af `Evt`, som næsten altid er nul. Så bliver svaret "nul
kanaler", og hele genkendelsen af sol- og batterikanaler forsvinder
lydløst, uden en eneste fejl i loggen.

Zbox Raspberry har denne fejl i dag i `app/modbus_controller.py`
(`M160_N = 5`). Vi har ikke rørt Zbox, men den bør rettes der.

### Elmålerens modeller er ikke "float" fordi tallet er over 111

Nummereringen ser sådan her ud:

```
101 102 103 104     inverter, heltal med skalafaktor
111 112 113 114     inverter, flydende tal
201 202 203 204     elmåler, heltal med skalafaktor
211 212 213 214     elmåler, flydende tal
```

En test som `id >= 111` er rigtig for inverteren og forkert for alle
fire målermodeller. Resultatet er at målerens registre bliver læst som
flydende tal, og skærmen viser 0 W på NETTET mens måleren melder 5 kW
eksport. Ingen fejlmeddelelse, bare et forkert tal.

I `zs_fronius.c` står alle otte modelnumre skrevet ud, og der er 18
tests der holder dem fast.

### Batteriets kanaler ligger til sidst, ikke på plads 3 og 4

Fronius' manual (42,0410,2649) siger:

> For devices with a storage solution, there are two additional blocks
> (charging (MPP3) and discharging (MPP4))

**Additional** betyder at de lægges i enden, efter solstrengene. Et
anlæg med to strenge har batteriet på kanal 3 og 4, men et med én
streng har det på kanal 2 og 3. Tæller man forfra og går ud fra fire
kanaler, læser man solstreng nummer to som batteriets ladeside.

zScreen tæller bagfra: de to sidste kanaler er batteriets.

Det gælder kun når inverteren ikke navngiver sine kanaler. Gør den det,
bruger vi navnene, og så er antallet af strenge ligegyldigt.

### STDISCHA indeholder STCHA

`STCHA` er en delstreng af `STDISCHA`. Tjekker man ladenavnet først,
bliver afladekanalen kaldt ladekanal, og batteriets fortegn vender
forkert: skærmen siger "lader 3 kW" mens batteriet aflader.
Afladning skal altid tjekkes først.

### En kanal der sover kan stå med en gammel værdi

DCW bliver ikke nulstillet når en streng kobles fra. Vi tæller kun
kanaler hvor DCSt er 4 (sporer maksimalpunkt) eller 5 (begrænset).
Tilstand 3 (starter op) tæller ikke: effekten er ikke troværdig endnu.

Uden det filter viste et anlæg uden solceller 824 W spøgelses-sol.

### "Ikke implementeret" er ikke nul

SunSpec har ingen NULL. Hver datatype har i stedet en værdi der betyder
"det kan jeg ikke måle":

| Type | Værdi |
|---|---|
| uint16 | 0xFFFF |
| int16 | 0x8000, altså -32768 |
| sunssf | 0x8000 |
| acc32 | 0 |
| float32 | NaN |

Filtrerer man dem ikke fra, får man tal som -32768 W på skærmen, eller
værre: 10 opløftet i -32768, som bliver til nul og ligner et rigtigt
målt nul.

---

## Fortegn

| Værdi | Positiv betyder |
|---|---|
| Elmåler W | Køb fra nettet |
| Inverter AC W | Der leveres til huset |
| Batteri (vores) | Aflader, altså leverer |

Forbruget udledes:

```
forbrug = inverterens AC-effekt + det der købes fra nettet
```

Kontrolregning:

| Situation | Inverter | Net | Forbrug |
|---|---|---|---|
| Sol 4200, huset 1800, sælger 2400 | +4200 | -2400 | 1800 |
| Nat, batteri leverer 1000, køber 500 | +1000 | +500 | 1500 |
| Lader fra net 2000, huset 500 | -2000 | +2500 | 500 |

**Ikke efterprøvet mod et levende anlæg endnu.** SunSpec siger ikke
entydigt hvilken vej der er positiv på måleren, og det afhænger af
hvordan strømtangen er vendt ved installationen.

Derfor kan det vendes fra Indstillinger på skærmen, under "Byt køb og
salg". Skærmen tæller også hvor mange gange forbruget er blevet regnet
negativt, hvilket ikke kan lade sig gøre, og viser det på
Detaljer-siden med en forklaring.

Sådan efterprøves det på et rigtigt anlæg: tving en kendt eksport,
sammenlign med SolarWeb, og se om NETTET viser salg med nogenlunde
samme størrelse.

### Hvor sidder måleren

Formlen forudsætter at elmåleren sidder ved nettilslutningspunktet, som
er den normale Fronius-montering. Sidder den i stedet på en forbrugsgren,
måler den noget andet, og så bliver forbruget forkert uden at fortegnet
er vendt. Det ses ved at forbruget er konsekvent for lavt.

---

## Elmålerens Modbus-enhed

Fronius' manual siger 200 til 204. I marken har Zbox-flåden fundet den
på **201**, fordi måleradressen lægges oven i offsettet:

```
offset 200 + RTU-adresse 1 = 201
```

Ældre Datamanager-opsætninger bruger 240 og 241.

zScreen kigger først i inverterens egen modelkæde (på mange GEN24 er
måleren broet ind der), og prøver derefter 200, 201, 202, 203, 204,
240, 241, 2, 3, 100 og 247.
