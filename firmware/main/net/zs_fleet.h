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
const char *zs_fleet_unique_id(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_FLEET_H */
