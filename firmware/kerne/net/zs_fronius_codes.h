/*
 * zScreen - Fronius' egne fejlkoder.
 *
 * FILEN ER GENERERET af tools/gen-fronius-codes.py. Ret ikke i haanden.
 *
 * Kilden er Fronius' EGEN fil, FroniusEventFlags.json, som de udgiver
 * under Solar Electronics, Info og Support, Third-party Downloads.
 * Vi opfinder INGEN betydninger: hver linje herunder svarer til en
 * linje i deres fil.
 *
 * Tallene bagerst er Fronius' egne statuskoder, dem der ogsaa staar i
 * Solar.web. De vises paa skaermen saa en montoer kan slaa dem op.
 *
 * 66 fejltyper.
 *
 * VIGTIGT: listen er Fronius' faelles saet ("all"), som daekker Symo,
 * Galvo, Primo og IG Plus. GEN24 er nyere, og Fronius har ikke
 * udgivet et tilsvarende saet for den. Derfor viser skaermen ALTID
 * ogsaa den raa vaerdi i hex, saa den kan slaas op selv hvis en bit
 * betyder noget andet paa en GEN24.
 */

#ifndef ZS_FRONIUS_CODES_H
#define ZS_FRONIUS_CODES_H

#include <stdint.h>

typedef enum {
    ZS_SEV_OK = 0,     /* alt er som det skal vaere        */
    ZS_SEV_INFO,       /* vaerd at vide, ikke et problem   */
    ZS_SEV_WARN,       /* noget er begraenset              */
    ZS_SEV_FAULT,      /* fejl                             */
} zs_sev_t;

typedef struct {
    uint8_t     bit;        /* hvilken bit i feltet            */
    zs_sev_t    sev;
    const char *tekst;      /* hvad der staar paa skaermen     */
    const char *koder;      /* Fronius' egne statuskoder       */
} zs_bit_text_t;

/* EvtVnd1, 31 bit */
static const zs_bit_text_t zs_evtvnd1_bits[] = {
    {  1, ZS_SEV_FAULT , "Netfejl", "101, 104, 107, 108, 109, 117, 127, 137, 205, 206, 305" },
    {  2, ZS_SEV_FAULT , "Overstrøm på AC", "301, 321" },
    {  3, ZS_SEV_FAULT , "Overstrøm på DC", "302" },
    {  4, ZS_SEV_FAULT , "For høj temperatur", "303, 304, 322" },
    {  5, ZS_SEV_WARN  , "For lav effekt", "306" },
    {  6, ZS_SEV_FAULT , "For lav DC-spænding", "307, 310, 522, 523" },
    {  7, ZS_SEV_FAULT , "Fejl i mellemkreds", "308, 426" },
    {  8, ZS_SEV_FAULT , "Netfrekvens for høj", "105, 115, 125, 135, 203" },
    {  9, ZS_SEV_FAULT , "Netfrekvens for lav", "106, 116, 126, 136, 204" },
    { 10, ZS_SEV_FAULT , "Netspænding for høj", "102, 112, 122, 132, 201" },
    { 11, ZS_SEV_FAULT , "Netspænding for lav", "103, 113, 123, 133, 202" },
    { 12, ZS_SEV_FAULT , "Jævnstrøm ud på nettet", "408" },
    { 13, ZS_SEV_FAULT , "Fejl på relæ", "207, 208, 457" },
    { 14, ZS_SEV_FAULT , "Fejl i effekttrin", "417, 419, 421, 427, 428, 429, 431, 432, 433, 436, 437, 43..." },
    { 15, ZS_SEV_FAULT , "Fejl i styringen", "409, 413" },
    { 16, ZS_SEV_FAULT , "Overvågning: fejl på netspænding", "453" },
    { 17, ZS_SEV_FAULT , "Overvågning: fejl på netfrekvens", "454" },
    { 18, ZS_SEV_FAULT , "Kan ikke levere energi", "443" },
    { 19, ZS_SEV_FAULT , "Referencespænding uden for tolerance", "455" },
    { 20, ZS_SEV_FAULT , "Fejl under test for ø-drift", "456" },
    { 21, ZS_SEV_FAULT , "Fast spænding under MPP-spænding", "412" },
    { 22, ZS_SEV_FAULT , "Hukommelsesfejl", "403, 414, 451, 505, 506, 507, 510, 511, 711, 712, 713, 71..." },
    { 23, ZS_SEV_FAULT , "Fejl på display", "464, 465, 466, 467" },
    { 24, ZS_SEV_FAULT , "Intern kommunikationsfejl", "401, 402, 416, 425, 452, 490, 491, 519, 799" },
    { 25, ZS_SEV_FAULT , "Temperaturfølere defekte", "406, 407, 487, 532, 533" },
    { 26, ZS_SEV_FAULT , "Fejl på DC- eller AC-print", "460, 461, 518" },
    { 27, ZS_SEV_FAULT , "ENS-fejl", "248, 404, 405, 415" },
    { 28, ZS_SEV_FAULT , "Fejl på blæser", "530, 531, 534, 535, 536, 537, 540, 541, 555, 557" },
    { 29, ZS_SEV_FAULT , "Defekt sikring", "471, 472, 551" },
    { 30, ZS_SEV_FAULT , "Udgangsspole forkert tilsluttet", "469" },
    { 31, ZS_SEV_FAULT , "Relæ åbner ikke ved høj DC-spænding", "470" },
};

