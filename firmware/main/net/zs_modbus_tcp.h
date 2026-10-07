/*
 * zScreen - Modbus TCP klient. KUN LAESNING.
 *
 * ============================================================
 *  SIKKERHED: her findes kun funktionskode 3.
 *
 *  Modbus har fem skrive-funktionskoder (5, 6, 15, 16, 22).
 *  Ingen af dem er implementeret i denne fil, og de maa aldrig
 *  blive det. zScreen er en skaerm, ikke en styring. Naar der
 *  ikke findes skrive-kode i binaeren, kan en fejl et helt
 *  andet sted i programmet heller ikke komme til at aendre
 *  noget paa kundens inverter.
 *
 *  Skal der en dag styres fra en Frekvenz-enhed, sker det fra
 *  Zbox'en, som er bygget til det og har sikkerhedslaasene.
 * ============================================================
 *
 * Hvorfor egen klient og ikke esp-modbus:
 *   - esp-modbus binder unit-ID til en post i en IP-liste. Vi skal
 *     tale med unit 1 (inverter) og unit 200/201/240/241 (elmaaler)
 *     paa praecis samme IP, og det passer daarligt i deres model.
 *   - SunSpec-adresser kendes foerst efter en model-vandring ved
 *     runtime. esp-modbus er bygget til statiske registertabeller.
 *   - Vi vil have korte timeouts under netvaerksscanning (300 ms) og
 *     normale under drift (1500 ms), skiftende pr. kald.
 *   - Read-only-garantien ovenfor.
 *
 * Koden bruger almindelige BSD-sockets. Det virker baade paa lwIP
 * (ESP-IDF) og paa macOS, saa den kan testes mod simulatoren uden
 * at have hardware fremme.
 */

#ifndef ZS_MODBUS_TCP_H
#define ZS_MODBUS_TCP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modbus FC3 kan hoejst laese 125 registre i én forespoergsel.
 * Graensen kommer af at svarets byte-taeller er ét byte: 125 * 2 = 250. */
#define ZS_MB_MAX_REGS          125

/* MBAP-header (7) + funktionskode (1) + byte-taeller (1) = 9 */
#define ZS_MB_HEADER_LEN        9
#define ZS_MB_MAX_FRAME         (ZS_MB_HEADER_LEN + ZS_MB_MAX_REGS * 2)

/* Standard Modbus TCP-port. Fronius kan ogsaa saettes til 1502. */
#define ZS_MB_DEFAULT_PORT      502

/*
 * Standard unit-id, altsaa hvilken enhed paa den anden ende vi taler med.
 *
 * Fronius leverer inverteren paa 1. En elmaaler der sidder for sig selv
 * kan have sit eget nummer, og det kan kunden saette i indstillingerne.
 * Nul er ikke en lovlig vaerdi i Modbus TCP, saa den bruges som
 * "ikke valgt".
 */
#define ZS_MB_DEFAULT_UNIT      1

typedef enum {
    ZS_MB_OK = 0,
    ZS_MB_ERR_ARG,          /* ugyldige argumenter fra kalderen        */
    ZS_MB_ERR_NOT_OPEN,     /* der er ingen forbindelse                */
    ZS_MB_ERR_CONNECT,      /* kunne ikke forbinde                     */
    ZS_MB_ERR_SEND,         /* skrivning paa socket fejlede            */
    ZS_MB_ERR_TIMEOUT,      /* intet svar inden for timeout            */
    ZS_MB_ERR_CLOSED,       /* modparten lukkede forbindelsen          */
    ZS_MB_ERR_FRAME,        /* svaret var ikke et gyldigt Modbus-svar  */
    ZS_MB_ERR_EXCEPTION,    /* serveren svarede med en exception-kode  */
    /*
     * Skrivningen kom frem, svaret var paent, og registret staar
     * ALLIGEVEL med den gamle vaerdi.
     *
     * Den har sin egen kode fordi den betyder noget helt andet end en
     * daarlig ramme: forbindelsen er sund, inverteren forstod os, og den
     * valgte at lade vaere. Fronius' manual siger det direkte: "If an
     * attempt is made to write to such registers, the inverter does not
     * return an exception code!" Det sker naar "Inverter control via
     * Modbus" ikke er slaaet til paa inverterens webside, eller naar en
     * anden styring har forrang.
     *
     * Uden den her kode ville vi melde "ugyldigt svar" og lede efter
     * fejlen i netvaerket i stedet for paa inverterens indstillingsside.
     */
    ZS_MB_ERR_NOT_WRITTEN,
} zs_mb_err_t;

