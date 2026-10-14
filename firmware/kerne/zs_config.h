/*
 * zScreen - faste vaerdier ét sted.
 *
 * Alt der kan taenkes at skulle justeres senere staar her, saa man ikke
 * skal lede i seks filer efter det tal der skal aendres.
 */

#ifndef ZS_CONFIG_H
#define ZS_CONFIG_H

/* ── Version ───────────────────────────────────────────────────────── */
/*
 * Versionsnummeret staar KUN i firmware/version.txt.
 *
 * ESP-IDF laeser den fil og skriver tallet ind i den byggede fil, hvor
 * esp_app_get_description() henter det. Stod det ogsaa her, ville de
 * to komme ud af trit den dag nogen kun rettede det ene, og saa ville
 * skaermen vise ét nummer mens opdateringen sammenlignede med et
 * andet.
 *
 * Brug zs_version() fra zs_app.h.
 */
/* ── Hvilket board ─────────────────────────────────────────────────── */
/*
 * To produkter deler den her fil. Det der er FORSKELLIGT staar her, og
 * kun her, saa resten af koden kan spoerge om en egenskab i stedet for
 * at spoerge hvilken skaerm det er.
 *
 * ZS_BOARD_D1001 saettes fra byggeriet i
 * firmware/boards/zdisplay/CMakeLists.txt, ikke fra en header. Samme
 * grund som skaermens maal i ui/zs_layout.h: fra byggeriet gaelder det
 * for ALT der oversaettes, og saa kan en fil ikke komme til at
 * oversaette med det forkerte board fordi nogen glemte en include.
 *
 * zScreen er standarden naar intet er sat. Det er med vilje:
 * tests/host bygger uden boardvalg, og den skaerm der staar paa
 * skrivebordet i dag er en D1.
 */
#if defined(ZS_BOARD_D1001)
#define ZS_PRODUCT_NAME      "ZDisplay"
/*
 * RADIOEN SIDDER IKKE I CHIPPEN.
 *
 * ESP32-P4 har ingen radio. Wifi kommer fra en ESP32-C6 over SDIO, og
 * kalibreringstallene til den radio ligger i C6'erens EGEN flash.
 *
 * To steder betyder det noget:
 *
 *   1. esp_phy_init.h findes slet ikke for esp32p4. ESP-IDF registrerer
 *      esp_phy som en TOM komponent paa den chip, saa det er ikke en
 *      manglende afhaengighed - funktionen er der ikke.
 *   2. En fabriksnulstilling kan derfor ikke rydde radioens
 *      kalibrering. Den rydder P4'erens NVS, og C6'eren staar med det
 *      den havde. Se docs/zdisplay.md.
 */
#define ZS_HAR_LOKAL_RADIO   0
/*
 * Navnet paa firmware-filen i en udgivelse paa GitHub.
 *
 * De to produkter udgives fra SAMME repo med samme versionsmaerke, saa
 * der ligger to .bin-filer i hver udgivelse. En skaerm skal tage SIN.
 *
 * Det gjorde den ikke foer: zs_ota tog den FOERSTE fil der sluttede paa
 * ".bin". ESP-IDF ville have afvist den - chip-id'et staar i billedets
 * hoved - saa der var ikke fare for at en ZDisplay-firmware kom til at
 * koere paa en zScreen. Men skaermen ville hente 2 MB over kundens
 * forbindelse, afvise dem, og proeve igen en halv time senere. For evigt.
 *
 * Praefikset skal passe med hvad .github/workflows/release.yml laegger
 * op.
 */
#define ZS_OTA_ASSET_PREFIX  "zdisplay-"
/*
 * Skaermens navn paa flaaden, altsaa ZS_FLEET_NAVN_PREFIX plus MAC'en.
 *
 * Det SKAL vaere et andet end zScreens. Serveren kan ellers ikke se
 * hvilket produkt en skaerm er, og to produkter med samme navnerum er
 * ikke til at holde ude fra hinanden naar der skal fejlsoeges.
 */
#define ZS_FLEET_NAVN_PREFIX "zdisplay-"
/*
 * HVILKEN MAC DER GOER SKAERMEN UNIK, og hvorfor det ikke er den samme
 * som paa zScreen.
 *
 * ESP_MAC_WIFI_STA FINDES IKKE paa ESP32-P4. Radioen er i en anden chip,
 * saa chippen har ingen wifi-MAC: esp32p4's soc_caps.h saetter ikke
 * SOC_WIFI_SUPPORTED, og i ESP-IDF's mac_addr.c ligger hele
 * ESP_MAC_WIFI_STA-grenen bag netop den. esp_read_mac() svarer
 * ESP_ERR_NOT_SUPPORTED.
 *
 * Og det ville IKKE have vaeret en byggefejl. Det ville have vaeret
 * endnu vaerre: zs_fleet_unique_id() falder tilbage til "ukendt" naar
 * kaldet fejler, saa HVER ENESTE ZDisplay ville melde ind paa flaaden
 * med praecis det samme navn. To skaerme med samme klient-id slaar
 * hinanden af, og en doed MQTT-session lever halvandet minut - saa det
 * ville se ud som et daarligt netvaerk hos kunden.
 *
 * ESP_MAC_BASE er fabrikkens MAC fra eFuse, som alle de andre udledes
 * af. Den findes paa alle chips.
 *
 * zScreen bliver paa ESP_MAC_WIFI_STA. Paa ESP32-S3 er de to PRAECIS de
 * samme seks bytes - ESP_MAC_WIFI_STA er en ren memcpy af base-MAC'en
 * uden forskydning - men en skaerms navn paa flaaden er ikke noget man
 * aendrer paa en slutning. Det er maalt i ESP-IDF's kilde, ikke paa en
 * skaerm, og det er forskellen.
 */
#define ZS_MAC_TYPE          ESP_MAC_BASE
#else
#define ZS_PRODUCT_NAME      "zScreen"
#define ZS_HAR_LOKAL_RADIO   1
#define ZS_OTA_ASSET_PREFIX  "zscreen-"
#define ZS_FLEET_NAVN_PREFIX "zscreen-"
#define ZS_MAC_TYPE          ESP_MAC_WIFI_STA
#endif

