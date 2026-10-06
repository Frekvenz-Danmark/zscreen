## 2026-10-06 14:27

### Fejlsøgningsværktøjet tav om fejl der ikke var plads til
Fundet ved at lede efter samme mønster som de to prisfejl: noget koden
opdager og så ikke gør noget ved.

Jeg målte alle 149 felter i vores strukturer efter om de bliver skrevet
uden nogensinde at blive læst. Tre gjorde. Det ene var `afkortet` i
fejllisten: har inverteren flere samtidige fejl end der er plads til,
sættes flaget, og **ingen læste det**. Så viste `zs-probe` de første
fjorten og tav om resten. En montør kunne rette dem og køre hjem mens
årsagen stod på plads femten.

Det er præcis det modulets egen header advarer imod: "At tie om en fejl
fordi man ikke kender den, er den dårligste af alle muligheder."

Rettet, og prøvet af med en inverter der melder alt på én gang. Nu står
der at listen er klippet, og med én enkelt fejl står der det ikke.

**To ting jeg undersøgte og som ikke var fejl.** De to andre ubrugte
felter betyder intet: opdateringssiden skelner allerede mellem "ikke
søgt endnu" og "nyeste" via sin tilstand, og `has_mppt` er kun
oplysning. Og fejlmodulet bygges ind i firmwaren uden at blive brugt,
siden fejlsiden blev taget ud, men linkeren smider det ud af sig selv.
Målt i den færdige binær: symbolerne er der ikke. Ingen omkostning.

Fejlkodetabellerne blev samtidig gennemgået for dubletter, altså to
poster med samme bit hvor den anden aldrig kunne nås. Fem tabeller, 95
poster, ingen dubletter.

581 enhedstest og 58 ende til ende, alle bestået på Mac og Linux.

## 2026-10-06 14:00

### Gårsdagens elpriser kunne blive stående hele dagen
Samme slags fejl som den forrige, og den slags kommer sjældent alene.

Efter midnat er gårsdagens priser ikke dagens. Skærmen opdagede godt at
de var gamle og gik i gang med at hente nye, men **den fjernede dem
ikke imens**. Lykkedes hentningen ikke, for eksempel fordi nettet var
nede klokken halv et om natten, blev gårsdagens priser stående. Og man
kunne ikke se det: siden viser ingen dato, og den fremhævede time pegede
på gårsdagens tal. De så præcis lige så rigtige ud som rigtige priser.

Prøves der igen hvert tiende minut, så en API der er nede i timevis
betød gårsdagens priser på væggen hele formiddagen.

**Rettet:** er priserne ikke fra i dag, bliver de fjernet med det samme,
og siden siger "Henter dagens priser ...". Den tekstboks fandtes i
forvejen, den blev bare ikke brugt til det her. Hellere sige at vi henter
end at vise noget forkert.

Datosammenligningen er flyttet ud i den fil der kan prøves af på en
almindelig maskine, og har fået 14 tests: samme dag, i går, i morgen,
samme dag sidste måned og sidste år, nytårsnat hvor dag, måned og år
skifter på én gang, enkeltcifrede datoer der skal have nul foran, tom
dato, NULL, skrald, og et ur der er gået helt galt i byen.

581 enhedstest og 55 ende til ende, alle bestået på Mac og Linux.

## 2026-10-06 13:42

### Elprisen var forkert i en time, én nat om året
En rigtig fejl, fundet ved at lede efter den.

Den nat sommertiden slutter, sidste søndag i oktober, findes klokken to
**to gange**: én gang i sommertid og én gang i normaltid. Filen fra
elprisenligenu.dk har begge, 25 poster i alt, og de to timer har som
regel vidt forskellige priser.

Opslaget sammenlignede kun klokketimen og tog den første der passede. Så
i den anden time stod der den første times pris. En hel time med et
forkert tal på væggen, én gang om året, og umuligt at opdage uden at vide
det.

Koden vidste godt at døgnet kan have 25 timer, der er plads til dem og
der står en note om det. Det var kun selve opslaget der ikke vidste det.

**Rettet** ved at gemme forskydningen fra UTC sammen med hver time.
Tidsstemplet fra kilden har den i forvejen, `+02:00` eller `+01:00`, vi
smed den bare væk. Opslaget kræver nu at både klokketimen og
forskydningen passer, og falder tilbage til kun klokketimen hvis en
kilde ikke oplyser den, så gemte priser fra før stadig virker.

Vores egen forskydning regnes ud af forskellen mellem lokal tid og UTC.
Ikke med `tm_gmtoff`: den er en udvidelse, og den findes ikke i ESP-IDF's
newlib. Efterset, ikke antaget.

Beslutningen ligger nu for sig selv uden ur, så den kan prøves af, og der
er 21 tests på den: den almindelige dag, natten med 25 timer hvor begge
toere skal rammes rigtigt, natten med 23 timer hvor klokken to slet ikke
findes, gamle data uden forskydning, og det der ikke giver mening.
Testene er prøvet ved at sætte den gamle opførsel tilbage, og de fanger
den.

Prissiden blev efterset for det samme og er i orden: den placerer kun
akse-tallene 0, 6, 12 og 18, og ingen af dem er dobbelt den nat.

567 enhedstest og 55 ende til ende, alle bestået på Mac og Linux.
Version 0.11.0.

## 2026-10-06 13:25

### Fem tests mere, og en grænse vi skal kende
**Inverteren lyver om hvor mange DC-kanaler den har.** N-feltet i model
160 siger otte kanaler, men modellen er kun lang nok til fire. Uden
vagten i koden ville skærmen læse ud over modellens data og vise gammelt
indhold fra bufferen som en rigtig solstreng, med et tal der ser helt
plausibelt ud. Vagten findes og virker: der læses præcis de fire der er
plads til, og solen er stadig rigtig. Nu har den en test, så den ikke kan
forsvinde ved et uheld.

**Inverteren sidder ikke på unit 1.** Et anlæg med flere invertere bag én
Datamanager lægger dem på unit 1, 2, 3. Målt: søgningen finder **kun**
unit 1. Vælger man unitten i hånden virker alt, inklusive elmåleren på
sin egen unit ved siden af. Det er en grænse vi skal kende, ikke en fejl:
at scanne flere units ville gange søgetiden med antallet, og en bedre vej
er at prøve et par ekstra units på den adresse hvor der ÉR fundet en
inverter. Det kræver at listen kan rumme en unit per fund, og det er en
beslutning og ikke en rettelse.

Undervejs rettede jeg to ting i simulatoren: den skrev uden for sin egen
tabel når N løj, og den lod en nægtet model ramme elmåler-unitten også.
Begge dele var fejl i testværktøjet, ikke i firmwaren.

546 enhedstest og 55 ende til ende, alle bestået på Mac og Linux.

## 2026-10-06 12:37

### Ti nye ende til ende-tests mod ting der sker i virkeligheden
Tre scenarier der ikke var dækket, og som kunne gemme en fejl. Ingen af
dem fandt én, men to af dem prøvede kode af som aldrig havde været rørt.

**Noget andet taler Modbus på samme port.** Et kundenetværk har ofte en
varmepumpe, en PLC eller en energimåler på 502. De svarer pænt på
funktionskode 3, men der står ikke "SunS" i registrene. Kaldte søgningen
dem invertere, ville kunden vælge sin varmepumpe fra listen og undre sig
over at der aldrig kommer tal. Prøvet med tre slags: lutter nuller,
skrald, og en der tager imod men aldrig svarer. Alle tre afvises, og den
tavse hænger ikke søgningen.

**En model står i kæden men kan ikke læses.** Det sker efter en
firmwareopdatering på inverteren, hvor modellisten og indholdet ikke
følges ad. Nægtes hele modellen, stopper kæden der, solen står som streg
i stedet for nul, resten læses, og skærmen siger selv at listen ikke
kunne læses færdig. Nægtes kun dataene, kan hovedet læses, og så hentes
hele kæden alligevel: alle fire tal er der, og kun det den model bærer
forsvinder.

**Elmåleren i inverterens egen kæde.** Den kodevej har ligget i firmwaren
hele tiden og var aldrig prøvet af, fordi simulatoren altid lagde måleren
på sin egen unit. Den virker: samme fire tal som når måleren ligger for
sig, og skærmen ser selv forskellen.