typedef struct {
    int      fd;                 /* socket, -1 naar lukket                    */
    char     host[46];           /* IPv4 eller IPv6 som tekst                 */
    uint16_t port;
    uint16_t next_tid;           /* transaktions-ID, taeller op pr. kald      */
    uint32_t timeout_ms;         /* svartimeout for det naeste kald           */
    uint8_t  last_exception;     /* Modbus exception-kode ved ZS_MB_ERR_EXCEPTION */
    /*
     * HVORFOR vi lukkede sidst.
     *
     * Vi lukker soklen ved ENHVER fejl, og det er med vilje: en
     * Modbus TCP-forbindelse hvor et svar udeblev er i ukendt tilstand,
     * for svaret kan komme bagefter og forskyde alt det naeste.
     *
     * Men saa ser det udefra ud som om nettet faldt ud, ogsaa naar
     * sandheden er at inverteren holdt op med at svare. De to sender
     * én ud at lede helt forskellige steder: den ene efter et kabel
     * eller en switch, den anden efter en inverter der har travlt.
     * Derfor huskes grunden her.
     */
    zs_mb_err_t last_error;
    uint32_t stat_requests;      /* taellere til fejlsoegningssiden           */
    uint32_t stat_errors;
} zs_mb_t;

/* Nulstiller struct'en. Skal kaldes foer alt andet. */
void zs_mb_init(zs_mb_t *mb);

/*
 * Aabner en TCP-forbindelse.
 *
 * connect_timeout_ms gaelder kun selve opkoblingen. Selve laesningerne
 * bruger den timeout der gives med til zs_mb_read_holding().
 *
 * Er der allerede en aaben forbindelse, lukkes den foerst. Det er med
 * vilje: en halvdoed socket skal ikke kunne overleve et reconnect-forsoeg.
 */
zs_mb_err_t zs_mb_connect(zs_mb_t *mb, const char *host, uint16_t port,
                          uint32_t connect_timeout_ms);

/* Lukker forbindelsen. Sikker at kalde paa en allerede lukket klient. */
void zs_mb_close(zs_mb_t *mb);

bool zs_mb_is_open(const zs_mb_t *mb);

/*
 * Laeser holding-registre (funktionskode 3).
 *
 *   unit_id   Modbus-enhedsadresse. Fronius: 1 = inverter,
 *             200/201/240/241 = elmaaler.
 *   address   Foerste registeradresse, 0-baseret paa ledningen.
 *             SunSpec-dokumentation taeller fra 40001, saa
 *             "register 40072" er address 40071. Vi bruger
 *             gennemgaaende 0-baserede adresser i denne kode.
 *   count     Antal registre, 1 til 125.
 *   out       Buffer med plads til mindst count uint16.
 *             Vaerdierne er allerede vendt fra Modbus' big-endian
 *             til vaertens egen byte-orden.
 *
 * Ved enhver protokolfejl (forkert laengde, forkert transaktions-ID,
 * exception, timeout) lukkes forbindelsen. Det er bevidst: en
 * desynkroniseret Modbus-stroem giver forkerte tal, og forkerte tal
 * paa en energiskaerm er vaerre end ingen tal. Kalderen forbinder igen.
 */
zs_mb_err_t zs_mb_read_holding(zs_mb_t *mb, uint8_t unit_id, uint16_t address,
                               uint16_t count, uint16_t *out, uint32_t timeout_ms);