/* ── Skaerm ────────────────────────────────────────────────────────── */
/*
 * Maalene staar i ui/zs_theme.h, ikke her.
 *
 * De stod OGSAA her engang, og det gik galt: konstanten for
 * statuslinjens hoejde hed ZS_STATUSBAR_H, praecis som include-guarden
 * i zs_statusbar.h. Fordi zs_config.h blev laest foerst, troede
 * praeprocessoren at headeren allerede var med, og sprang hele filen
 * over. Der kom ingen advarsel, kun en fejl et helt andet sted om en
 * type der ikke fandtes.
 *
 * Derfor to regler:
 *   1. Et maal staar ét sted. Layout hoerer til i zs_theme.h.
 *   2. Ingen konstant slutter paa _H. Det er forbeholdt include-guards.
 *      tools/check-headers.sh haandhaever det.
 */

/* ── Aflaesning ────────────────────────────────────────────────────── */
/*
 * Hvor tit vi spoerger inverteren.
 *
 * To sekunder er rigeligt til en skaerm man kigger paa, og det holder os
 * pænt inden for Fronius' egen anbefaling om at spoerge sekventielt med
 * mindst ét sekunds timeout. Hver runde er fire Modbus-kald.
 */
#define ZS_POLL_INTERVAL_MS  2000

/*
 * Hvor laenge et tal maa vaere gammelt foer vi daemper det paa skaermen.
 *
 * Vi rydder ikke kortene ved forbindelsestab. Det sidst kendte tal med
 * en tydelig markering af at det er gammelt, er mere brugbart end fire
 * tomme felter, og det fortaeller ogsaa hvad der skete lige foer.
 */
#define ZS_STALE_AFTER_MS    10000

/* Efter saa laenge uden svar giver vi op og forbinder helt forfra. */
#define ZS_RECONNECT_AFTER_MS 30000

/* ── Genforbindelse ────────────────────────────────────────────────── */
/*
 * En Fronius Gen24 holder sin side af en afbrudt forbindelse aaben i
 * op til et kvarter og afviser nye forsoeg imens. Derfor proever vi
 * flere gange hurtigt efter hinanden foer vi gaar over til at vente
 * laengere og laengere.
 */
#define ZS_RECONNECT_BURST      3
#define ZS_RECONNECT_BURST_MS   500
#define ZS_RECONNECT_MIN_MS     2000
#define ZS_RECONNECT_MAX_MS     60000

/*
 * Hvornaar vi holder op med at banke paa den gemte adresse og leder
 * efter inverteren i stedet. Se zs_locate.h.
 *
 * Tre mislykkede forsoeg, ikke ét: en inverter der genstarter er vaek i
 * et halvt minut, og saa skal skaermen ikke sende en scanning ud over
 * hele nettet. Med den voksende pause er tre forsoeg omkring fjorten
 * sekunder, altsaa laenge nok til at en genstart er ovre, og kort nok til
 * at kunden ikke staar og venter.
 *
 * Fandt vi den ikke, ventes der FOER vi leder igen. En scanning tager
 * omkring tyve sekunder, regnet af ZS_SCAN-tallene: 254 adresser, tolv ad
 * gangen, to runder a 250 ms. Det er ikke noget at lave hvert halve
 * minut, hverken for nettet eller for stroemmen. Fem minutter betyder at
 * en inverter der kommer tilbage paa en ny adresse er fundet igen inden
 * kunden har hentet kaffe.
 */
#define ZS_LOCATE_AFTER_FAILS   3
#define ZS_LOCATE_RETRY_MS      (5 * 60 * 1000)

/* ── Naar skaermen siger fra ───────────────────────────────────────── */
/*
 * Hvor laenge noget skal vaere vaek foer det er en fejl et menneske skal
 * se paa. De to tal er IKKE ens, og de er regnet og ikke skudt efter.
 *
 * WIFI, tyve sekunder. En router der genstarter, en nabo der taender en
 * mikroboelgeovn, en kanal der skifter: den slags sker, og skaermen
 * henter sig selv uden at nogen behoever vide det. Tyve sekunder er
 * laenge nok til at de tilfaelde er ovre, og kort nok til at en montoer
 * der lige har trukket stikket kan se at skaermen opdagede det.
 *
 * INVERTEREN, to minutter. Den skal vaere LAENGERE end alt det
 * skaermen allerede selv gOEr for at komme tilbage, ellers raaber den
 * op mens reparationen er i gang:
 *
 *     tre hurtige forsoeg          ZS_RECONNECT_BURST a 500 ms
 *     voksende pause              2, 4, 8, 16, 32, 60 s
 *     og efter tre fejl, en soegning over hele undernettet, ~20 s
 *
 * Summen af de foerste fem pauser er 62 sekunder, og en soegning oveni
 * er omkring firs. To minutter ligger over det hele, saa siden kun
 * kommer frem naar skaermen faktisk har GIVET OP - og ikke mens den
 * stadig er ved at finde en inverter der bare har skiftet adresse.
 */
#define ZS_FEJL_WIFI_GRAENSE_MS       (20 * 1000)
#define ZS_FEJL_INVERTER_GRAENSE_MS   (120 * 1000)

/*
 * HVOR LAENGE "ANLAEGGET SOVER" KAN VAERE SANDT.
 *
 * Soevn afgoeres af hvad inverteren SELV meldte foer den forsvandt. Den
 * oplysning bliver ikke nyere af at tiden gaar: en inverter der gik i
 * stykker ét minut efter at have meldt "sover", ser ud som en der
 * sover - og den ville blive meldt som sovende i al evighed. Graat
 * maerke, intet blink, ingen lyd, ogsaa en tirsdag i juli.
 *
 * Tyve timer. Den laengste nat i Danmark er omkring sytten timer fra
 * solnedgang til solopgang, saa tyve giver luft til en graa
 * decemberdag hvor inverteren vaagner sent. Er den vaek laengere end
 * det, er det ikke solen.
 */
#define ZS_FEJL_SOVER_MAKS_MS         (20 * 60 * 60 * 1000)

/* ── Lyden ─────────────────────────────────────────────────────────── */
/*
 * Summeren sidder paa RP2040'eren og ikke paa ESP32'en, saa et bip er en
 * pakke over UART. Se ui/zs_lyd.h for hvorfor, og tools/rp2040/ for den
 * firmware der skal ligge i den anden chip foer der kommer en lyd.
 */