Simulatoren har fået tre nye muligheder til det: `--naegt`,
`--naegt-kun-data` og `--maaler-i-kaeden`, plus et lille værktøj
`ikke-inverter.py` der spiller en anden slags Modbus-enhed.

546 enhedstest og 50 ende til ende, alle bestået på både Mac og Linux.

## 2026-10-06 11:58

### Gennemgang af hele kodebasen med analyseværktøjer
Kørt `cppcheck` over al firmware og gcc's egen analysator over hvert
modul der kan oversættes på en almindelig maskine. Fire fund, og to af
dem var værktøjerne der tog fejl.

**Formatfejl i Modbus-laget.** `%u` med et fortegnsbehæftet tal:
`expect_count` er `uint16_t`, og `* 2` gør den til `int`. Harmløs i
praksis, men det er en rigtig uoverensstemmelse. Rettet.

**Afkortning af strenge var ikke testet.** cppcheck påstod at grænsen i
`zs_ss_dec_string` aldrig rammes. Det er netop den grænse der står mellem
et serienummer fra inverteren og en buffer på stakken, så påstanden blev
afgjort med en test i stedet for en diskussion: grænsen rammes, den
virker, og nu er den beskyttet mod at forsvinde igen. Syv nye tests.

**Påstået lækket filbeskrivelse i scanningen.** gcc's analysator mente at
en socket slap væk. På en ESP32 med en håndfuld sockets ville det slå
søgningen ud efter få forsøg, så det blev målt: tre fulde scanninger,
samme antal åbne filbeskrivelser før og efter. Falsk alarm, fordi
analysatoren ikke kan følge en filbeskrivelse der gemmes i et array.

**Duplikeret include-blok.** `zs_config.h`, `zs_app.h` og
`zs_screen_setup.h` stod to gange i indstillingssiden. Hele kodebasen
gennemgået for det samme bagefter: ingen flere.

### Flådens status vises nu på skærmen
Fire funktioner i flåde-modulet var aldrig blevet taget i brug, og det
var grunden: en tekniker kunne stå foran skærmen uden at kunne se om den
var med i flåden, for svaret lå kun i en log han ikke kan nå. Detaljer
viser nu "Flåde" og, når den er indmeldt, dens flåde-id. Er den slået fra
eller mangler certifikatet, står der "Slået fra" og ikke en fejl, for det
er to forskellige ting.

Struktur målt samtidig: ingen TODO eller FIXME nogen steder, ingen løse
tal uden for den centrale konfiguration i det der kan skrues på, og 184
offentlige funktioner hvoraf 13 ikke bruges uden for deres egen fil.

546 enhedstest og 40 ende til ende, alle bestået.

## 2026-10-06 09:54

### Genfindingen stjæler ikke længere skærmen fra brugeren
To fejl i det jeg selv skrev i går, fundet ved at læse efter.

**Knaptryk forsvandt.** En søgning efter inverteren tager omkring tyve
sekunder, og i det tidsrum læser hovedopgaven ikke kommandoer. Køen
venter aldrig: de første otte tryk lagde sig i kø og blev udført alle på
én gang bagefter, og tryk nummer ni og frem forsvandt i stilhed. Og det
er ikke et opfundet tilfælde: søgningen går i gang netop når skærmen
siger at der ikke er forbindelse, hvilket er præcis når man går ind i
indstillingerne for at se hvorfor.

Nu giver søgningen op så snart der ligger et tryk i kø. Brugeren vinder,
og vi prøver igen af os selv.

**Baggrundssøgningen skrev i opsætningsskærmen.** Den brugte samme
fremdriftsvisning som søgningen under opsætning, altså en skærm brugeren
slet ikke står på. Den har nu sin egen, der ikke rører brugerfladen.

539 enhedstest og 40 ende til ende, alle bestået.

## 2026-10-06 03:01

### Vi kan altid nå skærmene, og en dårlig opdatering rammer ikke alle
Skærmen hentede allerede selv ny firmware fra GitHub hvert 30. minut, og
den ringer **ud**, så den virker bag kundens router uden at vi skal ind.
Men alle skærme tog nyeste version. En udgave der starter fint, kører sine
to minutter og **først derefter** er ubrugelig, for eksempel sort skærm
eller intet WiFi, ville ramme hele flåden inden for en halv time, og
tilbagerulningen redder os ikke, for den nåede at blive godkendt.

**Nu kan vi sætte en målversion per skærm.** Vi skriver den i
dashboardet, skærmen lytter og retter sig efter den. Tomt betyder "følg
nyeste", som alle gør indtil nogen siger andet. Dermed kan en opdatering
rulles ud til én skærm først, og man kan se at den stadig lever, før
resten får den.

**Og den må gå begge veje.** Uden et mål opdaterer vi kun opad, så en
udgivelse med et forkert nummer ikke kan sende flåden baglæns og frem og
tilbage for evigt. Med et mål er der ingen ring: når den kørende udgave
er lig målet, sker der ikke mere. Derfor er en bevidst nedgradering kun
mulig her, og det er netop det der gør den til en nødbremse.

**Skærmen melder nu hvilken version den kører.** Uden det kunne vi sætte
en målversion, men ikke se om den nåede frem.

Målversionen er den eneste vej fra serveren og ned i skærmen, og den
ender inde i en adresse vi henter firmware fra. Derfor efterses teksten
tegn for tegn: nøjagtig tal.tal.tal med et valgfrit v foran. Skråstreg,
punktum-punktum, spørgsmålstegn, kolon og alt andet forkastes, og så
bliver det gamle mål stående. 22 nye tests på netop det.

**Rettigheder, så ingen kan sætte sin egen målversion.** Skærmen må læse
feltet men ikke skrive det, og omvendt for versionsfeltet. Målt på en
rigtig server: skærmen forsøgte at sætte sig selv til 9.9.9 og blev
afvist.

**En fælde der kostede en halv time.** Ved allerførste indmeldelse af en
ny skærm lukker serveren forbindelsen et sekund efter at den har sagt
success. I loggen står "User asset links have changed for a connected
user with active subscriptions". Derfor skal alt der hænger på
forbindelsen, også abonnementet på målversionen, sættes op inde i
indmeldelsen og ikke én gang ved opstart. Det står nu i zs_fleet.h som
den sjette fælde.

**Scanningen leder nu på den port kunden har valgt.** Den ledte kun på
502, selv om indstillingerne altid har haft et portfelt. En inverter på
en anden port kunne vælges i hånden, men aldrig findes af søgningen. Det
kom frem fordi CI ikke kunne binde port 502: den er under 1024 og dermed
privilegeret, og det havde jeg skrevet det modsatte om.

**Tre forsøg på at sove under et sekund.** Scanningen holder en lille
pause mellem hvert opkald. `usleep` blev fjernet af POSIX i 2008 og er
skjult på Linux, `nanosleep` findes slet ikke i ESP-IDF, og den første
rettelse, `_POSIX_C_SOURCE 200809L`, var netop den der tog `usleep` væk.
Nu bruges `select`, som er der alle tre steder og ikke kræver nogen
erklæring. Alle tre udfald målt i en gcc-beholder og med en rigtig
firmware-oversættelse, ikke gættet.

**Oprydning undervejs**
- De to minutter før en ny firmware meldes i orden stod som et løst tal
  midt i koden. Nu i den centrale konfiguration med begrundelsen
- Lyt-emnet blev lavet ved at klippe "write" ud af skrive-emnet med
  memmove. Nu bygges begge samme sted af samme funktion, og der er tests
  på at de ikke kan forveksles
- Valideringen af versionsnumre lå i en fil der ikke kan testes på en
  almindelig maskine. Flyttet til versionsmodulet, som er rent
- En ende til ende-test fejlede en gang imellem uden at der var noget
  galt: solen nåede at flytte sig mellem at simulatoren sagde hvad den
  havde, og skærmen nåede at læse det. Simulatoren kan nu stå stille, og
  sammenligningen bruger det. Tre kørsler i træk uden fejl
- To nye testrækker om versioner dublerede noget der allerede fandtes.
  Fjernet igen

539 enhedstest og 40 ende til ende, alle bestået. Version 0.10.0.

