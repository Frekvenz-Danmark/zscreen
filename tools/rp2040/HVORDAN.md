# Lyd på skærmen

Skærmen kan bippe når noget er galt. Det kræver to ting, og rækkefølgen
er ligegyldig:

1. `ZS_LYD_ENABLED` sat til `1` i `firmware/main/zs_config.h`
2. Firmwaren her i mappen flashet i skærmens **anden** chip

Mangler den ene, er der ingen lyd. Blinket og baglys-pulsen virker
uanset — de sidder på ESP32'en, som også kører resten af skærmen.

## Hvorfor der skal flashes to chips

Summeren sidder på RP2040'en, på dens GPIO 19. ESP32'en, som kører
zScreen, kan ikke nå den: det eneste der går mellem de to chips er en
seriel ledning.

```
ESP32-S3 GPIO 19  (TX)  ──►  RP2040 GPIO 17  (RX)
ESP32-S3 GPIO 20  (RX)  ◄──  RP2040 GPIO 16  (TX)
115200 baud, 8N1, pakker i COBS
```

Et bip er altså en pakke fra ESP32'en, og det er RP2040'en der laver
lyden.

**Firmwaren fra fabrikken svarer ikke på den pakke.** Den kender den
godt — kommandoen `0xA1` står i Seeeds egen kilde, og de har en
`beep_on()` — men deres pakkemodtager kalder den aldrig. Den har kun ét
tilfælde, `0xA3`, som slukker sensorerne. Det blev målt 13. oktober 2026
ved at læse `examples/indicator_rp2040/indicator_rp2040.ino` i
[Seeed-Solution/SenseCAP_Indicator_RP2040](https://github.com/Seeed-Solution/SenseCAP_Indicator_RP2040).

Så uden det her trin bliver pakken sendt, og ingen svarer. Det er tavst:
der kommer ingen fejl, hverken i loggen eller på skærmen.

## Hvad der mistes

Ingenting på en D1. Seeeds firmware passer sensorer, SD-kort og LoRa, og
en D1 har ikke noget af det — se `docs/hardware.md`. `zscreen_rp2040.ino`
er derfor lille: den passer summeren, og det er det.

Skal en D1S eller D1Pro en dag have lyd, skal `paa_pakke()` i stedet
sættes ind i Seeeds egen firmware. Så bliver sensorerne passet som før.

## Sådan flashes den

Fremgangsmåden er Seeeds, fra
[How To Flash The Default Firmware](https://wiki.seeedstudio.com/SenseCAP_Indicator_How_To_Flash_The_Default_Firmware/).

Først én gang:

1. Installér Arduino IDE
2. **Indstillinger → Additional Boards Manager URLs:**
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
3. **Værktøjer → Board → Board Manager:** søg `indicator`, installér
   *Raspberry Pi Pico/RP2040*
4. **Library Manager:** installér *PacketSerial*

Vi bruger kun PacketSerial. Seeeds egen vejledning beder også om fem
Sensirion-biblioteker og AHT20 — de er til sensorerne, og dem rører
`zscreen_rp2040.ino` ikke.

Og så hver gang:

1. **Hold den indvendige knap nede med en nål.** Seeed skriver "long
   press this internal button using a needle". Hold den mens USB-C-kablet
   sættes i, og slip den derefter
2. Der kommer en disk frem på computeren. Det er beviset på at
   RP2040'en er i programmeringstilstand
3. **Board:** `Seeed INDICATOR RP2040`
4. **Port:** den der kom frem i trin 1-2
5. Åbn `zscreen_rp2040.ino` og tryk upload

**Hvilken af de to USB-C-porte, står der ikke noget om** — hverken i
Seeeds vejledning eller i deres wiki, og det er ikke noget vi gætter på.
Kommer der ingen disk frem, så prøv den anden port. De to chips har hver
sin: Seeed skriver at ESP32'en melder sig som `USB-SERIAL CH340` og
RP2040'en som `USB Serial Device`.

## Virker det

Et kort bip hver gang RP2040'en starter. Det er med vilje det eneste
livstegn: kommer der et bip når du sætter strøm til skærmen, virker både
summeren og firmwaren. Seeeds egen firmware bipper også ved opstart, så
det er ikke en ny lyd for nogen der kender enheden.

**Skærmen selv bipper ikke ved opstart.** En enhed der mister strømmen
klokken tre om natten og kommer igen, skal ikke vække nogen. Og den
bipper slet ikke mellem 22 og 06 — se `ZS_NIGHT_START_HOUR` i
`zs_config.h`, det er de samme timer som baglyset dæmper i.

Vil du høre den uden at vente på en fejl, så tag strømmen fra routeren.
Efter tyve sekunder siger skærmen fra.

## Hvor meget den bipper

Fire gange for én fejl: med det samme, efter fem minutter, efter et
kvarter og efter tre kvarter. Så er der stille, mens beskeden bliver ved
med at stå og pulsere på fladen.

Grunden til at det stopper: en fejl kan vare i dagevis — en inverter til
reparation, et anlæg slukket for vinteren — og en skærm der bipper hvert
femte minut i tre døgn bliver taget ned af væggen. Tallene står i
`ZS_LYD_PAUSER_MS`.