/*
 * ÉT bip er to korte toner. Én tone alene kan forveksles med et knirk
 * fra huset; to med en kort pause imellem er tydeligt noget der kommer
 * fra en maskine. Laengere end knap et femtedels sekund bliver det en
 * alarmklokke, og det er ikke det her er.
 */
#define ZS_LYD_TONE_MS          180
#define ZS_LYD_TONER            2
#define ZS_LYD_TONE_PAUSE_MS    220
/*
 * TONENS FREKVENS STAAR IKKE HER, og det er med vilje.
 *
 * Den stod her som ZS_LYD_HZ - og blev aldrig brugt af en eneste linje
 * firmware. Det KAN den ikke: tonen laves af RP2040'eren, og ESP32'en
 * sender kun "bip saa mange millisekunder". To tal der skal vaere ens,
 * hvor kun det ene bliver brugt, er et tal der stille og roligt kommer
 * til at vaere forkert.
 *
 * Frekvensen staar derfor ÉT sted: TONE_HZ i
 * tools/rp2040/zscreen_rp2040.ino, hos den chip der faktisk laver
 * lyden.
 */

/*
 * HVOR MANGE GANGE DER BIPPES FOR ÉN FEJL, og hvor laenge der er
 * imellem.
 *
 * Fire gange i alt: med det samme, efter fem minutter, efter et kvarter
 * og efter tre kvarter. Saa er der stille.
 *
 * Grunden til at det stopper: en fejl kan vare i dagevis - en inverter
 * der er sendt til reparation, et anlaeg der er slukket for vinteren -
 * og en skaerm der bipper hvert femte minut i tre doegn bliver taget ned
 * af vaeggen. Lyden er der for at FAA nogen hen til skaermen den foerste
 * gang. Derefter staar beskeden paa fladen og blinker, og den bliver
 * ved saa laenge fejlen er der.
 *
 * Pauserne er dem EFTER hvert bip. Det sidste tal bliver aldrig brugt,
 * men staar der saa opslaget altid er inden for tabellen.
 */
#define ZS_LYD_PAUSER_MS { 5 * 60 * 1000, 10 * 60 * 1000, 30 * 60 * 1000, 0 }

/*
 * ER LYDEN SLAAET TIL. Standard er NUL, altsaa nej.
 *
 * Grunden er hardware og ikke en holdning: summeren sidder paa
 * RP2040'eren, og den firmware der ligger i den fra fabrikken svarer
 * KUN paa sluk-kommandoen. Den kender godt bip-kommandoen - den staar i
 * dens egen kilde - men den kalder den aldrig. Maalt 13. oktober 2026
 * ved at laese Seeeds egen indicator_rp2040.ino.
 *
 * Saetter man den her til 1 UDEN at have flashet den anden chip, sker
 * der ingenting: pakken bliver sendt, og ingen svarer. Det er tavst, og
 * tavse ting er dyre at fejlsoege. Derfor staar den paa nul indtil
 * nogen har gjort trin to, og trin to staar i tools/rp2040/HVORDAN.md.
 *
 * Blinket og baglys-pulsen virker UANSET. De er paa ESP32'en, og de
 * virker paa hver enhed der staar ude i dag.
 */
#define ZS_LYD_ENABLED          0

/* ── Netvaerksscanning ─────────────────────────────────────────────── */
#define ZS_SCAN_PARALLEL        12      /* samtidige sockets            */
#define ZS_SCAN_PORT_TIMEOUT_MS 250     /* er der noget paa port 502    */

/*
 * Pause mellem hvert connect() under soegningen.
 *
 * Uden den finder skaermen ingenting. Det er maalt paa en rigtig D1
 * mod et net med en Fronius paa .100:
 *
 *   12 samtidige, 250 ms, ingen pause   ->  0 fundet
 *   12 samtidige, 250 ms, 10 ms pause   ->  1 fundet
 *   12 samtidige, 1200 ms, ingen pause  ->  0 fundet
 *    4 samtidige, 800 ms, ingen pause   ->  0 fundet
 *    4 samtidige, 800 ms, 10 ms pause   ->  1 fundet
 *    1 ad gangen, 300 ms                ->  1 fundet
 *
 * Laeg maerke til at hverken laengere ventetid eller faerre samtidige
 * hjaelper. Det eneste der virker er en pause MELLEM kaldene.
 *
 * Forklaringen: connect() vender tilbage med det samme, men SYN-pakken
 * skal videre gennem lwIP og ud af wifi-senderen. Fyrer man tolv af i
 * en tot, er der ikke sendebuffere nok, og de fleste bliver smidt vaek
 * uden at nogen faar besked. lwIP proever foerst igen efter flere
 * sekunder, og da har vi for laengst givet op.
 *
 * Hvor stor skal pausen vaere? Maalt paa enheden mod samme net:
 *
 *    1 ms -> 0 fundet        5 ms -> 1 fundet
 *    2 ms -> 0 fundet       10 ms -> 1 fundet
 *    3 ms -> 1 fundet       20 ms -> 1 fundet
 *
 * Graensen ligger ved tre millisekunder. Vi bruger ti, altsaa tre
 * gange margen, saa det ogsaa holder paa et travlt net eller med et
 * svagere signal. Det koster to sekunder paa en hel gennemgang.
 */
#define ZS_SCAN_CONNECT_GAP_MS  10

/*
 * Hvor mange gange hver adresse proeves.
 *
 * Pausen ovenfor hjaelper, men den er et maalt tal og ikke en
 * aarsagsrettelse. Den rigtige fejl var at koden gik ud fra at ét
 * probe-forsoeg altid naar frem. Wifi taber pakker: et andet apparat
 * sender samtidig, signalet dykker, aksesspunktet har travlt. Saa den
 * antagelse holder ikke, uanset hvor lang pausen er.
 *
 * Derfor proever vi hver adresse igen hvis den ikke svarede foerste
 * gang. Det virker uanset HVORFOR pakken forsvandt, og det er billigt:
 * anden runde roerer kun de adresser der ikke allerede har svaret, og
 * paa et almindeligt hjemmenet svarer de faerreste.
 */
