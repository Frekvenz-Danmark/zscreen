# Andre mærker end Fronius

Hvad der skal til for at skærmen virker på en inverter vi ikke har
papir på. Først Kostal, fordi det er den der er spurgt om.

**Dette er research, ikke en måling på hardware.** Kilderne står
nederst. Registeradresser og modellister skal efterprøves mod den
faktiske inverters firmware før der bygges på dem — samme regel som i
`batteristyring.md`.

---

## Fundamentet: hvad er mærkeuafhængigt i dag

`zs_maerker.h` deler det op, og delingen holder:

| Hvad | Hvor det kommer fra | Mærkeafhængigt |
|---|---|---|
| effekt, spænding, driftstilstand | SunSpec-modellerne | nej |
| `Evt1`, `DCEvt` fejlflag | SunSpec | nej |
| modellernes adresser | kæden, gået igennem | nej |
| `EvtVnd1`–`EvtVnd4` | producentens egne bits | **ja** |
| `StVnd` | producentens egen tilstand | **ja** |
| DC-kanalernes roller uden navne | hvordan MPPT deles | **ja** |

Kender vi ikke mærket, vises producentens egne felter **råt**. Det er
med vilje: før blev Fronius' fejltabeller brugt på enhver inverter, så
en Huawei med bit 1 sat fik teksten "Netfejl" og "Fronius-kode 101" på
skærmen — en forkert fejl, skrevet med fuld sikkerhed.

Vi læser disse SunSpec-modeller: **1, 103, 113, 120, 124, 160** og
elmåler **201–204** og **211–214**.

---

## Kostal PLENTICORE / PIKO IQ, inkl. hybrid

### Hvad Kostal udstiller

Fra Kostals egen interface-beskrivelse, afsnit 4.2 "Implemented SunSpec
Models":

| Model | Navn | Læser vi den? |
|---|---|---|
| 1 | Common | **ja** |
| 103 | Three Phase Inverter | **ja** |
| 113 | Three Phase Inverter, float | **ja** |
| 120 | Nameplate | **ja** |
| 123 | Immediate Controls | nej |
| 160 | Multiple MPPT | **ja** |
| 802 | Battery Base Model | **nej** |

Startadresser: 1 på 40003, 103 på 40071, 113 på 40123, 120 på 40185,
123 på 40213, 160 på 40239, 802 på 40309.

Forbindelsen: **TCP-port 1502** og **Unit-ID 71** som standard. Unit-ID'et
kan ændres i inverteren. Der er også et `MODBUS Enable`-register
(0x02, bool, R/W), så Modbus skal være slået til.

### Tre ting spærrer, og den første er den enkleste

**1. Skærmen kan ikke nå den.** Søgningen kigger på port 502 med unit 1.
Begge tal findes i `zs_nvs.h` som `inverter_port` og `inverter_unit`, de
gemmes, de læses ved opstart og de gives til `zs_fr_connect` — men der
findes **ingen kommando** i `zs_app.h` til at sætte dem, og intet felt på
skærmen. En montør foran en Kostal kan altså ikke pege skærmen derhen.

Det er en lille, velafgrænset mangel: et felt i opsætningen og en
kommando.

**2. Batteriet bliver ikke fundet.** Vi kræver model **124** (Storage).
Kostal udstiller **802** (Battery Base Model). Målt gennem vores egen
vandring med en opdigtet Kostal-kæde: alle syv modeller findes, 123 og
802 springes pænt over, de fem vi bruger findes — og 124 mangler. Så
BATTERI-kortet vil sige "Intet batteri".

**3. Elmåleren bliver ikke fundet.** Kostals SunSpec-liste har slet ingen
elmåler-model. NETTET og FORBRUG vil sige "Ingen elmåler". Tallene
findes hos Kostal, men i deres **eget** registerkort, eller i en separat
KSEM-måler.

### Hvad der ville virke med det samme

Når den først kan nås: fabrikat, model, serienummer og firmware fra
model 1. AC-produktionen fra 103 eller 113 — vi håndterer begge, og
heltal og flydende tal har helt forskellige registerkort, så det er ikke
ligegyldigt. Mærkepladen fra 120. DC-kanalerne fra 160.

Og `EvtVnd`/`StVnd` vises råt i stedet for med Fronius-tekster, fordi
Kostal ikke står i mærketabellen. Det er den rigtige opførsel.

### Og for ladeplanen, hvis den nogensinde skal styre en Kostal

Kostal implementerer model **123, Immediate Controls** — SunSpecs egen
standardvej til at skrive — plus 802 til batteriet. Altså **ikke**
Fronius' `OutWRte`/`InWRte`, som `zs_ladeplan.c` er bygget på.

Tabellen i `zs_ladeplan.c` er netop en tabel, så en Kostal ville være en
række mere, ikke en omskrivning. Men der skal måles først, og der er
mere end registre i det:

- batteristyring over Modbus kræver at inverteren sættes til **ekstern
  batteristyring** (fællesskabet melder register 1080, værdi 0x02)
- den funktion kom med en firmwareopdatering, og kræver PLENTICORE G3
  fra softwareversion 03.05

---

## Forbehold, så det ikke bliver antaget

- Den interface-beskrivelse vi læste modellisten i, er **© 2020** og
  gælder PIKO IQ / PLENTICORE. Kostals nuværende revision er 2.9
  (17.07.2026) og dækker PLENTICORE G3 på SW 3.07
- Vi fandt **ingen** kilde der bekræfter model 124 på en G3. Det
  fællesskabet bruger, er 802
- Registeradresser og modellister er firmwareafhængige. De skal læses af
  den inverter der står foran en, ikke af en PDF
- Vi har **ikke** haft en Kostal i hænderne. Alt ovenfor er læst og
  regnet, ikke målt på hardware

## Kilder

- [KOSTAL Interface description MODBUS (TCP) & SunSpec, PIKO IQ / PLENTICORE](https://www.kostal-solar-electric.com/fileadmin/downloadcenter/kse/BA/Protokollbeschreibung/BA_KOSTAL_Interface_MODBUS-TCP_SunSpec_with_Control.pdf)
- [Samme, hybrid-udgaven](https://cdn-production.kostal.com/-/media/document-library-folder---kse/2023/11/16/13/29/ba_kostal-interface-description-modbus-tcp_sunspec_hybrid.pdf)
- [evcc: Kostal Modbus, port 1502 og ID 71](https://github.com/evcc-io/evcc/discussions/11389)
- [evcc: batteriets SoC læst som 802:SoC](https://github.com/evcc-io/evcc/discussions/886)
- [Home Assistant: ekstern batteristyring over Modbus TCP](https://github.com/orgs/home-assistant/discussions/974)
