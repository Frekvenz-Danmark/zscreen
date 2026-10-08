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

/*
 * Hvad en fejl fra send() eller recv() betyder.
 *
 * Vurderingen stod skrevet forfra to steder, i send_all og i recv_exact,
 * og den kunne ikke proeves af fordi den sad midt i en socket-loekke. En
 * mutationstest viste hvad det kostede: "errno == EAGAIN" kunne laves om
 * til "errno != EAGAIN" uden at en eneste proeve sagde fra. Med den
 * aendring ville ALLE fejl, ogsaa en afbrudt forbindelse, blive meldt som
 * "svarede ikke i tide". Vi har én gang foer brugt en halv dag paa en
 * misvisende fejlbesked, saa det skal der vaere en proeve paa.
 *
 * EAGAIN og EWOULDBLOCK er samme tal paa baade macOS og Linux, men det
 * er ikke givet af standarden, saa begge staar der.
 */
typedef enum {
    ZS_SOK_IGEN = 0,    /* afbrudt af et signal, proev igen med det samme */
    ZS_SOK_TIMEOUT,     /* der kom ingenting inden tiden loeb ud          */
    ZS_SOK_DOED         /* forbindelsen duer ikke mere                    */
} zs_sok_t;

zs_sok_t zs_mb_errno_art(int fejlnr);

/*
 * Hvad select() sagde, da vi ventede paa at forbindelsen kom op.
 *
 * Vurderingen stod som "if (rc <= 0)" midt i connect_with_timeout og kunne
 * derfor ikke proeves. En mutationstest aendrede den til "rc < 0", og ikke
 * én proeve sagde fra. Med den aendring bliver en TIMEOUT regnet som en
 * forbindelse der kom op: nul betyder at tiden loeb ud, og saa ville vi
 * fortsaette som om vi var forbundet. Paa en scanning af 254 adresser
 * ville vi melde at der sad en inverter paa dem alle.
 */
typedef enum {
    ZS_CON_KLAR = 0,    /* soklen er skrivbar, forbindelsen kan proeves */
    ZS_CON_TIMEOUT,     /* nul: tiden loeb ud                          */
    ZS_CON_FEJL         /* under nul: select selv gik galt              */
} zs_con_t;

zs_con_t zs_mb_select_art(int rc);

/*
 * Hvad send() sagde, da vi proevede at sende.
 *
 * Beslutningen stod midt i send_all, og den kan ikke proeves derfra: der
 * skal en rigtig sokkel til, i en bestemt tilstand. En mutationstest
 * aendrede "if (n == 0)" til "if (n != 0)" uden at nogen proeve sagde fra.
 *
 * NUL er det usaedvanlige og det farlige. send() tog ingenting, selv om vi
 * bad om mere end nul. Saa er errno IKKE sat af dette kald, og vi maa ikke
 * se paa det: stod der tilfaeldigvis EINTR fra noget tidligere, ville
 * loekken koere for evigt.
 */
typedef enum {
    ZS_SEND_VIDERE = 0, /* over nul: der blev sendt noget, taeller op   */
    ZS_SEND_IGEN,       /* afbrudt af et signal, proev igen med det samme */
    ZS_SEND_TIMEOUT,    /* der kunne ikke sendes inden tiden loeb ud    */
    ZS_SEND_FEJL        /* og alt andet, ogsaa n lig nul                */
} zs_send_t;

zs_send_t zs_mb_send_art(long n, int fejlnr);

/*
 * Naeste transaktions-ID, og nul bliver sprunget over.
 *
 * Nul er lovligt i Modbus, men et logfilsudtraek hvor hvert eneste svar
 * staar med 0 er ubrugeligt naar man skal finde ud af hvilket svar der
 * hoerte til hvilken forespoergsel.
 *
 * Laa foer skrevet forfra TRE steder: i zs_mb_connect, i
 * zs_mb_read_holding og i zs_mb_write_verified. En mutationstest aendrede
 * "== 0" til "!= 0" paa ét af dem, og ingen proeve sagde fra. Nu er der ét
 * sted, og det er proevet af hele vejen rundt til 65535 og om igen.
 *
 * taeller bliver rykket frem. Returnerer det ID der skal bruges NU.
 */
uint16_t zs_mb_naeste_tid(uint16_t *taeller);

