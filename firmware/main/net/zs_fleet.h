/*
 * zScreen - forbindelsen til Frekvenz' egen floedestyring.
 *
 * Skaermen melder sig selv ind paa vores OpenRemote-server med et
 * certifikat, og sender derefter sine maalinger. Ingen manuel
 * oprettelse per kunde, og intet kodeord gemt i firmwaren.
 *
 * DET VIGTIGSTE: skaermen skal virke praecis lige saa godt uden
 * serveren. Alt heri er frivilligt. Er der ingen forbindelse, er
 * serveren nede, eller mangler certifikatet, viser skaermen stadig
 * anlaegget som den altid har gjort. Floedestyringen er en ekstra, og
 * den maa ikke kunne tage skaermen med sig ned.
 *
 * Forloebet, maalt mod en koerende OpenRemote 1.31 og ikke laest i en
 * manual. Deres dokumentation har ikke emnerne, de er hentet fra
 * UserAssetProvisioningMQTTHandler.java:
 *
 *   1. forbind over TLS, UDEN brugernavn og kodeord
 *   2. lyt paa   provisioning/{id}/response
 *   3. send      provisioning/{id}/request   {"type":"x509","cert":"..."}
 *   4. faa       {"type":"success","realm":"...","asset":{"id":"..."}}
 *      hvorefter SAMME forbindelse er godkendt som servicebrugeren
 *   5. skriv     {realm}/{id}/writeattributevalue/{felt}/{enhedsid}
 *
 * Fire ting kostede en halv dag at finde, og de staar her saa de ikke
 * skal findes igen:
 *
 *   MQTT-klientnavnet skal vaere det rene id. Saetter man ps-{id},
 *   altsaa det navn servicebrugeren faar, lukker serveren forbindelsen
 *   foer indmeldelsen er behandlet: "connection is now closed".
 *
 *   Der maa ikke skrives i samme oejeblik svaret kommer. Serveren skal
 *   have et oejeblik til at opgradere forbindelsen. Se
 *   ZS_FLEET_READY_DELAY_MS.
 *
 *   Felterne paa serveren skal vaere markeret accessRestrictedWrite,
 *   ellers nAEgtes hver skrivning med "does not have permission SEND".
 *   Det er serverens side, men det er den samme fejl.
 *
 *   Mister vi forbindelsen, skal vi melde ind FORFRA. Godkendelsen
 *   hAEnger paa forbindelsen, ikke paa enheden.
 *
 * Og en femte, fundet ved at koere tres enheder mod en rigtig server:
 *
 *   Indmeldelsen skal spredes. Melder tres enheder sig ind inden for
 *   fem sekunder, loeber serverens Keycloak-kald tOErt, den lukker
 *   forbindelserne, enhederne melder ind igen, og stormen foeder sig
 *   selv. Maalt: 49 tidsudloeb og 109 tvungne afbrydelser mod nul naar
 *   det samme sker over femten sekunder. Derfor venter skaermen et
 *   tilfaeldigt stykke tid foer den foerste indmeldelse, se
 *   ZS_FLEET_START_SPREAD_MS. Det betyder at en skaerm kan vaere op til
 *   et minut om at vise sig i flaadeoversigten efter en genstart.
 *   Tallene paa vaeggen kommer med det samme uanset hvad.
 *
 * Og en sjette, som koster en halv time hvis man ikke ved den:
 *
 *   Ved ALLERFOERSTE indmeldelse af en ny skaerm lukker serveren
 *   forbindelsen et sekund efter at den har sagt success. I loggen staar
 *   "User asset links have changed for a connected user with active
 *   subscriptions": enheden er lige blevet knyttet til en ny asset, og
 *   serveren kaster abonnementerne vaek for at regne rettighederne ud
 *   paa ny. Vi melder ind igen af os selv, og anden gang bliver den
 *   staaende. Derfor skal ALT der haenger paa forbindelsen, ogsaa
 *   abonnementet paa maalversionen, saettes op inde i indmeldelsen og
 *   ikke én gang ved opstart.
 */

#ifndef ZS_FLEET_H
#define ZS_FLEET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zs_fronius.h"
#include "zs_kvarter.h"
#include "zs_energi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hvor langt vi er naaet. Vises paa Detaljer-siden, saa en tekniker
 * kan se hvorfor der ikke kommer data ind. */
typedef enum {
    ZS_FLEET_OFF = 0,        /* slaaet fra, eller intet certifikat   */
    ZS_FLEET_CONNECTING,     /* paa vej op                           */
    ZS_FLEET_ENROLLING,      /* sendt certifikat, venter paa svar    */
    ZS_FLEET_READY,          /* indmeldt, sender maalinger           */
    ZS_FLEET_REJECTED,       /* serveren afviste vores certifikat    */
    ZS_FLEET_ERROR,          /* netvaerksfejl, proever igen          */
} zs_fleet_state_t;

/*
 * Starter klienten. Blokerer ikke: esp-mqtt koerer i sin egen opgave,
 * og alt herunder er haendelsesdrevet.
 *
 * Returnerer false hvis der ikke er noget at starte, altsaa hvis
 * floedestyringen er slaaet fra eller der ikke er et certifikat paa
 * enheden. Det er ikke en fejl, og kalderen skal ikke goere andet end
 * at komme videre.
 */
bool zs_fleet_start(void);

