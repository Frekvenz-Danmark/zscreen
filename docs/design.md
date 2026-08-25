# Designsystem

Den røde tråd fra Frekvenz' brand ind på en 4 tommer skærm.
Alt herunder står i kode i `firmware/main/ui/zs_theme.h`. Står der et
tal her, står det ét sted i koden, ikke fem.

---

## Hårde regler

De gælder over alt andet. De er de samme som i Zbox' `DESIGN.txt`.

- **Ingen em-dash.** Almindelig bindestreg eller komma
- **Ingen AI-design.** Ingen blå eller lilla forløb, ingen matteret
  glas, ingen glød, ingen skinnende animationer, ingen unødige emoji
- **Ingen marketing-sprog.** Ikke "intelligent", ikke "kraftfuld".
  Beskriv hvad noget gør
- **Ingen ikon hvor et ord er tydeligere.** Et tandhjul er i orden,
  en tegning af et hus der skal betyde "eget forbrug" er ikke
- **Hverdagsdansk.** Ord som SoC, Modbus og inverter er i orden,
  fordi de står på inverterens egen webside

---

## Farver

Brandets egne, fra `brand/Frekvenz - logo payoff.svg`:

| | |
|---|---|
| `#174A48` | Frekvenz mørkegrøn, primærfarven |
| `#FBAC18` | Frekvenz orange, accentfarven |
| `#FFFFFF` | hvid |

Skærmen bruger en mørk udgave. Den hænger på en væg i en stue og skal
kunne ses om dagen uden at lyse rummet op om aftenen.

| Navn | Kode | Bruges til |
|---|---|---|
| `ZS_C_BG` | `#0E2A29` | baggrund, brandgrøn trukket ned i lys |
| `ZS_C_CARD` | `#16403E` | kortenes flade |
| `ZS_C_CARD_PRESSED` | `#1D504D` | et kort der trykkes på |
| `ZS_C_BORDER` | `#205A57` | 1 px kant om kort og rækker |
| `ZS_C_TEXT` | `#FFFFFF` | overskrifter og tal |
| `ZS_C_TEXT_DIM` | `#B6D0CE` | klokkeslæt og sekundær tekst |
| `ZS_C_LABEL` | `#8FB3B1` | etiketter, enheder, ikoner i hvile |
| `ZS_C_STALE` | `#5C8280` | tal der er for gamle til at tro på |
| `ZS_C_ACCENT` | `#FBAC18` | tal der er i bevægelse, primær knap |
| `ZS_C_GOOD` | `#4ADE80` | salg til nettet, batteri der lader |
| `ZS_C_BAD` | `#F87171` | køb fra nettet, nulstil |
| `ZS_C_WARN` | `#FBBF24` | inverteren svarer ikke |

**Farvereglen for de store tal er én sætning:** tallet står i orange
når der sker noget på kortet, og i hvidt når der ikke gør. Så lyser
skærmen op når anlægget arbejder, uden at alle fire kort råber ad
brugeren hele tiden.

Grænsen for "der sker noget" er 25 W. Under det er det stikkontakter i
standby, og et kort der skifter mellem "lader" og "aflader" hvert andet
sekund fordi tallet vipper omkring nul, er uroligt at kigge på.

---

## Skrifttype

**Funnel Sans**, samme som frekvenz.nu og Zbox-webfladen.
SIL Open Font License, fri at indlejre.

| Navn | Vægt | Størrelse | Bruges til |
|---|---|---|---|
| `zs_font_num_64` | SemiBold | 64 | de store tal. Kun cifre og komma |
| `zs_font_28` | SemiBold | 28 | enheder og sideoverskrifter |
| `zs_font_kb_24` | Medium | 24 | tastaturet, med tre ikoner flettet ind |
| `zs_font_20` | Medium | 20 | listerækker og knapper |
| `zs_font_16` | Medium | 16 | undertekst og brødtekst |
| `zs_font_13` | SemiBold | 13 | etiketter med versaler |

Tegnsættet er ASCII plus æ, ø, å, Æ, Ø og Å. Et fuldt Latin-1-sæt ville
fylde tre gange så meget uden at give os noget.

Det store taltema har kun `0123456789,.-%`. Et fuldt tegnsæt i 64
pixels ville fylde over 200 KB uden at ét af bogstaverne blev tegnet.

---

## Ikoner

**Lucide**, samme pakke som Zbox-webfladen. ISC-licens.