#define ZS_SCAN_TRIES           2
/*
 * Hvor laenge vi venter paa et SunSpec-svar under en soegning.
 *
 * Var 800 ms, og det var UNDER Fronius' egen anbefaling. Deres manual
 * siger: "Perform the requests with a timeout of at least 1 second."
 * Vores egen konfiguration citerer netop den saetning nogle linjer
 * laengere oppe, ved aflaesningstakten, saa vi vidste det godt ét sted og
 * ikke det andet.
 *
 * Hvad det betoed: en inverter der havde travlt, fx fordi den samtidig
 * serverede sin egen webside eller sendte til Solar.web, kunne bruge mere
 * end 800 ms paa det foerste svar. Saa kaldte vi den "taler ikke SunSpec"
 * og sprang den over. Resultatet paa skaermen var "der blev ikke fundet
 * nogen inverter", af og til, hvilket er den vaerste slags.
 *
 * To sekunder giver margen oven i deres minimum. Det koster ingenting i
 * det normale tilfaelde, hvor svaret kommer paa faa millisekunder:
 * timeouten bider kun naar noget ER langsomt, og der vil vi hellere vente
 * end at overse anlaegget.
 *
 * Bemaerk at ZS_SCAN_PORT_TIMEOUT_MS er noget andet: det er en TCP-
 * forbindelse, ikke en Modbus-forespoergsel, og den gaelder 254 adresser
 * ad gangen. Den skal blive hvor den er.
 */
#define ZS_SCAN_SUNSPEC_TIMEOUT_MS 2000
/*
 * Hvor mange invertere vi kan holde paa ad gangen.
 *
 * ÉT tal. Det stod foer to steder: 12 her og 8 i zs_discovery.h, og de 12
 * blev aldrig brugt til noget. Tallet saetter tre bufferes stoerrelse,
 * hver zs_found_t er omkring 130 bytes, saa otte er cirka en kilobyte per
 * styk. Otte er rigeligt: et hus har én inverter, et stort anlaeg to
 * eller tre.
 */
#define ZS_DISCOVERY_MAX        8

/*
 * Hvor mange raekker a 254 adresser vi hoejst gennemsoeger.
 *
 * Et almindeligt hjemmenet er et /24, altsaa ÉN raekke, og saa aendrer
 * det her ingenting. Men et /20 er helt almindeligt hos erhverv og i
 * nyere routere, og der er seksten raekker. Vores egen maskine sidder
 * paa et /20 i dag.
 *
 * Seksten raekker er omkring fire minutter med de maalte tal: 254
 * adresser tager cirka seksten sekunder. Det er for laenge til at staa og
 * se paa under opsaetning, men det er ikke der det betyder noget: den
 * raekke skaermen SELV sidder i bliver scannet foerst, og der er
 * inverteren naesten altid. De oevrige er for den baggrundssoegning der
 * leder efter en inverter der er flyttet, og der maa det godt tage tid.
 */
#define ZS_SCAN_MAX_BLOKKE      16

/* ── Wifi ─────────────────────────────────────────────────────────── */
#define ZS_WIFI_SCAN_MAX        20
#define ZS_WIFI_CONNECT_TIMEOUT_MS 20000

/* ── Ur ───────────────────────────────────────────────────────────── */
/*
 * Uret er pynt, ikke en forudsaetning. Er der ingen internetforbindelse,
 * skjuler vi klokkeslaettet og viser alt det andet som normalt.
 */
#define ZS_NTP_SERVER_1      "dk.pool.ntp.org"
#define ZS_NTP_SERVER_2      "pool.ntp.org"
/* Dansk tid med sommertid, i POSIX-format. */
#define ZS_TIMEZONE          "CET-1CEST,M3.5.0,M10.5.0/3"

/* ── Lysstyrke ────────────────────────────────────────────────────── */
/* ── Floedestyring ────────────────────────────────────────────────── */
/*
 * Forbindelsen til Frekvenz' egen OpenRemote-server.
 *
 * ALT om den staar her, saa der kun er ét sted at aendre takt, vaert og
 * om den overhovedet er med.
 *
 * 0 slaar den helt ud af den byggede fil, som demoen. En skaerm uden
 * floedestyring skal ikke have koden med.
 */
#ifndef ZS_FLEET_ENABLED
#define ZS_FLEET_ENABLED        1
#endif

/* Serveren. Kan overskrives per enhed i lageret, saa den samme
 * firmware kan pege paa en proeveserver og paa den rigtige. */
#define ZS_FLEET_HOST           "fleet.frekvenz.nu"
#define ZS_FLEET_PORT           8883
#define ZS_FLEET_REALM          "master"

/*
 * Hvor ofte der sendes, maalt i aflaesninger og ikke i sekunder.
 *
 * 1 betyder hver gang vi har laest inverteren, altsaa realtid. Tallet
 * er med VILJE i aflaesninger: saa foelger takten automatisk
 * ZS_POLL_INTERVAL_MS, og de to kan ikke komme ud af trit. Skal der
 * sendes sjaeldnere, er det ét tal her: 15 giver hvert halve minut ved
 * to sekunders aflaesning.
 */
#define ZS_FLEET_PUBLISH_EVERY_N_POLLS  1

/*
 * Hvor laenge vi venter efter indmeldelsen foer vi skriver.
 *
 * Maalt: skriver man i samme oejeblik svaret kommer, bliver hver
 * skrivning nAEgtet og forbindelsen lukket. Serveren skal foerst
 * opgradere forbindelsen til servicebrugeren. Halvandet sekund var nok
 * i alle forsoeg; to giver luft paa et langsomt net.
 */
#define ZS_FLEET_READY_DELAY_MS 2000

/*
 * Plads til at samle et svar der kommer i stykker.
 *
 * HVORFOR DEN FINDES, og det er den dyreste fejl vi har haft.
 *
 * esp-mqtt har en modtagebuffer paa 1024 bytes. Er en besked stoerre,
 * kommer den i FLERE stykker, og kun det FOERSTE har et emne paa sig.
 * Foer samlede vi dem ikke: hvert stykke blev laest som om det var en
 * hel besked.
 *
 * MAALT mod vores egen OpenRemote: svaret paa en indmeldelse er
 * 2604 bytes, fordi det indeholder hele enheden med alle tretten
 * attributter. Det kommer altsaa i tre stykker, ingen af dem er gyldig
 * JSON alene, og alle tre blev forkastet. Skaermen fik derfor aldrig sit
 * enheds-id, og uden det maa den ikke skrive en eneste maaling.
 *
 * Det passer med hvad serveren har set: enheden findes, men nul af dens
 * tretten attributter har nogensinde faaet en vaerdi.
 *
 * Otte kilobyte og ikke fire: svaret vokser med antallet af attributter,
 * og vi vil hellere have luft end at skulle rette det igen naar der
 * kommer et felt mere. Den ligger i PSRAM, hvor der er otte megabyte.
 */