## 2026-10-05 11:33

### Inverteren bliver fundet igen når den har skiftet IP-adresse
En IP-adresse er ikke en identitet. Routeren uddeler dem på lån, og en
inverter der har været slukket længe nok, eller som bliver genstartet
samtidig med routeren, kan komme tilbage på en anden adresse. Skærmen
huskede kun adressen, så den stod og bankede på den gamle for evigt, med
voksende pause, og kunden skulle selv ind i indstillingerne og scanne
forfra.

Den anden halvdel var værre, og den så ud som om alt virkede: adressen
kan imens være givet til en **anden** enhed. Er det også en inverter,
svarer den, skærmen forbinder, og kunden ser en fremmed inverters tal som
om det var anlægget på taget. Kurverne ser rigtige ud. Man kan se på
sådan en skærm i måneder uden at opdage noget.

**Identiteten er nu serienummeret**, fra SunSpec model 1 felt SN. Det
sidder i hardwaren og skifter ikke. Det blev i forvejen læst ved hver
forbindelse, så det koster ikke et eneste ekstra kald. Afkodningen er
efterset og er deterministisk: samme registre giver altid samme streng,
så en sammenligning tegn for tegn er til at stole på.

Hvad der sker nu, i den rækkefølge det koster:

1. Prøv den gemte adresse. Tager under et sekund, og i det almindelige
   tilfælde, hvor inverteren bare var slukket et øjeblik, er vi færdige.
2. Svarer der en inverter med et **andet** serienummer, bruges den ikke,
   og der ledes straks videre. Ikke om tre forsøg: vi ved at adressen er
   forkert.
3. Efter tre mislykkede forsøg scannes hele undernettet, med den gemte
   adresse først.
4. Der vælges efter serienummer, ikke efter hvad der tilfældigvis svarer.
5. Ny adresse gemmes med det samme, så en strømafbrydelse bagefter ikke
   koster en ny søgning.

**Og vi gætter ikke.** Kender vi et serienummer og finder vi det ikke, så
siges der fra i stedet for at tage den eneste inverter der var. I en
lejlighed eller et rækkehus kan naboens inverter sagtens svare, og at
vise naboens produktion som kundens er værre end at sige at anlægget
ikke kan findes.

Beslutningen ligger for sig selv uden netværk, så den kan prøves af uden
hardware. Der er 20 nye enhedstest på den, og fire nye ende til ende mod
den simulerede Fronius over en rigtig TCP-forbindelse, blandt dem den
farlige: en fremmed inverter på præcis den adresse vi havde gemt bliver
afvist. Værktøjet `zs-probe --genfind` kan køre motoren i hånden mod et
rigtigt net.

Vælger kunden selv en inverter i indstillingerne, følger serienummeret
med fra scanningen. Ellers ville den nye kontrol afvise netop det valg
kunden lige havde truffet.

**Oprydning:** antallet af invertere vi kan holde på stod to steder, 12 i
zs_config.h og 8 i zs_discovery.h, og de 12 blev aldrig brugt. Nu ét tal
ét sted.

506 enhedstest og 40 ende til ende, alle bestået. Version 0.9.0.

## 2026-10-05 01:36

### Lasttest med tres skærme, og tre fund i flødestyringen
Tres enheder kørt mod en rigtig OpenRemote med hver sit certifikat, og
derefter en gennemgang af det der gik skævt.

**Indmeldelsen skal spredes.** Meldte tres enheder sig ind inden for fem
sekunder, løb serverens Keycloak-kald tørt: 49 tidsudløb og 109 tvungne
afbrydelser på et minut. Og hver afbrydelse fik enheden til at melde ind
igen med det samme, så stormen fødte sig selv. Det samme spredt over
femten sekunder gav nul af begge. Skærmen venter nu et tilfældigt stykke
tid mellem nul og et minut før den første indmeldelse. Alle skærme på
samme gade får strøm tilbage i samme sekund efter et svigt, og det er
præcis det tilfælde der gik galt. Prisen er at en skærm kan være op til
et minut om at vise sig i flådeoversigten. Tallene på væggen kommer med
det samme som før.

**Afvist for evigt.** Den værste af de tre. Bliver et certifikat afvist,
svarer serveren UNAUTHORIZED og holder så forbindelsen åben, den lukker
den ikke. Vi forsøgte kun indmeldelse når abonnementet var nyt, altså én
gang per forbindelse, så der kom aldrig et nyt forsøg. Skærmen var afvist
for evigt, også efter at certifikatet var rettet på serveren, indtil
nogen tog strømmen. Det samme gjaldt en afsendelse der ikke gik igennem
og et svar der aldrig kom. Nu er der én regel styret af tiden i stedet
for tre halve: er vi forbundet, mangler vi et enheds-id, og er uret
gået, så prøver vi igen. Reglen ligger i den del af modulet der kan
testes på en almindelig maskine, og der er otte nye tests på den.

**Alle forbandt igen i takt.** esp-mqtt venter præcis det samme tal hver
gang, der er ingen voksende pause og ingen spredning indbygget. Havde
alle skærme det samme tal, ville tres skærme som serveren afbryder i
samme øjeblik forbinde igen i samme øjeblik, og blive ved med det. Hver
skærm trækker nu sit eget interval mellem ti og tyve sekunder ved opstart
og beholder det.

**Lagerplads.** Målt at et datapunkt fylder 297 bytes. Med tusind skærme
der sender fem felter hvert 2. sekund er det 216 millioner rækker om
dagen. OpenRemote leverer databasen med syv dages chunks og komprimering
efter syv dage, så halvdelen af de fjorten dage ligger ukomprimeret,
omkring 490 GB. Sat til ét døgn begge steder bliver det omkring 150 GB.
Takten er uændret, det er serverens opsætning der er rettet.

Alt målt, intet gættet: 486 enhedstest og 36 ende til ende, alle
bestået. Version 0.8.0.

## 2026-10-04 20:58

### Bug check af floedestyringen: fire fund
Gennemgang af zs_fleet.c, scenarie for scenarie.

**Kapløb mellem to opgaver.** esp-mqtt skriver tilstanden fra sin egen
opgave, hovedopgaven laeser den naar den sender. Uden laas kunne
hovedopgaven laese et enheds-id der var halvt overskrevet, eller se
"klar" med et id der netop var ryddet. Resultatet ville vaere et emne der
peger paa en anden enhed eller ingen. Der er nu en laas, og den holdes
kun om kopieringen af nogle faa felter. Hovedopgaven tager en KOPI under
laas og sender derefter uden, saa laasen aldrig holdes over netvaerket.

**Uendelige forsoeg ved afvisning.** Blev certifikatet afvist, sendte vi
det igen ved hver genforbindelse, altsaa hvert tiende sekund for evigt.
Med tres skaerme bliver det stoej paa serveren uden at nogen bliver
klogere. Nu ventes der ti minutter, saa en rettelse paa serveren bliver
opdaget af sig selv, men ingen skal ud og genstarte en skaerm.

**Laekage.** Kunne MQTT-klienten ikke startes, blev certifikaterne
liggende i heapen, og et nyt forsoeg ville laegge endnu et saet. Rettet.

**Noeglen var et krav men blev aldrig brugt.** Modulet naegtede at starte
uden den private noegle og sendte den aldrig nogen steder. Den er nu
valgfri.

Og det sidste afdaekkede noget vigtigere: i dette forloeb er
CERTIFIKATET ALENE legitimationen. Serveren tjekker at det er signeret
af vores CA og at navnet passer, men beder aldrig om bevis paa at vi har
den private noegle. Et certifikat er normalt offentligt, men her skal det
behandles som en hemmelighed paa linje med et kodeord: kan nogen laese
det ud af en skaerm, kan de melde sig ind som den skaerm.

Noeglen laeses alligevel hvis den er der, saa vi kan skifte til mTLS paa
en egen port en dag uden at skulle ud til enhederne. Maalt: mTLS virker
IKKE gennem deres HAProxy, for den afslutter TLS selv, saa
klientcertifikatet aldrig naar brokeren.

### Hovedafbryderen virker
Med ZS_FLEET_ENABLED paa 0 bygger firmwaren uden advarsler, og der er
NUL forekomster af "writeattributevalue" i den byggede fil. Koden er
vaek, ikke bare utilgaengelig.

