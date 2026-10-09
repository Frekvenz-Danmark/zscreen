## 2026-10-09 19:41

### Fase B: koden naar serveren, og en tom streng er selve beskeden
Skaermen sender nu tilmeldingskoden op i feltet `enrollCode`, og rydder
det igen naar den holder op med at vise koden. Det er regel 2 der bliver
til kode: koden virker kun mens skaermen faktisk viser den.

**En TOM streng er ikke "ingenting at sende".** Den er beskeden om at
koden ikke gaelder laengere. Derfor bygges kroppen i
`zs_fleet_publish_tilmeld` i haanden og IKKE med `send_tekst`:
`send_tekst` springer ud paa en tom streng, og det er rigtigt for
modelnavne, hvor et tomt navn er ingenting at sige. Her ville det
betyde at koden blev ved med at virke paa serveren efter skaermen var
holdt op med at vise den.

Og det er maalt mod serveren, for det var ikke selvfoelgeligt: en tom
streng bliver IKKE lavet til null. `None` og `''` er to forskellige
tilstande paa attributten, saa "har aldrig haft en kode" og "koden er
ryddet" kan skelnes. Mange systemer slaar de to sammen.

**Hvornaar der sendes** ligger i `zs_tilmeld_skal_sende`, saa det kan
proeves paa en almindelig maskine. Samme tre tilstande som
inverterfeltet, og af samme grund: "har det aendret sig" er ikke nok.
Melder skaermen sig ind IGEN, staar feltet tomt paa serveren mens vi
stadig tror vi har sagt det - og saa ville montoeren se en kode paa
skaermen som serveren ikke kendte. Tilstanden nulstilles derfor naar vi
ikke er indmeldt.

Mutationstesten fandt én ting her, og den var reel: `n >= sizeof` byttet
til `n > sizeof` overlevede, fordi ingen proeve ramte PRAECIS syv tegn. Og
med den mutant ville syv tegn slippe igennem og kopiere otte bytes ind i
en buffer paa syv. Det er ikke en afrunding, det er en overskrivning. Der
staar nu proever paa baade seks og syv tegn, og alle 21 mutanter doer.

**Loggen siger at koden er sendt, men ALDRIG hvad den er.** En kode i en
log er en kode der kan laeses af alle der kan laese loggen.

**Det er ikke faerdigt endnu.** Skaermen laver ingen kode af sig selv:
den bliver til naar brugerfladen kalder `zs_app_tilmeld_vis(true)`, og
den side findes ikke. Det kan man se i binaeren - linkeren har fjernet
`zs_tilmeld_bed_om` og hele alfabetet, fordi intet kalder dem. Tilbage af
Fase B er altsaa siden med QR'en, og vores API som Frekvenz kalder med
koden.

## 2026-10-09 19:24

### Tre moduler slap uden om mutationstesten, og en enkeltmodul-koersel kunne slet ikke koere
Mutationstesten har en fast liste over de moduler den oedelaegger med
vilje. Listen stod i samme raekkefoelge som `run.sh`, med kommentaren
"saa det er let at se om der mangler et".

Det var ikke nok. `zs_tid`, `zs_kvarter` og `zs_tilmeld` var skrevet med
proever og stod i `run.sh`, men ingen havde skrevet dem i listen. De slap
altsaa helt uden om, og **intet sagde fra**: testen meldte "alle mutanter
blev fanget" om de elleve moduler den kendte, og det lignede at hele koden
var daekket.

Nu laeser `tjek_listen` selve `run.sh`. Staar et modul der, men hverken i
`MODULER` eller i `UDEN_MUTATION` med en grund, falder koerslen med
udfald 1 og siger hvilket. Proevet ved at tage et modul ud af listen:
den siger fra. 14 moduler og 399 mutanter nu, mod 11 foer.

**Og en fejl i vagten mod foraeldede undtagelser.** `tjek_undtagelser`
maalte undtagelserne mod de moduler der var VALGT paa kommandolinjen i
stedet for mod dem alle. Saa faldt `mutationstest.py zs_tekst` altid:
undtagelsen for `EAGAIN` staar i Modbus-laget og passer naturligvis ikke
paa et modul om tekst. Den dokumenterede enkeltmodul-koersel har altsaa
vaeret umulig siden vagten kom ind, og fejlen lignede en foraeldet
undtagelse. Undtagelserne maales nu mod alle moduler. Det koster kun at
laese filerne, ikke at oversaette.

### En gren i dagnummeret kunne ikke skelnes fra sin egen mutant
Da `zs_tid` endelig kom med i mutationstesten, overlevede én af tre
mutanter: `aar >= 0` byttet til `aar > 0`.

Den kunne ikke slaas ihjel af nogen proeve, og det var ikke proevernes
skyld. De to giver det SAMME svar: de kan kun vaere uenige om aar 0, og
dér trunkerer C mod nul, saa baade `0/400` og `-399/400` er nul.

Grenen er Hinnants egen maade at faa divisionen til at runde nedad for aar
foer aar 0. Den er doed hos os: aaret kommer fra `localtime_r`'s
`tm_year`, som ikke giver et negativt aarstal, og alt foer 1970 fanges i
forvejen af `d < 0`. En gren hvis to sider ikke kan skelnes, er ikke en
vagt. Den er vaek, og nu doer alle mutanter i modulet.

Det er anden gang mutationstesten finder doed kode i en graense. Det er
ogsaa derfor de tre moduler skulle have vaeret paa listen fra starten.

## 2026-10-09 19:23

### Fase B: engangskoden der binder skaermen til en kunde
Skaermen melder sig selv ind paa flaaden med sit certifikat, og det siger
HVILKEN skaerm det er. Men ikke hvis kunde den staar hos. Tilmeldingen er
den der siger "den her skaerm hoerer til Hansen paa Testvej 1", og det
foerste stykke af den er koden.

Skaermen laver en engangskode og viser den som seks tegn og en QR. En
montoer laeser den og giver den til Frekvenz sammen med kundens
oplysninger. Frekvenz kalder vores API med koden, og saa oprettes kundens
mappe og anlaeg. Det er praecis de oplysninger der i dag skrives i haanden
i `scripts/kunder.json` paa serveren.

**Koden er ikke adgang.** Den siger kun hvilken skaerm, og den er opbrugt
bagefter. Hvem der maa laese kundens tal, afgoeres af en servicebruger med
adgang til netop de kunder de maa se. Var koden adgang, kunne enhver der
fotograferede en skaerm laese kundens forbrug.

**Derfor staar enhedens identitet ikke i QR'en.** Den baerer KUN de seks
tegn. Stod enhedens id i den, kunne enhver der fotograferer en skaerm i et
vindue tilmelde den senere, ogsaa naar koden er doed. Og der staar ingen
adresse i den: en adresse der er forkert eller flytter, er en QR der peger
paa ingenting i tusind skaerme der allerede er sat op.

**Fire ting holder koden taet, og laengden er ikke den foerste.** Den
udloeber efter ti minutter. Den virker kun mens skaermen faktisk viser
den, saa holder skaermen op, ryddes den ogsaa paa serveren. Den kan bruges
én gang. Og serveren lukker ned efter faa forsoeg - det sidste er
serverens, for kun den kan taelle forsoeg fra hele verden.

Koden ligger **aldrig** i flashen. Den lever i RAM og kun der, saa en
genstart doeber den. En kode der overlevede en genstart, ville vaere en
kode man kunne finde i en skaerm der har staaet slukket i et halvt aar.

**Alfabetet er Crockfords base32**, altsaa cifrene og de store bogstaver
uden I, L, O og U. To grunde: 32 er en potens af to, saa fem bit bliver ét
tegn uden rest og koden er jaevnt fordelt - med alle 36 tegn og modulo
ville de foerste tegn komme lidt oftere. Og koden bliver LAEST paa en
skaerm og SKREVET et andet sted af et menneske, hvor 0 og O, 1 og I og L
ligner hinanden. Det koster en milliard muligheder i stedet for 2,2
milliarder, og det er i orden: laengden var aldrig det der holdt den taet.

**Ingen kode foer inverteren er fundet**, efter beslutningen 8. oktober.
Et anlaeg uden inverter er ikke et anlaeg. Det koster at en montoer kan
staa med en skaerm der ikke vil give en kode, saa standen siger
udtrykkeligt HVORFOR: mangler inverter, eller mangler flaade. Og der laves
slet ingen kode naar forudsaetningerne mangler - en kode serveren ikke
kender, ville staa paa skaermen og se rigtig ud, og montoeren ville lede
efter fejlen et helt andet sted.

37 proever, og alle 14 mutanter i modulet doer.

## 2026-10-09 18:42

### Et testfelt var ét tegn for lille, og kun Linux sagde det
`test_version.c` lagde den laengste gyldige version i `laengst`, og
skrev den saa med et `v` foran i `med_v`. Begge felter var
`ZS_VERSION_MAX * 2`, altsaa 48 tegn - men `med_v` skal rumme ét tegn
mere end `laengst`, netop fordi v'et skal med.

I praksis staar der 17 tegn, saa der blev aldrig klippet noget. Men gcc
kan ikke se at strengen er kort, kun at et 48-tegns felt kan blive 49, og
`-Wformat-truncation` er en fejl hos os. Resultatet var at **repoet ikke
kunne bygge paa gcc** med vores egne flag, mens clang lod det ligge. Det
blev foerst fundet da enhedstestene blev koert i en Linux-container.

Feltet er nu `sizeof(laengst) + 1`, saa maalet siger hvad det skal rumme i
stedet for at gentage et tal. Bygger nu rent paa baade clang og gcc med
`-Wall -Wextra -Werror`, uden at noget er slaaet fra.

CI var groen hele tiden, fordi ubuntu-24.04's gcc ikke brokkede sig over
netop den linje. Den ville have bidt den dag CI flyttede billede.

## 2026-10-09 18:41

### Tidszonen hang paa at SNTP blev startet i den rigtige raekkefoelge
`setenv("TZ", ...)` og `tzset()` stod nede i `clock_start`, efter
`esp_sntp_init` og **bag en tidlig udgang** paa `esp_sntp_enabled()`.

Det virkede, fordi `clock_start` er det eneste sted der starter SNTP i
dag. Men saa hang hele den danske tid paa netop det: bliver SNTP en dag
startet et andet sted foerst, springer `clock_start` ud paa foerste linje,
TZ bliver aldrig sat, og saa giver **hver** `localtime_r` i firmwaren UTC
i stedet.

Det er fem steder, og ingen af dem ville klage: uret paa skaermen,
natdaempningen i `zs_display.c`, elpriserne i `zs_price.c` og
`zs_price_now.c`, timeenergien og kvartersgennemsnittet. Alle ville ligge
en eller to timer forskudt, og alle ville se rigtige ud.

Tidszonen saettes nu i `tidszone_saet()`, kaldt som det **foerste** i
`zs_app_start`, foer nogen opgave er startet og dermed foer noget kan
naa at laese et ur. Den koster to kald og kan ikke fejle. SNTP bliver ved
med at ligge i `clock_start`, hvor den hoerer til.

Efterset samtidig at der ikke er andre steder der blander urene: alle
forbrugere bruger `localtime_r`. Det ene `gmtime_r` i `zs_price_now.c`
staar der med vilje, for den sammenligner netop lokal tid med UTC.

Firmwaren bygger, alle 9 vagter er tilfredse, 1274 tjek bestaaet.

## 2026-10-09 18:34

### Fase A i firmwaren: ét gennemsnit per kvarter, og dag og time fra samme ur
Fundamentet siger at effekt gemmes hvert kvarter i 8 uger, og at skaermen
selv skal regne gennemsnittet. Grunden er at `attributeLinks` fordeler ved
HVER skrivning: fem felter gange 43.200 om dagen er tre millioner raekker
om maaneden per skaerm. Nu sendes 96 raekker per felt om dagen, og et
kvarter er ogsaa den takt optimeringen regner i, saa de faar praecis den
oploesning de bruger.

`zs_kvarter.c` samler tallene mens de kommer ind og giver ÉT gennemsnit
naar kvarteret skifter. Et felt uden maaler eller uden batteri kommer
**ikke** med som nul: et nul for "koebt fra nettet" ser ud som et anlaeg
der aldrig koeber stroem, og det er en loegn man ikke kan gennemskue.

**Undervejs blev en fejl maalt i det der allerede koerte.**

Dagnummeret blev regnet af UTC-sekunder mens timen kom fra `localtime_r`.
`zs_energi_tik` lagde dem sammen til ét maal for hvor laenge der var
gaaet, og de to skiftede ikke samtidig:

- ved lokal midnat wrappede timen 23 -> 0 mens UTC-dagnummeret stod
  stille, saa svaret blev -23
- ved UTC-midnat (lokal 01 om vinteren, 02 om sommeren) skiftede
  dagnummeret mens timen gik jaevnt videre, saa svaret blev 25

Begge blev kaldt huller, og timen blev smidt vaek. Maalt gennem modulets
egen kode over syv doegn: **time 23 og time 00/01 blev aldrig sendt.** To
af 24 timer hver eneste dag, i netop den historik forbrugsprognosen
regner paa. Over et aar 730 huller.

`util/zs_tid.c` regner nu dagnummeret af den **lokale dato**, saa dag og
time skifter samtidig ved lokal midnat. Et helt aar igennem giver ét hul,
ved foraarsskiftet hvor timen 02 ikke findes. Om efteraaret kommer timen
02 to gange, og den anden lander i samme boette som den foerste: intet
tabt, men den time daekker to. `tm_gmtoff` findes IKKE i ESP-IDF's newlib,
kun bag `__TM_GMTOFF` som ikke er defineret, saa dagnummeret regnes af
`tm_year`, `tm_mon` og `tm_mday`.

De to ting hoerer sammen. Boetten skal taelle opad over midnat, ellers
smider kvartersmodulet selv kvarteret vaek som et hul - og saa havde det
nye modul arvet fejlen fra det gamle.

**Hvorfor 1258 groenne tjek ikke saa det.** De gav modulet et
boettenummer vi selv havde regnet ud, med dag og kvarter fra samme ur.
Firmwaren gav det to tal fra hver sit ur. `test_tid.c` fodrer nu
modulerne som `zs_app.c` goer, med klokken fra `localtime_r` og
firmwarens egen `ZS_TIMEZONE`. Muteret tilbage til dagskifte kl. 02 giver
11 roede tjek, heriblandt "time 23 blev sendt 0 gange" og "et helt aar
giver 734 huller".

Efterproevet: 1274 tjek paa Mac og paa Linux, hele datavejen, fuzzerne og
vagterne. Firmwaren bygger til ESP32-S3 med 50 procent af partitionen fri.
Linux fangede at `localtime_r` er POSIX og skjules af glibc under
`-std=c11`, praecis som i `zs_price_now.c`.

**Én ting at vide ved opdateringen:** udgangspunktet for timeenergien
ligger i flashen med det gamle UTC-dagnummer. Det kan afvige med én dag
fra det nye, og saa logges den foerste time som et hul. Én gang, og den
siger det i loggen.

## 2026-10-08 23:05

### Man kan nu se om en skaerm er i live, og om inverteren svarer
To ting, og de kommer fra hver sit sted med vilje.

**Boksen** saetter OpenRemote SELV. Der findes et meta-element
`userConnected` paa en boolean-attribut, og brokeren saetter feltet true
naar enhedens egen service-bruger forbinder og false naar den kobler af,
ogsaa naar forbindelsen bare falder. Jeg har laest det i OpenRemotes egen
kilde, `ConnectionMonitorHandler` i 1.31.1, og maalt det hele vejen: false
foer, true mens enheden var forbundet, false igen efter 0,1 sekund. Vi har
altsaa ikke bygget noget selv, og skaermen skal ikke sende noget.

Det kunne den heller ikke. "Jeg er offline" er en besked der kraever en
forbindelse at sende paa.

**Inverteren** kan OpenRemote ikke vide noget om, saa skaermen siger det
selv. Signalet fandtes allerede: `s_home.stale`, det samme der afgoer om
skaermen viser tallene som gamle. Kaldet staar praecis dér hvor det regnes
ud, saa serveren og skaermen ikke kan vise to forskellige ting.

### Feltet sendes KUN naar det skifter
Sendte vi det ved hver aflaesning, kom der en raekke hvert andet sekund for
et tal der naesten aldrig aendrer sig: 43.200 raekker om dagen per skaerm i
stedet for en haandfuld.

Beslutningen har TRE tilstande og ikke to, og det er den vigtige del. "Har
det aendret sig" er ikke nok: starter skaermen op med en inverter der
svarer, er der ingen aendring fra noget, og saa ville feltet staa TOMT paa
serveren for evigt. Et tomt felt ser ud som en skaerm der ikke kan finde ud
af det, ikke som en der aldrig har sagt noget.

Og den siger det FORFRA efter en ny indmeldelse. Vi kan ikke vide hvad
serveren har: enheden kan vaere oprettet forfra, eller beskeden kan vaere
gaaet tabt.

Tretten proever, blandt dem at hundrede uaendrede runder giver ÉN
afsendelse og hundrede skiftende giver hundrede. Seks mutanter er efterset
enkeltvis og doer alle.

### Og et format der svigter tavst
Maalt mod serveren: `true` og `false` virker, og saa goer `1` og `0`. Men
`True` med stort bliver TAVST ingenting. Feltet staar tomt, og der kommer
ingen fejl nogen steder. Firmwaren sender smaa bogstaver, og det staar
skrevet ved konstanten hvorfor.

Serverens proeve sender nu feltet BEGGE veje og laeser begge tilbage, saa
vi ogsaa beviser at skrivningen flyttede noget. Efterset at proeven
faktisk siger fra paa `True` med stort og paa en tekst.

## 2026-10-08 20:04