#define ZS_FLEET_SVAR_MAX       8192

/*
 * Tilfaeldig ventetid foer skaermen melder sig ind foerste gang.
 *
 * Alle skaerme paa samme gade faar stroem tilbage i samme sekund efter
 * et svigt, og uden den her banker de paa serveren samtidig.
 *
 * Maalt paa tres enheder mod en rigtig OpenRemote:
 *   indmeldelse spredt over 4,8 sek -> 49 tidsudloeb mod Keycloak og
 *                                      109 tvungne afbrydelser, og hver
 *                                      afbrydelse giver en ny
 *                                      indmeldelse, saa stormen foeder
 *                                      sig selv
 *   indmeldelse spredt over 15 sek  -> nul af begge
 *
 * Et minut giver tusind skaerme omkring sytten indmeldelser i sekundet,
 * godt under det maalte punkt hvor det gik galt. Prisen er at en skaerm
 * kan vaere op til et minut om at vise sig i flaadeoversigten efter en
 * genstart. Den viser tal paa vaeggen med det samme uanset hvad.
 *
 * Nul slaar spredningen fra, til naar man staar med én enhed og ikke
 * vil vente.
 */
#define ZS_FLEET_START_SPREAD_MS  60000

/*
 * Hvor laenge vi venter foer vi proever at melde ind igen, naar
 * indmeldelsen ikke lykkedes OG det ikke var en afvisning.
 *
 * Daekker to ting: en afsendelse der ikke gik igennem, og et svar der
 * aldrig kom. Begge betyder at vi sidder paa en aaben forbindelse uden
 * at vaere indmeldt, og uden den her ville vi sidde der for evigt, for
 * indmeldelsen blev foer kun forsoegt naar abonnementet var nyt.
 *
 * En afvisning er en anden sag. Der er noget galt med certifikatet, og
 * det retter sig ikke paa et halvt minut. Se AFVIST_PAUSE_MS i
 * zs_fleet.c.
 */
#define ZS_FLEET_ENROLL_RETRY_MS  30000

/*
 * Hvor laenge der gaar foer vi forbinder igen, og hvor meget det tal
 * varierer fra skaerm til skaerm.
 *
 * esp-mqtt venter PRAECIS det samme hver gang, der er ingen voksende
 * pause og ingen spredning indbygget, det er efterset i deres kode.
 * Satte alle skaerme det samme tal, ville tres skaerme som serveren
 * afbryder i samme oejeblik forbinde igen i samme oejeblik, ti sekunder
 * senere, og saadan bliver de ved. Maalt: serveren afbroed 109
 * forbindelser paa et minut under en indmeldelsesbyrde, saa det er ikke
 * et opfundet tilfaelde.
 *
 * Hver skaerm trAEkker sit eget tal mellem de to graenser ved opstart og
 * beholder det. Saa er de ude af trit med hinanden for altid, ogsaa
 * efter ting der rammer dem alle sammen.
 */
#define ZS_FLEET_RECONNECT_MS         10000
#define ZS_FLEET_RECONNECT_SPREAD_MS  10000

/*
 * Stak til MQTT-opgaven.
 *
 * ESP-IDF's egen standard er 6 KB. Men vores EGEN netvaerksopgave, som
 * ogsaa laver TLS, har 10 KB, og MQTT-opgaven laver samme slags arbejde:
 * et mbedTLS-haandtryk, her endda mod Mozillas rodliste naar der ikke er
 * et eget CA i lageret.
 *
 * Og det er den ene vej vi IKKE kan maale, fordi skaermen aldrig har
 * naaet en MQTT-server. Et stakoverloeb dér ville give en skaerm der
 * genstarter i ring paa en vaeg, saa her er luft billigere end at have
 * ret. Der er 184 KB intern hukommelse fri.
 *
 * Skaermen maaler selv og skriver det i loggen ved foerste forbindelse,
 * se ZS_LOGI i zs_fleet.c. Naar vi har et rigtigt tal fra marken, kan
 * det her saettes efter det i stedet for efter et skoen.
 */
#define ZS_FLEET_TASK_STACK      8192

/*
 * Hvor laenge vi venter med at gemme en indstilling, regnet fra den
 * sidste aendring.
 *
 * HVORFOR DEN FINDES. Lysstyrken er en skyder, og LVGL sender
 * LV_EVENT_VALUE_CHANGED hver gang vaerdien aendrer sig under traekket.
 * Det staar i deres egen kilde, lv_slider.c: inde i PRESSING sendes
 * haendelsen for hvert skridt. Et traek fra 5 til 100 er altsaa op til 95
 * haendelser, og foer det her gemte vi i flashen ved hver eneste af dem.
 *
 * To ting var galt med det. Flashen tager imod et begraenset antal
 * skrivninger, og en skrivning BLOKERER opgaven mens den staar paa, saa
 * aflaesningen fra inverteren gik i staa mens nogen trak i en skyder.
 *
 * Nu virker skaermen med det samme, og der gemmes ÉN gang naar der er
 * faldet ro paa. Et halvt sekund er laenge nok til at daekke et traek og
 * kort nok til at ingen naar at tage stroemmen imens.
 *
 * Det vigtige, altsaa wifi og valget af inverter, gemmes stadig med det
 * samme. Der er tab vaerre end slid.
 */
#define ZS_SETTINGS_SAVE_DELAY_MS  500

/*
 * Feltnavnene paa serveren. ÉT sted, saa en omdoebning ikke skal findes
 * fem steder i koden.
 *
 * ZS_FLEET_TARGET_FELT er det eneste felt der gaar den anden vej, altsaa
 * fra serveren og ned til skaermen. Vi skriver den i dashboardet, skaermen
 * lytter og retter sig efter den. Det er den der goer at vi kan rulle en
 * opdatering ud til ÉN skaerm foerst i stedet for til hele flaaden paa én
 * gang. Se zs_ota.h.
 *
 * ZS_FLEET_VERSION_FELT gaar den almindelige vej og fortaeller hvilken
 * udgave skaermen faktisk koerer. Uden den kan vi ikke se om en udrulning
 * gik godt, og saa er en maalversion ikke meget vaerd.
 */