## 2026-10-04 19:46

### Floedestyring, fase 2: firmwaren
Skaermen kan nu melde sig ind paa Frekvenz' egen OpenRemote-server og
sende sine maalinger. zs_fleet.c staar for det.

Alt om den staar ét sted i zs_config.h: om den er med, hvilken server,
og hvor ofte der sendes. Takten maales i AFLAESNINGER og ikke i
sekunder, saa den foelger ZS_POLL_INTERVAL_MS af sig selv og de to ikke
kan komme ud af trit. 1 betyder hver gang vi har laest inverteren,
altsaa realtid.

Det vigtigste: skaermen virker praecis lige saa godt uden serveren. Er
der ingen forbindelse, er serveren nede, eller mangler certifikatet,
viser skaermen anlaegget som den altid har gjort. Er floedestyringen
slaaet fra, findes koden ikke i den byggede fil.

Certifikat og noegle ligger i lageret og ikke i firmwaren. De er
forskellige paa hver enhed, og firmwaren er den samme paa alle. Laa de i
firmwaren, ville alle skaerme have samme identitet, og saa kunne den ene
skrive i den andens anlaeg. Enhedens navn udledes af chippens egen
MAC-adresse fra eFuse, som ikke kan aendres.

Serveren kan tjekkes mod et privat CA fra lageret i stedet for Mozillas
rodliste, fordi en selvhostet server kan have sit eget. Der findes
bevidst INGEN mulighed for at springe tjekket over: et saadant flag
ville foer eller siden slippe med i en udgivelse.

### Indmeldelsens JSON er trukket ud og testet
Indmeldelsen staar og falder paa at et certifikat paa halvanden kilobyte
bliver undsluppet rigtigt til JSON. Er ét linjeskift forkert, afviser
serveren os, og det ville vi foerst opdage ude hos en kunde.

Derfor ligger den i zs_fleet_msg.c uden afhaengigheder, med 25 tests:
at der ikke er raa linjeskift i beskeden, at der kommer lige saa mange
ud som ind, at begge markoerer er med, og at en for lille buffer giver
NUL i stedet for en afkortet besked. Det samme for emnet der skrives
til: et afkortet emne ville skrive i et andet felt eller i en anden
enhed.

### To fejl i vores eget header-tjek
Det meldte at zs_fleet.h manglede en include-guard. Den var der, men
tjekket kiggede kun i de foerste 40 linjer, og headeren har en lang
forklaring foerst. En veldokumenteret header skal ikke straffes.

Da jeg rettede det, indfoerte jeg to nye: et grep uden fund draebte hele
scriptet, fordi set -euo pipefail er slaaet til, og mine
"#ifndef ZS_FLEET_ENABLED" blev opsamlet som om de var include-guards.
Begge rettet, og tjekket er efterproevet paa en header uden guard og paa
en hvor indholdet staar foer guarden.

## 2026-09-23 17:13

### Fejlkode-siden er taget ud
Swipe-raekken har nu tre sider: de fire kasser, energiflow og elprisen.
Prikkerne regner sig selv ud fra antallet og midtstiller sig, saa der var
ikke andet at rette: de staar nu med 195 px luft i hver side, og
tryk-omraaderne overlapper stadig ikke.

Selve fejlkoderne er der stadig. zs_status.c oversaetter Fronius' bits til
dansk som foer, og E2E-testen proever dem stadig. Det er kun siden i
dashboardet der er vaek.

### Version 0.7.0

## 2026-09-23 16:51

### Smaa tal flakker ikke laengere
Et anlaeg staar aldrig helt stille. Maaleren svinger nogle faa watt frem
og tilbage, og uden en graense skiftede tallet paa vaeggen hvert andet
sekund mellem 12, 8 og 15 watt. Et tal der aldrig staar stille traekker
oejet til sig hele tiden, og man kan ikke se paa det om der sker noget
vaesentligt.

Under 50 watt staar der nu 0 W. Halvtreds er valgt fordi det er under en
enkelt paere i et moderne hus, altsaa under det man overhovedet kan
maerke.

Faelden ved sadan en graense er at den skal gaelde BAADE tallet og
retningen. Rundede vi kun tallet, ville der komme til at staa "0 W" og
"saelger" ved siden af hinanden, og det ser ud som en fejl.

Graensen laa i forvejen to steder som hver sin kopi af IDLE_W paa 25
watt, én i zs_screen_home.c og én i zs_flow.c, og de styrede pile og ord.
Nu er der ÉN graense, ZS_DEADBAND_W i zs_config.h, og baade tallet,
pilene og ordene henter den samme. Der er en test der tjekker at de
foelges ad.

### Version 0.6.0

## 2026-09-23 16:38

### Soegningen taaler nu at en pakke gaar tabt
Pausen mellem hvert connect loeste den systematiske fejl, men den daekker
kun den ene aarsag. Wifi taber ogsaa pakker tilfaeldigt: et andet apparat
sender samtidig, signalet dykker, aksesspunktet har travlt. Derfor proeves
hver adresse nu igen hvis den ikke svarede foerste gang, og anden runde
roerer kun dem der ikke allerede har svaret.

Det er efterproevet at gentagelsen IKKE kan staa alene: med pausen sat til
nul finder soegningen stadig ingenting, ogsaa med to forsoeg. Byger af
connect-kald bliver ved med at druknes, uanset hvor mange gange man
gentager dem. De to ting daekker hver sin fejl, og der skal begge til.

Pausens stoerrelse er ogsaa maalt, ikke gaettet:

    1 ms -> 0 fundet        5 ms -> 1 fundet
    2 ms -> 0 fundet       10 ms -> 1 fundet
    3 ms -> 1 fundet       20 ms -> 1 fundet

Graensen ligger ved tre millisekunder, og vi bruger ti. Tre gange margen,
saa det ogsaa holder paa et travlt net eller med svagere signal.

En hel gennemgang tager nu 12 sekunder mod 6. Det sker én gang under
opsaetningen, saa det er billigt for at vaere sikker paa at finde
inverteren i foerste forsoeg.

### En soegning uden ledige sockets siger det nu
Kunne der ikke skaffes en socket, sprang koden bare adressen over uden et
ord. En soegning der ikke fandt noget, saa praecis ud som en der gik godt.

### Version 0.5.0
Sendes som udgivelse, saa skaermene henter den selv over netvaerket.

## 2026-09-23 15:59

### Soegningen fandt aldrig inverteren
Skaermen gennemgik hele undernettet og meldte nul, mens der stod en
Fronius Symo GEN24 paa 192.168.1.100 med port 502 aaben.

Aarsagen var ikke den man ville gaette. Maalt paa enheden mod det
rigtige net:

    12 samtidige, 250 ms, ingen pause   ->  0 fundet
    12 samtidige, 250 ms, 10 ms pause   ->  1 fundet
    12 samtidige, 1200 ms, ingen pause  ->  0 fundet
     4 samtidige, 800 ms, ingen pause   ->  0 fundet
     4 samtidige, 800 ms, 10 ms pause   ->  1 fundet
     1 ad gangen, 300 ms                ->  1 fundet

Hverken laengere ventetid eller faerre samtidige hjaelper. Det eneste
der virker er en pause MELLEM kaldene til connect.

connect vender tilbage med det samme, men SYN-pakken skal videre gennem
lwIP og ud af wifi-senderen. Fyrer man tolv af i en tot, er der ikke
sendebuffere nok, og de fleste bliver smidt vaek uden at nogen faar
besked. lwIP proever foerst igen efter flere sekunder, og da har vi for
laengst givet op.

Ti millisekunder mellem hvert kald loeser det. Hele undernettet tager nu
6 sekunder mod 5 foer. Efterproevet paa enheden: den finder inverteren.

### SOLCELLER stod paa nul paa et anlaeg der producerede
Samme inverter melder DCSt som 65535, altsaa "ikke implementeret", paa
hver eneste kanal. Vores regel var "tael kun med hvis inverteren siger
at kanalen leverer", og saa blev begge solstrenge kasseret. SOLCELLER
stod paa 0 W mens de leverede 2429 og 2543 watt.