/* EvtVnd2, 32 bit */
static const zs_bit_text_t zs_evtvnd2_bits[] = {
    {  0, ZS_SEV_FAULT , "Ingen SolarNet-forbindelse", "504" },
    {  1, ZS_SEV_FAULT , "Forkert inverter-adresse", "508" },
    {  2, ZS_SEV_WARN  , "Ingen levering i 24 timer", "509" },
    {  3, ZS_SEV_FAULT , "Fejl i stikforbindelser", "410, 515" },
    {  4, ZS_SEV_FAULT , "Faserne sidder forkert", "473" },
    {  5, ZS_SEV_FAULT , "Netleder afbrudt eller fase mangler", "210" },
    {  6, ZS_SEV_FAULT , "Software er for gammel", "558" },
    {  7, ZS_SEV_WARN  , "Effekt sænket på grund af varme", "517" },
    {  8, ZS_SEV_FAULT , "Jumper sat forkert", "550" },
    {  9, ZS_SEV_FAULT , "Funktion passer ikke sammen", "559" },
    { 10, ZS_SEV_FAULT , "Blæser defekt eller luftindtag blokeret", "501" },
    { 11, ZS_SEV_WARN  , "Effekt sænket på grund af fejl", "560, 561" },
    { 12, ZS_SEV_FAULT , "Lysbue registreret", "240" },
    { 13, ZS_SEV_FAULT , "AFCI-selvtest fejlede", "245" },
    { 14, ZS_SEV_FAULT , "Fejl på strømføler", "247" },
    { 15, ZS_SEV_FAULT , "Fejl på DC-afbryder", "492, 493" },
    { 16, ZS_SEV_FAULT , "AFCI defekt", "249" },
    { 17, ZS_SEV_INFO  , "AFCI manuel test gennemført", "250" },
    { 18, ZS_SEV_FAULT , "Forsyning til effekttrin mangler", "476" },
    { 19, ZS_SEV_FAULT , "AFCI-forbindelse afbrudt", "477" },
    { 20, ZS_SEV_FAULT , "AFCI manuel test fejlede", "478" },
    { 21, ZS_SEV_FAULT , "AC-polaritet vendt om", "463" },
    { 22, ZS_SEV_FAULT , "Fejl på AC-måling", "488" },
    { 23, ZS_SEV_FAULT , "Fejl i flash", "781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 79..." },
    { 24, ZS_SEV_FAULT , "Generel fejl", "772, 773, 775, 776" },
    { 25, ZS_SEV_FAULT , "Jordfejl", "494" },
    { 26, ZS_SEV_FAULT , "Fejl i effektbegrænsning", "761, 762, 763, 764, 765, 766, 767, 768" },
    { 27, ZS_SEV_FAULT , "Ekstern kontakt åben", "486" },
    { 28, ZS_SEV_FAULT , "Ekstern overspændingsbeskyttelse er udløst", "597, 598, 599" },
    { 29, ZS_SEV_FAULT , "Intern processorstatus", "707, 708, 709, 710, 1000-1299" },
    { 30, ZS_SEV_FAULT , "Problem med SolarNet", "701, 702, 703, 704, 705, 706" },
    { 31, ZS_SEV_FAULT , "Fejl på forsyningsspænding", "495, 496, 497, 498, 499" },
};

