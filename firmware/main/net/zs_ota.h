/*
 * zScreen - opdatering over netvaerket.
 *
 * Skaermen henter selv ny firmware fra GitHub. Den ser efter ved hver
 * opstart og hver halve time, og installerer med det samme naar der er
 * noget nyt.
 *
 * Hvorfor det er sikkert:
 *
 *   Firmwaren er UNDERSKREVET med Frekvenz' egen noegle. Den koerende
 *   udgave baerer den offentlige noegle, og esp_ota_ops naegter at tage
 *   en opdatering i brug der ikke er underskrevet med den samme.
 *   En angriber der kan omdirigere skaermens trafik, eller endda
 *   overtage GitHub-kontoen, kan altsaa ikke skubbe sin egen firmware
 *   ind uden ogsaa at have noeglen.
 *
 *   Der er TO app-pladser i flashen. Den nye skrives i den ledige, og
 *   foerst naar den er hentet helt og signaturen er godkendt, peger
 *   opstarten paa den. Gaar stroemmen midt i en hentning, starter den
 *   gamle op som om intet var haendt.
 *
 *   Starter den nye op og gaar ned foer den naar at melde sig i orden,
 *   ruller opstarten selv tilbage til den gamle. En skaerm paa en vaeg
 *   har ingen til at trykke reset, saa den maa aldrig kunne blive
 *   ubrugelig af en daarlig opdatering.
 *
 * Der sendes INTET om enheden. Vi henter en fil, og det er alt: intet
 * serienummer, ingen maaledata, ingen tilbagemelding.
 */

#ifndef ZS_OTA_H
#define ZS_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hvor firmwaren hentes fra. Repoet er offentligt, saa der skal ingen
 * noegle eller adgangskode ligge i enheden. */
#define ZS_OTA_OWNER   "Frekvenz-Danmark"
#define ZS_OTA_REPO    "zscreen"

/*
 * Den version skaermen SKAL koere. Tom streng betyder "foelg nyeste",
 * som er standard og det alle skaerme goer indtil nogen siger andet.
 *
 * HVORFOR DEN FINDES.
 *
 * Uden den tager hver eneste skaerm den nyeste udgivelse inden for en
 * halv time. En udgave der starter fint, koerer sine to minutter og
 * FOERST derefter er ubrugelig, fx sort skaerm eller intet wifi, rammer
 * dermed hele flaaden, og tilbagerulningen redder os ikke, for den naaede
 * at blive godkendt. Med en maalversion kan vi rulle ud til én skaerm,
 * se at den stadig lever, og saa resten.
 *
 * Den maa gaa BEGGE veje. Uden maal opdaterer vi kun opad, saa en
 * udgivelse med et forkert nummer ikke kan sende flaaden baglaens og
 * frem og tilbage for evigt. MED et maal er der ingen ring: naar den
 * koerende udgave er lig maalet, sker der ikke mere. Derfor er en
 * bevidst nedgradering kun mulig her, og det er netop det der goer den
 * til en noedbremse.
 *
 * Teksten kommer udefra, fra serveren, og ender i en URL. Den bliver
 * derfor efterset tegn for tegn: kun cifre og punktummer, med et
 * valgfrit v foran, og en laengde der passer i feltet. Alt andet
 * forkastes og vi bliver ved med at foelge nyeste.
 *
 * Traad: kaldes fra MQTT-opgaven, laeses af netvaerksopgaven. Der er en
 * laas om de faa bytes.
 */
void zs_ota_set_target(const char *version);

/* Hvad maalet er lige nu. Kopieres ind i ud. Tom streng = foelg nyeste. */
void zs_ota_get_target(char *ud, size_t ud_len);


typedef enum {
    ZS_OTA_IDLE = 0,
    ZS_OTA_CHECKING,      /* spoerger GitHub                    */
    ZS_OTA_DOWNLOADING,   /* henter og skriver til flashen      */
    ZS_OTA_UP_TO_DATE,    /* vi koerer allerede den nyeste      */
    ZS_OTA_READY,         /* hentet og godkendt, klar til genstart */
    ZS_OTA_FAILED,
} zs_ota_state_t;

typedef struct {
    zs_ota_state_t state;
    char    nyeste[24];      /* versionen paa GitHub, tom hvis ukendt */
    char    koerende[24];    /* den vi koerer nu                      */
    uint8_t procent;         /* 0 til 100 under hentning              */
    char    fejl[80];        /* dansk tekst naar state er FAILED      */
    bool    har_tjekket;     /* har vi spurgt mindst én gang          */
} zs_ota_status_t;

/*
 * Ser efter en opdatering og installerer den hvis der er en.
 *
 * Blokerer i op til et par minutter, saa den skal koeres fra
 * netvaerksopgaven og ikke fra skaermens.
 *
 * Returnerer true hvis der er installeret noget nyt. Saa skal enheden
 * genstartes, og det goer kalderen.
 */
bool zs_ota_check_and_install(zs_ota_status_t *ud);

/*
 * Melder den koerende firmware i orden, saa opstarten ikke ruller
 * tilbage.
 *
 * Kaldes foerst naar skaermen har koert et stykke tid uden at gaa ned.
 * Kaldes den med det samme, mister man hele vaernet: en firmware der
 * gaar ned efter tredive sekunder ville blive godkendt paa de foerste
 * to.
 */
void zs_ota_mark_ok(void);

/* Er den koerende firmware en netop installeret opdatering der endnu
 * ikke er meldt i orden? */
bool zs_ota_pending_verify(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_OTA_H */