Batteriet virkede, fordi det brugte den modsatte regel: "med mindre vi
ved at den er stoppet". Det var forskellen mellem de to regler der var
fejlen, ikke reglen selv.

Nu er der ÉN regel, og den er den sikre: vi udelader kun naar
inverteren siger noget der betyder stoppet. En tilstand vi ikke kender,
og en der ikke er udfyldt, taeller med. Vi har maalingen, og vi har
ingen grund til at kassere den.

Efter rettelsen viser samme anlaeg 463 W sol, 2,6 kW forbrug, 3,3 kW til
batteriet og 5,5 kW fra nettet. Det stemmer: 463 minus 3300 plus 5500
giver 2663.

### Version 0.4.0

## 2026-08-26 20:45

### Tema vaelges nu paa en side, som prisomraade
Under Indstillinger staar der en raekke der hedder Tema og viser hvad der
er valgt. Trykker man paa den, aabner en side med én stor knap pr. tema,
praecis som DK1 og DK2 i opsaetningen. Det valgte har en tykkere kant i
accentfarven, saa man kan se hvor man staar.

Knappen findes ét sted, zs_choice_create i zs_theme.c. Prisomraade-siden
brugte foer sin egen kopi. Nu er der én, og de to sider kan ikke komme
til at se forskellige ud.

### Tredje tema: roedt
Bund #D73338 med hvid skrift. Moerkt er stadig standard og det skaermen
starter i.

To ting maatte laves om for at det kunne holde:

Logoet foelger nu en tabel i stedet for et spoergsmaal om temaet er lyst.
Skal et tema en dag have sit helt eget logo, er det den tabel der
udvides, og intet andet sted skal roeres.

Skriften paa statusmaerket er blevet en del af paletten. Foer var den
altid bundfarven, hvilket virkede saa laenge de to temaer lignede
hinanden. Paa roed bund duer det ikke: maerkerne er vendt om til lyse
flader med moerk skrift, fordi moerke maerker ikke kan skelnes fra
bunden, og lyse med lys skrift ikke kan laeses.

Vaer opmaerksom paa én ting ved den roede: paa en maettet roed i den
lysstyrke er det kun naesten hvid tekst der naar de 4,5:1 som smaa
bogstaver kraever. Derfor har temaet mindre forskel mellem overskrift og
etiket end de to andre, og groen og gul er kun en anelse toenet. "Dyrt
lige nu" er helt hvid: en roed tone paa roed bund naar kun 4,44 og ville
alligevel forsvinde i bunden. Ordene baerer betydningen. Det er en foelge
af farven, ikke en forglemmelse.

### To ting laast fast saa de ikke kan glide fra hinanden
Kontrast-tjekket fandt temaerne i en liste skrevet i haanden, saa den
roede palet blev ikke maalt foerste gang. Det laeser nu opregningen, og
et nyt tema kan ikke snige sig uden om maalingen.

Lageret klemte tema-tallet til nul hvis det var over 1, saa det roede
ville blive nulstillet ved hver genstart. Graensen staar nu ét sted, og
en _Static_assert stopper oversaettelsen hvis nogen tilfoejer et tema
uden at rette den. Afproevet ved at bryde den med vilje.

## 2026-08-26 19:07

### Skaermen startede demoen af sig selv
Halvandet sekund efter opstart stod der "demo startet" i loggen, uden at
nogen havde roert skaermen. Det skete ikke hver gang, hvilket er den
vaerste slags: den slags fejl finder man ikke ved at proeve.

Aarsagen sad i beroeringen. Seeeds egen kode har en note om at FT-kredsen
kan svare 0xff foer det foerste tryk, men den goer ikke noget ved det.
Maalt paa enheden lige efter opstart melder kredsen raa koordinater som
12204, 50336 og 65531, hvor gyldige vaerdier er 0 til 479.

Koden regnede 480 minus det tal og gav resultatet videre. Med 65531 giver
det -65051, som ikke kan vaere i den int16 LVGL bruger, saa tallet folder
rundt og lander et tilfaeldigt sted paa skaermen. Blev "pressed" sandt et
enkelt oejeblik, fik LVGL et tryk paa et vilkaarligt punkt, og hvad der
end laa der blev trykket paa. Paa velkomstsiden laa "Se demo".

Nu taeller et tryk kun hvis baade x og y ligger indenfor skaermen. Ellers
melder vi sluppet. Vi kaster hellere et rigtigt tryk vaek end at opfinde
ét.

Samtidig er der trukket én fra: skaermen er 480 pixels bred, saa den
hoejeste gyldige koordinat er 479. Foer kunne et tryk i venstre kant give
480, altsaa en pixel udenfor.

Efterproevet: to opstarter i traek uden at demoen startede. Fejlen var
uregelmaessig, saa det er ikke i sig selv et bevis. Beviset er maalingen:
de tal der blev sendt videre foer, kan ikke laengere komme igennem.

## 2026-08-26 18:43

### Demoen er tilbage, med tal der kan lade sig goere
ZS_DEMO_ENABLED staar paa 1 igen. Knappen "Se demo" er tilbage paa
velkomstsiden.

Uret i toplinjen viser nu maskinens klokkeslaet, ikke demoens. Demoen
koerer et doegn paa tre minutter, saa dens eget ur sprang et kvarter frem
hvert andet sekund. Det ligner en fejl, ikke en fremvisning. Tallene i
kasserne foelger stadig demoens doegn.

Den fremhaevede time paa prissiden saettes af den samme funktion som
bruges til rigtige priser, saa pilen peger paa det klokkeslaet der staar
i toppen. Foer fulgte den demoens ur og stod et andet sted.

### Tallene er et dansk parcelhus, ikke et kraftvaerk
Solen toppede paa 7000 W, altsaa 100 procent af maerkeeffekten, og gav
60 kWh paa et doegn. Det svarer til 3175 kWh pr. kWp om aaret. EU's
PVGIS siger 985 for et anlaeg som det her ved Aarhus, altsaa tre gange
for meget.

Nu topper den paa 5500 W, som er 79 procent. Et panel yder sjaeldent sin
maerkeeffekt: solen staar lavt paa 56 grader nord, og panelerne bliver
varme om sommeren. Doegnet giver 37 kWh, hvilket er en klar dag i juni.
PVGIS' 31 kWh er et maanedsgennemsnit med graavejrsdagene talt med.

Spotpriserne laa paa en krone som bund og gik til 1,70. Nu er bunden 20
oere og toppen omkring en krone, som en almindelig dag paa det danske
marked.

### Nettet stod paa nul hele natten
Batteriets forsinkelse havde en tidskonstant paa 90 sekunder, men hvert
skridt i demoen er 960 sekunder. Batteriet var altsaa fremme paa foerste
skridt, daekkede forskellen mellem sol og forbrug praecis, og NETTET stod
paa nul. En rigtig regulering haenger altid lidt bagefter, og der loeber
hele tiden nogle hundrede watt til eller fra nettet. Tidskonstanten er nu
900, saa batteriet naar to tredjedele af vejen pr. skridt.

### 45 nye tests paa demoens tal
Det vigtigste af dem: forbrug skal vaere lig inverter plus net, i hvert
eneste skridt, tre doegn igennem. De fire tal kan ikke vaelges frit, og
passer regnestykket ikke, staar der fire tal paa skaermen der tilsammen
siger noget umuligt. Afvigelsen er under en watt.

Derudover: solen topper mellem 5 og 6 kW og bliver aldrig negativ,
doegnet giver 30 til 45 kWh, forbruget 11 til 22 kWh, batteriet holder
sig under 5 kW, et fuldt batteri lader ikke videre og et tomt aflader
ikke videre, og priserne er billigst midt paa dagen og dyrest morgen
eller aften.

zs_price_update_now er flyttet ud i sin egen fil. Den bruger kun time.h,
mens resten af prismodulet trAEkker hele HTTP-laget med, og nu kan baade
den og demoen oversaettes paa en almindelig maskine.

### Version 0.3.0
Nummeret er hoejnet fordi koden er aendret siden v0.2.0 blev udgivet. Der
er ikke lavet en ny udgivelse: demoen er til fremvisning, ikke til
kundernes skaerme.

## 2026-08-25 23:47