#define ZS_FLEET_TARGET_FELT   "targetVersion"
#define ZS_FLEET_VERSION_FELT  "firmwareVersion"

/*
 * LADEPLANEN, ogsaa den anden vej: fra serveren og ned i skaermen.
 *
 * Platformen skriver planen paa BATTERIET hos OpenRemote - der er den
 * bundet, og kun der. Et attributeLink baerer de to felter over paa
 * skaermens eget asset, som er det eneste skaermen kan se. Formen staar
 * i scripts/felter.py og scripts/anlaeg.py i zScreen-server.
 *
 * SETPUNKTET ER IKKE MED HER, OG DET ER MED VILJE. powerSetpoint
 * bliver ikke baaret over paa skaermen, fordi raten ikke kan omregnes
 * fra kilowatt til noternes raa registertal uden skalafaktoren - punkt 2
 * paa maalelisten i docs/batteristyring.md. Derfor kan skaermen i dag
 * modtage netop de tre tilstande der IKKE ser paa raten: normal,
 * zeroExport og laas. Se zs_ladeordre.h.
 */
#define ZS_FLEET_LADE_TILSTAND_FELT  "ladeplanTilstand"
#define ZS_FLEET_LADE_SEKUNDER_FELT  "ladeplanSekunder"

/*
 * PRISOMRAADET KOMMER NED FRA SERVEREN, OG VAELGES IKKE PAA SKAERMEN.
 *
 * Det var et trin i opsaetningen foer, hvor montoeren trykkede DK1 eller
 * DK2. Det er vaek, besluttet 12. oktober 2026, af to grunde.
 *
 * Den ene: det er ikke montoerens oplysning. Platformen kender kundens
 * adresse, og den ved derfor hvilken landsdel anlaegget staar i. Et
 * menneske der skal gaette paa en skaerm, gaetter nogle gange forkert, og
 * saa viser skaermen priser fra den anden side af Storebaelt uden at
 * nogen kan se at det er sket.
 *
 * Den anden: zs_price_fetch falder tilbage til DK2 naar der ikke er et
 * omraade. Det er den rigtige opfoersel - der skal staa NOGET - men det
 * betyder at et glemt trin bliver til stille forkerte tal for den halve
 * del af landet. Kommer vaerdien ned fra serveren, kan den rettes ét
 * sted bagefter, ogsaa for en skaerm der allerede haenger paa en vaeg.
 *
 * Feltet heder det samme paa serveren, se prisZone i anlaeg.py.
 */
#define ZS_FLEET_ZONE_FELT           "prisZone"

/*
 * KUNDENS NAVN, OG DET ER SIGNALET OM AT TILMELDINGEN LYKKEDES.
 *
 * Skaermen kan ikke selv se at den er blevet tilmeldt: det sker paa
 * serveren, og indtil nu fik skaermen ikke noget at vide. Derfor stod
 * QR-koden paa skaermen for evigt, ogsaa efter at anlaegget var oprettet
 * og kunden var paa plads. Montoeren kunne kun gaa ud af siden og haabe.
 *
 * Noten i zs_tilmeld.h beskrev hvorfor tilstanden ZS_TILMELD_TILMELDT
 * blev TAGET UD: den kunne ikke naas. Det her felt er det der gor den
 * naabar.
 *
 * Serveren skriver kundens navn paa skaermens eget asset naar
 * tilmeldingen er gaaet igennem - se tilmeld.py. Kommer der et navn,
 * ER skaermen tilmeldt, og den kan sige HVEM. At montoeren kan se
 * navnet, er ikke pynt: det er den eneste maade han kan opdage at
 * koden endte hos den forkerte kunde.
 */
#define ZS_FLEET_KUNDE_FELT          "kundeNavn"

/*
 * Hvor langt et kundenavn vi tager imod.
 *
 * 64 byte. Kunder.json's navne er efternavne og firmanavne, og det
 * laengste i listen i dag er under tyve tegn. Serveren kan sende hvad
 * som helst, saa der skal staa en graense et sted - og den skal vaere
 * her og ikke i en buffer paa stakken, hvor den ville vaere let at
 * glemme naar nogen laaner koden.
 */
#define ZS_FLEET_KUNDE_MAKS          64

/*
 * Hvor laenge "Anlaegget er tilmeldt" staar foer skaermen gaar hjem.
 *
 * Tolv sekunder. Montoeren skal kunne LAESE navnet - det er hele grunden
 * til at det staar der - og han staar maaske med en skruetraekker i den
 * anden haand. Kortere, og han ser det ikke. Laengere, og skaermen
 * haenger og viser en kvittering i stedet for tal.
 */
#define ZS_TILMELDT_VIS_MS           12000

/*
 * ALLE FELTER DER GAAR NED I SKAERMEN, ÉT STED.
 *
 * Listen BLIVER BRUGT: zs_fleet.c abonnerer paa netop den, og
 * kryds.py i zScreen-server laeser den for at vide hvilke felter
 * skaermen LAESER i stedet for skriver.
 *
 * Foer stod reglen i kryds.py som "alt andet end targetVersion
 * skriver skaermen". Den slags "hvad den ikke er"-regel holder lige til
 * der kommer et tredje felt - og saa meldte den at firmwaren skrev i
 * ladeplanen, hvad den ikke gor. Det skete 10. oktober 2026.
 *
 * Et nyt felt ned i skaermen skal skrives HER, og saa foelger baade
 * abonnementet og krydstjekket med af sig selv.
 */
#define ZS_FLEET_NED_FELTER \
    ZS_FLEET_TARGET_FELT, \
    ZS_FLEET_LADE_TILSTAND_FELT, \
    ZS_FLEET_LADE_SEKUNDER_FELT, \
    ZS_FLEET_ZONE_FELT, \
    ZS_FLEET_KUNDE_FELT

