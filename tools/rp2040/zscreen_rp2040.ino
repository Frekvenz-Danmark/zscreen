/*
 * zScreen - firmware til RP2040'eren i SenseCAP Indicator D1.
 *
 * HVORFOR DEN FINDES.
 *
 * Summeren sidder paa RP2040'erens GPIO 19. ESP32'en, som kOErer
 * zScreen, kan ikke naa den: det eneste der gaar mellem de to chips er
 * en seriel ledning. Et bip er derfor en PAKKE fra ESP32'en, og det er
 * den her firmware der laver selve lyden.
 *
 * Seeeds egen firmware kan det IKKE. Den kender kommandoen - 0xA1 staar
 * i deres egen kilde, og de har en beep_on() - men deres
 * onPacketReceived kalder den aldrig. Den har kun ét tilfaelde, 0xA3,
 * som slukker sensorerne. Maalt 13. oktober 2026 ved at laese
 * examples/indicator_rp2040/indicator_rp2040.ino i
 * Seeed-Solution/SenseCAP_Indicator_RP2040.
 *
 * Derfor den her. Den er lille med vilje: D1'eren har hverken CO2-,
 * tVOC- eller temperatursensor, intet SD-kort i brug og ingen LoRa, saa
 * alt det deres firmware ellers laver, er der ikke noget af paa den her
 * model. Det der er tilbage, er summeren.
 *
 * HVAD DEN GOER BEDRE END SEEEDS:
 *
 *   - Den SVARER paa bip. Det er hele pointen.
 *   - Tonen har en laengde, som ESP32'en bestemmer. Seeeds beep_on er
 *     laast til 50 ms, hvilket er et klik og ikke et bip.
 *   - Tonen har en FREKVENS. Summeren er passiv: den skal have en
 *     vekslende spaending for at lave lyd, og hvor hoejt den lyder,
 *     afhaenger helt af frekvensen. 2,7 kHz er hvor et piezoelement er
 *     kraftigst. Seeeds kode bruger analogWrite uden at saette
 *     frekvensen og faar dermed kernens standard, omkring 1 kHz, hvor
 *     elementet er maerkbart svagere.
 *   - Den BLOKERER IKKE. Seeeds beep_on har delay(50) inde i
 *     pakkemodtageren. Her slaas tonen til, og loop() slaar den fra naar
 *     tiden er gaaet, saa chippen kan tage imod den naeste pakke med det
 *     samme.
 *
 * SAADAN FLASHES DEN: se HVORDAN.md ved siden af.
 *
 * Kraever Arduino-kernen arduino-pico af Earle Philhower (board: Seeed
 * INDICATOR) og biblioteket PacketSerial. analogWriteFreq og
 * analogWriteRange hoerer til netop den kerne.
 */

#include <Arduino.h>
#include <PacketSerial.h>

/* ── pakketyperne ────────────────────────────────────────────────────
 * Tallene er Seeeds, ikke vores. De staar baade i deres ESP32-kode og i
 * deres RP2040-firmware, saa de maa ikke vaelges om: gjorde vi det,
 * kunne en skaerm med vores firmware i den ene chip og deres i den
 * anden komme til at goere noget helt andet end der blev bedt om. */
#define PKT_TYPE_ACK          0x00
#define PKT_TYPE_CMD_BEEP_ON  0xA1   /* nyttelast: uint32 ms, mindste byte foerst */
#define PKT_TYPE_CMD_BEEP_OFF 0xA2
#define PKT_TYPE_CMD_SHUTDOWN 0xA3

/* ── benene ──────────────────────────────────────────────────────────
 * Ledningen til ESP32'en, og summeren. Alle tre er Seeeds hardware og
 * staar ogsaa i deres egen kilde. */
#define PIN_TIL_ESP_TX   16   /* -> ESP32 GPIO 20 */
#define PIN_FRA_ESP_RX   17   /* <- ESP32 GPIO 19 */
#define PIN_SUMMER       19

/*
 * Tonens frekvens og styrke.
 *
 * 2700 Hz: et piezoelement har sin egen resonans deromkring, og det er
 * ogsaa dér et menneskeligt oere er mest foelsomt.
 *
 * Tallet staar KUN her. Firmwaren paa ESP32'en kan ikke bruge det -
 * den sender bare "bip saa mange millisekunder" - og et tal der skal
 * vaere ens to steder, hvor kun det ene bliver brugt, bliver forkert.
 *
 * Halv styrke, altsaa 128 af 255: en firkantbOElge med lige lange
 * halvdele giver den kraftigste grundtone. Skubbes forholdet til én
 * side, bliver tonen tynd uden at blive svagere.
 */
#define TONE_HZ          2700
#define TONE_STYRKE      128
#define TONE_SKALA       255

/* Loft over hvor laenge én tone kan vare. En pakke der er blevet
 * forvansket paa vejen, maa ikke kunne saette summeren til at hyle i en
 * time - og der er ingen inde i kabinettet der kan slukke den. */
#define TONE_MAKS_MS     1000
#define TONE_STANDARD_MS 60

PacketSerial myPacketSerial;

