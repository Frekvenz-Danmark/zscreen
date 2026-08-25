#!/usr/bin/env python3
"""
zScreen - lav C-tabellen over Fronius' fejlkoder.

Kilden er Fronius' EGEN fil, FroniusEventFlags.json, som de udgiver
under Solar Electronics, Info og Support, Third-party Downloads. Den
ligger ogsaa i grawlinson/fronius-docs paa GitHub sammen med resten af
Fronius' Modbus-dokumentation.

Vi opfinder INGEN betydninger. Hver linje i tabellen svarer til en
linje i Fronius' fil, og de talkoder der staar bagerst er Fronius'
egne, saa en montoer kan slaa dem op i Fronius' dokumentation eller
Solar.web.

    python3 gen-fronius-codes.py --hent      hent kilden igen
    python3 gen-fronius-codes.py             skriv C-headeren
"""

import json
import os
import subprocess
import sys

HER = os.path.dirname(os.path.abspath(__file__))
KILDE = os.path.join(HER, ".assets-cache", "FroniusEventFlags.json")
UD = os.path.join(HER, "..", "firmware", "main", "net", "zs_fronius_codes.h")

REPO = "grawlinson/fronius-docs"
STI = ("modbus/SE_EI_Modbus_Sunspec_Maps_State_Codes_Events/"
       "FroniusEventFlags.json")

# Dansk tekst til hver af Fronius' fejlklasser. Teksterne er skrevet
# til den der staar med skaermen, ikke til den der har skrevet koden.
DA = {
    "EvtVnd1": {
        1: "Netfejl",
        2: "Overstrøm på AC",
        3: "Overstrøm på DC",
        4: "For høj temperatur",
        5: "For lav effekt",
        6: "For lav DC-spænding",
        7: "Fejl i mellemkreds",
        8: "Netfrekvens for høj",
        9: "Netfrekvens for lav",
        10: "Netspænding for høj",
        11: "Netspænding for lav",
        12: "Jævnstrøm ud på nettet",
        13: "Fejl på relæ",
        14: "Fejl i effekttrin",
        15: "Fejl i styringen",
        16: "Overvågning: fejl på netspænding",
        17: "Overvågning: fejl på netfrekvens",
        18: "Kan ikke levere energi",
        19: "Referencespænding uden for tolerance",
        20: "Fejl under test for ø-drift",
        21: "Fast spænding under MPP-spænding",
        22: "Hukommelsesfejl",
        23: "Fejl på display",
        24: "Intern kommunikationsfejl",
        25: "Temperaturfølere defekte",
        26: "Fejl på DC- eller AC-print",
        27: "ENS-fejl",
        28: "Fejl på blæser",
        29: "Defekt sikring",
        30: "Udgangsspole forkert tilsluttet",
        31: "Relæ åbner ikke ved høj DC-spænding",
    },
    "EvtVnd2": {
        0: "Ingen SolarNet-forbindelse",
        1: "Forkert inverter-adresse",
        2: "Ingen levering i 24 timer",
        3: "Fejl i stikforbindelser",
        4: "Faserne sidder forkert",
        5: "Netleder afbrudt eller fase mangler",
        6: "Software er for gammel",
        7: "Effekt sænket på grund af varme",
        8: "Jumper sat forkert",
        9: "Funktion passer ikke sammen",
        10: "Blæser defekt eller luftindtag blokeret",
        11: "Effekt sænket på grund af fejl",
        12: "Lysbue registreret",
        13: "AFCI-selvtest fejlede",
        14: "Fejl på strømføler",
        15: "Fejl på DC-afbryder",
        16: "AFCI defekt",
        17: "AFCI manuel test gennemført",
        18: "Forsyning til effekttrin mangler",
        19: "AFCI-forbindelse afbrudt",
        20: "AFCI manuel test fejlede",
        21: "AC-polaritet vendt om",
        22: "Fejl på AC-måling",
        23: "Fejl i flash",
        24: "Generel fejl",
        25: "Jordfejl",
        26: "Fejl i effektbegrænsning",
        27: "Ekstern kontakt åben",
        28: "Ekstern overspændingsbeskyttelse er udløst",
        29: "Intern processorstatus",
        30: "Problem med SolarNet",
        31: "Fejl på forsyningsspænding",
    },
    "EvtVnd3": {
        0: "Fejl på uret",
        1: "USB-fejl",
        2: "For høj DC-spænding",
    },
}