### En undtagelse i mutationstesten var holdt tavst op med at virke
Mutationstesten meldte en overlevende paa `zs_modbus_tcp.c:596`:

    if (fejlnr == EAGAIN || fejlnr == EWOULDBLOCK) {

Byttes `eller` til `og`, sker der ingenting. Og det er rigtigt: de to er
SAMME tal. Maalt, ikke antaget. Paa macOS er begge 35, og i ESP-IDF's
newlib staar der ligefrem `#define EWOULDBLOCK EAGAIN`. Ingen proeve kan
skelne de to grene, for der er kun én gren. Begge staar der alligevel,
fordi POSIX tillader at de er forskellige.

Den var ALLEREDE undtaget. Noeglen hed bare:

    "errno == EAGAIN || errno == EWOULDBLOCK"

Da beslutningen blev trukket ud i `zs_mb_errno_art`, kom variablen til at
hede `fejlnr` i stedet for `errno`. Saa passede noeglen ikke mere, og
undtagelsen holdt TAVST op med at virke. Mutanten dukkede op som
overlevende i en koersel langt senere, hvor man skal regne hele
begrundelsen ud forfra.

To ting er rettet. Noeglen er nu `"EAGAIN || "`, altsaa den del der ikke
aendrer sig naar en variabel bliver omdoebt. Og vigtigere: der er kommet en
vagt, `tjek_undtagelser`, der naegter at koere hvis en undtagelse ikke
passer paa ÉN eneste linje mere. Den koerer FOER de tre timers
oversaettelser, saa man faar det at vide med det samme.

Begge veje er efterset: undtagelsen passer paa den rigtige linje fra
koden, og vagten siger fra paa en linje der ikke passer. Det er den vigtige
af de to, for en undtagelse der er faldet af, gemmer en mutant uden at
sige det.

## 2026-10-08 19:12

### En laengdevagt i versionstjekket var doed kode, og derfor uproevelig
De sidste to mutanter i runden sad paa `n == 0 || n >= ZS_VERSION_MAX` i
`zs_version_tag_ok`. Jeg kunne ikke skrive en proeve der faldt, og saa saa
jeg efter hvorfor i stedet for at skrive en proeve der snoed.

Begge halvdele var doed kode. En tom streng bliver afvist paa den sidste
linje alligevel, for `tal` og `cifre` staar paa nul. Og laengden kan
simpelthen ikke naa 24: formatet godtager tre tal med hoejst fem cifre
hver og to punktummer, altsaa hoejst 17 tegn. Graensen blev aldrig naaet.
Derfor kunne man vende begge halvdele uden at én proeve faldt. En vagt man
ikke kan proeve af, er ikke en vagt, den er pynt man tror paa.

Vagten er fjernet, og sammenhaengen staar i stedet som et tjek ved
bygningen:

    #define ZS_VERSION_MAX_CIFRE  5
    #define ZS_VERSION_LAENGST    (3 * ZS_VERSION_MAX_CIFRE + 2)
    _Static_assert(ZS_VERSION_LAENGST + 1 <= ZS_VERSION_MAX, ...)

Cifferloftet i loekken bruger nu den samme konstant, saa de to ting ikke
kan glide fra hinanden. Saetter nogen loftet op til otte, falder bygningen
med teksten "formatet kan sige ja til mere end ZS_VERSION_MAX kan rumme".
Efterset: bygningen falder faktisk. Det er bedre end et tjek der koerer
hver gang uden nogensinde at gribe ind.

Og det er proevet fra den anden side ogsaa: den laengste version formatet
siger ja til, `99999.99999.99999`, skal fylde praecis det udregnede, skal
godtages, og skal kunne gemmes som maal HELT, ikke klippet. Baade med og
uden `v` foran.

### Samtidig: der maales ikke laengere paa en lang streng foer den afvises
Da laengden ikke skal maales mere, gaar loekken frem til nulbyten i stedet
for til en laengde regnet ud foerst. En lang streng fra serveren stopper
nu paa det sjette ciffer, i stedet for at hele strengen loebes igennem én
gang bare for at maale den. Samme svar, mindre arbejde paa noget der
kommer udefra.

Antal mutanter faldt fra 381 til 369, fordi der er mindre kode at mutere.
Otte mutanter paa den nye kode er efterset enkeltvis og doer alle.

## 2026-10-08 18:39

### Forskydningen fra UTC kunne vendes fire steder uden at nogen sagde fra
Prisskaermen skal vide hvilken time det er LOKALT, for priserne staar i
UTC. Forskydningen blev regnet inde i den funktion der ogsaa kalder
`time()` og `localtime_r`, og den kan man ikke stille uret paa i en proeve.
Saa de fire regler der haandterer dagsskiftet stod helt uproevede, og en
mutationstest kunne vende alle fire uden at én eneste proeve faldt.

Reglerne er der af en grund. Klokken 01 dansk tid er det 23 i UTC dagen
foer, og trækker man bare timerne fra hinanden, giver det 23 timers
forskydning i stedet for minus én. Saa viser skaermen prisen for en helt
anden time. De to sidste regler er nytaarsskiftet: dagnummeret springer
fra 365 tilbage til 0, saa forskellen bliver et par hundrede i stedet for
én, og natten mellem nytaarsaften og nytaarsdag ville vise forkert.

Udregningen er flyttet ud i `zs_price_utc_offset`, der kun faar fire tal
ind og giver et tal igen. Samme regler, ord for ord, men nu kan de proeves.
Funktionen der kalder den er to linjer.

Proeven daekker vinter- og sommertid, begge natteskift, begge
nytaarsskift (ogsaa i et skudaar), og de to vagter paa funktionen
udenom. Og saa en egenskabsproeve der regner ALLE forskydninger fra minus
12 til plus 14 igennem, for hver af aarets 366 dage og hver af dagens 24
timer. 237.168 kombinationer, hvor det udregnede skal passe med det der
blev sat ind.

Alle syv mutanter paa stedet er efterset enkeltvis og doer nu, ogsaa den
der bytter plus og minus om, og den der laver `eller` til `og` i vagten.

## 2026-10-08 18:28

### Et emne der var præcis én for langt, kunne slippe igennem afkortet
Den grimmeste i denne runde. Emnet der skrives til er
`master/<enhed>/writeattributevalue/<felt>/<enheds-id>`, og grænsen
`n >= bufferens størrelse` kunne laves til `>`. Så slipper netop det emne
igennem der fylder bufferen **helt**, altså uden plads til nulbyten, og så
er emnet afkortet.

Et afkortet emne peger på et **andet felt eller en anden enhed**. Målingen
havner altså et forkert sted, og det er ikke til at se på noget.

Min første prøve fangede den ikke. Den brugte en meget lille buffer, så
emnet var langt **over** grænsen, og det afviser begge udgaver. Nu er
emnet regnet til præcis 32 tegn, og der er to prøver: med en buffer på 32
afvises det, og med 33 kommer det igennem helt. Så ved vi at grænsen
ligger det rigtige sted og ikke bare altid afviser.

### Og fire vagter mere der ikke var prøvet
Det der skriver i kalderens buffer havde `er NULL eller længden er nul`
fem steder, og ingen af dem var prøvet med NULL. Laves `eller` til `og`,
skrives der i NULL når længden er sat.

To af dem ligger i talformateringen, som skærmen kalder hver gang et tal
skal vises, altså konstant. Går en af dem ned, går skærmen ned. Og to
ligger i den kode der bygger indmeldelsen og emnerne: går den ned, kommer
skærmen aldrig på flådestyringen.

Prøven på længde nul fylder bufferen med et mønster først og tjekker at
der **ikke** bliver rørt ved den, i stedet for bare at se at der ikke sker
et nedbrud.

### To mutanter lukket ved at fjerne kode
Transaktions-ID'et blev sat til 1 ved hver forbindelse, og det var
overflødigt: funktionen der uddeler dem klarer selv nul, både første gang
og efter en ombrydning, og den er prøvet hele vejen rundt. To steder der
gør det samme er ét sted at tage fejl for meget.

Og svartiden ved forbindelse blev valgt med sin egen linje, så der var tre
steder der valgte en svartid og kun to af dem var prøvet. Nu bruger alle
tre den samme prøvede beslutning.

**Status:** 9 vagter, 1183 prøver på maskinen, 89 hele vejen igennem,
firmwaren bygger.

## 2026-10-08 18:11

### Markøren der afgør om det er en inverter, kunne passe halvt
En SunSpec-enhed starter med de fire bogstaver "SunS". Mutationstesten
lavede `første halvdel OG anden halvdel` til `ELLER`, og så er en **halv**
markør nok. Ingen prøve sagde fra.

Det betyder noget på et kundenet. Der sidder tit andet på Modbus, en
varmepumpe eller en PLC, og et tilfældigt registerpar kan godt ramme den
ene halvdel. Så ville vi kalde den en inverter og afkode dens registre som
om de var SunSpec. Tallene der kom ud ville ikke se forkerte ud.

Prøvet af med alle otte kombinationer: hele markøren, hver halvdel alene,
hver halvdel med den anden tæt på, ombyttet, tom og alt sat. Kun den hele
bliver godkendt.

### Og koden hvilede på at `&&` kortsluttede
Kæden slutter med en slut-markør. Nogle enheder lægger den helt ude på
kanten af deres registerplads, så der er plads til markøren men ikke til
længdefeltet efter. En læsning af to registre fejler, en læsning af ét
lykkes, og det er en **pæn** afslutning og ikke en fejl.

Der stod `uint16_t one;` uden en værdi, og så `hvis læsningen lykkedes OG
der står slut`. Det er rigtigt, men **kun fordi `&&` kortslutter**: fejler
læsningen, bliver `one` aldrig læst. At koden var rigtig hvilede altså på
rækkefølgen i en betingelse.

Mutationstesten lavede `&&` til `||`, og så afhang svaret af hvad der
tilfældigvis lå i hukommelsen. Var det slut-markøren, meldte vi kortet
komplet i stedet for ufuldstændigt, og så konkluderer man "her er ingen
elmåler" på et halvt kort.

`one` har nu altid en værdi, og de to ting står hver for sig, så man kan se
hvad der blev afvist. Og den sjældne sti med markøren ude på kanten er nu
prøvet af, hvilket den ikke var før. Alle tre mutanter dør.

**Status:** 9 vagter, 1157 prøver på maskinen, 89 hele vejen igennem,
firmwaren bygger.

## 2026-10-08 16:16

### De to rettelser fra før havde ingen prøver, og det var et hul
Jeg rettede to ting i kommandovejen og byggede firmwaren, og så var de
uprøvede. `zs_ota.c` kan ikke oversættes på en almindelig maskine: den
hænger på `esp_https_ota` og på FreeRTOS. Så lå fire regler uden en eneste
prøve, og de var netop blevet vigtige.

Beslutningen er trukket ud til `zs_version.c`, som kan oversættes på
maskinen. `zs_ota_set_target` er nu en tynd skal om den. De fire regler:

- **tom eller intet** betyder "følg nyeste igen", og det **er** en ordre
  hvis der var et mål før
- **skrald** beholder det gamle. At falde tilbage til "nyeste" fordi nogen
  tastede forkert, ville være en overraskelse
- **v foran** fjernes, for mærket på GitHub har det, og det sættes på igen
  når vi slår op
- **samme mål igen** er **ikke** en ordre

Den sidste er den der betyder mest. Serveren sender målversionen igen ved
hver indmeldelse, og en genforbindelse er ikke en ordre. Uden den regel
ville en skærm på et dårligt net banke på GitHub i stedet for at vise tal.
Og siger reglen for lidt, bliver opdateringen liggende som før.

Prøvet af med 28 tjek, og efterprøvet ved at vende hver enkelt regel om:
"ændret" vendt om, altid sand, altid falsk, ryddet der ikke tæller,
skrald der overskriver, skrald meldt som gyldigt, og v der ikke fjernes.
**Alle syv dør.**

### Hvad der stadig IKKE er prøvet, og hvorfor
De fire linjer der finder målversionen i indmeldelsessvaret bruger cJSON,
og cJSON er ikke i host-bygningen. Jeg valgte at lade det være: prøverne
skal kunne køre med bare `cc` uden ESP-IDF, og det er værd at bevare.

Så den del er dækket to steder i stedet: serveren prøver at svaret
**indeholder** målversionen, og firmwaren skriver i loggen at den fandt
den. Begge dele kan ses, men ingen af dem beviser at nøglestien i koden er
rigtig. Det bliver først bevist på en rigtig skærm.

**Status:** 9 vagter, 1141 prøver på maskinen, 89 hele vejen igennem,
firmwaren bygger.

## 2026-10-08 14:03

### Hvor hurtigt kommer en kommando frem? Nu er det målt, og to fejl kom frem
Vi sagde at skærmen er live. Det var ikke målt. Nu er det, og målingen
kører med hver gang.

**Tallene**, på localhost: skærmen sender og tallet står på serveren efter
**53 ms**. Vi sætter en målversion og skærmen har den efter **59 ms**.

Begge tal indeholder ét API-kald, så de er et loft og ikke MQTT alene.
Skilt ud: API-skrivningen er 54 ms, og **36 ms af det er at hente en ny
nøgle hver gang**. MQTT-leddet er altså omkring **5 ms**. Netværket til
Danmark er målt til 12 til 36 ms herfra, så i drift lander det omkring et
tiendedel sekund. Vores kode er ikke det der tager tid.

**Men de 5 millisekunder var meningsløse, og det er den første fejl.** En
målversion blev bare **gemt**. Opdateringen skete først når det periodiske
tjek løb, og det er hver halve time. Der gik altså mellem nul og tredive
minutter fra man trykkede til der skete noget, i gennemsnit et kvarter. De
fem millisekunder var 180.000 gange hurtigere end det der fulgte efter.

Nu udløser et **nyt** mål et tjek med det samme. Kun et nyt: serveren
sender værdien igen ved hver indmeldelse, og en genforbindelse er ikke en
ny ordre. Uden den sammenligning ville en skærm på et dårligt net banke på
GitHub i stedet for at vise tal.

**Den anden fejl er værre: en ordre sat mens skærmen var slukket blev
tabt.** Et abonnement giver kun beskeder mens man lytter, og forbindelsen
er "clean session", så serveren gemmer ingenting. Værdien stod på serveren,
men skærmen spurgte aldrig efter den. Målt: skærmen fik **ingenting** ved
at komme tilbage. Man kunne altså sætte en opdatering på hundrede skærme,
og de der var slukket ville aldrig få den, mens dashboardet viste at den
var sat.

Rettelsen krævede ikke et ekstra kald. **Indmeldelsessvaret indeholder den
allerede:** målt til 4218 bytes med alle nitten felter og deres værdier,
også målversionen. Skærmen læser den nu derfra.

Og målingen prøver netop det der afgør sagen: at svaret **har**
målversionen med. Holder serveren op med at sende værdier i svaret, er
ordren tabt igen, og så får vi det at vide.

**Begge veje er MQTT**, bekræftet i koden: data op med QoS 0, som er det
rigtige for noget der sendes hvert andet sekund og hvor næste måling
alligevel afløser den, og kommandoer ned via abonnement.

**Status:** 9 vagter, 1113 prøver på maskinen, 89 hele vejen igennem,
firmwaren bygger.

## 2026-10-08 07:26

### Et forkert svar kunne blive meldt som et godt svar
Den alvorligste af de 23 overlevende mutanter. Vendes `if (err != ZS_MB_OK)`
om efter at svaret er pakket ud, bliver et svar der **ikke** kunne pakkes
ud, meldt som godt. Så viser skærmen hvad der nu stod i bufferen: gamle tal,
eller ingenting. Et forkert tal der ser ud som et rigtigt tal.

**Ingen prøve fangede det**, og det er værd at forstå hvorfor. Rammerne
bliver prøvet grundigt på maskinen, men der bankes direkte på udpakningen.
Hele vejen igennem svarer simulatoren **altid** rigtigt, så den sti hvor
udpakningen fejler blev aldrig gået.

Simulatoren kan nu svare pænt men forkert: alt rigtigt i svaret, kun
transaktions-ID'et ét forkert. Det er præcis hvad et svar på den **forrige**
forespørgsel ser ud som, og det sker på et net hvor noget bliver forsinket.

Og det tog tre forsøg at lave en prøve der faktisk måler noget:

1. Første forsøg svarede forkert fra første forespørgsel. Så falder
   SunSpec-genkendelsen, skærmen siger nej af den grund, og mutanten
   overlevede alligevel. Ødelagte data falder på næste tjek.
2. Andet forsøg svarede forkert efter 40 forespørgsler. En hel kørsel
   bruger omkring 20, så alt var forbi inden. Prøven målte ingenting.
3. Tredje forsøg: **18**, målt. Ved 12 og 18 siger værktøjet fra, ved 24 og
   derover er alt forbi inden. 18 rammer midt i aflæsningen.

Og fordi tallet 18 kan blive forkert den dag kæden bruger flere
forespørgsler, er der nu en prøve på at værktøjet **faktisk fejlede**. Står
der nul fejl, siger prøven at tallet skal sættes ned, i stedet for at blive
grøn uden at måle noget.

Med mutanten inde viser prøven skaden sort på hvidt: "Solgt til net 262144
Wh". Et skraldtal præsenteret som en måling.

**Status:** 9 vagter, 1113 prøver på maskinen, 89 hele vejen igennem,
fuzzerne rent, firmwaren bygger.

## 2026-10-08 04:02

### Mutationstesten døde selv, og den fandt tre ting mere
Kørslen stoppede ved 149 af 374 med et nedbrud: en mutant skrev bytes der
ikke er gyldig UTF-8, og værktøjet kunne ikke læse dem. Det er netop der den
skal holde: en mutant der ødelægger en tekst er præcis den slags der laver
skrald. Den tåler det nu.

**En exception er ikke en fejl på forbindelsen.** Reglen stod skrevet forfra
to steder, og mutationstesten vendte den om på det ene uden at nogen prøve
sagde fra. Det betyder noget i praksis: søgningen går modellerne igennem og
får en exception på hver model inverteren ikke har. Lukkede vi forbindelsen
på en exception, skulle der forbindes forfra for hver af dem, og en søgning
der tager sekunder ville tage minutter og banke på inverteren hele vejen. Nu
ét sted, prøvet af for hver fejlkode, og med en prøve på at **præcis to**
koder beholder forbindelsen, så der ikke kan snige sig en tredje ind.

**En afkoder var glemt i kanttesten.** `zs_ss_dec_enum16` kunne få sin
`off >= n` lavet til `off > n`, altså læse ét register for langt, uden at
noget sagde fra. Jeg havde skrevet de afkodere ned jeg kom i tanke om, og
glemt én af syv.

Og det er rettet ved roden, ikke kun ved at tilføje den: antallet af
afkodere står nu i headeren, og prøven falder hvis der kommer en til. Så er
der en beslutning at tage i stedet for et hul der ligger og venter.
Efterprøvet ved at sætte tallet op.

### Og værktøjet tog en tastefejl som et værtsnavn
`zs-probe --hjaelp` forsøgte at forbinde til en maskine der hed "--hjaelp".
Alt der ikke blev genkendt blev brugt som vært, også et flag der manglede
sin værdi: `--port` til sidst blev værten.

Det er det værktøj man står med ude ved et anlæg, tit på en dårlig
forbindelse og med nogen der venter. Nu er der en hjælp der viser alle
flagene og et par eksempler, og en tastefejl siger hvad den ikke kender og
**forsøger ikke at forbinde**. Fire nye prøver hele vejen igennem.

**Status:** 9 vagter, 1091 prøver på maskinen, 83 hele vejen igennem,
fuzzerne rent, firmwaren bygger.

## 2026-10-08 03:40

### Læsning og skrivning tjekkede samme felt med forskellige regler
Svarets længdefelt blev tjekket på to måder. Læsningen krævede 3 til 253.
Skrivningen krævede bare "mindst 1", og havde desuden bufferens grænse i
samme betingelse.

Det er det **samme felt i den samme protokol**. To steder der tjekker det
samme med forskellige regler er ikke to lag, det er ét sted hvor nogen
senere retter det ene og tror begge er rettet. Nu bruger begge den samme
regel, og bufferens grænse står for sig.

**Og det skal siges ærligt: det var ikke farligt.** Jeg efterprøvede det.
En for kort ramme bliver afvist længere nede, hvor den bliver pakket ud:
den kræver mindst 9 bytes. Så den slappe grænse kunne ikke udnyttes. Det
her er en rettelse af en inkonsekvens, ikke af et hul.

Skrivningen hele vejen igennem er kørt efter: alle fem prøver består, så den
strammere grænse afviser ikke et rigtigt svar.

## 2026-10-08 03:29

### Fladen var ikke så uafhængig af skærmstørrelsen som den skulle være
Geometrien står ét sted, i `ui/zs_layout.h`, og regnes ud af bredden og
højden. Men fire steder stod målene **alligevel** skrevet ind:

- **statuslinjen**: tandhjulet på 430, statusikonet på 400. Regnet i hånden
  fra en 480 px skærm, med en kommentar der sagde det. På en 800 px skærm
  ville tandhjulet sidde midt på skærmen i stedet for i hjørnet.
- **flow-siden**: midterlinjen på 240, sideknuderne på 68 og 412, stregerne
  der stopper på 92 og 388. Tallene hører sammen i par, 412 er 480 minus 68,
  men de stod som om de ikke gjorde.
- **prissiden**: hele den lodrette stak, 14 til 380. På en højere skærm ville
  alt blive siddende foroven med et tomt felt nederst.
- **ventesiden**: teksten på 160. De 160 er spinderen på 70 plus dens 64 plus
  26 luft, men det forhold var ikke til at se.

Alle fire er flyttet til det centrale sted og regnes nu. **Tallene er de
samme på 480**, og der er en vagt ved oversættelsen på hvert enkelt af dem,
så skærmen ser præcis ud som før. Det er efterprøvet to gange: den
signerede binær har **samme størrelse** som før, og tallene er printet ud
ved 480 og ved 800. På 800 flytter tandhjulet til 750, midterlinjen til 400,
og søjlerne i grafen bliver højere.

Hvor den ekstra plads går hen er et **valg**, ikke et regnestykke. En
skrifttype bliver ikke større af at skærmen gør, og et tal på 64 px fylder
54 px uanset hvad. Det eneste der bruger mere plads fornuftigt, er søjlerne
i grafen: en højere graf viser det samme døgn bedre. Det valg står nu ét
sted, så det kan ændres på én linje.

### Ny vagt: skærmens mål må ikke stå skrevet ind i fladen
Jeg fandt de fire ved at lede. Nu bliver der ledt hver gang. Reglen er smal
med vilje: ikke alle tal er forbudt, kun de tal der **er** skærmens mål,
altså bredden, højden, sidens højde og halvdelen af bredden. De kan ikke
være rigtige som faste tal, for de betyder noget andet på en anden skærm.

Første forsøg gav **fjorten falske fund**, fordi den læste hver linje i en
blokkommentar som kode. En vagt med fjorten falske fund bliver slået fra,
så den følger nu kommentarblokke linje for linje. Og "halvdelen af sidens
højde" er taget af listen: zs_flow.c havde `Y_SIDE_VALUE 204`, og 204 er
tilfældigvis halvdelen af 408, men tallet betyder en plads i en lodret stak.
Et falsk fund er dyrere end et manglende.

Efterprøvet ved at lade to faste tal snige sig ind igen. Begge fanges. Og
CI kører den af sig selv, fordi vagtlisten står ét sted.

**Status:** 9 vagter, 1075 prøver på maskinen, 79 hele vejen igennem,
fuzzerne rent, firmwaren bygger med samme størrelse som før.

## 2026-10-08 03:09

### En enhed der lyver om hvor langt svaret er
MBAP-headeren fortæller hvor mange bytes der følger, og tallet kommer fra
den anden ende, altså fra noget vi ikke bestemmer over. Siger den 65535 og
vi tror på det, læser vi 65535 bytes ind i en buffer på 259.

Mutationstesten kunne fjerne reglen på 3 til 253 uden at noget sagde fra.
Reglen står nu for sig og er prøvet af for **alle 65536** længder, ikke for
nogle udvalgte tal.

**Og her skal jeg rette mig selv.** Jeg skrev først at den manglende vagt
gav et overløb. Det gør den ikke. Der er en vagt mere lige efter, på om det
kan være i bufferen, og den fanger det. Det er målt: med kun den første
fjernet består alt, med **begge** fjernet falder prøven. Reglen står
alligevel fast, af to grunde: den afviser et meningsløst tal tidligt med en
besked man kan se i loggen, og to lag er bedre end ét når tallet kommer
udefra.

Ny prøve hele vejen igennem: en server der lyver om længden **og** sender
80 KB. Den gamle prøve med skrald var ikke nok, for den sender 160 bytes og
lukker, så modtagelsen giver op før der sker noget. Den nye er efterprøvet
ved at fjerne begge vagter: så falder den. Det er den eneste måde at vide
at den er værd at have.

Og sammenhængen mellem de to tal, længdegrænsen og bufferens størrelse,
holdes nu fast ved **oversættelsen**, der hvor begge tal findes. Sættes et
af dem en dag, falder bygningen i stedet for at nogen opdager det på en væg.
Efterprøvet ved at gøre bufferen mindre.

### Vagten mod en gammel binær virkede af sig selv
Midt i arbejdet nægtede prøverne at køre, fordi jeg havde ændret kildekode
efter sidste bygning af værktøjet. Præcis det den blev lavet til i dag.

**Status:** 8 vagter, 1075 prøver på maskinen, 79 hele vejen igennem,
fuzzerne rent, firmwaren bygger.

## 2026-10-08 02:45

### Testkørslen byggede alt undtagen det der skal ud på skærmen
Den kørte 1064 prøver på maskinen, hele datavejen mod en simuleret
inverter, fuzzerne og otte vagter, og sagde "Alt bestået" uden nogensinde
at have oversat koden til en ESP32. En header der mangler en include, eller
en `_Static_assert` der falder på en anden arkitektur, så man først efter et
push.

Firmwaren bygges nu som sidste trin, så den hurtige besked kommer først.
Mangler værktøjskæden på maskinen, siges det, og det er ikke en fejl.
Efterprøvet ved at ødelægge en struktur: bygningen falder, og trinnet
rapporterer det. Den viser også appens størrelse, 50 procent af
partitionen fri, så en firmware der ikke kan være der bliver opdaget nu og
ikke ved en udgivelse.

Første forsøg greppede forkert og viste **bootloaderens** størrelse, altså
"30% free" om noget der intet havde med firmwaren at gøre. Rettet til at
nævne filnavnet, så der kun kan komme ét svar.

### Fem ting i Modbus-laget stod skrevet forfra
Mutationstesten fandt dem, og det var samme slags hver gang: en beslutning
der stod flere steder, eller midt i en løkke hvor den ikke kan prøves.

**Transaktions-ID'et** sprang nul over i kode skrevet forfra **tre**
steder. Nul er lovligt i Modbus, men et logudtræk hvor hvert svar står med
0 er ubrugeligt når man skal finde ud af hvad der hørte til hvad. Nu ét
sted, prøvet af hele vejen rundt til 65535 og om igen: nul kommer aldrig
ud, og tælleren står aldrig på nul bagefter.

**Svartiden** stod to steder, og dens to betingelser kunne ikke prøves.
Den grimme af dem: "skal den sættes" vendt om. Beder man om en kortere
svartid fordi man er ved at scanne, og den ikke bliver sat, tager
scanningen elleve gange så lang tid som den skal. Nu er det én funktion,
og alle 36 kombinationer er prøvet af.

**Standardsvartiden 1500** stod fem steder. Fem steder der skal være ens,
bliver det ikke.

**Grænsen på en skrivning** stod som et nøgent 123 fire steder. Den er 123
og ikke 125 fordi en FC16-ramme har adressen og byte-tælleren med. Et nøgent
tal er et sted hvor nogen senere retter det til 125 fordi det ligner
læsegrænsen. Den har et navn nu, og prøven holder fast at den er lavere end
læsningens.

**Og fire vagter i én betingelse** på at skrive, alle uprøvede. Fire led er
fire steder at tage fejl, og man kan ikke se hvilket der sagde fra. De står
hver for sig nu.

Otte mutanter sat ind igen, alle dør.

**Status:** 8 vagter, 1064 prøver på maskinen, 77 hele vejen igennem,
fuzzerne rent, og firmwaren bygger og signeres.

## 2026-10-08 02:31

### Formen på det der ligger i flashen står nu fast
Udgangspunktet for timens energi gemmes i flashen, så en genstart ikke
koster en time. Indlæseren afviser en klump af en anden **størrelse**, og
så koster det én time. Det er håndteret.

Men bytter man to felter om **uden** at størrelsen ændrer sig, er der ingen
der siger fra. Så bliver et gammelt serienummer læst som tællerstande,
eller en tællerstand som en dato, og timens energi bliver et vildt tal der
ikke ser forkert ud. Det er præcis den fejl der blev lukket ved at binde
udgangspunktet til serienummeret, og den må ikke komme ind ad bagdøren.

Formen står nu fast ved oversættelsen: størrelsen og hvert enkelt felts
plads. Ændrer nogen strukturen, falder oversættelsen med det samme, og så
er der et valg at tage i stedet for en overraskelse: skal de gamle data
kunne læses, så skal der et versionsnummer med i klumpen, eller er det i
orden at hver skærm mister én time.

Efterprøvet med tre ændringer: et felt der bliver større, to felter byttet
om, og et længere serienummer. Alle tre fanges.

## 2026-10-08 02:21

### Hvad send() sagde, trukket ud så det kan prøves
`if (n == 0)` stod midt i sendeløkken, og den linje skrev jeg selv tidligere
i dag. Mutationstesten lavede den til `if (n != 0)`, og ingen prøve sagde
fra.

Nul er det farlige tilfælde. `send()` tog ingenting, selv om vi bad om mere
end nul, og så er `errno` **ikke sat af det kald**. Ser man alligevel på
det, og står der tilfældigvis `EINTR` fra noget helt andet, prøver vi igen
i en **evig løkke**. Skærmen ville stå og hænge uden en fejl nogen steder.

Beslutningen ligger nu i `zs_mb_send_art`, og den er prøvet af for alle
kombinationer af n fra -3 til 3 og 60 errno-værdier. Den vigtigste prøve er
ikke en enkelt værdi, men at **ingen** kombination giver "prøv igen" uden at
`send()` faktisk fejlede med `EINTR`. Det er den egenskab der udelukker en
evig løkke, og den kan ikke prøves ved at vælge nogle tal.

Og samme slags vagt på at læse: `mb er NULL eller out er NULL` kunne laves
til `og`, så en NULL slipper igennem når den anden er sat. Prøvet af nu,
begge veje og begge på én gang.

**Status:** 8 vagter, 1034 prøver på maskinen, 77 hele vejen igennem.

## 2026-10-08 02:01

### To vagter mere på at forbinde, og et || der ikke gav noget
Mutationstesten fandt tre ting i `zs_modbus_tcp.c`, alle uprøvede.

**NULL som forbindelse kunne slippe igennem.** Vagten var `mb er NULL
eller målet er ikke i orden`. Blev `eller` lavet til `og`, slipper en
NULL-pointer igennem når vært og port er fine, og så skrives der i NULL
lige efter.

**En vært præcis så lang som feltet kunne slippe igennem.** Grænsen var
`strlen(host) >= 46`. Blev den `>`, er der ikke plads til nulbyten. En
IPv6-adresse kan fylde det meste af de 46 tegn, så det er ikke en
teoretisk grænse. Begge er nu prøvet af med én under, præcis, og én over.

**Og et `eller` der ikke gav noget.** `getsockopt(...) < 0 eller soerr !=
0` er to uafhængige ting skrevet som én. De står hver for sig nu, ligesom
`fcntl` tidligere i dag. Det er den bedste slags rettelse: fejlen kan ikke
laves, i stedet for at blive fanget.

Og min egen prøve var forkert først: jeg satte en nulbyte længere ude i
samme buffer uden at fjerne den første, så de to længder var identiske og
prøverne målte det samme. De var grønne og sagde ingenting. Nøjagtig samme
slags fejl som koden selv skal fange. Nu bygges hver længde forfra.

**Status:** 8 vagter, 1021 prøver på maskinen, 77 hele vejen igennem.

## 2026-10-07 23:36

### Tre huller i socket-laget, fundet ved at ødelægge koden med vilje
Mutationstesten arbejder sig gennem 374 mutanter. Tre af dem overlevede i
`zs_modbus_tcp.c`, og alle tre var reelle.

**At lukke kunne gå ned.** Vagten i `zs_mb_close` var `mb er NULL eller fd
under nul`. Blev `eller` lavet om til `og`, bliver `fd` læst selv om
pointeren er NULL. Og vi lukker ved **enhver** fejl, så det er den sti der
bliver gået oftest af alle når noget er i vejen. Ingen prøve nævnte den.
Nu prøves at lukke NULL, at lukke en der aldrig var åben, og at lukke to
gange i træk.

**En timeout kunne blive regnet som en forbindelse.** `if (rc <= 0)` stod
midt i `connect_with_timeout` og kunne derfor ikke prøves. Laves den til
`rc < 0`, bliver nul regnet som succes, og nul betyder at **tiden løb ud**.
På en scanning af 254 adresser ville vi melde en inverter på hver af dem.
Beslutningen ligger nu for sig, og hele området fra -50 til 50 er prøvet af.

**To tjek stod som ét.** `flags < 0 eller fcntl(...) < 0` gav ingenting og
var et sted at tage fejl: blev `eller` lavet til `og`, slap en mislykket
`F_SETFL` igennem, og vi kørte videre med en sokkel der **blokerer**. Så
ville `connect` hænge i stedet for at give op, og hele timeouten ville ikke
virke. De står hver for sig nu, så den fejl findes ikke længere. Det er
den bedste slags rettelse: fejlen kan ikke laves, i stedet for at blive
fanget.

Alle tre mutanter er sat ind igen og dør. Samme fremgangsmåde som før: en
prøve der ikke er efterprøvet mod den fejl den skal fange, er et gæt.

**Status:** 8 vagter, 1005 prøver på maskinen, 77 hele vejen igennem,
fuzzerne rent.

## 2026-10-07 23:30

### Fase 2: live-tallene bliver ikke gemt mere
Skærmen sender alle fem live-tal ved hver aflæsning, altså hvert andet
sekund, og serveren skrev en række i databasen for **hver** skrivning. Også
når værdien var uændret: målt ved at skrive den samme værdi tre gange, og
der kom tre rækker.

Serveren rydder selv op efter to uger, så det der betyder noget er hvad der
ligger i de to uger:

- **før:** 5 felter gange 43.200 om dagen gange 14 dage = **3.024.000
  rækker** pr. skærm
- **efter:** 6 felter gange 24 om dagen gange 14 dage = **2.016 rækker**
  pr. skærm

1500 gange færre. Rækkerne er facit, for de følger af takten og af
ingenting andet.

**Live-tallene forsvinder ikke.** De sendes som før og står på enheden som
før. Det er kun det at gemme hver ændring der holder op, og historikken er
nu timens energi i rigtige kilowattimer regnet af inverterens egne
livstællere.

Fremgangsmåden var at prøve på **én** enhed først. Målt at de fem live-tal
ikke længere gemmes, at energifelterne stadig gemmes, og at live-tallene
stadig **står** på enheden med de rigtige værdier. Derefter rullet ud på
begge opsætninger og alle 66 enheder. Kører man igen sker der ingenting.

Politikken står ét sted på serveren, og skriptet rører kun det ene flag:
enheder, adgang og skrivebeskyttelse bliver stående. Felter der ikke står i
tabellen bliver slet ikke rørt.

### Serverens egen gennemgang krævede det modsatte
Den tjekkede at `solarPower` **blev** gemt, og det var rigtigt før. Efter
fase 2 var den prøve forkert. Nu prøver den begge veje: at timens energi
bliver gemt, og at live-tallene ikke bliver.

Begge dele skal med. En prøve der kun siger "live gemmes ikke" ville også
være grøn hvis **ingenting** blev gemt, og så havde vi tabt historikken
uden at nogen sagde fra. 13 af 13 består, også på en helt nyindmeldt skærm,
hvilket samtidig beviser at skabelonen til nye skærme er rettet.

### Ingen dashboards eller regler hang på live-historikken
Tjekket før udrulningen, ikke antaget: der er nul dashboards og nul regler
på serveren, så ingenting gik i stykker. Når der laves dashboards, skal de
tegne energifelterne og ikke live-tallene.

### Og et tal i dokumentationen var forkert
Der stod at live-tal kostede 183 GB mod 142 MB. To ting var galt: tallet var
gættet og ikke målt, og det var regnet "om året", hvilket ikke findes når
serveren rydder op efter to uger. Rækketallene står nu, og de er præcise.
Størrelsen i bytes står med forbehold, for prisen pr. række er målt på en
lille tabel og er et overslag opad.

**Status:** 8 vagter, 995 prøver på maskinen, 77 hele vejen igennem på
firmwaren. 13 af 13 i serverens gennemgang. Alt består.

## 2026-10-07 22:45

### Mutationstesten arbejdede, men man kunne ikke se det
Kørslen havde kørt en time uden et tegn på liv, og spørgsmålet var rimeligt
nok: arbejder den, eller venter vi på ingenting? Den arbejdede. Men at man
ikke kunne se det, er i sig selv en fejl: en kørsel man ikke kan se ind i,
kan ikke skelnes fra en der hænger.

Tre ting er rettet.

**Fremdriften skrives nu ned.** Hver mutant giver en linje med hvor langt
den er, hvad der fangede den, og hvor lang tid der er igen. Den skrives
både på skærmen og til en fil der kan følges med `tail -f`. Stien står
øverst i kørslen. Og den ligger **uden for** kopien: første forsøg lagde
den indeni, og så blev den ryddet op sammen med kopien, netop når man ville
læse den.

**Den rydder op når den bliver afbrudt.** Før blev kopien af koden liggende
hver gang en kørsel blev stoppet, og den blev ikke fundet igen. Et `finally`
dækker ikke et signal, så det håndteres nu for sig. Efterprøvet ved at
stoppe den og se at mappen er væk.

**Tålmodigheden var for stor.** En mutant der får prøverne til at **hænge**
fik et kvarter før den blev opgivet. Hele datavejen tager 150 sekunder når
alt er normalt, så grænsen er nu 420 sekunder: rigeligt til en langsom
maskine, og kort nok til at et hæng koster minutter i stedet for timer.

Og tiden er nu **målt** i stedet for gættet: 16 mutanter på det første
minut, alle fanget af enhedstestene. Den tidligere kørsel målte i øvrigt på
kode der var en time gammel, altså fra før portene blev lagt om, så dens
resultat ville ikke have beskrevet det vi har nu. Den er startet forfra.

## 2026-10-07 22:26

### Porten vælges nu af styresystemet, så to kørsler aldrig kan støde sammen
Først stod testportene **fast** på 15020. Det væltede den dag der kørte to
ting på maskinen samtidig: mutationstesten kører også hele vejen igennem,
så to kørsler så 15020 som fri i **samme øjeblik** og tog den begge to. To
simulatorer på én port, og prøverne læste på den forkerte. 25 prøver
fejlede med tal fra et helt andet anlæg, og fejlen pegede på firmwaren.

Så blev der ledt efter en ledig port i stedet. Det er bedre, men det er
stadig et gæt: mellem at vi ser porten fri og at simulatoren binder den,
kan en anden have taget den. Et vindue på et par millisekunder er stadig et
vindue, og den slags fejl kommer igen når man mindst har tid til den.

Nu starter simulatoren med `--port 0`. Så vælger **styresystemet**, og det
er den eneste der kan garantere at ingen anden har porten. Simulatoren
skriver hvilken den fik, og prøverne læser den. Det samme for den falske
enhed og for de prøver der selv binder en port.

De to prøver der skal have en port hvor der **ikke** er noget, gættede før
på 15999 og `port + 1`. Nu holdes en socket åben på en port
styresystemet har givet, uden at lytte: et forsøg på at forbinde bliver
afvist præcis som når der ikke er noget, og ingen anden kan tage porten
imens.

Efterprøvet ved at køre **tre** komplette kørsler samtidig. Alle tre:
77 tjek, alle bestået. Før kunne det ikke gøres.

### En gammel binær kan ikke længere få prøverne til at lyve
Da `util/zs_tekst.c` blev glemt i værktøjets byggescript, faldt bygningen,
og det kunne man se. Men testkørslen **kørte videre**, og prøverne hele
vejen igennem målte på den gamle binær der stadig lå på disken. De meldte
"70 tjek, alle bestået". Det var ikke til at se på noget.

To ting, så det ikke kan gentage sig:

Prøverne siger nu **selv** fra. Er der én kildefil der er nyere end
binæren, nægter de at køre og skriver hvilken fil og hvad man skal gøre.
Det virker også når man kører dem i hånden, og det er netop det man gør når
man fejlsøger.

Og falder bygningen i testkørslen, bliver de prøver der bruger binæren
**sprunget over** med gul skrift, i stedet for at køre på noget andet end
det vi lige byggede. Begge dele er efterprøvet ved at fjerne stien igen og
ved at røre en kildefil.

**Status:** 8 vagter, 984 prøver på maskinen og 77 hele vejen igennem,
alle består. Tre kørsler samtidig går også igennem.

## 2026-10-07 23:45

### Hvis elmåleren falder ud i én time, kunne hele livstælleren blive sendt
Fundet ved at ødelægge koden med vilje. Vagten der kræver at **begge**
tidspunkter har tælleren kunne laves om fra "eller" til "og" uden at én af
de knap tusind prøver sagde fra.

Med den ændring: havde vi **ikke** tælleren sidste time og har den **nu**,
så står der nul i udgangspunktet, og forskellen bliver hele inverterens
livstæller. Tusinde kilowattimer sendt som én times forbrug, gemt på
serveren for evigt.

Og det er ikke teori. Elmåleren sidder på sin **egen** Modbus-unit, så den
kan være uden svar et øjeblik mens inverteren svarer fint. Så mangler købt
og solgt i én aflæsning og er der igen i den næste. Det sker på et rigtigt
anlæg.

Koden var rigtig. Prøven manglede. Nu er begge veje prøvet af: måleren der
forsvinder og kommer igen, og måleren der falder ud. I begge tilfælde
springes feltet over, mens produktion og batteri regnes som de skal. Og
begrundelsen står nu i koden, så ingen senere retter "eller" til "og" i
god tro.

**Hele fase 1's kerne er nu dækket:** alle 25 måder koden kan ødelægges på
bliver fanget af prøverne.

### Og mutationsværktøjet kunne hænge for evigt
Det havde ingen tidsgrænse. En mutant der får en løkke til at køre for
evigt, for eksempel hvis "mindre end" bliver "ikke lig med", ville få
værktøjet til at stå stille for evigt på den ene mutant. Det skete: 28
mutanter var en halv time inde uden at være færdige. Nu er der en grænse
på alle tre kald, og en mutant der hænger regnes som fanget, for så er
opførslen ændret på en måde prøverne kan se.

## 2026-10-07 23:10

### Ombytningen af store og små bogstaver lå to steder
SunSpec-afkodningen normaliserede kanalnavne med `toupper` fra systemets
eget bibliotek. Det virkede, men det ser på maskinens **sprogvalg**, og vi
havde allerede den samme ombytning den anden vej i vores eget modul. To
steder at tage fejl er ét for mange. Begge veje ligger nu samme sted, og
begge er prøvet af for alle 256 byteværdier, også at op og ned igen giver
det samme.

Det er ikke pedanteri: det er netop ombytningen af store bogstaver der i
dag viste sig at være forkert i kanten, og den fejl ville have kostet at
et mærke ikke kunne findes. Her bruges den til at genkende batteriets
**aflade**kanal, og bliver den ikke genkendt, får batteriets effekt
omvendt fortegn. Så siger skærmen "lader 3 kW" mens batteriet aflader.

### Fuzzerne var den fjerde bygning der faldt
Koden bygges fire steder, hver med sin egen liste over hvor headerne
ligger. Da det nye modul kom til, blev det glemt i tre af dem, én ad
gangen, med en hel prøvekørsel imellem hver gang. Fejlen kommer **sent**:
alle 966 prøver består først, og så vælter bygningen af noget helt andet.

Vagten der tjekker det kendte tre bygninger. Nu kender den også fuzzerne.
Efterprøvet i alle fire ved at fjerne stien: den siger fra, og den siger
hvilken fil der beder om hvad.

## 2026-10-07 22:40

### En advarsel der ville fylde loggen 1800 gange i timen
Advarslen om at timeenergien ikke kan regnes stod i den gren der bliver
kaldt **hvert andet sekund**. Det er 1800 linjer i timen, og så fylder den
loggen og skjuler alt andet. En log man ikke kan læse er lige så slem som
ingen log. Nu siges det én gang, og igen hvis det sker på ny efter at det
har virket.

### Prøverne rydder op efter sig selv, også når de bliver afbrudt
Bliver prøverne stoppet midt i, med Ctrl-C eller fordi en ydre tidsgrænse
løber ud, levede simulatoren og værktøjet videre som forældreløse
processer og blev ved at banke på porten. Næste kørsel startede sin egen
simulator, den gamle scanning åd forbindelserne, og en prøve faldt med en
besked der intet havde at gøre med sagen. **Det er sket to gange i dag**,
og begge gange blev der ledt efter en fejl i koden der ikke var der.

Nu lukker prøverne deres hjælpeprocesser ned når de bliver afbrudt.
Efterprøvet ved at stoppe en kørsel midtvejs: der ligger ingenting
tilbage.

## 2026-10-07 22:05

### Timeenergien kunne sende et vildt tal hvis inverteren blev skiftet
Den alvorligste af dagens fund. Udgangspunktet for timens energi ligger i
flashen, så en genstart ikke koster en time. Det er meningen. Men det
overlevede også at **inverteren blev skiftet**, at kunden valgte en anden i
indstillingerne, eller at skærmen blev flyttet til et andet anlæg.

Timens energi er den nye tællerstand minus den gamle. Efter et skift ville
den altså blive **den nye inverters livstæller minus den gamles**. Er den
nye højere, fordi anlægget er ældre eller større, sender skærmen et tal på
mange tusinde kilowattimer for **én time**. Det bliver gemt på serveren for
evigt og ødelægger hver graf og hver sum bagefter. Og det ser ikke ud som
en fejl: det sker én gang, det ser ud som et stort tal, og bagefter kan
ingen sige hvorfor.

Udgangspunktet er nu **bundet til inverterens serienummer**. Er
serienummeret ikke det samme, er tællerstanden ikke vores, og der startes
forfra. Prøvet af både med et skift til en inverter med en meget højere
tæller, med et skift tilbage igen, og med ét tegn galt helt ude i enden af
serienummeret.

Og uden et serienummer regnes der **ingenting**. Serienummeret læses ved
hver forbindelse, så er det tomt, er der noget galt, og så er det bedre at
springe timen over end at tilskrive den en inverter vi ikke kan navngive.
Det siges nu i loggen; før stod den gren i `default` og blev tiet ihjel.

### Tællerne var slet ikke prøvet hele vejen igennem
De fem livstællere blev læst af firmwaren, regnet på, og sendt til
serveren. Men **ikke én prøve** fulgte dem hele vejen fra inverteren og ud.
Hullet var reelt: da tællerne blev lagt ind, blev batteriets to kun sat i
den gren der håndterer kanaler **uden navn**. Simulatoren navngiver sine,
så de to tællere var tomme hele vejen. Det blev fanget ved et tilfælde, ikke
af en prøve.

Nu er der syv nye prøver hele vejen igennem: at alle fem læses, at ingen
står på nul, at produktionen er større end det solgte så to tællere ikke er
byttet om, at de **vokser** mellem to læsninger, at ingen **falder**, og at
batteriets to ikke står der på et anlæg uden batteri. Prøven er efterprøvet
ved at sætte den oprindelige fejl ind igen: den falder.

### Værktøjet viser nu tællerne, så de kan ses på et anlæg
`zs-probe` viste dem ikke. Timens energi er en forskel mellem to
tællerstande en time fra hinanden, og går noget galt, kan man ikke se det
på resultatet. Nu står de rå stande i udskriften, så man kan køre to gange
med en times mellemrum og regne efter. Og står der **nej** ud for en
tæller, er det den man skal lede efter, i stedet for at bruge en time på
anlægget med at lede efter en fejl i udregningen.

### En fejl i mutationsværktøjet selv
Det byggede kun den almindelige `zs-probe`, men prøverne hele vejen
igennem kører mod den **sanitiserede**. Så prøvede de en binær som
mutationen aldrig havde rørt, og de kunne derfor ikke fange noget som
helst. Værktøjet ville melde huller der ikke var der. Det bygger nu begge
udgaver.

### Og en prøve der ikke kunne fejle
I min egen nye prøve bestod tjekket "ingen af dem står på nul" selv om alle
fem manglede, fordi et manglende tal ikke er nul. Rettet: mangler der en
tæller, stopper prøven der i stedet for at bestå på ingenting.

**Status:** 959 prøver på maskinen og 77 hele vejen igennem, alle består.
Firmwaren bygger og signeres, 50 procent af pladsen er fri.

## 2026-10-07 21:22

### 25 prøver fejlede, og det var ikke koden
Efter at have lukket de fem huller faldt 25 af prøverne hele vejen
igennem. Tallene kom fra et helt andet anlæg, og fejlen pegede lige ind i
den kode der netop var rettet. Den var ikke der.

Det der skete: **to testkørsler sloges om den samme port.** En
mutationskørsel kørte i baggrunden, og den kører også hele vejen igennem.
Begge kørsler begyndte at lede efter en ledig port ved 15020, så begge
syntes den var fri i det samme øjeblik, og begge tog den. Så lå der to
simulatorer på porten, og prøverne læste på den forkerte.

To grunde til at det kunne ske, og begge er lukket:

**Den ledige port blev prøvet forkert.** Vi bandt `127.0.0.1` for at se om
porten var fri, men simulatoren lyttede på `0.0.0.0`. Man kan godt binde
den ene adresse mens en anden proces lytter på den anden, så porten så fri
ud og var det ikke. Nu bindes `0.0.0.0`, og der spørges bagefter om der
alligevel svarer noget.

**Alle begyndte samme sted i rækken.** Nu begynder hver kørsel et sted der
følger af dens eget procesnummer, så to kørsler ikke lander på det samme.

Og vigtigst: er porten i brug når en simulator skal starte, er det nu en
**hård fejl der stopper alt** med en besked der siger hvad man skal gøre.
Før kørte prøverne bare videre mod den forkerte simulator og viste 25
fejl der ikke havde noget med sagen at gøre. Det er den slags der koster
mest, for man leder efter fejlen det forkerte sted.

Samtidig: simulatoren lytter nu altid kun på `127.0.0.1`, og det står
**ét sted** i stedet for i hver enkelt prøve. Nogle prøver gav det med,
andre glemte det, og de sidste lagde så en åben Modbus-server ud på
netværket mens prøverne kørte. Og simulatoren kører nu under **præcis den
samme Python** som prøverne selv, ikke den der tilfældigvis står først i
PATH.

### En grøn testkørsel kunne være løgn
Da `util/zs_tekst.c` blev lagt ind, kom den med i firmwaren og i
enhedstestene, men blev glemt i værktøjets bygning. Bygningen fejlede, og
det var til at se. Det der **ikke** var til at se var det næste:
testkørslen gik videre, og hele vejen igennem kørte mod den **gamle
binær** der stadig lå der. Den meldte "70 tjek, alle bestået". Den løj.

Ny vagt, `tools/check-byg.py`: hver af de tre bygninger skal kunne finde
hver header som dens egne filer beder om, også de headere en header selv
tager med. Og hver `.c` skal stå i firmwarens byggefil, ellers bliver den
aldrig oversat til skærmen uden at nogen siger noget. Vagten er prøvet af
ved at fjerne stien igen og se at den bliver fanget.

### Listen over vagter stod to steder
Den stod i testkørslen og i CI. To lister der skal være ens, bliver det
ikke: lagde man en ny vagt ind og glemte CI, var der en regel vi troede
blev håndhævet og som ikke blev det. Nu står listen i
`tools/check-alle.sh`, og begge kalder den.

**Status:** 8 vagter, 959 prøver på maskinen og 77 hele vejen igennem,
alle består. Fuzzerne kører rent.

## 2026-10-07 21:00

### Timens energi set hele vejen igennem, og serveren har felterne nu
De seks felter er oprettet på serveren, og det blev gjort med et skript og
ikke i hånden. Grunden: skabelonen for en ny skærm findes **to steder**,
og retter man kun det ene, får halvdelen af skærmene aldrig felterne.
Skriptet lægger dem på begge og på de enheder der allerede er oprettet,
68 i alt, og kører man det igen sker der ingenting.

Derefter er hele vejen prøvet af med skærmens **egne formater**: seks
energital sendt som skærmen sender dem, og læst tilbage på enheden
bagefter. Alle seks kom igennem.

### Fem fejl fundet ved at ødelægge koden med vilje
En prøve der ikke kan fejle, er ingen prøve. Derfor blev der sat fejl ind
i koden med vilje, fem steder hvor **ingen** af de 848 prøver sagde fra.
Én af dem var en rigtig fejl der bare ikke var sprunget endnu:

**Et mærke med Z i navnet kunne ikke findes.** Søgningen efter fabrikat
lavede store bogstaver om til små med en grænse der var sat ved siden af,
så det sidste bogstav i alfabetet gik fri. Det betød ingenting i dag, for
det eneste mærke i tabellen hedder Fronius. Men **Zeversolar** og **ZCS
Azzurro** er rigtige fabrikater, og den dag et af dem blev skrevet ind i
tabellen, ville mærket ikke kunne findes, og vi ville lede efter fejlen i
afkodningen. Det er præcis den slags fejl tabellen blev lavet for at
undgå.

De fire andre var ikke fejl endnu, men steder hvor **den samme kode stod
skrevet forfra** og kun den ene halvdel var prøvet af.

### De små tekstgreb samlet ét sted
Vagten `hvis ud ikke er NULL og der er plads` stod skrevet forfra **fire
steder**. Hver gang den skrives forfra, er der en chance for at `og`
bliver `eller`, og så skriver vi i NULL. Det giver ingen advarsel, det
giver et nedbrud på en væg hos en kunde. Nu står den ét sted, og det sted
er prøvet af med NULL, med plads nul og med en tekst der er for lang.
Samme med ombytningen af store bogstaver: ét sted, og alle 256 byteværdier
prøvet af.

### To beslutninger trukket ud så de kan prøves
**Hvad en fejl fra netværket betyder** stod midt i to løkker og kunne
derfor ikke prøves. Vendte man den om, blev **hver** fejl meldt som
"svarede ikke i tide", også en forbindelse der var lukket. Så leder kunden
efter et langsomt netværk i stedet for efter en inverter der har lukket, og
den slags har kostet en halv dag før. Nu er det en funktion for sig, og
hele talområdet er prøvet af.

**Om vært og port overhovedet kan forsøges** stod to steder, og de to var
**ikke ens**: den ene afviste en tom tekst, den anden gjorde ikke. En tom
tekst er ikke en vært, for nogle maskiners navneopslag svarer "det er dig
selv", og så kunne vi melde inverteren fundet fordi der stod noget på
**vores egen** port. Nu er der én vagt.

### Prøverne kunne forurene hinanden
Her blev en halv time brugt på at lede efter en fejl der ikke fandtes.
Oprydningen efter en simulator sendte et stopsignal, ventede tre sekunder,
og **gav så tavst op**. Døde simulatoren ikke i tide, levede den videre på
porten. Fejlen faldt så i en senere prøve, nemlig den der tjekker at
skærmen siger fra når inverteren forsvinder: inverteren var ikke
forsvundet, den gamle simulator svarede stadig.

To ting er rettet. Oprydningen stopper nu hårdt hvis det pæne ikke virker,
og den **beviser** bagefter at porten er fri, ellers råber den op med det
samme. Og hver kørsel finder sin egen ledige port, så to kørsler ikke kan
rode i hinanden.

### Mutationstesten er nu et værktøj
Den blev lavet i hånden hver gang. Nu ligger den i
`tools/mutationstest.py`, med de mutanter der ikke betyder noget skrevet
på en liste med begrundelse, så de ikke skal vurderes forfra. Den arbejder
på en **kopi** af koden: et værktøj der skriver fejl ind i rigtige
kildefiler kan efterlade én, og så bygger man en mutant uden at vide det.

**Status:** 941 prøver på maskinen og 70 hele vejen igennem, alle består.
Firmwaren bygger og signeres.

## 2026-10-07 17:05

### Feltnavnene samlet ét sted
Ni af de navne skærmen skriver til serveren stod som **løse tekster midt
i afsendelsen**, mens to stod centralt og fem i en tabel. Det er den
værste slags inkonsekvens, for navnet skal matche serverens felt
**præcis**.

Rammer det ved siden af, sker der **ikke noget synligt**: serveren tager
imod beskeden og smider den væk, og på dashboardet står feltet tomt for
evigt. Der kommer ingen fejl nogen steder.

Nu er alle elleve samlet i `zs_config.h`, og energiens fem bliver i deres
egen tabel fordi de er nummererede og hører til den regning. Skal man
svare på "hvad skriver skærmen egentlig", er det nu én liste i stedet for
en funktion man skal læse igennem.

**Og en prøve der fanger de to fejl man faktisk laver.** En **dublet**,
altså to felter med samme navn, hvor det ene ville overskrive det andet så
et tal hoppede mellem to værdier uden grund. Og et **ulovligt tegn**, for
et mellemrum eller en bindestreg bliver taget imod og tiet ihjel. Begge
prøvet af ved at indsætte fejlen: pakken falder.

### Og det der IKKE skal centraliseres
`/ 1000` står stadig flere steder, og det skal det blive ved med. De er
ikke det samme: watt til kilowatt, wattimer til kilowattimer, og
millisekunder til sekunder. De deler kun tallet, ikke betydningen, og at
samle dem under ét navn ville binde moduler sammen der ikke har noget med
hinanden at gøre.

Det der er samlet, er det der kan **være forkert**: et feltnavn der skal
matche en server, en grænse der stod to steder med hvert sit tal, og et
tal der skal ændres ét sted den dag noget skifter.

848 enhedstest og 70 ende til ende, bestået på Mac. Firmwaren bygger rent
uden advarsler og er uændret i størrelse. Version 0.22.1.

## 2026-10-07 16:20

### Fase 1: timens energi i kWh, fra inverterens egne tællere
Først en rettelse af mig selv. Jeg skrev forleden at batteriets energi
**ikke** findes som tæller i SunSpec, og at den måtte regnes ud af
effekten. **Det var forkert**, og Yassin havde ret i at det måtte være
der.

Den findes, bare ikke hvor jeg kiggede. Model 124 er batteriets
tilstand og har ingen energifelter. Men på en Fronius ligger lade- og
afladesiden som to ekstra MPPT-kanaler, og **hver kanal har sin egen
`DCWH`-tæller** i model 160, på offset 12 som `acc32` i wattimer.
Efterprøvet i SunSpecs egen `model_160.json`.

Så **alle fem tal er præcise tællere**, ikke beregninger:

| | Hvor det står |
|---|---|
| Produceret | model 103 `WH` eller 113 `WH` |
| Købt fra nettet | model 203 eller 213 `TotWhImp` |
| Solgt til nettet | samme `TotWhExp` |
| Ind i batteriet | model 160, ladekanalens `DCWH` |
| Ud af batteriet | model 160, afladekanalens `DCWH` |

En time regnes som tælleren nu minus tælleren ved timens start. Det tæller
også med hvad der skete mens skærmen var slukket, og det kan sammenlignes
med en elregning. Et gennemsnit af effekten kan ingen af delene.

### To fejl i prøveudstyret, fundet før jeg byggede noget
**Simulatoren lagde målerens skalafaktor på offset 40.** Specen siger
**52**, og 40 er `TotWhExpPhB`, altså en faseopdelt energitæller. Det
gjorde ingen skade så længe ingen læste tællerne, men det ville have fået
den første måling af timeforbrug til at se rigtig ud mod et forkert svar.

**Og alle tællerne stod på nul.** Simulatoren skrev `DCWH` som et fast
nul, og produktionstælleren rørte sig ikke. Så ville enhver time blive nul
og se helt rigtig ud. Anlægsmodellen tæller nu rigtigt op, hver vej for
sig, og tallene hænger sammen fysisk: ved middagstid 6,9 kWh produceret,
hvoraf 1,7 solgt og 4,9 i batteriet, altså 347 W husforbrug.

### Og en fejl i min egen kode, fanget ved at prøve
Jeg satte batteriets tællere i den gren der bruges når inverteren **ikke**
navngiver sine kanaler. Simulatoren navngiver dem, så koden tog den anden
vej og tællerne manglede. Nu sættes de begge steder.

### Timeberegningen, og det den nægter at gætte på
Regningen ligger som en ren funktion uden ur, net eller lager, så de
tilfælde der er svære at fremkalde på en skærm kan prøves på et øjeblik.
32 nye tjek:

- **Hen over midnat.** Fra klokken 23 til 0 er der gået én time, ikke 23
  baglæns. Uden det ville hver eneste nat blive sprunget over.
- **Skærmen har været væk.** Er der gået tre timer, springer vi over i
  stedet for at lægge dem sammen og kalde det én time. En søjle der dækker
  tre timer ser ud som en time med tre gange forbrug.
- **Uret bliver stillet tilbage.** Springes over, ikke et negativt tal.
- **En tæller der løber rundt.** En `acc32` når sin ende ved cirka 4,29
  milliarder wattimer, og en udskiftet inverter starter forfra. Vi kan
  ikke se forskel, så feltet springes over i stedet for at vise fire
  millioner kilowattimer på én time.
- **En inverter uden elmåler.** Produktionen sendes, og de felter vi ikke
  har sendes **ikke**. Et nul for "købt" ville se ud som et anlæg der
  aldrig køber strøm.

### Udgangspunktet overlever en genstart
Efter dit valg. Tællerstanden ved timens start gemmes i flashen i sit eget
navnerum, så en strømafbrydelse klokken 14.30 ikke koster timen 14 til 15.
Det koster én skrivning i timen, og flashen tåler størrelsesordner mere.

812 enhedstest og 70 ende til ende, bestået på Mac. Firmwaren bygger rent
uden advarsler. Version 0.22.0.

**Ikke færdigt endnu:** felterne skal oprettes på serveren, og hele vejen
skal ses virke mod den. Docker svarer ikke på maskinen lige nu, så det
venter.

## 2026-10-07 14:40

### Mutationstest: jeg ødelagde koden med vilje og så om testene opdagede det
En test der ikke fejler når koden bliver forkert, beviser ingenting. Så i
stedet for at lede efter fejl ændrede jeg koden 25 steder og så efter.

**Første svar var 13 af 25 fanget**, altså tolv huller. Men det tal var
forkert, og det var **min måling** der var det: harnesset kørte kun
enhedstestene, ikke ende til ende. Fem af de tolv sad i socket-koden, som
enhedstestene per definition ikke rører.

Kørt om mod hele pakken: **fire af de tolv blev fanget af ende til ende**.
Tilbage stod fem, og af dem var **to ækvivalente**, altså ændringer der
ikke betyder noget:

- `errno == EAGAIN || errno == EWOULDBLOCK`: de to er **samme tal** på
  begge platforme, målt til 35 på macOS og 11 på Linux. Muterer man den
  ene halvdel, dækker den anden. Begge bliver stående, for POSIX tillader
  at de er forskellige.
- Store bogstaver i mærke-opslaget: grænsen ved `Z` betyder først noget
  den dag der kommer et mærke med Z i navnet.

### Den rigtige fejl: "forbindelsen lukkede" når den stod åben
Det her fandt jeg ved at bygge en ny prøve til et af hullerne.

Simulatoren kunne afvise, men ikke **tie stille med forbindelsen åben**.
Og det er præcis hvad der sker på et anlæg når en inverter har travlt,
for eksempel fordi den samtidig serverer sin egen hjemmeside. Ny tilstand
`--tavs-efter N`, og en ende til ende-prøve der bruger den.

Da jeg så hvad værktøjet **faktisk** skrev, stod der:

    forbindelsen lukkede under maaler-soegning
    forbindelsen gik tabt under opstart

Men forbindelsen stod **åben hele tiden**. Beskeden er teknisk sand, for
vi lukker selv ved enhver fejl, og det er med vilje: en Modbus-forbindelse
hvor et svar udeblev er i ukendt tilstand. Men den er **misvisende om
årsagen**, og på et anlæg sender den dig ud at lede efter et kabel eller
en switch, mens fejlen er en inverter der tier.

Nu husker Modbus-laget **hvorfor** det lukkede, og beskeden siger det:

    gav op under maaler-soegning: Inverteren svarede ikke i tide

### Og fuzzeren var blind for off-by-one
Et af hullerne var at `off >= n` kunne ændres til `off > n` uden at nogen
test opdagede det. Det er en læsning ét forbi enden.

Fuzzeren burde have fanget det, og det gjorde den ikke. Grunden er værd at
kende: den gav et fast array på 200 registre og sagde "der er n gyldige".
Læste koden ét forbi `n`, var det stadig **inde i** arrayet, så
sanitizeren så ingenting. En off-by-one er den almindeligste fejl i en
parser, og fuzzeren kunne altså ikke finde den.

Nu er bufferen præcis så stor som den logiske længde. Efterprøvet: med
ændringen indsat afbryder den nu med adressesanitizer, hvor den før var
grøn.

### Og noget der IKKE kan prøves, skrevet ned som sådan
Tjekket i `skriv_ud` nås først når der er fundet en inverter, og det
kræver en rigtig scanning. Mutationstest viste at ændringen fra `&&` til
`||` slipper igennem hele pakken. Koden er rigtig, men beskyttet af et
tjek ingen prøve kommer forbi. Det eneste der gør det ufarligt er at
hverken firmwaren eller `zs-probe` nogensinde giver NULL. Det står nu i
testen, så ingen tror området er dækket.

780 enhedstest, 70 ende til ende med sanitizer, fuzzing af begge parsere.
Alt bestået på Mac og Linux. Version 0.21.0.

## 2026-10-07 13:05

### Dokumentationen efterprøvet mod koden, og fem ting var forkerte
Dokumentation er det eneste i repoet som intet prøver af. Koden har tests,
farverne har en vagt, layoutet har en. En tabel i en md-fil kan stå forkert
i et år uden at nogen opdager det.

**Registertabellen var rigtig**, og det er den vigtigste. Alle seksten
rækker i `docs/fronius-modbus.md` er holdt op mod både vores egne
konstanter **og** mod SunSpecs egen definition hentet fra `sunspec/models`.
Dokumentation, kode og specifikation er enige på alle seksten.

**Fem ting var derimod forkerte:**

1. **Opsætningsscriptet sagde at ESP-IDF v5.2 og nyere ikke kan oversætte
   leverandørens kode.** Det er efterprøvet og passer ikke: firmwaren
   bygger på **5.3.6**. Der skulle én ting til, og den var ikke i
   LCD-driveren men i en lydkodek vi ikke bruger. Påstanden ville have
   stoppet den næste før de prøvede, og ESP32-P4 kræver mindst v5.3.

2. **Testplanen sagde "218 tjek".** Der er **775**. Et forkert tal i en
   testplan er værre end ingen tal: man tror man ved hvor meget der er
   dækket. Nu står der at man skal køre den og se.

3. **To steder sagde at simulatoren skal ligge på port 502.** Det passede
   dengang søgningen kun kiggede der. Både skærmen og `zs-probe --scan`
   kan sættes til en anden port, og den kommando står der nu.

4. **Testplanen sagde at søgningen når 100 % på under ti sekunder.** Målt
   til omkring **16** på et /24.

5. **Påstanden om en fejl i Zbox var forældet.** Dokumentationen sagde at
   `app/modbus_controller.py` har `M160_N = 5`. Den har nu **6**, altså
   fejlen er rettet der. Efterprøvet på maskinen. Vores eget tal er
   samtidig kontrolleret mod SunSpecs model 160, hvor `N` ligger på
   offset 8 regnet med `ID` og `L`, altså 6 som vi regner.

Og mappeoversigten i README manglede `secure/`, hvor signeringsnøglen
ligger, og nævnte kun én af fire testmapper.

### Og en vagt, så det ikke kan skride igen
`tools/check-dokumentation.py` holder dokumentationen op mod koden: hver
række i registertabellen mod konstanterne, hver `ZS_`-konstant der nævnes,
og hver filhenvisning. Prøvet af på begge fejlmåder, med et forkert offset
og en konstant der ikke findes.

Den kan **ikke** tjekke om teksten er sand. Det må et menneske læse, og
det står i scriptet.

775 enhedstest, 68 ende til ende med sanitizer, fuzzing af begge parsere,
og syv vagter. Alt bestået på Mac og Linux. Version 0.20.1.

## 2026-10-07 12:15

### Mærkerne samlet ét sted, så vi kan udvide uden at rode i afkodningen
Spørgsmålet var om vi kan tage flere inverter-mærker ind uden at
Modbus-reglerne står hårdt i koden. Jeg målte det først.

**Fronius står 85 steder i koden**, men 57 af dem er kommentarer og de
fleste af resten er bare modulnavnet `zs_fronius.h`. Den **rigtige**
mærke-logik var tre ting:

1. Hvordan DC-kanalerne deles mellem solstrenge og batteri, valgt med et
   `strstr` efter "Fronius" **midt i afkodningen**.
2. Producentens egne fejlbits, `EvtVnd1` til `EvtVnd3`.
3. Producentens eget tilstandsregister.

Resten, altså registrenes adresser, driftstilstanden, `Evt1` og `DCEvt`,
er **SunSpec-standard** og gælder alle mærker. Det hører ikke hjemme i en
mærketabel, og det ligger det heller ikke i.

**En rigtig fejl, som dog er latent.** Fronius' fejltabeller blev brugt på
**enhver** inverter der forbandt. En Huawei med bit 1 sat ville få teksten
"Netfejl" og henvisningen "Fronius-kode 101". En forkert fejl, skrevet med
fuld sikkerhed, om en inverter vi ikke har papir på. Den er latent fordi
`zs_status.c` oversættes med i firmwaren men **aldrig kaldes af den**: kun
fejlsøgningsværktøjet bruger det lag i dag. Kanalopdelingen bruges
derimod af firmwaren og rammer hovedskærmen.

**Nu er det én tabel i `zs_maerker.c`.** Et mærke mere er en **række**, ikke
en ny gren i afkodningen. Og kender vi ikke mærket, gætter vi ikke:
producentens felter vises råt med den rå værdi og et telefonnummer, mens
alt det SunSpec dækker virker som før. Det er altid rigtigt, bare mindre
hjælpsomt.

Tabellen må **ikke** få et mærke vi ikke har dokumentationen til. Det står
i filen. 25 nye tjek holder reglerne, blandt andet at et mærke med en
fejltabel også **skal** have et kode-præfiks, så kunden kan se hvis manual
koden skal slås op i.

### Og værktøjet til at stå foran et rigtigt anlæg
`zs-probe` kunne **skrive** et register men ikke **læse** et råt. Og det er
netop det man skal bruge på et anlæg: holde producentens manual op mod
hvad der faktisk står.

    zs-probe 192.168.1.50 --laes 40072 8

Hvert register vises som hex, som tal med og uden fortegn, og naboparret
som 32-bit og som tekst. For man ved ikke på forhånd hvilken af dem der er
den rigtige, og det er netop det man er der for at finde ud af.

Prøvet mod simulatoren på registre hvor svaret er kendt: `SunS` på 40000,
model-ID 1 og længde 66 på 40002, og producentnavnet som tekst fra 40004.

Det er også vejen til at støtte et nyt mærke: læs blokken, hold den op mod
deres manual, og skriv **derefter** rækken i `zs_maerker.c`.

775 enhedstest, 68 ende til ende med sanitizer, og fuzzing af begge
parsere. Alt bestået på Mac og Linux. Version 0.20.0.

## 2026-10-07 11:20

### Fuzzing af parserne, og et hul i testopsætningen
To fund med værktøjer jeg ikke havde brugt endnu.

**Ende til ende kørte uden sanitizer.** Enhedstestene har altid haft
adressesanitizer, men ende til ende-testene kørte mod den almindelige
binær. Og det er netop dér de interessante fejl ville være: enhedstestene
fodrer opdigtede rammer, mens ende til ende kører rigtige sockets mod en
rigtig simulator, altså den vej netværksdata faktisk tager.

Prøvet: alle 68 består med sanitizer, nul fund. Men det skal **blive**
prøvet sådan, så der bygges nu en `zs-probe-san` som testene kører mod.
Den almindelige bliver stående, for den er værktøjet vi fejlsøger med.

**Og et byg der fejlede blev slugt.** I `tests/run-all.sh` stod der
`build.sh >/dev/null 2>&1 || true`. Både udskriften og fejlen blev smidt
væk, så holdt værktøjet op med at kunne bygge, sagde testkørslen
ingenting og kørte videre mod en **gammel** binær. Prøvet af ved at bryde
kilden med vilje: nu exitkode 1 og "Noget fejlede", hvor den før var grøn.

### Ødelagte rammer kastet ind i parserne
Modbus- og SunSpec-koden læser data direkte fra en enhed vi ikke styrer.
En inverter med en fejl i firmwaren, eller noget helt andet der svarer på
port 502, kan sende hvad som helst. Enhedstestene prøver de tilfælde vi
har tænkt på. Fuzzeren prøver dem vi ikke har.

Kørt med adressesanitizer over otte frø:

| | |
|---|---|
| Modbus-rammer | 900.000 |
| SunSpec-kædevandringer | 500.000 |
| Fund | **nul** |

Halvdelen af Modbus-rammerne er **muterede gyldige**, ikke rent
tilfældige, for rent tilfældige bliver afvist i headeren og når aldrig
ind. Og SunSpec-fuzzeren bygger en rigtig enhed med markør og modeller og
ødelægger så en håndfuld registre, herunder længdefelterne, som er dem der
kan få en vandring til at løbe løbsk.

**En grøn fuzzer der ikke når koden er værre end ingen fuzzer**, og det
lærte jeg på den hårde måde: min første udgave svarede med rent skrald og
nåede **aldrig** ind i kæden. Nul af 200.000 runder gav et kort, og den
var grøn. Den siger nu fra hvis den ikke kom ind, og i den færdige udgave
når 98,8 % af vandringerne ind, hvoraf 7,9 % rammer afkortnings-vejen.

Begge kører nu kort i testpakken og i CI, så de bliver ved at virke. En
længere kampagne køres i hånden med `./tests/fuzz/koer.sh 500000`.

750 enhedstest, 68 ende til ende med sanitizer, og fuzzing af begge
parsere. Alt bestået på Mac og Linux. Version 0.19.0.

## 2026-10-07 10:32

### Vi skal sende det vi har testet
Fundet ved at revidere mit eget arbejde fra fase to. Da ESP-IDF blev løftet
til 5.3 på grenen, rettede jeg `test.yml` og **glemte** `release.yml`.

Det ville betyde at vi tester med én oversætter og sender en anden ud til
kunderne. Forskellen er ikke kosmetisk: den er målt til næsten **tredive
kilobyte** i den færdige fil, og GCC 13 fanger fejl GCC 12 lader ligge. Den
firmware kunderne hentede, ville være bygget af en oversætter ingen test
havde set.

`tools/check-udgivelse.py` holder nu de to i trit. Den tjekker også at det
**ikke** er et flydende mærke som `latest`, så en udgivelse kan bygges igen
om et år og give det samme. Prøvet af på begge fejlmåder.

På main er begge stadig v5.1.7, og vagten er grøn. På grenen er begge
v5.3.6.

### Og hvordan et board sætter skærmens mål, efterprøvet
Headeren sagde at målene "kan sættes udefra", men ikke hvordan. Det er
præcis den slags den næste gætter sig til.

Nu står fremgangsmåden der, og **den er prøvet af**: jeg satte en ugyldig
bredde fra byggeriet med `idf_build_set_property(COMPILE_DEFINITIONS ...)`,
og vagten i headeren stoppede byggeriet med den danske forklaring. Så
mekanismen virker, den er ikke bare skrevet ned.

Headeren siger også **hvorfor** det skal komme fra byggeriet og ikke fra en
anden header: en header skulle hentes før `zs_layout.h` i hver eneste fil,
og den første der glemte det ville stille og roligt få 480 igen.

750 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Version 0.18.4.

## 2026-10-07 09:40

### Revision af mit eget arbejde: testen beviste ikke det den påstod
Fase et sagde at layoutet går op på **enhver** skærm. Men testene regnede
kun efter på 480 × 480, altså netop den ene skærm vi har. De beviste at
tallene er rigtige der, og det var aldrig påstanden.

**Nu kan skærmens mål sættes udefra**, med `#ifndef`. To ting på én gang:
et board kan give sine egne mål uden at nogen retter i layoutfilen, og
testene kan faktisk prøve en anden størrelse.

**Og layoutet er prøvet på 800 × 1280**, altså den rigtige skærm fra
reTerminal D1001. Seksten nye tjek, og alle reglerne holder: siden går op
i bredden og højden, teksten fylder kortet præcis, og de fysiske mål som
trykfeltet er uændrede.

**Men den samme test siger også at designet ikke skalerer**, og det skal
stå der så ingen tror at grønne tests betyder "klar til den nye skærm".
Kortet bliver mere end dobbelt så højt mens teksten i det er lige så stor,
så mellemrummet vokser fra 33 til 233 pixels. Det bliver **større end
tallet selv**. Et kort ville stå næsten tomt med tre små linjer spredt ud.
På en høj skærm er det rigtige **flere** kort, ikke større kort.

### Reglerne tjekkes nu når der bygges, ikke kun i to tests
To størrelser er to størrelser. Kommer der en tredje, for eksempel en
skærm med en **ulige** bredde, ville heltalsdivisionen tabe en pixel, og
ingen test ville fejle fordi ingen test kender den størrelse.

Seks `_Static_assert` i `zs_layout.h` tjekker nu reglerne for den størrelse
der **faktisk** bygges med, hver eneste gang. Prøvet af på seks skærme:

| Skærm | |
|---|---|
| 480 × 480, den vi har | bygger |
| 800 × 1280, reTerminal D1001 | bygger |
| 1024 × 600, en almindelig 7 tommer | bygger |
| 481 × 1280, ulige bredde | **stoppet** |
| 800 × 1281, ulige højde | **stoppet** |
| 100 × 100, for lille til et trykfelt | **stoppet** |

Beskeden viser både regnestykket med de rigtige tal og forklaringen på
dansk. Den er skrevet i ren ASCII, for oversætteren skriver æøå ud som rå
bytes og så kan den ikke læses.

### Efterprøvet: kæden er hel
Jeg påstod at skærmens mål kun står ét sted. Det er nu efterprøvet ved at
lede efter hvert eneste afledte tal i brugerfladens kode: 480, 456, 408,
222, 186, 158, 140, 53, 44 og 28. **Ingen af dem står noget sted.** De 44
faste pixeltal der er tilbage er små lokale afstande og ikonstørrelser,
som ikke skal skalere.

750 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent, og binæren er uændret i størrelse. Version 0.18.3.

## 2026-10-07 08:58

### Fladen gjort klar til en anden skærm, uden at en pixel flytter sig
Fase et af forberedelsen til reTerminal D1001. Alt herunder er gjort på
den skærm vi har, og den binære fil er **præcis lige så stor som før**, så
ingenting har ændret opførsel.

**Ikke alt skal skalere, og det er pointen.** En finger bliver ikke større
af at skærmen gør. Så målene er delt i to slags:

- **Fysiske mål der bliver stående:** mindste trykfelt på 44, listerader
  på 56, knapper på 52, valgfelter på 92. Dem skal man kunne ramme, og det
  afhænger af en finger og ikke af hvor mange pixels der er. Fire nye tjek
  kræver at de alle er mindst så store som trykfeltet.
- **Mål der følger skærmen:** kortets bredde og højde, og placeringen af
  teksten inde i kortet.

**Kortets indhold fulgte allerede en præcis regel**, den var bare skrevet
af. Tre tekstblokke med to **ens** mellemrum der fylder kortet helt ud:

    overskrift   0 ..  20
    tallet      53 .. 107
    undertekst 140 .. 158   præcis kortets indvendige højde

Mellemrummet er (158 − 20 − 54 − 18) / 2 = 33, og så lander tallet på 53
og underteksten på 140. Nøjagtig de tal der stod der i forvejen. Nu
regnes de ud, så de følger med hvis kortet skifter størrelse.

Højderne 20, 54 og 18 bliver derimod stående: det er skriftstørrelser,
valgt efter hvad man kan læse på to meters afstand.

**Og geometrien er flyttet derhen hvor den kan prøves af.** `zs_theme.h`
henter `lvgl.h`, så en test på en almindelig maskine kunne ikke nå tallene.
Det opdagede jeg ved at testen ikke kunne bygge. Alle målene ligger nu i
`zs_layout.h`, som ikke henter noget, og `zs_theme.h` er alias hele vejen.
Det er også den rigtige struktur til et skærmskifte: geometrien skal kunne
regnes igennem uden at starte et helt UI-bibliotek.

Elleve nye tjek oven i de sytten fra før. De vigtigste er ikke tallene men
reglerne: at de to mellemrum er **ens** så tallet står optisk i midten, og
at underteksten slutter **præcis** i bunden af kortet. Er der en pixel for
lidt, flyder teksten, og er der en for meget, løber den ud over kanten.

734 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler, og binæren er uændret i størrelse.
Version 0.18.2.

## 2026-10-07 08:20

### Kan vi flytte til reTerminal D1001 med ESP32-P4? Undersøgt og målt
Ikke et skøn. Alt herunder er hentet fra Espressifs og Seeeds egne kilder,
eller målt på vores egen kode.

**Hardwaren**, fra Seeeds eget datablad (SKU 100058144): ESP32-P4, 32-bit
RISC-V med to kerner og **32 MB PSRAM**. En **ESP32-C6 ved siden af** der
klarer wifi 6, BLE og Zigbee, forbundet til P4'eren over **SDIO**. Skærmen
er 8 tommer, **800 × 1280 over MIPI-DSI**, og der er kamera, mikrofoner,
mPCIe til 4G og et batteri på 2500 mAh.

**ESP32-P4 har ingen radio.** Det er efterset i chippens egne
egenskaber: hverken `SOC_WIFI_SUPPORTED` eller `SOC_BT_SUPPORTED` findes.
Til gengæld har den kablet ethernet, MIPI-DSI, og **SD-kort direkte på
hovedprocessoren**, hvilket vores nuværende board ikke har.

**Vores ESP-IDF er for gammel.** v5.1.7 kender slet ikke esp32p4.
Espressifs egen tabel siger at P4 er "preview" i v5.2 og **"supported" fra
v5.3**. Så v5.3 er gulvet. Det passer med at wifi-laget til P4 har Kconfig
fra netop v5.3 og opefter.

**Et falsk svar undervejs, værd at skrive ned.** Mit første tjek sagde at
ingen IDF-version havde esp32p4, hvilket var forkert: vores klon er
overfladisk, så et opslag i et undertræ fejlede i stilhed. Jeg fangede det
ved at prøve metoden af på noget jeg vidste fandtes, og spurgte derefter
GitHubs API i stedet.

### Hvor meget af vores kode flytter med?
Målt på hele firmwaren, kommentarer trukket fra:

| Hvad koden hænger på | Filer | Linjer | Andel |
|---|---|---|---|
| Ren C, flytter sig gratis | 35 | 3650 | **36 %** |
| ESP-IDF i øvrigt | 9 | 2375 | 24 % |
| `esp_wifi` | 1 | 330 | 3 % |
| LVGL, brugerfladen | 17 | 2775 | 28 % |
| Board og leverandør | 5 | 885 | **9 %** |

Altså: **60 % flytter uden at blive rørt.** Ni procent er board-lim der
skal skrives om. Og de 28 procent brugerflade virker, men layoutet er
bygget til en kvadratisk 480 × 480 skærm og den nye er høj og smal.

**Wifi-laget flytter uændret.** Espressifs `esp_wifi_remote` giver samme
API, og det er efterset funktion for funktion: jeg trak alle `esp_wifi`-kald
ud af vores kode, alle **tolv**, og slog hver enkelt op i den genererede
API. Alle tolv er dækket, inklusive scanningen som opsætningen afhænger af.

### To mål var skrevet af, og det ville have gjort ondt
Skærmens størrelse står ét sted, og næsten alt andet er regnet ud af den.
Men **to** tal brød kæden: kortets bredde stod som 222 og højden som 186,
begge med regnestykket i en kommentar ved siden af.

Det er netop den slags der gør ondt den dag skærmen skifter: alt det andet
retter sig selv, og så står de to tilbage og giver et layout der er **lidt**
forkert i stedet for tydeligt forkert.

Begge er nu udregnede, og tallene er de samme, så ingen pixel flytter sig.

**Og sytten nye tjek holder det fast.** Ikke kun tallene, men at siden
**går op**: kant plus kort plus mellemrum plus kort plus kant skal give
præcis skærmens bredde, og linjen foroven plus siden plus prikkerne skal
give højden. Holder de regler, er layoutet rigtigt på enhver skærm. De
tjekker også at delingen går op uden en pixel til overs, for en ny skærm
kan ramme et ulige tal.

723 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.18.1.

## 2026-10-07 07:05

### Gennemgang af OpenRemote, og målinger der ændrer arkitekturen
Serveren blev tændt og gennemgået: alle fire tjenester sunde, dashboardet
svarer, og hele vejen fra indmeldelse til skrivning virker.

**Jeg væltede databasen undervejs.** Klokken 01:59:56 brød en
Postgres-proces sammen under min egen komprimeringsmåling, og hele
databasen gik i genopretning i cirka ti sekunder. Intet er tabt, det er
talt efter: 58.599 datapunkter, 69 enheder, 2 indmeldelsesopsætninger,
præcis som før. Men det afdækker en rigtig risiko: Postgres genstarter
**alle** forbindelser hvis bare én proces dør, så en enkelt tung
forespørgsel kan tage hele databasen ned.

**Målt, ikke gættet:** 304 bytes per datapunkt råt, **41,7** komprimeret,
altså faktor 7,3. Det giver for tusind skærme **66 GB per døgn** og
**183 GB** i de to uger data gemmes.

**Komprimeringen er sat rigtigt op men har aldrig kørt.** Jobbet kører
fejlfrit, fem gange, nul fejl. Men intervallet blev sat til ét døgn
**efter** at den nuværende chunk blev lavet, så den spænder stadig syv
dage og lukker først 8. oktober. En chunk kan ikke komprimeres før den er
lukket.

**Sikkerhedskopien kan gendannes.** Gendannelsesprøven kørt: alle fire tal
passer præcis mellem det der blev taget og det der kom tilbage. Men der er
kun to kopier, begge manuelle, og hverken cron eller launchd kører den.
Venter til Coolify efter aftale.

I øvrigt fundet: en NullPointerException i OpenRemote udløst af at en
MQTT-forbindelse lukker midt i en indmeldelse, hvor en besked blev tabt.
Gentagne advarsler fra hawkBit, en firmware-tjeneste vi ikke bruger. Og 69
enheder i dashboardet hvoraf én er rigtig hardware.

Sikkerheden er velsat: beskyttelse mod kodeordsgætning slået til, fem
forsøg og femten minutters pause, selvregistrering slået fra, og
attributterne rigtigt opsat med historik på kun de fem live-tal og
`targetVersion` læsbar men ikke skrivbar for enheden. Admin-kodeordet er 19
tegn, altså stærkt, men mangler et stort bogstav og et ciffer og lever
derfor ikke op til serverens egen politik.

### Tre fund der ændrer planen for historikken
**Der er intet SD-kort på denne hardware.** Ikke "ikke tilsluttet endnu":
boardets egen definition har alle SD-ben som `GPIO_NUM_NC` og
`FUNC_SDMMC_EN = 0`. På de større modeller sidder kortpladsen på
RP2040-chippen, ikke på ESP32'en.

**Men det er ikke nødvendigt.** En times målinger fylder **35 KB**.
Skærmen har 8 MB PSRAM, så det er under en halv promille af den
hukommelse vi har i forvejen.

**Serveren tager ikke imod vores tidsstempler.** Prøvet på fire måder:
råt tal, objekt med timestamp, `attributevalue` uden write, og hele
attributten. Kun det rå tal landede, stemplet med serverens egen tid. De
tre andre forsvandt i stilhed.

Så "send timens data" kan ikke være 1800 tilbagedaterede punkter. Det skal
være timens **opsummering**, og det bliver 183 GB til **142 MB** for tusind
skærme, altså en faktor 1286. Live-tallene og kommandoer bliver ved at gå
hvert andet sekund, så dashboardet er levende og styring er øjeblikkelig.

### Energiregistrene, verificeret mod SunSpecs egen definition
Første skridt mod timeværdier i kWh: registrene, hentet fra
`sunspec/models` og lagt ind med kilden skrevet ved.

Specen tæller `ID` og `L` med som de to første felter, så deres offsets er
præcis **to større** end vores, der regnes fra datablokkens start. Det er
efterset på et felt vi har brugt længe: specen har model 203 `W` på 18 og
vi har 16. Så talmåden stemmer.

Nyt, i vores talmåde: model 203 `TotWhExp` på 36, `TotWhImp` på 44,
skalafaktor på 52. Model 213 har dem på 58 og 66 som flydende tal uden
skalafaktor.

**Og et nej der er værd at kende:** model 124, batteriet, har slet ingen
energitællere. Det er efterset i specen, der er ikke et eneste felt med Wh
i hele modellen. Batteriets ladet og afladet må derfor regnes ud af
effekten over tiden, og med en måling hvert andet sekund ligger fejlen
langt under en procent.

Sytten nye tjek låser offsets fast mod specens egne tal, for en tastefejl i
et offset giver en kunde forkerte kWh, og det ser ud som et rigtigt tal.
De tjekker også blokkens form: otte registre fra Exp til Imp, og otte
videre til skalafaktoren.

707 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.18.0.

## 2026-10-07 06:02

### En blindgyde: kunden kunne ikke komme hjem fra netværkssiden
Fundet ved at kortlægge hele vejen fra kunden tænder til skærmen er
forbundet, side for side, og lede efter en side uden vej videre.

Trykker en kunde på **Netværk** i indstillingerne, for eksempel fordi de
har skiftet routerens kodeord, og så fortryder: tilbage-knappen førte til
**velkomstsiden**. Og velkomstsiden har kun én knap, "Kom i gang", som
fører tilbage til netværkslisten.

Der var altså **ingen vej hjem**. Kunden skulle gå hele opsætningen
igennem igen, inklusive en ny søgning efter inverteren, som er målt til 16
sekunder på et /24 og 3 minutter 50 på et /20. Og det for en skærm hvor
intet var gået i stykker.

**Og det kunne ikke rettes i brugerfladen alene.** Appen står i `ST_SETUP`
mens opsætningen er åben, og der står `continue` i løkken, så den hverken
forbinder eller aflæser. Viste brugerfladen bare hovedskærmen igen, ville
den stå død med gamle tal for evigt.

Derfor går fortryd nu gennem appen, som ejer tilstanden og er den eneste
der ved om der er en opsætning at gå tilbage til. Er der et netværk og en
inverter gemt, slippes `ST_SETUP` og kunden kommer hjem. Er der ikke, er
velkomstsiden det rigtige sted.

Opsætningen blev i øvrigt aldrig slettet undervejs, det er tjekket:
`SETUP_RESTART` rører ikke kundens gemte valg.

### Og en vagt, så en side ikke kan komme ind uden vej tilbage
`tools/check-flow.py` kræver at hver side har en tilbage-knap.
Undtagelser skal skrives i scriptet **med en grund**, og der er præcis én:
velkomstsiden, hvor der ikke er noget før.

Prøvet af ved at fjerne tilbage-knappen fra netværkslisten og se den falde
med exitkode 1. En skærm på en væg har kun én slags input, en finger. Er
der ingen vej tilbage, er der ingen tast, ingen mus og ingen menu, og den
eneste udvej er at tage strømmen.

### Teksten pegede på en knap der ikke fandtes
Stod der ingen netværk, sagde hintet "Prøv igen" mens knappen nedenfor
hedder "Søg igen". To ord for det samme. Nu peger teksten på den knap der
faktisk er der.

### Lysstyrken flyttet først i runden
Mens jeg var i løkken: lysstyrkens plads blev læst **efter** kølæsningen,
og den gren slutter med `continue`. Så længe der stod kommandoer i køen,
blev lysstyrken altså sprunget over, netop når skærmen havde travlt. Nu
står den allerførst.

Og gemningens kommentar var kommet til at stå over den forkerte kode efter
min egen ændring i går. Rettet.

### Hele flowet gennemgået, og det der var i orden
- Alle elleve sider er nåelige, og ni af ti har en tilbage-knap. Den tiende
  er velkomstsiden.
- **Intet skærmskift ligger i hovedløkken.** Hvert skift er ét ved
  opstart, ét per tryk eller ét per søgning. Så appen kan ikke rive
  kunden tilbage til en side de lige forlod, og der findes ingen ring.
- Prisområdets tilbage-knap har et mål der sættes af den der åbner den,
  så fra indstillingerne fører den til indstillingerne og fra opsætningen
  til inverterlisten. Ikke en blindgyde.
- Netværkslisten har en "Søg igen"-knap, og inverterlisten har også en, så
  en tom liste er ikke en blindgyde.
- Opstarten lander rigtigt: er skærmen sat op, går den direkte til
  hovedskærmen i `ST_CONNECTING`, ellers til velkomstsiden i `ST_SETUP`.

690 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.17.1.

## 2026-10-07 05:12

### Et netværksnavn på præcis 32 tegn kunne skærmen aldrig forbinde til
Den værste slags fejl: navnet stod rigtigt på skærmen hele vejen, og det
blev først klippet i det allersidste hop.

`snprintf` skriver højst størrelsen **minus ét**, fordi den altid sætter
en afslutning. ESP-IDF's felter er anderledes: `wifi_sta_config_t` har
`ssid[32]` og `password[64]`, og de er præcis så store som værdien må
være. De skal **ikke** have en afslutning når værdien fylder dem helt.

Så vi skrev højst 31 tegn af navnet og 63 af kodeordet. Begge de klippede
længder er lovlige: 32 er 802.11's grænse for et netværksnavn, og 64 er et
råt PSK skrevet som hex.

Resultatet for en kunde med et langt netværksnavn: skærmen viser navnet på
listen, man taster kodeordet, og den forbinder aldrig. Uden en forklaring
nogen kunne gennemskue.

Nu kopieres der med `memcpy` og en længde, ind i et felt der er nulstillet
først, så en kortere værdi afsluttes af sig selv.

**Og knappen er bundet til standarden.** To `_Static_assert` kræver at
vores egne felter er præcis ét tegn større end ESP-IDF's. Ændrer de deres
felt, fejler **byggeriet** i stedet for at klippe i stilhed hos en kunde.

Jeg prøvede vagten af ved at sætte `ZS_SSID_MAX` til 32 og se byggeriet
falde. En vagt man ikke har set fejle, ved man ikke virker. Beskeden er
ren ASCII, for oversætteren skriver æøå ud som rå bytes og så kan den ikke
læses.

Hele kæden fra søgning til lager er gennemgået: alle mellemled bruger de
rigtige størrelser, så fejlen sad kun i det sidste hop. Og der findes
ingen andre steder i koden hvor vi skriver i et af ESP-IDF's faste felter.

### Lysstyrkeskyderen kunne ende et andet sted end den stod
En skyder er en **kontinuerlig** kontrol. LVGL sender en hændelse for hvert
trin under et træk, så et træk fra 5 til 100 giver op mod halvfems værdier
på under et sekund.

De gik gennem kommandokøen, som holder otte, med `xQueueSend` uden
ventetid. Er køen fuld, smides resten væk, og svaret blev ignoreret af
alle tien kaldere. Var den **sidste** værdi blandt dem der blev smidt væk,
stod skyderen på ét og skærmen lyste som noget andet. Og det gemte også.

Rettet ved roden: skyderen skriver nu **én plads** hvor nyeste værdi
vinder, uden om køen. Hovedopgaven ser efter den i hver runde og retter
skærmen hvis den er anderledes end den der står nu.

Pladsen nulstilles med vilje **aldrig** efter læsning. I stedet
sammenlignes den med den lysstyrke der står nu, så der ikke findes et
øjeblik mellem læsning og nulstilling hvor en ny værdi kan gå tabt. Og at
skrive den samme værdi to gange koster ingenting.

Kommandoen `ZS_CMD_SET_BRIGHTNESS` er fjernet, for ingen sender den
længere, og en død vej er en vej nogen kommer til at bruge.

### En tabt kommando kan nu ses i loggen
Køen er fuld hvis hovedopgaven er optaget af noget der blokerer, og så er
brugerens tryk væk uden at nogen ved det. Symptomet er "jeg trykkede og
der skete ingenting", og det kan ikke fejlsøges uden en linje i loggen.
Nu står den der.

### Tjekket og i orden
- Statisk analyse på **hele** firmwaren, også brugerfladen, som ikke var
  kørt før. Intet nyt i vores kode.
- Alle atten steder hvor en streng klippes på bytes: ingen af dem kan få
  æøå, fordi SunSpec-strenge renses til printbar ASCII og resten er
  maskintekst. Så ingen kan blive klippet midt i et tegn.
- `on_inv_pick` afgrænser sit indeks mod listens længde, så et tryk på en
  liste der er blevet kortere imens kan ikke læse ved siden af.

690 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.17.0.

## 2026-10-07 02:41

### README's sikkerhedsafsnit var forkert på to punkter
Fundet fordi jeg skulle skrive nøglens opbevaring ned og læste afsnittet
ved siden af. Det er den tekst en kunde eller en partner ville læse, så
det er ikke en lille fejl.

Der stod: *"Ingen af Modbus' fem skrive-funktionskoder er implementeret,
og de må aldrig blive det."* Det passer ikke længere. Funktionskode 16
findes i det fælles Modbus-lag siden fase 1, og den skal blive der, for
det var beslutningen.

Og der stod: *"Skærmen lytter ikke på nogen port, sender ingenting ud af
huset, og har ingen konto eller nøgle."* Den sidste halvdel er direkte
forkert. Skærmen sender målinger hvert andet sekund, henter priser og
firmware, og har et klientcertifikat og en privat nøgle i flash.

Nu står der hvad der faktisk sker, og **hver påstand er efterprøvet i
koden** inden den blev skrevet:

- Firmwaren kalder ikke skrive-vejen nogen steder. Det gør kun `zs-probe`
  på en bærbar. Så "skærmen ændrer ikke noget på inverteren" holder
  stadig, men af en anden grund end den der stod.
- Der findes hverken `bind`, `listen` eller `accept` i koden, så den
  lytter ikke.
- Alle fem værtsnavne, porte og takter er læst ud af koden, ikke af
  hukommelsen. To af mine egne tal var forkerte i første udgave:
  opdateringer er hvert 30. minut og ikke hver time, og priser hentes en
  gang i døgnet og ikke "nogle gange".

Tabellen er nu præcis nok til at bruge direkte som en firewall-regel, for
det er det spørgsmål en kunde med et stramt net stiller.

### Og nøglens opbevaring står nu skrevet
To steder der ikke kan ryge sammen: en delt hælving i adgangskodeboksen,
og en krypteret kopi på en USB-nøgle på en anden fysisk adresse. En
bærbar og en hemmelighed i den samme GitHub-konto er ét sted, ikke to.

690 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Version 0.16.3.

## 2026-10-07 02:14

### Signeringsnøglen lå læsbar for alle på maskinen
Filen `secure/zscreen-signing.pem` stod med rettigheder 644, altså
læsbar for enhver bruger på Mac'en, og mappen var 755. Nu er filen 600 og
mappen 700.

Nøglen har aldrig været i git, det er tjekket hele historikken igennem.
Men en nøgle der kan læses af enhver proces på maskinen er ikke
beskyttet.

### Nøglen kan ikke skiftes over luften, og det står der nu
Jeg gik efter om opdateringerne faktisk er beskyttede, og det er de. Men
undervejs fandt jeg en egenskab der betyder meget for en flåde, og som
ikke stod nogen steder.

Vi kører med `CONFIG_SECURE_SIGNED_ON_UPDATE` uden sikker opstart. ESP-IDF
siger selv i `secure_boot.c` hvad det betyder:

> "We rely on the keys used to sign this app to verify the next app on OTA"

Altså: en skærm efterprøver den **næste** firmware med den offentlige
nøgle der sidder i den firmware den kører **lige nu**. Og når sikker
opstart er fra, bruges kun den første underskriftsblok.

Konsekvensen:

- En skærm med firmware signeret med nøgle A tager **aldrig** imod
  firmware signeret med nøgle B.
- Skiftes nøglen, skal hver enkelt skærm flashes med ledning.
- **Mistes nøglen, kan ingen skærm nogensinde opdateres igen.** Der er
  ingen vej rundt, heller ikke med adgang til serveren.

Til gengæld er skærmen ikke låst: sikker opstart er slået fra med vilje, så
en skærm altid kan flashes med ledning. Det er netop det der gør at et
tabt nøglepar ikke er en kasseret skærm, bare en skærm der skal have
besøg.

Det hele står nu i `release.yml`, hvor den der rører nøglen vil læse det.

### Bevist at en pillet firmware bliver afvist
Ikke bare tjekket at flaget er sat. Jeg verificerede den byggede fil, vendte
**én** byte midt i programmet, og verificerede igen:

    Signature block image digest does not match the actual image digest

Så selv hvis dashboardet blev overtaget, kan ingen lægge fremmed firmware
ind uden nøglen. Porten findes allerede i udgivelsen, og nøglen slettes
bagefter.

### De angreb målversionen kunne bære
Målversionen kommer fra serveren og ender i en URL, så `zs_version_tag_ok`
**er** grænsen. Den var streng i forvejen, men tre angrebsveje manglede i
testene. Fjorten nye tjek, og vognretur-linjeskift står først, for det er
den der kunne lave to HTTP-headere ud af én linje.

Alle fjorten blev afvist i forvejen. Nu kan de ikke holde op med det uden
at en test falder.

### Og en der lignede MQTT-fejlen, men ikke var det
GitHub sender firmwaren videre til et andet værtsnavn, og jeg målte den
viderestilling på en rigtig udgivelse: **ét** hop, og adressen er
**915 tegn**, fordi filen ligger bag en tidsbegrænset underskrift.

`esp_http_client` har en standardbuffer på **512 bytes**, altså under
halvdelen. Det ser ud præcis som fejlen i flådestyringen, hvor en besked
større end bufferen kom i stykker.

Men det er det ikke: klienten lægger header-værdien til i heapen stykke
for stykke, så en lang adresse klarer sig uanset bufferens størrelse.
Efterset i deres egen kilde. Tallene og begrundelsen står nu i
`zs_ota.c`, så ingen "retter" det på et gæt senere.

### Tjekket og i orden
- `zs_fleet_stop` kaldes aldrig, så der er ingen samtidighed mellem den og
  flåde-opgaven at bekymre sig om.
- `esp_mqtt_client_publish` og `subscribe` tjekker begge selv for en
  NULL-klient og svarer pænt. Efterset i deres kilde.
- Flåden startes kun ved opstart. Kommer certifikatet på bagefter, kommer
  den først med efter en genstart. Det er fint i dag, hvor certifikatet
  lægges på ved produktion.

690 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.16.2.

## 2026-10-07 01:28

### Serverens eget svar ligger nu i testene
Prøven for samleren brugte et opdigtet svar i den rigtige størrelse. Nu
ligger det **faktiske** svar fra vores egen server med som prøveklud:
2604 bytes, type success, en enhed med tretten attributter.

Testen deler det præcis som esp-mqtt gør, samler det igen, og kræver at
resultatet er **byte for byte** det samme som serverens. Den tjekker også
det der var hele pointen: enheds-id'et står i den samlede besked, og det
stod **ikke** i det første stykke alene. Altså var det umuligt for koden at
få det før.

Så kan ingen sige at fejlen kun fandtes i en test.

Prøvekluden er fra en kasseret test-enhed og indeholder hverken
certifikater, nøgler eller kodeord. Det er tjekket.

677 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Version 0.16.1.

## 2026-10-07 01:12

### Skærmen kunne aldrig melde sig ind på flåden. Målt, ikke gættet
Den dyreste fejl indtil nu, og den sad i den ende ingen test rørte.

esp-mqtt læser ind i en buffer på **1024 bytes**. Er en besked større,
kommer den i **flere stykker**, og kun det **første** har et emne på sig.
De næste har `topic_len` nul. Det står i deres egen kilde, omkring
`post_data_event` i `mqtt_client.c`.

Vi samlede dem ikke. Hvert stykke blev læst som om det var en hel
besked.

**Målt mod vores egen OpenRemote:** svaret på en indmeldelse er
**2604 bytes**, fordi det indeholder hele enheden med alle tretten
attributter. Det kommer altså i tre stykker. Ingen af dem er gyldig JSON
alene, så alle tre blev forkastet med "svaret kunne ikke læses". Skærmen
fik derfor aldrig sit enheds-id, og uden det må den ikke skrive en eneste
måling.

**Og det passer med hvad serveren har set.** Skærmens enhed findes på
serveren, men **nul af dens tretten attributter** har nogensinde fået en
værdi. I serverens egen log står svaret som 2762 bytes på tråden, sendt,
læst og kvitteret, igen og igen.

Grunden til at det ikke blev fanget: indmeldelsen er altid blevet prøvet
med Python-scripter, og paho har ingen sådan grænse. Firmwarens egen vej
gennem svaret var aldrig kørt af nogen.

**Rettet ved roden.** Stykkerne samles nu, emnet fra det første huskes, og
beskeden læses først når den er hel. Samleren er en ren funktion uden
noget fra ESP-IDF, så den kan prøves af på en almindelig maskine, og det
er den: 25 nye tjek med de rigtige tal, altså 2604 bytes i tre stykker
hvor kun det første har et emne.

Pladsen er otte kilobyte i PSRAM. Fire ville være nok i dag, men svaret
vokser med antallet af attributter, og vi vil ikke rette det igen når der
kommer et felt mere.

### Og en besked blev kendt på hvad den ikke var
Samme sted: beskederne blev sorteret efter reglen "indeholder emnet ordet
targetVersion et eller andet sted? ellers er det et indmeldelsessvar".

Det er den forkerte vej rundt. Kommer der en tredje slags besked en dag,
bliver den læst som et svar. Og ordet blev søgt **hvor som helst** i
emnet, så et emne der tilfældigvis indeholdt ordet som del af et andet ord
ville tælle.

Nu kendes en besked på et helt **led** mellem skråstregerne, og den skal
matche positivt. Kender vi ikke emnet, siger vi det i loggen i stedet for
at gætte.

Halen kunne ikke bruges i stedet, og det er værd at skrive ned: lytte-emnet
**slutter** på enhedens id, så `targetVersion` står i midten.

### En fejl jeg selv lavede, og hvad der fangede den
Første udgave af led-sammenligningen kaldte `memcmp` med ordets længde
uden at se på hvor meget der var **tilbage** af emnet. Den læste altså ud
over strengens ende.

Enhedstesten kørte med adressesanitizer og afbrød med det samme. På en
skærm ville den have været en fejl der kommer og går efter hvad der
tilfældigvis ligger i hukommelsen bagefter, altså den slags der ikke kan
fejlsøges. Grænsen står nu eksplicit i koden med en note om hvorfor.

664 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.16.0.

## 2026-10-07 00:18

### Vagthunden lovede at skærmen kom sig selv, men gjorde det ikke
Kommentaren i opsætningen stod der i forvejen: "En skærm der hænger på en
væg skal komme sig selv hvis en opgave går i stå. Ingen er i nærheden til
at trykke reset."

Den gjorde den ikke. `CONFIG_ESP_TASK_WDT_PANIC` var ikke slået til, og
uden den **advarer** vagthunden bare i loggen og lader opgaven hænge. Så
en skærm der gik i stå, stod frosset indtil nogen tog strømmen. Intentionen
var skrevet ned, men ikke slået til.

Nu er den slået til, efter dit valg. Går en tomgangsopgave i stå i 30
sekunder, skriver panikhåndteringen et bagspor ud over serieporten og
genstarter.

**Og den linje der skulle skrives med.** Panik betyder ikke automatisk
genstart: det afhænger af `CONFIG_ESP_SYSTEM_PANIC`. Stod den på
`PRINT_HALT`, ville vagthunden gøre det **modsatte** af det vi vil, altså
standse skærmen helt i stedet for bare at advare. Den står på
`PRINT_REBOOT` i dag, men kun som ESP-IDF's standard, og nu afhænger vores
valg af den. Derfor står den nu sort på hvidt i `sdkconfig.defaults`, så
en opdatering af IDF ikke kan flytte den under os.

**Hvorfor det er trygt.** To ting var på plads i forvejen:
tilbagerulning er slået til, og firmwaren melder sig først rask efter 120
sekunder. Hænger en **ny** udgave ved opstart, genstarter skærmen, og
bootloaderen ser at den nye aldrig meldte sig rask og ruller tilbage til
den der virkede. En dårlig opdatering kan altså ikke lægge flåden ned.

Og hvorfor den ikke vælter af sig selv: vagthunden holder kun øje med
tomgangsopgaverne, og alle vores ventetider er `select()` eller blokerende
sokler med timeout, så tomgang kommer til. Også under en søgning på fire
minutter.

**Den risiko der står tilbage,** så den er sagt højt: hænger en udgave der
allerede har meldt sig rask, altså efter de 120 sekunder, genstarter
skærmen i ring i stedet for at stå frosset. Begge tilstande kræver at
nogen gør noget, men en genstartsløkke er hurtigere at se end en frossen
skærm.

629 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.15.3.

## 2026-10-06 19:34

### LVGL blev rørt uden låsen, og det gik kun godt ved et sammentræf
Den vigtigste i denne runde.

LVGL er ikke bygget til at blive kaldt fra flere opgaver på én gang.
Skærmopgaven tegner i sit eget tempo, og rører en anden opgave de samme
objekter imens, kan listerne LVGL går igennem skifte under den. Det viser
sig som en skærm der fryser eller et billede der går i stykker, en gang om
ugen uden mønster. Den slags kan ikke fejlsøges bagefter.

Reglen i huset er at alt i `firmware/main/ui` selv tager låsen. Jeg
efterprøvede den mekanisk i stedet for at stole på den, og **én** faldt
igennem: `zs_theme_set_mode` rører LVGL tolv steder, blandt andet den
aktive skærm, og tog ikke låsen. Den kaldes fra `zs_app.c`.

Det har aldrig gået galt, men ikke fordi koden er rigtig. Den eneste
kalder udefra er `zs_app_load_settings()`, som i `main.c` tilfældigvis
kører **før** brugerfladen findes, så funktionen når sin tidlige retur og
rører ingenting. Det er et sammentræf man kan ødelægge ved at flytte en
linje.

Nu tages låsen om LVGL-delen. Den ligger med vilje **efter** den tidlige
retur: ved opstart findes mutexen slet ikke endnu, og en lås der er NULL
ville vælte hver gang. Mutexen er rekursiv, og `lv_port_sem_take` gør
desuden ingenting når den kaldes fra skærmopgaven selv, så det er trygt
også når `zs_ui_set_theme` allerede holder den.

### Og en vagt, så reglen ikke kun står som en kommentar
En kommentar holder ingen i hånden. `tools/check-lvgl-laas.py` tjekker nu
hver funktion i `ui/`: kaldes den fra app, net eller main, og rører den
LVGL, skal den tage låsen. Den kører lokalt og i CI.

Jeg prøvede den mod koden **før** rettelsen, og den fangede fejlen med
exitkode 1. En vagt man ikke har set fejle, ved man ikke virker.

Scriptet skriver selv sine grænser: det læser teksten, ikke programmet, så
et kald gennem en pegepind ser det ikke. Det er en bund, ikke et bevis.

### To tal i lageret stod skrevet i hånden
`zs_nvs.c` satte port 502 og unit 1 direkte. Skal en Fronius på 1502
rettes, skal det kunne gøres ét sted. Standard-unit har fået sin egen knap
ved siden af porten, hvor Modbus-begreberne hører til.

### Målt, ikke gættet
Fire ting efterprøvet som faktisk var i orden, så de ikke skal mistænkes
igen:

- **Filbeskrivelser i søgningen.** `gcc -fanalyzer` melder et læk.
  Målt over tre søgninger, både hvor intet svarer og hvor simulatoren
  svarer: samme antal åbne før og efter. Advarslen er falsk.
- **Delte flag mellem opgaverne.** Alle er `volatile`, og de sættes efter
  et funktionskald, som oversætteren ikke kan flytte en skrivning hen
  over. `.bss` ligger i intern SRAM, som er sammenhængende mellem
  kernerne.
- **Vagthunden.** Holder kun øje med tomgangsopgaverne, 30 sekunder, og
  vores opgaver blokerer i `select()` så tomgang kommer til. En søgning på
  fire minutter kan ikke vælte den.
- **Listen af invertere.** Kopieres med `memcpy` ind i skærmens egen
  buffer, og grænsen tjekkes før kopien. Skærmen får ingen pegepind ind i
  hovedopgavens hukommelse.

Og tre gennemgange uden fund: SunSpec' "ikke understøttet"-værdier er
dækket for alle de typer vi læser, prisparseren er afgrænset hele vejen og
har plads til 25 timer så den nat sommertiden slutter holder, og hverken
signeringsnøgle, logoer eller private nøgler har nogensinde været i git.

629 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.15.2.

## 2026-10-06 18:52

### Samme hul som før, men i søgningen fra indstillingerne
Da jeg havde lukket hullet hvor en ændret indstilling kunne ligge ugemt
gennem en søgning, gik jeg efter om der var **flere** steder i samme
opgave der blokerer længe. Der var ét.

`do_inverter_scan`, altså søgningen kunden selv starter i
indstillingerne, kører i hovedopgaven og blokerer lige så længe som den
automatiske. Køen af skærmkommandoer læses i linje 1014, og gemmepunktet
ligger i linje 1002, altså **før**. Trækker kunden i lysstyrken og
trykker søg inden for et halvt sekund, lå ændringen ugemt hele søgningen
igennem.

Begge søgninger gemmer nu først.

Tjekket samtidig, og det var i orden: priser og firmwareopdatering kører
i deres **egen** opgave, så de blokerer ikke hovedopgaven uanset hvor
længe de tager.

### CI låst fast, fordi GitHub flytter sig den 19. oktober
Kørslen gav en advarsel med en dato på: `ubuntu-latest` bliver Ubuntu 26
fra den 19. oktober 2026. Sker det af sig selv, kan en udgivelse falde på
en maskine ingen har rørt, og firmwaren til skærmene er ikke et sted at
blive overrasket.

Alle tre kørsler står nu på `ubuntu-24.04`. Vi flytter når vi selv vil,
og prøver først.

629 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.15.1.

## 2026-10-06 18:31

### Vi gav inverteren mindre tid end Fronius selv beder om
Fronius' egen manual siger: udfør forespørgslerne med en timeout på
**mindst ét sekund**. Den sætning står citeret i vores egen opsætning,
lige over aflæsningstakten. Alligevel ventede søgningen kun **800 ms** på
svar, altså under grænsen.

Hvad det betød i praksis: en inverter der havde travlt, for eksempel
fordi den samtidig serverede sin egen hjemmeside eller sendte til
Solar.web, kunne bruge mere end 800 ms på det første svar. Så blev den
afskrevet som "taler ikke SunSpec" og sprunget over, og skærmen meldte at
der ikke blev fundet nogen inverter. **Af og til**, hvilket er den
værste slags fejl, fordi den ser ud som et netværksproblem.

Nu er grænsen **2 sekunder**. Det koster ingenting når alt er normalt:
en timeout bider kun når noget faktisk er langsomt, og der vil vi hellere
vente et øjeblik end at overse anlægget.

To steder mere havde deres eget tal. Standarden i `zs_fronius.c` var
600 ms, og den ramte ingen i dag, men den sad og ventede på den næste der
ikke selv angav en grænse. Og fejlsøgningsværktøjet havde 800 skrevet
direkte i koden, så værktøjet og skærmen søgte forskelligt. Alle fire
steder bruger nu den **ene** knap i `zs_config.h`.

### Værktøjet søgte på sin egen måde, ikke skærmens
Den her er værre end den ser ud.

`zs-probe --scan` havde sin egen løkke: én adresse ad gangen, med sin egen
tålmodighed på 200 ms. Skærmen åbner tolv forbindelser på én gang, venter
250 ms, og prøver hver adresse to gange.

Det ligner det samme og er det ikke. Melder en kunde at skærmen ikke kan
finde inverteren, og finder værktøjet den så alligevel, har vi ikke fundet
fejlen. Vi har målt to forskellige ting og draget den forkerte slutning.
Et fejlsøgningsværktøj der ikke genskaber fejlen er værre end ingenting.

`--scan` kalder nu `zs_discovery_scan`, altså præcis den kode skærmen
kører, og følger med når tallene bliver rettet. `--port` virker nu også
ved en søgning; før blev det stiltiende ignoreret, så en inverter på en
anden port end 502 kunne vælges i hånden men aldrig findes med værktøjet.

Tre nye ende til ende-tests holder det fast: værktøjet skal finde
simulatoren med skærmens motor, læse serienummeret, og melde tomt på en
port hvor der ikke er noget.

### Søgetiden er målt nu, ikke gættet
Koden lovede tre forskellige ting om hvor længe en søgning tager, og ingen
af dem passede.

Målt, på et net hvor ingenting svarer:

| Net | Tid |
|-----|-----|
| /24 | 16 sekunder |
| /20 | 3 minutter 50 sekunder |

I `zs_discovery.c` stod "omkring fem sekunder". Det var fra før hver
adresse blev prøvet to gange og før pusten mellem hvert kald. I
`zs_locate.h` stod der at søgningen blokerer "op til omkring tyve
sekunder", og det passede dengang vi kun ledte i vores eget /24. Da
søgningen blev udvidet til hele nettet, blev kontrakten stående. En
udvikler der stolede på de tyve sekunder ville blive snydt med faktor
elleve.

### En ændret indstilling kunne gå tabt i næsten fire minutter
Fundet fordi målingen ovenfor gav et tal at regne med.

Indstillinger gemmes et halvt sekund efter den sidste ændring, så en
skyder der trækkes ikke skriver i flashen hundrede gange. Gemmepunktet
ligger **tidligt** i hovedløkken, og søgningen ligger **senere i samme
omgang**.

Så: er inverteren væk, og skifter kunden tema eller lysstyrke i netop det
øjeblik, går vi ind i en søgning der kan tage næsten fire minutter før
gemningen er nået. Ryger strømmen i det vindue, er ændringen væk.

Rettet ved roden: der gemmes nu **før** vi går ind i noget der blokerer
længe. Begrundelsen for at udskyde gælder ikke der, for der kommer ikke
flere ændringer mens vi står stille.

### Én vej til flashen
Mens jeg var der: seks steder i koden kaldte `zs_nvs_save` direkte, og
ingen af dem slog "der venter noget" fra. Så hvis en udskudt gemning lå
og ventede, og noget andet gemte i mellemtiden, blev der skrevet i flashen
**to gange** for én ændring. Samme slags spild som lysstyrkeskyderen
havde.

Alle gemninger går nu gennem én funktion, der slår flaget fra når det er
lykkedes. Kalderen bestemmer stadig selv hvad der skal stå i loggen, for
det er ikke lige alvorligt at glemme et prisområde og at glemme hvilken
inverter man hører til.

### Hjælpeteksterne løj
Simulatoren skrev at port 502 er "den eneste port skærmens scanning leder
efter". Det passer ikke længere, hverken for skærmen eller for værktøjet.
Teksten viser nu hvordan man prøver en søgning af på en høj port, uden
sudo.

629 enhedstest og 68 ende til ende, alle bestået på Mac og Linux.
Firmwaren bygger rent uden advarsler. Version 0.15.0.

## 2026-10-06 17:47

### Søgningen ledte det forkerte sted på alt andet end et /24
Den vigtigste fejl indtil nu, og den sad på vores egen maskine.

Søgningen tog et **netværk**, for eksempel `10.1.0.0`, og gennemsøgte de
tre første tal plus 1 til 254. På et almindeligt hjemmenet med /24 er det
rigtigt. Men vores eget net er et **/20**: netværket hedder 10.1.0.0 mens
enhederne sidder på 10.1.4.x. Vi ledte altså i rækken 10.1.0.x, hvor der
ikke var nogen, og skærmen meldte at der ingen inverter var.

På det net kunne inverteren **aldrig** findes. /20 og /16 er helt
almindelige hos erhverv og i nyere routere.

**Rettet ved roden.** Søgningen tager nu skærmens **egen** adresse og
netmaskens længde, og regner selv ud hvilke rækker der skal gennemsøges.
Vores egen række kommer først, for der er inverteren næsten altid, og en
søgning der finder den på få sekunder er en anden oplevelse end en der
finder den efter fire minutter. Derefter resten af nettet, op til seksten
rækker. Et /24 er uændret ét hug.

Beslutningen om hvilke rækker ligger for sig selv uden netværk og har 21
tests: /24, vores eget /20 hvor alle seksten rækker skal være med uden
dubletter, /23 hvor naboen skal være den rigtige, /16 der stopper ved
grænsen, et præfiks vi ikke forstår, plads til kun én række, og det der
ikke giver mening.

**En fælde i min egen rettelse.** `zs_wifi_get_subnet` returnerede `false`
med det samme hvis man gav NULL for bufferen, og jeg kaldte den netop
sådan for kun at få præfikset. Så var præfikset blevet stående på nul,
nul regnes som /24, og alt ville have opført sig præcis som før uden at
det kunne ses. Bufferen er nu valgfri.

**Og en til, som kun Linux fangede.** Oversætteren kunne bevise
længdegrænsen da rækken var et array, men ikke da den blev en peger. Den
advarsel står der en kommentar om i filen i forvejen, fra sidste gang.
Grænsen er gjort bevislig igen.

### Inverteren hopper af og på nettet
Ny ende til ende-test: tre gange af og på, og svaret skal være det samme
hver gang. Og når den er væk, må der ikke stå en adresse tilbage som om
den var fundet.

629 enhedstest og 65 ende til ende, alle bestået på Mac og Linux.
Version 0.14.0.

## 2026-10-06 16:22

### Lysstyrkeskyderen skrev i flashen op mod hundrede gange per træk
Jeg ledte efter den slags fejl der først viser sig efter måneder, og
fandt én.

Skyderen lytter på `LV_EVENT_VALUE_CHANGED`, og LVGL sender den **hver
gang værdien ændrer sig under trækket**. Det er efterprøvet i deres egen
kilde, `lv_slider.c`: inde i PRESSING sendes hændelsen for hvert skridt.
Et træk fra 5 til 100 er altså op til 95 hændelser, og hver eneste af dem
udløste en fuld skrivning af indstillingerne til flash.

To ting var galt. Flash tåler et begrænset antal skrivninger, og en
skrivning **blokerer** opgaven mens den står på, så aflæsningen fra
inverteren gik i stå mens nogen trak i en skyder.

**Rettet centralt, ikke kun for skyderen:** ændringer fra brugerfladen
virker med det samme, og der gemmes én gang når der er faldet ro på, et
halvt sekund efter sidste ændring. Det vigtige, altså WiFi og valget af
inverter, gemmes stadig øjeblikkeligt. Der er tab værre end slid.

Undervejs fandt jeg at min første placering af gemningen lå efter
demo-tilstandens `continue`, så et temaskift i demo aldrig ville blive
gemt. Flyttet op før alle grene.

### Detaljer viser nu ledig hukommelse og oppetid
En langsom hukommelseslæk ville have været helt usynlig indtil skærmen
gik ned: tallet blev kun skrevet én gang ved opstart, i en log ingen kan
nå på en væg. Nu kan en montør se det, og to besøg med måneder imellem
kan sammenlignes. Oppetiden står ved siden af, for en skærm der
genstarter af sig selv har et lille tal der, og det er det første man
skal se.

**Efterset og i orden, så vi ved det og ikke bare håber:** Detaljer-siden
rydder med `lv_obj_clean` før den bygger om, og et temaskift river alle
tre skærme ned før det bygger nye. Ingen af dem lækker objekter. Alle
andre steder der bygger lister om, rydder også først.

608 enhedstest og 63 ende til ende, alle bestået på Mac og Linux.
Version 0.13.0.

## 2026-10-06 16:07

### MQTT efterset mod den vej der aldrig har kørt
Flødestyringens MQTT har kun kørt mod en simuleret enhed. Den rigtige
skærm har aldrig nået en server, så alt på den vej er uprøvet, og det er
præcis der en fejl kan gemme sig uden at nogen opdager det.

**Målt, og det så galt ud:** indmeldelsen sender certifikatet som JSON,
og det fylder 1286 bytes. esp-mqtt har en standardbuffer på 1024, og vi
sætter den ikke. Men i deres kode står der direkte *"Provide support for
sending fragmented message if it doesn't fit buffer"*, og den deler
beskeden op. Ikke en fejl, men det kunne ingen have vidst uden at kigge.

**Rettet:** MQTT-opgaven havde 6 KB stak, som er ESP-IDF's standard. Men
vores **egen** netværksopgave, der laver samme slags TLS-arbejde, har 10
KB. Det er bevis fra vores eget projekt på at 6 KB er knapt, og det er
netop den vej vi ikke kan måle, fordi skærmen aldrig har nået en
MQTT-server. Et stakoverløb dér ville give en skærm der genstarter i ring
på en væg. Nu 8 KB, og skærmen **måler selv** hvor meget der var tilbage
efter TLS-håndtrykket og skriver det i loggen. Når vi har et rigtigt tal
fra marken, kan knappen sættes efter det i stedet for efter et skøn.

**Efterset og i orden, så vi ikke bygger noget vi ikke behøver:** vi
mangler ikke en sidste vilje-besked for at kunne se døde skærme. Hver
enhed har et tidsstempel på hvert felt, så "sidst hørt fra" kan regnes ud
direkte, og det er prøvet af på alle 66 enheder i oversigten.

Statisk analyse kørt på det nye skrivelag med både cppcheck og gcc's
analysator: ingen fund.

608 enhedstest og 63 ende til ende, alle bestået på Mac og Linux.

## 2026-10-06 15:13

### Fase 1 af at kunne styre inverteren: fundamentet, og kun det
Vi har kun læst indtil nu. At skrive på en kundes inverter er et andet
ansvar, så her er kun fundamentet. Firmwaren sender ingen kommandoer af
sig selv, og intet har rørt en rigtig inverter.

**Research først, og den ændrede planen to gange.**

Fra Fronius' egen manual: *"If an attempt is made to write to such
registers, the inverter does not return an exception code!"* En afvist
skrivning ser altså **præcis** ud som en der lykkedes. Det sker når
"Inverter control via Modbus" ikke er slået til på inverterens webside,
eller når en anden styring har forrang.

Derfor læses der **altid tilbage** efter en skrivning. Et pænt svar
beviser kun at rammen kom frem, ikke at inverteren gjorde noget.

Om tilbagerulningsuret, som skulle have været sikkerhedsnettet, er
beviserne modstridende. Manualen beskriver det som den rigtige måde.
evcc fjernede det i PR #18386 fordi Fronius selv fortalte dem at det
udløste en spændingsfejl på GEN24, og vores egen Zbox har det derfor
slået fra med netop den begrundelse. Et tredje projekt bruger det som
designet. Vi har ingen inverter at måle på, så jeg vælger ikke side: uret
bliver en central knap der er slået fra, og beslutningen tages den dag vi
kan prøve det af.

**Hvad der er bygget**
- Funktionskode 16 til at skrive, med ekko-kontrol af adresse og antal.
  Svarer inverteren med en anden adresse, har den skrevet et andet sted
- Tilbagelæsning efter hver skrivning, og en egen fejlkode til det
  tilfælde hvor alt ser rigtigt ud og registret alligevel ikke ændrede
  sig. Teksten peger på inverterens indstillingsside, ikke på netværket
- Simulatoren kan nu tage imod skrivninger på tre måder: normalt, tavst
  ignoreret, og en rigtig afvisning
- `zs-probe --skriv` til at prøve det i hånden mod simulatoren

**Prøvet af:** 24 nye enhedstest på rammerne, og fem ende til ende hvor
den vigtigste er at en tavst ignoreret skrivning bliver fanget. Uden
tilbagelæsningen ville den have set ud som en succes.

Vores læsevej følger i forvejen Fronius' anbefaling: fire sekventielle
kald per runde, ikke parallelt, og mindst ét sekunds timeout.

608 enhedstest og 63 ende til ende, alle bestået på Mac og Linux.
Version 0.12.0.

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