/*
 * ALLE DE ANDRE FELTER skaermen skriver til serveren.
 *
 * HVORFOR DE STAAR HER OG IKKE I AFSENDELSEN.
 *
 * Navnet skal matche serverens felt PRAECIS. Rammer det ved siden af,
 * sker der ikke noget synligt: serveren tager imod beskeden og smider
 * den vaek, og paa dashboardet staar feltet bare tomt for evigt. Der
 * kommer ingen fejl nogen steder.
 *
 * Foer laa de ni som loese tekster midt i afsendelsen, mens to af dem
 * stod heroppe. Skulle man svare paa "hvad skriver skaermen egentlig",
 * skulle man laese en funktion igennem. Nu er det én liste.
 *
 * Energiens fem felter staar for sig i zs_energi.h, fordi de er
 * nummererede og hoerer sammen med den regning. Se ZS_ENERGI_TIME_FELT.
 */
#define ZS_FLEET_SOL_FELT        "solarPower"
#define ZS_FLEET_FORBRUG_FELT    "housePower"
#define ZS_FLEET_BATTERI_FELT    "batteryPower"
#define ZS_FLEET_NET_FELT        "gridPower"
#define ZS_FLEET_SOC_FELT        "batteryLevel"
#define ZS_FLEET_MODEL_FELT      "inverterModel"
#define ZS_FLEET_SERIENR_FELT    "inverterSerial"
#define ZS_FLEET_EFFEKT_FELT     "ratedPower"
#define ZS_FLEET_KAPACITET_FELT  "batteryCapacity"

/*
 * Svarer inverteren?
 *
 * Boksens egen online-status kommer vi IKKE med: den saetter OpenRemote
 * selv ud af MQTT-forbindelsen, se boxOnline og meta-elementet
 * userConnected paa serveren. Vi ville ikke kunne sige "jeg er offline"
 * alligevel, for saa er der ingen forbindelse at sige det paa.
 *
 * Men om INVERTEREN svarer, kan kun vi vide. Den sendes KUN naar det
 * skifter: sendte vi den ved hver aflaesning, ville der komme en raekke
 * hvert andet sekund for et tal der naesten aldrig aendrer sig.
 *
 * Vaerdien skal vaere true eller false med SMAA bogstaver. Maalt mod
 * serveren: "true" og "false" virker, og saa gør 1 og 0. "True" med stort
 * bliver TAVST ingenting, feltet staar tomt og der kommer ingen fejl.
 */
#define ZS_FLEET_INVERTER_FELT   "inverterOnline"

/*
 * Kvartersgennemsnittene, og hvorfor de ikke er de samme felter.
 *
 * De fem live-tal sendes hvert andet sekund og GEMMES IKKE: fem felter
 * gange 43.200 om dagen er tre millioner raekker om maaneden per skaerm.
 * De er til at vise live, og til at se at skaermen er i live.
 *
 * Men saa er der ingen historik, og OpenRemotes forbrugsprognose regner
 * paa GEMT historik. Uden den planlaegger en batterioptimering efter et
 * hus der ikke bruger stroem.
 *
 * Derfor de her: ÉT gennemsnit per kvarter, altsaa 96 om dagen i stedet
 * for 43.200. Et kvarter er ogsaa den takt OpenRemotes optimering regner
 * i, saa de faar praecis den oploesning de bruger.
 *
 * Det er DE HER felter serveren fordeler videre til energi-assets, og det
 * er dér historikken ligger. Live-tallene fordeles ikke.
 *
 * ZS_FLEET_KVARTER_FELT er hvilket kvarter gennemsnittet daekker, 0 til
 * 95. Den sendes med af samme grund som timen paa energien: tidsstemplet
 * paa et datapunkt er hvornaar beskeden kom frem, ikke hvad den daekker,
 * og en besked der blev forsinket af en genopkobling ville ellers ligge i
 * det forkerte kvarter uden at nogen kunne se det.
 */
#define ZS_FLEET_SOL_KV_FELT      "solarPowerQuarter"
#define ZS_FLEET_FORBRUG_KV_FELT  "housePowerQuarter"
#define ZS_FLEET_BATTERI_KV_FELT  "batteryPowerQuarter"
#define ZS_FLEET_NET_KV_FELT      "gridPowerQuarter"
#define ZS_FLEET_SOC_KV_FELT      "batteryLevelQuarter"
#define ZS_FLEET_KVARTER_FELT     "quarterOfDay"

/*
 * Tilmeldingens engangskode. Fase B.
 *
 * Skaermen skriver den HER, og serveren laeser den naar Frekvenz melder
 * en kunde ind med den. Feltet er ejet af zScreen som alle de andre, saa
 * ingen anden end skaermen kan skrive en kode ind.
 *
 * Det er et TEKSTfelt, og en TOM streng betyder "koden gaelder ikke
 * laengere". Det er ikke det samme som at feltet aldrig har haft en
 * vaerdi: skaermen rydder det udtrykkeligt naar den holder op med at vise
 * koden, og det er netop den regel der goer at en kode man har
 * fotograferet, ikke kan bruges bagefter.
 *
 * Og feltet GEMMES IKKE som historik. En liste over alle koder en skaerm
 * har vist, er en liste man ikke vil have liggende.
 */
#define ZS_FLEET_TILMELD_FELT     "enrollCode"

/*
 * Under den her graense kalder vi det nul.
 *
 * Et anlaeg staar aldrig helt stille. Maaleren svinger nogle faa watt
 * frem og tilbage, og uden en graense skifter tallet paa vaeggen hvert
 * andet sekund mellem 12, 8 og 15 watt. Et tal der aldrig staar stille
 * traekker oejet til sig hele tiden, og man kan ikke se paa det om der
 * sker noget vaesentligt.
 *
 * VIGTIGT: den samme graense skal bruges baade til TALLET og til
 * RETNINGEN. Rundede vi kun tallet, ville der komme til at staa
 * "0 W" og "saelger" ved siden af hinanden, og det ser ud som en fejl.
 * Derfor staar den her ét sted, og baade zs_fmt_power og alle
 * pile og ord henter den samme.
 *
 * Halvtreds watt er valgt fordi det er under en enkelt paere i et
 * moderne hus, altsaa under det man overhovedet kan maerke.
 */
#define ZS_DEADBAND_W           50.0f

#define ZS_BRIGHTNESS_DEFAULT   80