34 ikoner, valgt fra listen i `tools/icons.py`. Den samme liste
bestemmer både hvilke ikoner der kommer med i skrifttypen og hvilke
navne C-koden kender, så de ikke kan komme til at pege på hver sit.

Ikoner er 20 px i rækker og på kort, 28 px hvor de står alene.
Farven er `ZS_C_LABEL` i hvile og en semantisk farve når de siger noget.

---

## Layout

Alt er regnet ud, ikke skudt efter. Skærmen er 480 x 480.

```
Vandret:  12 + 222 + 12 + 222 + 12 = 480
Lodret:   44 + 12 + 200 + 12 + 200 + 12 = 480
```

| Navn | Værdi |
|---|---|
| `ZS_BAR_HEIGHT` | 44, statuslinjen |
| `ZS_EDGE` | 12, luft ud til kanten |
| `ZS_GRID_GAP` | 12, mellem to kort |
| `ZS_CARD_WIDTH` | 222 |
| `ZS_CARD_HEIGHT` | 200 |
| `ZS_CARD_PAD` | 14, luft inde i kortet |
| `ZS_CARD_RADIUS` | 18 |

Kortets indvendige mål bliver 194 x 172. De tre bånd:

| Bånd | y | højde |
|---|---|---|
| overskrift | 0 | 20 |
| stort tal | 60 | 54 |
| undertekst | 154 | 18 |

Tallet står optisk midt imellem: fri plads mellem 20 og 154 er 134,
minus tallets 54 giver 80, halvdelen er 40, altså y = 60.
Underteksten slutter på 172, præcis kortets indvendige underkant.

Alle værdier er lige tal, så intet kan ende på en halv pixel når noget
bliver centreret.

### Grundlinjen under tallet

"kW" skal stå på **samme grundlinje** som tallet. De to har hver sin
skriftstørrelse, så stiller man dem op efter underkanten, hopper
enheden et par pixels.

LVGL oplyser hvor grundlinjen ligger:

```
grundlinje fra overkant = line_height - base_line

zs_font_num_64    54 - 9 = 45
zs_font_28        31 - 5 = 26
```

Enheden sænkes altså 19 px. Det regnes ud på stedet ud fra
skrifttyperne, ikke skrevet ind som et tal der holder op med at passe
hvis en størrelse ændres.

### Sider

Alle sider ud over hovedskærmen bruger samme ramme, så tilbageknappen
og overskriften står det samme sted hver gang.

```
┌────────────────────────────────────────┐
│ ←   Vælg netværk                       │  56 px
├────────────────────────────────────────┤
│   indhold, kan rulles                  │  424, eller 348 med fod
├────────────────────────────────────────┤
│        [ knap ]                        │  76 px, valgfri
└────────────────────────────────────────┘
```

---

## Fingerflader

**Alt man kan trykke på er mindst 44 x 44 pixels.** Det er cirka 8 mm
på denne skærm, og det er den grænse både Apple og Google har stået på
i mange år.

Derfor er tandhjulet i statuslinjen en 44 x 44 knap med et 20 px ikon
i midten, ikke bare et ikon.

**Én undtagelse: tastaturet.** Hver tast er omkring 43 x 48. Det er
med vilje. Alle telefoner har smallere taster, fordi de står i et
gitter man kender i forvejen og sigter efter med tommelfingeren.

---

## Bevægelse

Der er præcis to bevægelser i hele fladen:

1. Et tryk gør fladen lysere, med det samme
2. Snurrehjulet mens der forbindes, og bjælken mens der søges

Ingen indgangsanimationer. Ingen tal der tæller op. Ingen kort der
folder sig ud. En energiskærm skal kunne aflæses på et halvt sekund
fra den anden ende af rummet.

---

## Sprog

Alt brugeren læser er på dansk med rigtige æ, ø og å.

Kommentarerne i kildekoden er derimod med omskrevne bogstaver, altså
"skaerm" og ikke "skærm". Det er en bevidst opdeling: kommentarer
havner i byggelogge, diffs og fejlmeddelelser fra oversætteren, hvor
tegnsættet ikke altid følger med.

Fejlbeskeder siger hvad der skete og hvad man kan gøre, ikke hvad
funktionen hed:

| Nej | Ja |
|---|---|
| "WIFI_REASON_AUTH_FAIL" | "Kodeordet passer ikke" |
| "timeout" | "Inverteren svarede ikke i tide" |
| "ESP_ERR_NVS_NOT_FOUND" | "Skærmen skal sættes op" |
