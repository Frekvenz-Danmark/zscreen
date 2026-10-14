# Faseplanen

**Den her fil handler om FLÅDESTYRINGEN.** ZDisplay har sin egen vej i
mål, og den står i `zdisplay-faseplan.md`. De to planer deler to huller
— skærmen har aldrig stået på et rigtigt anlæg, og ladeplanen er ikke
målt — men ellers er de hver sit arbejde.

Arbejdet med flådestyringen er delt i fem faser, A til E.

**Den her fil findes fordi planen ikke stod nogen steder.** Faserne var
kun nævnt i kodekommentarer, spredt over to repoer, og det kostede en
forkert udmelding: "Fase B står ikke nogen steder" — den stod i
`scripts/anlaeg.py` i serverrepoet. Søg ikke efter planen, læs den her.

Datoerne er måledatoer. Står der at noget er målt, er det målt.

---

## Fase A — strukturen. FÆRDIG

Kundens mappe, anlægget, batteriet, solcellerne, elnettet og huset som
OpenRemotes egne energi-typer. Skærmen skriver i **sit eget** asset, og
serveren fordeler videre med OpenRemotes eget `attributeLinks`.

- Kvartersgennemsnit fordeles med fortegnsvending — to af fire vendes,
  og det sker **ét** sted
- Historikken ligger kun på energi-assets, 8 uger tilbage. Efterprøvet i
  den **kørende** container, ikke kun i compose-filen
- Firmware: `cfc5989`. Server: `74f00c1`, `e3ea91c`

## Fase B — tilmeldingen. FÆRDIG

Skærmen laver en engangskode på seks tegn, viser den med en QR, og
rydder den når siden forlades. Frekvenz kalder `POST /tilmeld` med koden
og kundens oplysninger, og får anlæggets asset-id og alle dele-id'er
tilbage.

- Koden lever 10 minutter og kan kun bruges **én** gang
- Fem fejlede forsøg i 10 minutter låser i 15
- 19 slags vrøvl i oplysningerne bliver sagt fra om
- En skærm kan **ikke** tilmeldes før den har fundet inverteren
- Firmware: `00a71a0`, `120306d`, `57fd60a`, `f6c7c3e`, `5b49025`

## Fase C — servicebrugerne. FÆRDIG

`frekvenz-laes` må læse, `frekvenz-styr` må skrive — og kun på
batteriet.

- En begrænset bruger ser **kun** felter med `accessRestrictedRead`.
  Uden den kommer der ingenting, og ingen fejl
- `frekvenz-styr` er bundet til batteriet alene. En skriver bundet til
  skærmens asset kan skrive i kundens målinger
- `powerSetpoint` har grænser, så 99999 kW ikke længere kan skrives
- `attributevalue/+/+` bliver **godtaget og leverer nul**

## Fase D — timehistorikken. FÆRDIG

Datapunkt-opslag, ingen nye felter. "SoC ved 11:00 mod ved 12:00" er to
opslag.

- Fire opslagstyper: `all`, `nearest`, `interval`, `lttb`
- `nearest` tager **sekunder**, de tre andre millisekunder
- `gapFill` giver bøtter **uden** `y`-nøgle
- Server: `6751b60`

## Fase E — ladeplanen. DELVIS, OG RESTEN ER SPÆRRET

### Det der er gjort

- **Oversættelsen.** Tilstand plus rate bliver fire registerværdier,
  prøvet række for række mod noterne i `batteristyring.md`
- **Navnene på tråden.** Én stavemåde, og firmwaren læser Platformens
  tekst uden at være tilgivende
- **Dommen.** `zs_ladeplan_kvitter()` afgør om planen faktisk blev sat,
  og udpeger hvilket register der svigtede. Dødmandsknappen efterses
  først
- **Serversiden.** Felterne på batteriet med grænser, og lænkerne ned i
  skærmens eget asset
- Firmware: `e8640f3`, `c2aa61c`, `58f664d`

### Det der SKAL måles på et anlæg først

Fra `batteristyring.md`, og intet af det er efterprøvet af os:

1. Hvilke registeradresser `OutWRte` og `InWRte` faktisk ligger på. Vi
   har **aldrig** fundet dem, og vores offsets er spec minus to
2. Hvad skalafaktoren er. 10000 og 500 **ligner** hundrededele procent
3. Om negative værdier overhovedet kan skrives, og hvordan de kodes
4. **Nulstillingstimeren.** Den skal findes i registrene
5. Om inverteren kvitterer. Fronius svarer uden fejl også når en
   skrivning bliver afvist

### Det der venter på en beslutning

- Planen er tre værdier i tre felter, og de kommer som **tre separate
  MQTT-beskeder**. Bygger skærmen planen på den første, blander den nyt
  og gammelt
- Skærmens to modtagefelter er erklæret i `felter.py` men ikke lagt på

### Hvor langt vejen ud er bygget

- `zs_modbus_tcp` **kan** skrive, og `zs_mb_write_verified` læser
  tilbage — netop fordi Fronius svarer uden fejl når en skrivning
  afvises
- `zs_fronius` er skrivebeskyttet og kalder den ikke
- **Ingen** i hele firmwaren kalder den

Vejen findes altså, men den er ikke forbundet. Og den tilbagelæsning er
præcis hvad `zs_ladeplan_kvitter` dømmer på.

---

## Og det der ikke er en fase

Skærmen har **aldrig stået på et rigtigt anlæg.** `test-plan.md` afsnit
3 har 13 punkter der kun kan gøres der, og ingen af dem er gjort —
heriblandt om nettets fortegn er rigtigt, og om forbruget stadig viser
noget om natten.

Det er et større hul end noget i Fase E.