### Version 0.2.0
version.txt stod stadig paa 0.1.0 efter tre commits med rigtigt arbejde.
Udgivelsen v0.1.0 var bygget fra en aeldre commit, saa der fandtes to
forskellige firmwares der begge kaldte sig 0.1.0. En skaerm der spurgte
efter opdateringer ville have faaet at vide at den var opdateret, og
aldrig have hentet det nye. Nummeret er hoejnet, og der laves en ny
udgivelse.

### Tre ubrugte variabler naar demoen er slaaet fra
next_demo_step, last_demo_ms og s_demo_restart blev staaende udenfor
kontakten. Bygningen gav advarsler, og CI stopper paa advarsler i vores
egen kode. De ligger nu inde i kontakten, og det er efterproevet at der
er nul advarsler BAADE med demoen slaaet til og fra.

## 2026-08-25 23:40

### Demoen er slaaet fra
ZS_DEMO_ENABLED staar paa 0. Knappen "Se demo" findes ikke laengere paa
velkomstsiden, og "Afslut demo" er vaek fra Indstillinger.

Kontakten holdt ikke helt hvad headeren lovede. Selve demo-modulet blev
stadig oversat og linket med, saa 1,8 KB kode og teksten DEMO laa i
flashen paa en kundeenhed uden at kunne naas fra noget. Nu ligger hele
zs_demo.c, maerket i toplinjen og demo-grenen i hovedloekken inde i
kontakten.

Efterproevet paa den byggede fil: ingen af teksterne "Se demo", "Afslut
demo", "DEMO" eller "demo startet" findes i den, og der er nul symboler
med zs_demo. Koden er 7932 bytes mindre, dataene 1096 bytes.

Velkomstsiden er maalt igen uden knappen: indholdet fylder y=90 til 250 i
et felt paa 348, altsaa 90 px luft over og 98 under. Balanceret som den
er, ingen flytning noedvendig.

Slaas den til igen, er det ét tal i zs_config.h.

## 2026-08-25 23:29

### Kun de kasser anlaegget faktisk har
Hovedskaermen viste fire kasser uanset hvad. Havde kunden ikke batteri,
stod der en tom kasse med "Intet batteri" og mindede ham om det hver gang
han gik forbi. Nu bestemmer anlaegget antallet:

    fire   inverter, elmaaler og batteri
    tre    inverter og elmaaler, den sidste fylder hele bredden
    to     inverter og batteri, to raekker i fuld bredde
    en     kun inverter, midtstillet

Raekkehoejden er den samme i alle fire, saa det store tal fylder lige
meget uanset hvor mange kasser der er.

Udregningen ligger i zs_tilegrid.c uden LVGL, saa maalene kan efterproeves
af tests i stedet for af oejet. 105 nye tjek: intet stikker ud over
kanten, ingen to kasser overlapper, alt flugter til begge sider, og der er
lige meget luft foroven og forneden. Paa enheden: 24 skift mellem
opsaetningerne, 932 bytes forskel i alt, altsaa engangsudgift til LVGL's
bufre og ikke et hul.

### Batteriet blev fundet forkert
Vi regnede med at der var et batteri hvis SunSpec-model 124 fandtes. Det
holder ikke. Fronius' egen Modbus-manual skriver om WChaMax i den model:
"If energy storage is not available, the register feeds back a value of
0". Modellen udgives altsaa ogsaa paa en inverter uden batteri.

En almindelig solcelleinverter ville derfor have faaet vist en
batterikasse der aldrig kom til at staa andet end tom. Nu spoerger vi om
maks ladeeffekt, og er den nul eller uudfyldt, er der intet batteri.

Fundet ved at laese Zbox-flaadens kode, som har koert paa rigtige anlaeg
siden 2026, og bekraeftet i Fronius' manual bagefter.

Simulatoren udgav heller ikke model 124 uden batteri, saa E2E-testen kunne
ikke se fejlen. Den opfoerer sig nu som en rigtig GEN24. Med den gamle
detektion faejler testen, med den nye bestaar den.

### Elmaaleren soeges i den raekkefoelge den faktisk findes
Fronius' manual siger 200 for den foerste maaler. Zbox-flaaden finder den
paa 201. Vi proever begge, men 201 foerst, saa opsaetningen ikke staar og
leder unoedigt paa det almindelige anlaeg.

### é og É er med i skrifttyperne
"én" med accent betyder tallet ét og er noget andet end artiklen "en".
To glyffer, under 200 bytes.

## 2026-08-25 23:05

### Foerste udgivelse, v0.1.0
Signeringsnoeglen ligger nu som hemmelighed paa GitHub, og GitHub bygger
og signerer selv naar der saettes et versionsmaerke. Efterproevet: filen
der ligger i udgivelsen er hentet ned igen og underskriften er tjekket
mod noeglen. Den holder.

Skaermene koerer 0.1.0 og udgivelsen er 0.1.0, saa de henter ingenting.
Det er meningen: der opdateres kun opad.

### Omdirigering ved hentning stod ikke i koden
GitHubs hentelink svarer 302 og sender videre til en anden vaert, hvor
filen ligger bag en tidsbegraenset underskrift i adressen. Koden lænede
sig paa ESP-IDF's standard uden at skrive det. Det er det eneste sted
hvor en aendret standard ville betyde at ingen skaerm nogensinde fik en
opdatering, og fejlen ville ligne at GitHub var nede. Nu staar det der.

## 2026-08-25 22:53

### Repoet er offentligt
Skaermene henter opdateringer fra GitHubs API uden noegle. Paa et privat
repo gav det 404, altsaa ingen opdateringer nogensinde. Historikken er
gennemgaaet foerst: hverken signeringsnoeglen eller brand-mappen har
vaeret i den paa noget tidspunkt.

### Versionssammenligning kunne narres
Den brugte sscanf med %u, som baade tager mellemrum foran og et fortegn.
Maerket "-1.0.0" blev laest som 4294967295.0.0 og saa nyere ud end alt
andet. "1.2.3-rc1" blev laest som 1.2.3, saa en forhaandsudgave ville gaa
ud til alle skaerme.

Sammenligningen ligger nu i zs_version.c uden afhaengigheder, med en
streng parser der kun tager cifre og praecis to punktummer. 42 nye tests
daekker den, blandt andet at 0.10.0 er nyere end 0.9.0, hvilket den ikke
ville vaere hvis man sammenlignede teksterne.

## 2026-08-25 22:41

### Lyst tema
Man kan nu vaelge mellem moerkt og lyst under Indstillinger, foerst i
afsnittet SKAERM. Valget gemmes og bruges fra naeste opstart.

Farverne staar ikke laengere som faste tal rundt om i koden. De slaas op
i den palet der er valgt, og navnene er de samme som foer, saa der findes
ikke et sted der blev glemt ved skiftet. tools/check-colors.py haandhaever
at ingen skriver en farve udenom paletten.

I lyst tema kan brandets orange ikke bruges til tal: den har 1,8:1 mod
hvid, og tekst skal have 4,5:1. Derfor er tallene moerk brandgroen med
9,95:1, og orangen er toneret til 4,5:1 og brugt hvor den fylder nok til
at ses. Alle kombinationer i begge temaer maales ved hver bygning.

Logoerne skifter med. Negativt logo paa moerk bund, positivt paa lys.
Begge udgaver har praecis samme maal, saa intet flytter sig.

### Kontrasten i det moerke tema var to steder for lav
Gammel maaling havde 2,70:1 mod kortet og fejlfarven 4,13:1. Begge er
haevet til over kravet med samme kuloer, kun lysere.

### Sider bygges om uden at sive
Et temaskift river alle sider ned og bygger dem op igen, fordi farver der
sidder paa hvert objekt ellers bliver haengende i det gamle tema. Maalt paa
enheden: seks ombygninger, ti sider hver gang, 16 bytes forskel i alt.
Tastaturet frigav ikke sig selv naar dets side blev slettet. Det goer det
nu, uanset hvem der sletter den.

### Skaermen taender rigtigt fra start
Indstillingerne laeses foer baglyset og fladen bygges. Foer taendte
skaermen paa 80 % og rettede sig til kundens vaerdi 200 ms senere.