/*
 * Hvilken svartid skal soklen staa paa?
 *
 * Laa foer midt i zs_mb_read_holding som to betingelser der ikke kunne
 * proeves: der skal en rigtig sokkel til for at komme dertil. En
 * mutationstest vendte dem begge om uden at nogen sagde fra:
 *
 *   "timeout_ms == 0"            -> en svartid kalderen BAD om blev
 *                                   erstattet af standarden
 *   "timeout_ms != mb->timeout"  -> en aendret svartid blev ikke sat paa
 *                                   soklen, og en uaendret blev sat igen
 *
 * Den sidste er den grimme: beder man om en kortere svartid fordi man er
 * ved at scanne, og den ikke bliver sat, tager scanningen elleve gange saa
 * lang tid som den skal.
 *
 *   oenske      hvad kalderen bad om. Nul betyder "brug den der er".
 *   nuvaerende  hvad soklen staar paa nu. Nul betyder "ikke sat endnu".
 */
/*
 * Svartiden naar ingen har sagt noget andet.
 *
 * Tallet 1500 stod FEM steder i zs_modbus_tcp.c. Fem steder der skal vaere
 * ens, bliver det ikke, og en inverter der svarer langsomt ville saa
 * opfoere sig forskelligt alt efter hvilken vej man kom ind.
 */
#define ZS_MB_TIMEOUT_STANDARD_MS  1500

/*
 * Hoejeste antal registre i ÉN skrivning, funktionskode 16.
 *
 * 123 og ikke 125. En FC16-ramme har to bytes mere end en FC3: adressen og
 * byte-taelleren skal med i selve skrivningen. Tallet stod naegent som 123
 * midt i en vagt, og et naegent tal er et sted hvor nogen senere retter
 * det til 125 fordi det ligner laesegraensen.
 */
#define ZS_MB_MAX_WRITE_REGS  123

typedef struct {
    uint32_t ms;            /* den svartid der skal bruges          */
    bool     skal_saettes;  /* skal soklen have den sat paa?        */
} zs_mb_timeout_t;

zs_mb_timeout_t zs_mb_timeout_vaelg(uint32_t oenske, uint32_t nuvaerende);

/*
 * Er laengden i svarets header noget vi tror paa?
 *
 * MBAP-headeren fortaeller hvor mange bytes der foelger. Tallet kommer fra
 * den anden ende, altsaa fra noget vi ikke bestemmer over.
 *
 * DER ER TO VAGTER, OG DE ER IKKE DEN SAMME:
 *
 *   1. den her, som er PROTOKOLREGLEN: mindst unit + funktionskode +
 *      kode = 3 bytes, hoejst unit + funktionskode + byte-taeller + 250
 *      data = 253. Den afviser et meningsloest tal tidligt og skriver
 *      hvad der var i vejen, saa man kan se det i loggen.
 *
 *   2. og lige efter den, i modtagelsen: at MBAP_LEN + len kan VAERE i
 *      bufferen. Det er den haarde stopklods mod et overloeb.
 *
 * Og det skal siges praecist, for jeg skrev foerst noget andet her: at
 * fjerne DEN HER vagt alene giver IKKE et overloeb. Nummer to fanger det,
 * og proeven hele vejen igennem kan ikke se forskel. Det er maalt: med kun
 * den her fjernet bestaar alt, med BEGGE fjernet falder proeven "en loegn
 * om laengden giver intet overloeb".
 *
 * Den er altsaa to ting: en regel der staar skrevet ét sted i stedet for
 * at vaere et tal midt i en loekke, og et lag mere. Ikke den eneste
 * stopklods.
 */
bool zs_mb_pdu_laengde_ok(uint16_t len);

/*
 * Er vaert og port noget vi overhovedet kan forsoege?
 *
 * Vagten stod to steder, i zs_mb_connect og i zs_mb_probe_port, og de to
 * var ikke ens: den ene afviste en tom vaert, den anden gjorde ikke. Nu
 * er der ét sted.
 *
 * Og den ligger i headeren fordi den ellers ikke kan proeves af. Udefra
 * kan man ikke se forskel: fjerner man vagten, fejler maskinens eget
 * socket-lag alligevel, saa svaret bliver det samme og en mutationstest
 * siger god for det. Men det er maskinens tilfaeldighed og ikke vores
 * beslutning. Et navneopslag paa en TOM tekst svarer paa nogle maskiner
 * "det er dig selv", og saa ville vi melde at inverteren var fundet,
 * fordi der stod noget paa VORES egen port. Derfor skal vagten vaere der,
 * og derfor skal der vaere en proeve paa den.
 */
bool zs_mb_maal_ok(const char *host, uint16_t port);

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