/*
 * Hoejeste gyldige tema-nummer i lageret.
 *
 * Staar her og ikke i zs_theme.h, fordi lageret ikke maa afhaenge af
 * brugerfladen: zs_nvs.c skal kunne oversaettes uden LVGL. Der er en
 * test der tjekker at tallet passer med ZS_THEME_COUNT, saa de to ikke
 * kan komme ud af trit naar der tilfoejes et tema.
 */
#define ZS_THEME_MAKS           2

/*
 * Selvtest af sideombygning og temaskift.
 *
 * 0 i alt der sendes ud. Saettes til 1 naar der er roert ved en skaerm
 * eller ved temaet, saa man kan se paa enheden at intet siver og at
 * alle sider stadig kan tegnes. Se zs_selftest.c.
 */
#ifndef ZS_SELFTEST
#define ZS_SELFTEST             0
#endif
#define ZS_BRIGHTNESS_NIGHT     25
#define ZS_NIGHT_START_HOUR     22
#define ZS_NIGHT_END_HOUR       6

/* ── Baglyset naar der er en fejl ──────────────────────────────────── */
/*
 * HELE SKAERMEN PULSERER I LYSSTYRKE naar der er en fejl.
 *
 * Grunden er at skaermen haenger paa en vaeg og bliver set paa afstand,
 * og at den der skal se beskeden, maaske staar med ryggen til. En roed
 * overskrift kan ikke laeses fra den anden ende af et koekken, men en
 * flade der skifter lysstyrke kan ses i oejenkrogen. Det er samme
 * virkning som et blink i et vindue.
 *
 * Den virker paa HVER enhed der staar ude i dag, for baglyset sidder
 * paa ESP32'ens eget ben. Lyden gOEr ikke - se ZS_LYD_ENABLED.
 *
 * Takten er den SAMME som overskriftens puls paa fejlsiden, saa lys og
 * tekst aander sammen. To takter der ligger taet uden at vaere ens, ser
 * ud som en fejl i skaermen.
 *
 * 35 procent er hvor langt NED den dykker, regnet af den lysstyrke
 * kunden selv har valgt - ikke et fast tal. Staar skaermen paa 20 om
 * aftenen, pulserer den mellem 20 og 7, og ikke op til 100.
 *
 * Takten saettes af LVGL's animation og ikke af en timer her. Der stod
 * ogsaa et ZS_ALARM_STEP_MS paa 40 ms, fra dengang baglyset havde sin
 * egen esp_timer. Den er vaek, og tallet var dermed et man kunne rette
 * uden at der skete noget - den slags er vaerre end intet tal.
 */
#define ZS_ALARM_PERIOD_MS      1400
#define ZS_ALARM_DIM_PCT        35

/* ── Opdatering ───────────────────────────────────────────────────── */
/*
 * Hvor tit skaermen ser efter ny firmware.
 *
 * En halv time. Det giver to forespoergsler i timen mod GitHub, som
 * tillader tres i timen uden noegle, saa der er rigelig luft. Ved
 * opstart tjekkes der efter et halvt minut, saa wifi og ur naar at
 * komme op foerst.
 */
#define ZS_OTA_CHECK_INTERVAL_MS   (30 * 60 * 1000)

/*
 * Hvor laenge en ny firmware skal koere uden at gaa ned foer vi melder
 * den i orden.
 *
 * Indtil da staar den som "paa proeve", og gaar skaermen ned inden,
 * ruller opstarten selv tilbage til den gamle udgave. En skaerm paa en
 * vaeg har ingen til at trykke reset.
 *
 * To minutter er valgt fordi det daekker hele opstarten med god margen:
 * wifi, ur, foerste aflaesning fra inverteren og foerste tegning af
 * skaermen er maalt til under tyve sekunder tilsammen. Laengere ville
 * blot udskyde at vi tager den nye i brug.
 *
 * Og vaer klar over hvad den IKKE daekker: en udgave der starter fint og
 * foerst er ubrugelig bagefter, fx sort skaerm eller intet wifi,
 * overlever de to minutter og bliver godkendt. Mod den slags hjaelper
 * kun at rulle ud til én skaerm foerst, se ZS_FLEET_TARGET_FELT.
 */
#define ZS_OTA_OK_AFTER_MS         (120 * 1000)

/* ── Hjælp ────────────────────────────────────────────────────────── */
/*
 * Hvem kunden skal ringe til.
 *
 * Staar ét sted, fordi det er den slags der bliver aendret en dag og
 * saa skal findes igen. Bruges naar inverteren melder en fejlkode vi
 * ikke kan saette ord paa: saa er den eneste rigtige besked at
 * fortaelle hvem der kan.
 */
#define ZS_SUPPORT_NAME   "ZOL Energi"
#define ZS_SUPPORT_PHONE  "+45 7060 3676"

/* ── Demo ─────────────────────────────────────────────────────────── */
/*
 * Demo-tilstand: hovedskaermen med opdigtede tal, saa man kan se
 * hvordan skaermen opfoerer sig uden et anlaeg i naerheden.
 *
 * Saettes den til 0, findes hverken knappen "Se demo" eller koden bag
 * den i den byggede fil. Det er hovedafbryderen til produktionsenheder.
 *
 * Uanset hvad gemmes demo ALDRIG. En genstart slaar den altid fra, saa
 * en enhed hos en kunde kan ikke starte op i demo.
 *
 * SLAAET FRA 13. oktober 2026, besluttet med Yassin.
 *
 * Knappen "Se demo" og linjen "Se hvordan skaermen ser ud, uden et
 * anlaeg" stod paa FOERSTE side, altsaa det foerste en kunde ser naar
 * skaermen taendes. To knapper dér, hvor den ene fOErer til opdigtede
 * tal, er et valg kunden ikke skal traeffe: han har et anlaeg, og han
 * skal se sit eget.
 *
 * Afbryderen bliver brugt i stedet for at koden slettes. Den er traakket
 * igennem hele firmwaren - zs_demo.c ligger helt inde i den, og det
 * samme gOEr raekken i Indstillinger, maerket i statuslinjen og
 * kommandoen i appen - saa nul fjerner baade knappen OG koden bag den
 * fra den byggede fil. Og skal demoen med paa en messe en dag, er det
 * ét ciffer og ikke en genskrivning.
 */
#define ZS_DEMO_ENABLED   0

#endif /* ZS_CONFIG_H */