/*
 * Naar den tone der kOErer nu, skal slukkes - og OM der er en.
 *
 * Der er et flag ved siden af tidspunktet. Nul kunne ikke bruges som
 * "ingen tone": millis() + laengden KAN vaere nul, naemlig i det ene
 * millisekund hvert 49. doegn hvor uret loeber over, og saa ville tonen
 * blive ved indtil nogen tog stroemmen fra skaermen. Det sker praktisk
 * talt aldrig, og det er praecis derfor det er den slags fejl der staar
 * i aarevis.
 */
static bool     tone_koerer = false;
static uint32_t tone_slut_ms = 0;

static void summer_init(void)
{
    pinMode(PIN_SUMMER, OUTPUT);
    analogWriteFreq(TONE_HZ);
    analogWriteRange(TONE_SKALA);
    analogWrite(PIN_SUMMER, 0);
}

static void summer_til(uint32_t ms)
{
    if (ms == 0) {
        ms = TONE_STANDARD_MS;
    }
    if (ms > TONE_MAKS_MS) {
        ms = TONE_MAKS_MS;
    }
    analogWrite(PIN_SUMMER, TONE_STYRKE);
    tone_slut_ms = millis() + ms;
    tone_koerer = true;
}

static void summer_fra(void)
{
    analogWrite(PIN_SUMMER, 0);
    tone_koerer = false;
}

static void paa_pakke(const uint8_t *buffer, size_t size)
{
    if (size < 1) {
        return;
    }

    switch (buffer[0]) {
    case PKT_TYPE_CMD_BEEP_ON: {
        /*
         * Laengden er valgfri. Kommer der ingen med, bruges
         * TONE_STANDARD_MS - saa virker den ogsaa med Seeeds eget
         * eksempel, som sender kommandoen helt uden nyttelast.
         *
         * Fire bytes, mindste foerst. Samlet med skift og ikke med en
         * memcpy ind i en uint32_t: en pegepind ind i en pakke er ikke
         * noedvendigvis justeret rigtigt, og paa en Cortex-M0+ er en
         * ujusteret 32-bit laesning ikke bare langsom, den fejler.
         */
        uint32_t ms = 0;
        if (size >= 5) {
            ms =  (uint32_t)buffer[1]
               | ((uint32_t)buffer[2] << 8)
               | ((uint32_t)buffer[3] << 16)
               | ((uint32_t)buffer[4] << 24);
        }
        summer_til(ms);
        break;
    }

    case PKT_TYPE_CMD_BEEP_OFF:
        summer_fra();
        break;

    /*
     * Sluk-kommandoen er med for at vaere enig med Seeeds protokol. Der
     * er ingen sensorer at slukke paa en D1, saa den gOEr ingenting -
     * men den maa ikke falde ned i default og blive tiet om, for saa
     * ville en fremtidig model se ud som en der ikke svarede.
     */
    case PKT_TYPE_CMD_SHUTDOWN:
        break;

    default:
        break;
    }
}

void setup()
{
    Serial1.setRX(PIN_FRA_ESP_RX);
    Serial1.setTX(PIN_TIL_ESP_TX);
    Serial1.begin(115200);
    myPacketSerial.setStream(&Serial1);
    myPacketSerial.setPacketHandler(&paa_pakke);

    summer_init();

    /*
     * ÉN KORT TONE VED OPSTART, og det er med vilje.
     *
     * Den er det eneste bevis paa at summeren og ledningen virker, FOER
     * der er noget i vejen. Uden den ville den foerste lyd paa en skaerm
     * komme den dag noget gik i stykker - og hvis den saa ikke kom,
     * ville man ikke vide om det var fordi alarmen var tavs eller fordi
     * der ikke var noget at melde.
     *
     * Seeeds egen firmware bipper ogsaa ved opstart. Det er altsaa ikke
     * en ny lyd for nogen der kender enheden.
     *
     * Skaermen selv bipper IKKE ved opstart: en enhed der mister
     * stroemmen klokken tre om natten og kommer igen, skal ikke vaekke
     * nogen. Her er det et bip pr. GENSTART af chippen, ikke pr. fejl.
     */
    summer_til(40);
}

void loop()
{
    myPacketSerial.update();

    /*
     * Tonen slukkes HER og ikke i pakkemodtageren.
     *
     * Et delay() derinde ville betyde at chippen ikke kunne hoere efter
     * mens den bippede, og to bip taet efter hinanden - som er praecis
     * hvad ét bip fra skaermen er - ville faa den anden pakke at vente.
     *
     * millis() loeber over efter cirka 49 doegn. Trukket fra hinanden
     * som to uint32_t er forskellen stadig rigtig henover det
     * overloeb, hvilket en direkte > -sammenligning ikke ville vaere.
     */
    if (tone_koerer && (int32_t)(millis() - tone_slut_ms) >= 0) {
        summer_fra();
    }

    if (myPacketSerial.overflow()) {
        /* Der kom mere end bufferen kunne rumme. Vi laeser aldrig
         * andet end korte kommandoer, saa det betyder stOEj paa
         * ledningen og ikke en pakke vi mistede. Intet at goere. */
    }
}
