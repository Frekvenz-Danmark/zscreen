# qrcodegen, hentet ind udefra

Project Nayukis QR-Code-generator, C-udgaven. **MIT-licens**, og
licensteksten staar uroert i toppen af begge filer.

Hentet 9. oktober 2026 fra
`https://raw.githubusercontent.com/nayuki/QR-Code-generator/master/c/`

## Hvorfor netop den

Tilmeldingen viser en kode paa seks tegn. En QR ved siden af er ren
bekvemmelighed: montoeren slipper for at skrive dem af. Koden virker
uden den.

Valget stod mellem `espressif/qrcode` fra IDF's komponentregister og den
her. Registeret ville indfoere **managed components** i et projekt der
ikke bruger dem, og komponenten hentes ved bygningen - og vi laaser
med vilje alt, ogsaa IDF-udgaven, saa en udgivelse ikke kan aendre sig
under os. Den her er to filer uden afhaengigheder, og de ligger i git
som alt andet vi bygger.

## Hvad den blev maalt til foer den kom ind

    bygger med projektets flag       rent, ikke én advarsel
      -std=c11 -Wall -Wextra -Werror -Wshadow -Wpointer-arith
      -Wcast-align -Wstrict-prototypes -Wmissing-prototypes

    flash paa ESP32-S3, -Os          7512 bytes i alt
      heraf kode                     ~5,1 kB
      heraf tabeller og tekster      ~2,4 kB

    til sammenligning                1.568.768 bytes fri i partitionen

Altsaa under en halv promille, og det er FOER linkeren smider de
funktioner vi ikke kalder. Oversat med `-ffunction-sections`, saa
`qrcodegen_makeEci`, `qrcodegen_makeNumeric` og resten forsvinder af sig
selv.

## Hvad vi IKKE har aendret

Ingenting. Filerne er som de blev hentet, saa de kan sammenlignes med
kilden og opdateres uden at nogen skal lede efter vores rettelser.
Skal der noget om, skal det staa her.