/* EvtVnd3, 3 bit */
static const zs_bit_text_t zs_evtvnd3_bits[] = {
    {  0, ZS_SEV_FAULT , "Fejl på uret", "751, 752, 753, 754, 755, 756, 757, 758, 760" },
    {  1, ZS_SEV_FAULT , "USB-fejl", "731, 732, 733, 734, 735, 736, 737, 738, 739, 740, 741, 74..." },
    {  2, ZS_SEV_FAULT , "For høj DC-spænding", "309, 313" },
};

/*
 * Standard-SunSpec, model 103 og 113, felt Evt1.
 * Kilde: smdx_00103.xml fra github.com/sunspec/models.
 * De gaelder for ENHVER SunSpec-inverter, ikke kun Fronius.
 */
static const zs_bit_text_t zs_evt1_bits[] = {
    {  0, ZS_SEV_FAULT, "Jordfejl",                        "SunSpec" },
    {  1, ZS_SEV_FAULT, "For høj DC-spænding",             "SunSpec" },
    {  2, ZS_SEV_WARN,  "AC-afbryder er åben",             "SunSpec" },
    {  3, ZS_SEV_WARN,  "DC-afbryder er åben",             "SunSpec" },
    {  4, ZS_SEV_WARN,  "Nettet er koblet fra",            "SunSpec" },
    {  5, ZS_SEV_WARN,  "Kabinettet er åbent",             "SunSpec" },
    {  6, ZS_SEV_INFO,  "Slukket manuelt",                 "SunSpec" },
    {  7, ZS_SEV_FAULT, "For høj temperatur",              "SunSpec" },
    {  8, ZS_SEV_FAULT, "Netfrekvens over grænsen",        "SunSpec" },
    {  9, ZS_SEV_FAULT, "Netfrekvens under grænsen",       "SunSpec" },
    { 10, ZS_SEV_FAULT, "Netspænding over grænsen",        "SunSpec" },
    { 11, ZS_SEV_FAULT, "Netspænding under grænsen",       "SunSpec" },
    { 12, ZS_SEV_FAULT, "Sprunget strengsikring",          "SunSpec" },
    { 13, ZS_SEV_FAULT, "For lav temperatur",              "SunSpec" },
    { 14, ZS_SEV_FAULT, "Hukommelses- eller kommunikationsfejl", "SunSpec" },
    { 15, ZS_SEV_FAULT, "Hardwaretest fejlede",            "SunSpec" },
};

/*
 * Model 160, felt DCEvt: fejl paa en enkelt DC-kanal.
 * Kilde: smdx_00160.xml.
 */
static const zs_bit_text_t zs_dcevt_bits[] = {
    {  0, ZS_SEV_FAULT, "Jordfejl",                        "SunSpec" },
    {  1, ZS_SEV_FAULT, "For høj indgangsspænding",        "SunSpec" },
    {  3, ZS_SEV_WARN,  "DC-afbryder er åben",             "SunSpec" },
    {  5, ZS_SEV_WARN,  "Kabinettet er åbent",             "SunSpec" },
    {  6, ZS_SEV_INFO,  "Slukket manuelt",                 "SunSpec" },
    {  7, ZS_SEV_FAULT, "For høj temperatur",              "SunSpec" },
    { 12, ZS_SEV_FAULT, "Sprunget sikring",                "SunSpec" },
    { 13, ZS_SEV_FAULT, "For lav temperatur",              "SunSpec" },
    { 14, ZS_SEV_FAULT, "Hukommelsesfejl",                 "SunSpec" },
    { 15, ZS_SEV_FAULT, "Lysbue registreret",              "SunSpec" },
    { 20, ZS_SEV_FAULT, "Test fejlede",                    "SunSpec" },
    { 21, ZS_SEV_FAULT, "For lav indgangsspænding",        "SunSpec" },
    { 22, ZS_SEV_FAULT, "For høj indgangsstrøm",           "SunSpec" },
};

#endif /* ZS_FRONIUS_CODES_H */