/*
 * Skriver registre og LAESER TILBAGE for at se at det blev til noget.
 *
 * Det er ikke baelte og seler, det er den eneste maade at vide det paa.
 * Fronius svarer uden fejl ogsaa naar en skrivning bliver afvist, fx
 * fordi "Inverter control via Modbus" ikke er slaaet til paa inverterens
 * webside, eller fordi en anden styring har forrang. Uden tilbagelaesning
 * ville skaermen melde "sendt" mens inverteren gjorde som den ville.
 *
 * Returnerer ZS_MB_OK kun hvis alle registre staar med det vi skrev.
 */
zs_mb_err_t zs_mb_write_verified(zs_mb_t *mb, uint8_t unit_id, uint16_t address,
                                 const uint16_t *values, uint16_t count,
                                 uint32_t timeout_ms);

/* Tekst til logning og til Detaljer-siden. Altid en gyldig streng. */
const char *zs_mb_strerror(zs_mb_err_t err);

/*
 * Tjekker om der lytter noget paa host:port, uden at sende Modbus.
 * Bruges i netvaerksscanningen hvor vi proever 254 adresser og skal
 * kunne give op hurtigt paa dem der ikke svarer.
 */
bool zs_mb_probe_port(const char *host, uint16_t port, uint32_t timeout_ms);

/* ---------------------------------------------------------------------
 * Ren ramme-haandtering, uden sockets. Ligger i headeren fordi
 * enhedstestene banker direkte paa den med forvanskede pakker.
 * --------------------------------------------------------------------- */

/* Bygger en FC3-forespoergsel i buf. Returnerer antal bytes, eller 0
 * ved ugyldige argumenter. buf skal have plads til 12 bytes. */
size_t zs_mb_build_read_request(uint8_t *buf, size_t buf_len, uint16_t tid,
                                uint8_t unit_id, uint16_t address, uint16_t count);

/*
 * Bygger en SKRIVNING, funktionskode 16.
 *
 * Hvorfor 16 og ikke 6, selv til ét register: 16 skriver flere registre i
 * ÉN ramme, og flere af SunSpecs styrefelter skal saettes sammen for at
 * give mening. Saetter man dem ét ad gangen, staar inverteren i en halv
 * tilstand imellem. Og med kun én kode at vedligeholde er der kun ét sted
 * der kan vaere forkert.
 *
 * Returnerer antal bytes, eller 0 hvis noget ikke kan lade sig goere.
 */
size_t zs_mb_build_write_request(uint8_t *buf, size_t buf_len, uint16_t tid,
                                 uint8_t unit_id, uint16_t address,
                                 const uint16_t *values, uint16_t count);

/*
 * Laeser svaret paa en skrivning.
 *
 * Svaret er et EKKO af adresse og antal, ikke data. Passer ekkoet ikke med
 * det vi bad om, har inverteren skrevet et andet sted, og saa maa vi ikke
 * tro paa at det gik godt.
 *
 * VIGTIGT, og det staar i Fronius' egen manual: "If an attempt is made to
 * write to such registers, the inverter does not return an exception
 * code!" En skrivning der bliver ignoreret ser altsaa praecis ud som en
 * der lykkedes. Derfor er et godt svar her IKKE bevis for noget. Man SKAL
 * laese tilbage bagefter, se zs_mb_write_verified.
 */
zs_mb_err_t zs_mb_parse_write_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t expect_tid, uint8_t expect_unit,
                                       uint16_t expect_address,
                                       uint16_t expect_count,
                                       uint8_t *out_exception);

/*
 * Tjekker og pakker et FC3-svar ud.
 *
 * frame/frame_len er hele svaret inklusive MBAP-header.
 * expect_tid/expect_unit/expect_count er hvad vi bad om.
 * out modtager de udpakkede registre.
 * out_exception faar exception-koden hvis der returneres ZS_MB_ERR_EXCEPTION.
 */
zs_mb_err_t zs_mb_parse_read_response(const uint8_t *frame, size_t frame_len,
                                      uint16_t expect_tid, uint8_t expect_unit,
                                      uint16_t expect_count, uint16_t *out,
                                      uint8_t *out_exception);

#ifdef __cplusplus
}
#endif

#endif /* ZS_MODBUS_TCP_H */