### zs_theme.h blev laest to gange
Den afsluttende include-vagt stod 40 linjer for tidligt, saa alt om sider
laa udenfor. Det gik godt indtil headeren blev inkluderet to gange i samme
fil. tools/check-headers.sh fanger det nu.

### Wifi-ikonet manglede naar der ikke var wifi
Toplinjen skjulte ikonet naar forbindelsen var vaek. En tom plads laeser
man som at skaermen ikke har opdaget noget. Nu staar der wifi-off, altsaa
wifi-buerne med en streg over, i roed.

### Tal der ikke er maalinger
lround er UDEFINERET hvis resultatet ikke kan vaere i en long. Effekt,
energi og pris har nu et loft paa 1 GW, 1 TWh og 1 mia. kr. Over det viser
vi ingen data. Fundet af GCC paa Linux, som er strengere end clang.

### Gemte indstillinger, efterprovet paa enheden
Seks ikke-standard vaerdier skrevet direkte i lageret, genstart, og alle
seks blev laest OG taget i brug. Skaermen skriver nu i loggen hvad der er i
brug, saa man kan se forskel paa en indstilling der blev laest og en der
blev laest og derefter ignoreret.

## 2026-08-25 21:50

### Opdatering over netvaerket
Firmwaren henter selv nye versioner fra GitHub Releases. Den tjekker ved
hver opstart og derefter hvert 30. minut. Kun versioner der er hoejere end
den koerende bliver installeret, sammenlignet tal for tal saa 0.10.0 er
nyere end 0.9.0 og ikke omvendt.

Hver firmware er underskrevet med RSA-3072. Bootloaderen tjekker
underskriften mod noeglen i den koerende app, saa en fil der ikke er
underskrevet af os bliver afvist. Noeglen har aldrig ligget i repoet.

Virker den nye version ikke, ruller enheden tilbage til den gamle af sig
selv. Den nye bliver foerst godkendt naar den har koert i to minutter.

### Tal der ikke er maalinger
lround og lroundf er UDEFINERET hvis resultatet ikke kan vaere i en long.
Et forvansket register kunne altsaa ikke bare give et grimt tal, men en
fejl oversaetteren har lov til at goere hvad som helst med. Samtidig ville
strengen blive skaaret over midt i, saa den lignede en rigtig maaling.

Effekt, energi og pris har nu et loft for hvad der overhovedet kan vaere en
maaling: 1 GW, 1 TWh og 1 mia. kr. Over det viser vi ingen data. Tallene er
desuden klemt ned i den plads feltet har, saa oversaetteren kan bevise at
der er plads. Fundet af GCC paa Linux, som er strengere end clang paa Mac.
Ni nye tests.

### Wifi-ikonet manglede naar der ikke var wifi
Toplinjen skjulte ikonet helt naar forbindelsen var vaek. En tom plads
laeser man som at skaermen ikke har opdaget noget, hvilket er det modsatte
af hvad vi vil sige. Nu staar der wifi-off, altsaa wifi-buerne med en streg
over, i roed. Ikonet er fra Lucide som resten, og alle fire wifi-ikoner
fylder praecis 20 px i bredden, saa ikonet ikke flytter sig naar signalet
skifter.

### Logoerne er ude af det offentlige repo
De raa logofiler ligger ikke paa GitHub. De faerdige C-filer goer, saa
enhver kan bygge firmwaren, og kun den der har brand-mappen kan lave
logoerne om. Bygger man uden mappen, springer vaerktoejet logoerne over i
stedet for at faejle.

# Ændringer

Nyeste øverst. Dato, hvad der blev lavet, og hvilke fejl der blev
fanget undervejs.

## 2026-08-25 21:28

Elprisen som side 3, fejlkoder rykket til side 4. Statusmærket i
topbjælken viser nu forbindelsen. Ni fund fra en kodegennemgang rettet.

Priserne hentes fra elprisenligenu.dk, én gang i døgnet. Søjler for hele
døgnet, grønne under dagens gennemsnit og røde over, den aktuelle time i
orange. Ordet spotpris står to steder, fordi det ikke er det kunden
betaler. Prisområde vælges både som sidste trin i opsætningen og under
Indstillinger, og der står "Vest for Storebælt" og "Øst for Storebælt",
ikke DK1 og DK2.

Statusmærket hed DEMO og var ellers tomt. Nu siger det FORBUNDET grøn,
FORBINDER grå, INGEN INVERTER gul, INTET NETVÆRK rød, og DEMO orange når
tallene er opdigtede. Ét kald sætter både mærket og wifi-ikonet, så de
ikke kan sige hver sit. Ikonet viser kun signalstyrke, og kun når der er
en forbindelse: er der ingen, siger mærket det allerede med ord.

Fund fra gennemgangen:

To steder bestemte hvor brugeren skulle hen efter valg af inverter.
LVGL holder låsen gennem hele sin runde, så appens valg vandt altid, og
trinnet med prisområde blev aldrig vist. Nu bestemmer appen alene.

`fmt_kr` tabte minusset for alt mellem -1 og 0, fordi heltalsdivisionen
giver nul. -0,05 kr blev vist som 0,05. Negative timepriser forekommer
flere gange om året når det blæser. Funktionen er flyttet ned i
`zs_format.c` hvor den kan testes, og der er ni tests på den nu.

Demoens opdigtede priskurve blev stående efter demoen sluttede og så ud
som rigtige priser.

Demoens ur og maskinens ur sloges om hvilken søjle der var fremhævet, så
den skiftede en gang i sekundet.

Prissiden viste tomme pladsholdere når der ikke var valgt et område, i
stedet for at sige hvad man skal gøre.

Tilbage fra prisområde førte altid til opsætningens inverterliste, også
når man kom fra Indstillinger.

Hentningen af priser blokerede hovedopgaven i op til 12 sekunder, så
både tryk på skærmen og aflæsning fra inverteren stod stille imens.
Den kører nu i sin egen opgave med 10 KB stak, som mbedTLS' håndtryk har
brug for, og prioritet under hovedopgaven.

Ved en omdirigering blev svarets krop lagt foran priserne, så en
hentning der lykkedes endte som en læsefejl. Bufferen ryddes nu ved ny
forbindelse og ved en Location-header.

Byggede binærfiler lå i git. Fjernet og tilføjet til .gitignore.

---

## 2026-08-25 20:37

Side 2 på hovedskærmen: energiflow med linjer, pile og farve efter
retning. Swipe mellem siderne, prikker nederst viser hvor man er. Side 2
får præcis samme data som side 1, så de to ikke kan sige hver sit om
samme måling.

Demo-tilstand under "Kom i gang". Hovedskærmen med opdigtede tal fra
samme model som simulatoren, et døgn på tre minutter. Gemmes aldrig, så
en enhed hos en kunde ikke kan starte op i demo. `ZS_DEMO_ENABLED` i
`zs_config.h` fjerner den helt fra bygget.

Elmålerens fortegn stod som ikke efterprøvet. Det er det nu, mod evcc's
template til Fronius GEN24, som kører på tusindvis af anlæg. Batteri er
`-160:3:DCW + 160:4:DCW`, altså aflad minus lad. Elmåleren læses råt som
`20x:W` uden negering, positiv ved køb. Målerens Modbus-enhed er 200,
flere målere får 201 og 202. Alt sammen som vores. evcc PR 18386 om
Fronius handler om at skrive til model 124, og vi skriver ikke.

Fem fejl rettet:

Tastaturet skiftede tast under fingeren. Tre årsager, alle bekræftet i
LVGL's kilde. `CLICK_TRIG` manglede, så tasten blev sendt når fingeren
ramte og igen for hver tast den gled over. `NO_REPEAT` manglede, så en
holdt tast gentog sig. Og layoutskiftet skete inde fra tryk-håndteringen,
hvor `lv_btnmatrix_set_map()` bygger gitteret om mens LVGL står midt i
trykket. Nu sendes tasten først når fingeren slippes, og layoutskift
udskydes med `lv_async_call`.

En afbrudt inverter-søgning rev brugeren tilbage. Trykkede man tilbage
under søgningen, viste den alligevel resultatet et halvt sekund senere.

Prikkernes fingerflader overlappede, 207..251 og 228..272, så et tryk til
højre for første prik ramte den anden. Afstanden er nu 30 px og fladen
38.