# Alvorlighed. De fleste er fejl. Nogle faa er oplysninger eller
# begraensninger, og de skal ikke faa skaermen til at lyse roedt.
INFO = {("EvtVnd2", 17)}                      # AFCI manuel test gennemfoert
ADVARSEL = {("EvtVnd2", 7), ("EvtVnd2", 11),  # effekt saenket
            ("EvtVnd1", 5),                   # for lav effekt
            ("EvtVnd2", 2)}                   # ingen levering i 24 timer


def hent():
    os.makedirs(os.path.dirname(KILDE), exist_ok=True)
    r = subprocess.run(["gh", "api", f"repos/{REPO}/contents/{STI}",
                        "--jq", ".content"], capture_output=True, text=True)
    if r.returncode != 0:
        print("kunne ikke hente kilden:", r.stderr, file=sys.stderr)
        return False
    import base64
    with open(KILDE, "wb") as f:
        f.write(base64.b64decode(r.stdout))
    print(f"hentet {os.path.getsize(KILDE)} bytes")
    return True


def bit_of(dec):
    return dec.bit_length() - 1 if dec and (dec & (dec - 1)) == 0 else None


def main():
    if "--hent" in sys.argv:
        return 0 if hent() else 1
    if not os.path.exists(KILDE):
        if not hent():
            return 1

    data = json.load(open(KILDE, encoding="utf-8"))["devices"]
    alle = [x for x in data if "all" in x][0]["all"]

    linjer = []
    for felt in ("EvtVnd1", "EvtVnd2", "EvtVnd3"):
        poster = []
        for p in alle.get(felt, []):
            b = bit_of(p["dec"])
            if b is None or b not in DA[felt]:
                continue
            sev = ("ZS_SEV_INFO" if (felt, b) in INFO else
                   "ZS_SEV_WARN" if (felt, b) in ADVARSEL else
                   "ZS_SEV_FAULT")
            koder = ", ".join(c.strip() for c in p["codes"].split(","))
            if len(koder) > 60:
                koder = koder[:57] + "..."
            poster.append(f'    {{ {b:2d}, {sev:<13s}, "{DA[felt][b]}", "{koder}" }},')
        linjer.append((felt, poster))

    ialt = sum(len(p) for _, p in linjer)

    with open(UD, "w", encoding="utf-8") as f:
        f.write(f'''/*
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
 * {ialt} fejltyper.
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

typedef enum {{
    ZS_SEV_OK = 0,     /* alt er som det skal vaere        */
    ZS_SEV_INFO,       /* vaerd at vide, ikke et problem   */
    ZS_SEV_WARN,       /* noget er begraenset              */
    ZS_SEV_FAULT,      /* fejl                             */
}} zs_sev_t;

typedef struct {{
    uint8_t     bit;        /* hvilken bit i feltet            */
    zs_sev_t    sev;
    const char *tekst;      /* hvad der staar paa skaermen     */
    const char *koder;      /* Fronius' egne statuskoder       */
}} zs_bit_text_t;

''')
        for felt, poster in linjer:
            f.write(f"/* {felt}, {len(poster)} bit */\n")
            f.write(f"static const zs_bit_text_t zs_{felt.lower()}_bits[] = {{\n")
            f.write("\n".join(poster))
            f.write("\n};\n\n")

        f.write('''/*
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
''')
    print(f"  zs_fronius_codes.h     {ialt} Fronius-fejltyper "
          f"plus 16 SunSpec og 13 DC-kanal")
    return 0


if __name__ == "__main__":
    sys.exit(main())
