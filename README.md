# zScreen

Live energiskærm til Fronius-anlæg. Hænger på væggen og viser hvad
solcellerne, huset, batteriet og elnettet laver lige nu.

Hardware: **Seeed SenseCAP Indicator D1**, en 4 tommer touchskærm med
ESP32-S3. Skærmen kobles på samme wifi som inverteren og læser direkte
fra den over Modbus TCP. Ingen cloud, ingen konto, ingen app.

## Hvad den viser

```
┌────────────────────────────────────────┐
│  Z   Frekvenz              14:32   ᯤ ● │
├───────────────────┬────────────────────┤
│ SOLCELLER      ☀  │ FORBRUG         ⌂  │
│      4,2          │      1,8           │
│      kW           │      kW            │
├───────────────────┼────────────────────┤
│ BATTERI        ▤  │ NETTET          ⚡ │
│      78           │      2,1           │
│      %            │      kW            │
│  ↓ lader 1,4 kW   │  ↑ sælger          │
├───────────────────┴────────────────────┤
│                  ⚙                     │
└────────────────────────────────────────┘
```

## Sikkerhed

**Skærmen ændrer ikke noget på inverteren.** Den læser kun, med
funktionskode 3. Skrivning med funktionskode 16 findes i det fælles
Modbus-lag, men **ingen kode i firmwaren kalder den**: det gør kun
fejlsøgningsværktøjet `zs-probe`, som kører på en bærbar og ikke på
skærmen. Skal der styres på et anlæg, sker det fra en Zbox, som er
bygget til det.

**Skærmen lytter ikke på nogen port.** Der findes hverken `bind`,
`listen` eller `accept` i koden. Alt er udgående.

**Den taler kun med disse steder.** Alt er over TLS på nær
tidsopslaget, som er UDP og ikke bærer andet end et klokkeslæt:

| Værtsnavn | Port | Hvad | Hvor ofte |
|-----------|------|------|-----------|
| `fleet.frekvenz.nu` | 8883 | sender målinger, firmwareversion, inverterens model og serienummer | hvert 2. sekund |
| `www.elprisenligenu.dk` | 443 | henter priser, sender ingenting | en gang i døgnet, og hvert 10. minut hvis det mislykkes |
| `api.github.com` | 443 | spørger om der er ny firmware | hvert 30. minut |
| `release-assets.githubusercontent.com` | 443 | henter selve firmwaren | kun når der er en ny |
| `dk.pool.ntp.org`, `pool.ntp.org` | 123 | stiller uret | ved opstart og derefter sjældent |

Dertil Modbus TCP til inverteren på port 502 på det lokale net. Det er
den fulde liste, så den kan bruges direkte til en firewall-regel.

**Skærmen har en nøgle.** Et klientcertifikat og en privat nøgle i
flash, som den melder sig ind på flåden med. Certifikatet alene er
legitimationen, så det skal behandles som et kodeord: kan nogen læse det
ud af en skærm, kan de melde sig ind som den skærm.

**Firmware skal være underskrevet.** En skærm tager kun imod en
opdatering der er underskrevet med Frekvenz' nøgle. Det er efterprøvet
ved at vende én byte i en bygget fil og se den blive afvist.

### Signeringsnøglen

Nøglen kan **ikke** skiftes over luften. En skærm efterprøver den næste
firmware med nøglen der sidder i den firmware den kører lige nu, så en
skærm signeret med nøgle A tager aldrig imod firmware signeret med nøgle
B. Mistes nøglen, kan ingen skærm nogensinde opdateres igen, og hver
enkelt skal flashes med ledning. Hele begrundelsen står i
`.github/workflows/release.yml`.

Derfor skal den ligge to steder der ikke kan ryge sammen:

1. En delt hælving i adgangskodeboksen, så flere end én person kan nå
   den.
2. En krypteret kopi på en USB-nøgle på en anden fysisk adresse.

En bærbar og en hemmelighed i den samme GitHub-konto er **ét** sted, ikke
to. Filen ligger i `secure/`, som er udelukket fra git, med rettigheder
600 i en mappe med 700.

## Kom i gang

```bash
./tools/setup-toolchain.sh      # ESP-IDF v5.1.7, én gang
source tools/env.sh             # i hver ny terminal

cd firmware
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Test uden hardware

```bash
./tests/host/run.sh                                  # enhedstest
cd tools/fronius-sim && sudo python3 serve.py        # simuleret Fronius
./tools/zs-probe/zs-probe 127.0.0.1                  # se hvad skærmen ville vise
```

Simulatoren har profiler for anlæg uden batteri, uden elmåler, uden
kanalnavne, med flydende tal og med én solstreng:

```bash
sudo python3 serve.py --profile nobattery
```

Port 502 kræver `sudo`. Skal skærmens netværksscanning kunne finde
simulatoren, skal den ligge på 502.

## Mapper

```
brand/          logoer og farver
docs/           registerkort, designsystem, hardware, testplan
firmware/       koden der kører på skærmen
tools/          værktøjskæde, simulator, zs-probe
tests/host/     enhedstest der kører på en Mac
ÆNDRINGER.md    hvad der er lavet, hvornår, og hvorfor
```