`probe_batch` i netværksscanningen havde to betingelser i samme løkke.
Stoppede den anden tidligt, blev resten af `fds[]` og `alive[]` læst uden
at være skrevet. Kan ikke ske i dag, men ville ske hvis nogen ændrede den
ene grænse.

`zs_fr_t` lå på stakken hvert sekund i demoens detaljeside, 850 bytes af
opgavens 8 KB. Nu static.

Ny E2E-test i `tests/e2e/run.py`. Den starter en simuleret Fronius, lader
firmwarens egen kode tale med den over en rigtig TCP-forbindelse, og
sammenligner de fire tal med det simulatoren siger den har. Alle seks
anlægstyper, base 40000 og 40001, og fire fejltilstande: lukket port,
åben port uden Modbus, rent skrald som svar, og en inverter der
forsvinder midt i. 24 tjek.

`tests/run-all.sh` kører det hele: headere, tegnsæt, 218 enhedstest og
24 E2E.

---

## 2026-08-25 17:20

Første flashning på hardware. Enheden stod stille lige efter at baglyset
blev tændt, uden en eneste fejl i loggen.

`lv_port_sem_take()` brugte en almindelig FreeRTOS-mutex, som ikke kan
tages to gange af samme opgave. `app_main` tog låsen og kaldte
`zs_ui_init()`, som til sidst kaldte `zs_ui_show()`, der tager den igen.
Rettet to steder: `zs_ui_init` bruger nu en låsefri udgave, og mutexen er
gjort rekursiv så det ikke kan låse skærmen fast igen.

`tools/check-text.py` læser den faktiske tegndækning ud af de byggede
skrifttypefiler og fanger tegn vi skriver som skrifttypen ikke har. LVGL
tegner ingenting i det tilfælde, uden advarsel hverken ved byg eller
kørsel. Den fandt to: midterprik og punkttegn, som begge stod som huller.

Skærmen huskede ikke wifi efter en genstart hvis der ikke også var valgt
en inverter. Nu gemmes netværket så snart det virker, og "Kom i gang"
bruger det hvis det er der.

---

## 2026-08-25 16:43

Hele opsætningsflowet: vælg netværk, kodeord på dansk touchtastatur, find
inverteren automatisk på netværket, vælg den. Ingen telefon og ingen
computer. Plus Indstillinger og Detaljer om anlægget.

Netværksscanningen prøver 12 adresser ad gangen med et fjerdedels sekunds
tålmodighed, så hele undernettet er gennemgået på under ti sekunder. En
ad gangen ville tage over et minut.

Arbejdsdelingen er stram: én opgave laver alt det der tager tid,
brugerfladen tegner. Trykker man på en knap mens en wifi-søgning kører,
lægges beskeden i en kø og skærmen kører videre. Alle funktioner i
`zs_ui` tager selv LVGL-låsen, så ingen kalder skal huske det.

Fejl fanget: seks steder blev en etiket hentet med
`lv_obj_get_child(row, 1)`, hvor rækkefølgen af børn afhænger af om der
er et ikon og en værdi. `zs_row_create` afleverer nu sine dele i en
struct. Indstillingssiden blev fyldt fem gange i sekundet, også mens
brugeren trak i lysstyrke-skyderen, så den ville hoppe tilbage under
fingeren. Detaljesiden blev bygget helt om lige så tit. Fire steder kunne
en tekst løbe over sin buffer, fanget af oversætteren fordi vores egen
kode bygger med alle advarsler som fejl. Og `%u` blev brugt på en
`uint32_t`, som på ESP32 er en `unsigned long`.

---

## 2026-08-25 16:15

Firmwaren bygger og kan flashes. Designsystem, hovedskærm med fire
kasser, lysstyrke med natdæmpning, og skrifttyper, ikoner og logoer
bygget fra Funnel Sans, Lucide og brand-mappen.

Hele layoutet er regnet ud: vandret 12 + 222 + 12 + 222 + 12 = 480,
lodret 44 + 12 + 186 + 12 + 186 + 12 + 28 = 480. Enheden "kW" står på
samme grundlinje som tallet, regnet ud af LVGL's egne skriftmål i stedet
for at blive skrevet ind som et tal der holder op med at passe. Alt man
kan trykke på er mindst 44x44, som er cirka 8 mm på denne skærm.

Fejl fanget: `zs_config.h` havde `#define ZS_STATUSBAR_H 44` som
statuslinjens højde, præcis samme navn som include-guarden i
`zs_statusbar.h`. Fordi `zs_config.h` blev læst først, sprang
præprocessoren hele headeren over. Ingen advarsel, kun en fejl et helt
andet sted om en type der ikke fandtes. Målene står nu kun ét sted, og
`tools/check-headers.sh` håndhæver at ingen konstant slutter på `_H`.

`bsp_lcd_set_cb()` forventer `bool (*)(void *)` men fik `bool (*)(void)`.
Det virker i praksis på Xtensa, men er udefineret opførsel i
afbrydelsesstien. Ikonlisten stod to steder, så et ikon kunne findes i
koden uden at tegningen fulgte med. Og touch-læsningen var mærket
`IRAM_ATTR` uden grund.

Seeeds SDK oversætter ikke rent med ESP-IDF v5.1.7, fordi det er skrevet
til en ældre udgave hvor loggens tidsstempel var en `int`. Løst ved at
slå netop de advarsler fra for deres komponenter alene. Vores egen kode
bygger med nul advarsler.

---

## 2026-08-25 15:43

Datalaget: Modbus TCP-klient med kun funktionskode 3, SunSpec-vandring og
afkodning, og udregningen af sol, forbrug, batteri og net. Plus en
simuleret Fronius og et kommandolinjeværktøj der kører firmwarens egen
kode på en Mac.

Alle registeradresser er slået op i SunSpecs officielle modelfiler, ikke
skrevet efter hukommelsen. Simulatoren og C-testene er bygget uafhængigt
af hinanden og er enige om hvor modellerne ligger: begge siger at model
160 starter på adresse 40178.

Fejl fanget: N i model 160 ligger på offset 6, ikke 5. Feltet lige før,
Evt, er en bitfield32 og fylder både offset 4 og 5. Læser man antallet af
DC-kanaler på offset 5, får man den nederste halvdel af Evt, som næsten
altid er nul, og så forsvinder hele kanalgenkendelsen lydløst. Den samme
fejl står i Zbox Raspberry i dag, i `app/modbus_controller.py`.

Testen for om en model bruger flydende tal var `id >= 111`, hvilket er
rigtigt for inverteren og forkert for alle fire målermodeller, fordi 201
til 204 også er større end 111. Skærmen viste 0 W på nettet mens måleren
meldte 5 kW eksport. Fanget af `zs-probe` mod simulatoren, ikke af
enhedstestene, fordi testene var skrevet med den samme forkerte
antagelse. Nu står de otte modelnumre skrevet ud, låst med 18 tests.

Reserveløsningen for unavngivne kanaler talte forfra. Fronius' manual
siger at batteriets to kanaler lægges i enden, så et anlæg med én
solstreng har batteriet på kanal 2 og 3, ikke 3 og 4.

En model med længde 0 kunne få vandringen til at gå i ring.
`printf("%.1f")` runder halve værdier til nærmeste lige ciffer, så 4250 W
blev til 4,2 kW mens 4350 W blev til 4,4. Og simulatorens batteri ændrede
aldrig ladetilstand på grund af en indrykningsfejl.

---

## 2026-08-25 15:12

Værktøjskæden. `tools/setup-toolchain.sh` henter ESP-IDF v5.1.7,
`tools/env.sh` sætter miljøet op.

Både `python@3.12` og `python@3.14` fra Homebrew på denne maskine har et
`pyexpat` der loader systemets `libexpat` i stedet for Homebrews, så alt
Python-værktøj der rører XML fejler. Fejlen ser ud som
`ensurepip returned non-zero exit status 1`. Rettes med
`brew reinstall expat python@3.12`. Indtil da bruges Apples egen
`/usr/bin/python3`. Scriptet prøver hver kandidat af i praksis ved at
bygge et rigtigt venv med pip i, i stedet for at se på versionsnummeret.

ESP-IDF leverer ikke `cmake` og `ninja` på macOS. De installeres nu
automatisk.
