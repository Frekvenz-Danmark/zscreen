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

Skærmen kan ikke ændre noget på inverteren. Der findes kun
funktionskode 3 i koden, altså kun læsning. Ingen af Modbus' fem
skrive-funktionskoder er implementeret, og de må aldrig blive det.
Skal der styres, sker det fra en Zbox, som er bygget til det.

Skærmen lytter ikke på nogen port, sender ingenting ud af huset, og
har ingen konto eller nøgle. Den eneste udgående trafik ud over Modbus
er tidsopslag til et NTP-ur.

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