/* Lukker ned. Sikkert at kalde ogsaa hvis den aldrig blev startet. */
void zs_fleet_stop(void);

/*
 * Melder ind NU i stedet for at vente resten af spredningen af.
 *
 * Kaldes naar et menneske staar og venter paa svar fra serveren - i dag
 * er det tilmeldingssiden. Uden virkning hvis indmeldelsen allerede er i
 * gang. Se noten ved funktionen om hvorfor det ikke bryder spredningen.
 */
void zs_fleet_skynd_paa_indmeldelsen(void);

/*
 * Sender én runde maalinger, hvis vi er klar.
 *
 * Kaldes fra hovedopgaven i samme takt som inverteren laeses, styret af
 * ZS_FLEET_PUBLISH_EVERY_N_POLLS. Er vi ikke indmeldt endnu, gOEr den
 * ingenting og siger ikke fra: det er det normale billede de foerste
 * sekunder efter opstart og hver gang nettet har vaeret vaek.
 *
 * info maa vaere NULL. Er den sat, sendes anlaeggets oplysninger med,
 * og det goer vi kun én gang per indmeldelse.
 */
void zs_fleet_publish(const zs_fr_live_t *live, const zs_fr_info_t *info);

/*
 * Sender timens energi. Kaldes ÉN gang i timen, ikke ved hver
 * aflaesning.
 *
 *   wh       de fem vaerdier i wattimer, se zs_energi.h
 *   har      for hver: kunne den regnes. Dem der ikke kunne, sendes IKKE.
 *   time     hvilken time de daekker, 0 til 23. Sendes med som et felt,
 *            saa man paa serveren kan se hvilken time en vaerdi hoerer
 *            til, ogsaa hvis den kommer et oejeblik forsinket.
 *
 * VI SENDER IKKE ET FELT VI IKKE HAR. Et nul for "koebt fra nettet" paa
 * et anlaeg uden elmaaler ville se ud som et anlaeg der aldrig koeber
 * stroem, og det er en loegn. Mangler feltet, staar der ingenting paa
 * serveren, og det er til at gennemskue.
 */
void zs_fleet_publish_energi(const float *wh, const bool *har, int8_t time);

/*
 * Fortaeller serveren om inverteren svarer. Kaldes ved hver aflaesning,
 * og sender KUN naar det har aendret sig.
 *
 * Beslutningen om hvornaar der skal sendes ligger i
 * zs_fleet_inv_skal_sende i zs_fleet_msg.h, saa den kan proeves af paa en
 * almindelig maskine. Her staar kun afsendelsen.
 */
void zs_fleet_publish_inverter(bool online);

/*
 * Sender ét kvarters gennemsnit. Kaldes naar zs_kvarter_tik siger der er
 * et, altsaa 96 gange i doegnet.
 *
 * Et felt der ikke har noget, sendes IKKE. Et nul for "koebt fra nettet"
 * paa et anlaeg uden elmaaler ville se ud som et anlaeg der aldrig koeber
 * stroem, og det er en loegn. Samme regel som timeenergien.
 */
void zs_fleet_publish_kvarter(const zs_kvarter_ud_t *ud);

/*
 * Tilmeldingens engangskode, eller en TOM streng for at rydde den.
 *
 * En tom streng er ikke "ingenting at sende". Den er beskeden om at
 * koden ikke gaelder laengere, og den SKAL frem: ellers bliver koden ved
 * med at virke paa serveren efter skaermen er holdt op med at vise den.
 *
 * Beslutningen om hvornaar der sendes, ligger i zs_tilmeld_skal_sende i
 * zs_tilmeld.h, saa den kan proeves af paa en almindelig maskine.
 */
void zs_fleet_publish_tilmeld(const char *kode);

/* Hvor langt vi er. */
zs_fleet_state_t zs_fleet_state(void);

/* Tilstanden som dansk tekst til Detaljer-siden. */
const char *zs_fleet_state_text(void);

/*
 * Enhedens id paa serveren, kopieret ind i ud.
 *
 * En KOPI og ikke en pegepind: MQTT-opgaven rydder id'et naar
 * forbindelsen gaar tabt, og en laeser der holdt en pegepind kunne
 * ende med at vise en halvt ryddet streng. Kopien tages under laas.
 *
 * Skriver en tom streng hvis vi ikke er indmeldt.
 */
void zs_fleet_asset_id(char *ud, size_t ud_len);

/*
 * Skaermens unikke navn, udledt af chippens egen MAC-adresse.
 *
 * Den staar i eFuse, kan ikke aendres, og er forskellig paa hver
 * enhed. Derfor er den det rigtige id: den kan ikke komme til at vaere
 * ens paa to skaerme, og den foelger hardwaren og ikke firmwaren.
 */
/*
 * Serverens prisomraade, hvis der er kommet et nyt siden sidst.
 *
 * Returnerer true ÉN gang per aendring og skriver tre tegn plus nul i
 * ud. Saa kan app'en gemme det og hente priser forfra, uden at det sker
 * ved hvert tik.
 *
 * Kun DK1 og DK2 slipper igennem. Se laes_zone og ZS_FLEET_ZONE_FELT:
 * en zone skaermen ikke kender, ville gaa med ud i prisadressen og give
 * et svar ingen kan bruge, saa den afvises og den gamle bliver staaende.
 */
bool zs_fleet_zone_ny(char *ud, size_t ud_len);

const char *zs_fleet_unique_id(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLEET_H */
