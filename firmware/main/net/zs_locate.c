/*
 * zScreen - genfinder inverteren. Se zs_locate.h for hvorfor.
 *
 * Ingenting herinde kommer fra ESP-IDF. Undernettet gives udefra, og
 * resten er scanningen og en beslutning. Det er med vilje: netop
 * beslutningen om "er det her VORES inverter" er den der kan goere skade
 * hvis den er forkert, og den skal kunne proeves af uden hardware.
 */

#include "zs_locate.h"
#include "zs_fronius.h"
#include "zs_config.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Beslutningen                                                        */
/* ------------------------------------------------------------------ */

static bool tom(const char *s)
{
    return s == NULL || s[0] == '\0';
}

zs_pick_t zs_locate_pick(const zs_found_t *fundet, size_t n,
                         const char *gemt_serial, size_t *valgt)
{
    if (fundet == NULL || n == 0) {
        return ZS_PICK_INGEN;
    }

    if (!tom(gemt_serial)) {
        /*
         * Vi kender vores inverter. Saa er det KUN serienummeret der
         * taeller.
         *
         * Foerste traeffer vinder, og kalderen lagde den gemte adresse
         * foerst hvis den svarede. Saa foretraekkes den af sig selv, og
         * en inverter der svarer paa to adresser giver ikke anledning
         * til at skifte frem og tilbage.
         */
        for (size_t i = 0; i < n; i++) {
            if (strcmp(fundet[i].info.serial, gemt_serial) == 0) {
                if (valgt != NULL) { *valgt = i; }
                return ZS_PICK_SERIAL;
            }
        }
        /*
         * Ingen af dem er vores.
         *
         * Og saa tager vi IKKE den eneste der var. I en lejlighed eller
         * et raekkehus kan naboens inverter sagtens svare, og at vise
         * naboens produktion som kundens er vaerre end at sige at vi
         * ikke kan finde anlaegget. Kunden kan altid vaelge manuelt i
         * indstillingerne.
         */
        return (n == 1) ? ZS_PICK_INGEN : ZS_PICK_FLERE;
    }

    /*
     * Vi kender intet serienummer: en ny skaerm, eller en der er
     * opdateret fra en udgave der ikke gemte det. Er der praecis én
     * inverter paa nettet, er det den. Er der flere, maa kunden vaelge,
     * for saa er der ikke noget at gaa efter.
     */
    if (n == 1) {
        if (valgt != NULL) { *valgt = 0; }
        return ZS_PICK_ENESTE;
    }
    return ZS_PICK_FLERE;
}

const char *zs_locate_text(zs_loc_t r)
{
    switch (r) {
    case ZS_LOC_SAMME:     return "Inverteren sidder hvor den plejer";
    case ZS_LOC_NY:        return "Inverteren har fået en ny adresse";
    case ZS_LOC_TAGET:     return "Inverteren er fundet";
    case ZS_LOC_INGEN:     return "Inverteren blev ikke fundet på netværket";
    case ZS_LOC_FLERE:     return "Der er flere invertere, men ingen af dem er din";
    case ZS_LOC_INTET_NET: return "Der er ingen netværksforbindelse";
    case ZS_LOC_AFBRUDT:   return "Søgningen blev afbrudt";
    }
    return "Ukendt";
}

/* ------------------------------------------------------------------ */
/* Selve ledningen                                                     */
/* ------------------------------------------------------------------ */

/*
 * Plads til de fundne. Ligger som static og ikke paa stakken: tolv
 * zs_found_t er omkring 1,5 KB, og aflaesningsopgavens stak er ikke stor.
 * Der er kun én der leder ad gangen, kaldt fra den ene opgave.
 */
static zs_found_t s_fundet[ZS_DISCOVERY_MAX];

static void skriv_ud(const zs_found_t *f,
                     char *ip_ud, size_t ip_len,
                     char *serial_ud, size_t serial_len)
{
    if (ip_ud != NULL && ip_len > 0) {
        snprintf(ip_ud, ip_len, "%s", f->ip);
    }
    if (serial_ud != NULL && serial_len > 0) {
        snprintf(serial_ud, serial_len, "%s", f->info.serial);
    }
}

zs_loc_t zs_locate_find(const char *subnet,
                        const char *gemt_serial, const char *sidste_ip,
                        uint16_t port, uint8_t unit,
                        char *ip_ud, size_t ip_len,
                        char *serial_ud, size_t serial_len,
                        zs_discovery_progress_fn frem, void *ctx)
{
    (void) unit;    /* identiteten laeses paa model 1, som ligger samme
                     * sted uanset hvilken unit vi senere taler med */

    if (tom(subnet)) {
        return ZS_LOC_INTET_NET;
    }

    /*
     * Trin 1: proev den gemte adresse alene.
     *
     * Det tager under et sekund mod de omkring tyve en hel scanning
     * koster, og i det almindelige tilfaelde, hvor inverteren bare var
     * slukket et oejeblik, er vi faerdige her.
     */
    if (!tom(sidste_ip)) {
        zs_found_t en;
        memset(&en, 0, sizeof(en));
        snprintf(en.ip, sizeof(en.ip), "%s", sidste_ip);
        if (zs_fr_probe(sidste_ip, port, ZS_SCAN_SUNSPEC_TIMEOUT_MS, &en.info)) {
            size_t valgt = 0;
            zs_pick_t p = zs_locate_pick(&en, 1, gemt_serial, &valgt);
            if (p == ZS_PICK_SERIAL) {
                skriv_ud(&en, ip_ud, ip_len, serial_ud, serial_len);
                return ZS_LOC_SAMME;
            }
            if (p == ZS_PICK_ENESTE) {
                /* Vi kendte intet serienummer og den svarede. Saa er det
                 * den, og nu kender vi den. */
                skriv_ud(&en, ip_ud, ip_len, serial_ud, serial_len);
                return ZS_LOC_SAMME;
            }
            /*
             * Der svarede en inverter, men det er ikke vores. Adressen er
             * altsaa givet videre til en anden enhed. Vi leder videre, og
             * vi bruger IKKE den der svarede.
             */
        }
    }

    /*
     * Trin 2: hele undernettet. Den gemte adresse proeves foerst inde i
     * scanningen, saa den ligger foerst i listen hvis den svarer, og
     * "foerste traeffer vinder" i valget foretraekker den.
     */
    int n = zs_discovery_scan(subnet, tom(sidste_ip) ? NULL : sidste_ip, port,
                              s_fundet, ZS_DISCOVERY_MAX, frem, ctx);
    if (zs_discovery_was_aborted()) {
        return ZS_LOC_AFBRUDT;
    }
    if (n < 0) {
        return ZS_LOC_INTET_NET;
    }

    size_t valgt = 0;
    switch (zs_locate_pick(s_fundet, (size_t) n, gemt_serial, &valgt)) {
    case ZS_PICK_SERIAL:
        skriv_ud(&s_fundet[valgt], ip_ud, ip_len, serial_ud, serial_len);
        /* Samme adresse som foer? Saa svarede den bare ikke paa trin 1,
         * og det er ikke en flytning. */
        return (!tom(sidste_ip) && strcmp(s_fundet[valgt].ip, sidste_ip) == 0)
               ? ZS_LOC_SAMME : ZS_LOC_NY;
    case ZS_PICK_ENESTE:
        skriv_ud(&s_fundet[valgt], ip_ud, ip_len, serial_ud, serial_len);
        return ZS_LOC_TAGET;
    case ZS_PICK_FLERE:
        return ZS_LOC_FLERE;
    case ZS_PICK_INGEN:
        break;
    }
    return ZS_LOC_INGEN;
}
