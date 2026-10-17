/*
 * zScreen - opsaetningen, trin for trin paa selve skaermen.
 *
 *   1. Velkomst          logo og "Kom i gang"
 *   2. Vælg netværk      liste over wifi i nærheden
 *   3. Kodeord           tastatur med æ, ø og å
 *   4. Forbinder         mens der ventes, og hvis det gik galt
 *   5. Søger             gennemgår netværket efter inverteren
 *   6. Vælg inverter     de fundne, med model og serienummer
 *
 * Ingen telefon, ingen computer, ingen QR-kode. Kunden skal kunne
 * saette skaermen op med det den kom med i kassen.
 */

#ifndef ZS_SCREEN_SETUP_H
#define ZS_SCREEN_SETUP_H

#include "lvgl.h"
#include "zs_wifi.h"
#include "zs_discovery.h"
#include "zs_ui.h"
#include "zs_app.h"   /* zs_inv_fra_t */

#ifdef __cplusplus
extern "C" {
#endif

void zs_setup_create(void);

/* Siderne, saa zs_ui kan vise og skjule dem. */
lv_obj_t *zs_setup_root_welcome(void);
lv_obj_t *zs_setup_root_guide(void);
lv_obj_t *zs_setup_root_wifi(void);
lv_obj_t *zs_setup_root_password(void);
lv_obj_t *zs_setup_root_connecting(void);
lv_obj_t *zs_setup_root_scan(void);
lv_obj_t *zs_setup_root_inverter(void);
lv_obj_t *zs_setup_root_inverter_manual(void);

/*
 * DE TO MAADER ET FLOW KAN STARTES PAA, og oprindelsen er et KRAV.
 *
 * Samme tre sider - soeger, vaelg anlaeg, skriv adressen - bruges fra
 * tre steder, og de skal ende tre forskellige steder. Se zs_inv_fra_t i
 * zs_app.h for hvad hver af dem betyder.
 *
 * HVORFOR DET ER DE HER TO FUNKTIONER OG IKKE EN SAETTER.
 *
 * Foer var der en zs_setup_inverter_fra(), som man skulle huske at
 * kalde FOER man startede flowet. To af tre kaldesteder huskede det.
 * Det tredje - "Netværk" i Indstillinger - gjorde ikke, og brugte
 * derfor hvad der tilfaeldigvis stod fra sidste gang: havde ingen
 * vaeret inde i flowet siden opstart, sluttede et netvaerksskift med
 * tilmeldings-ceremonien og en ny kode paa en skaerm der havde koert i
 * maanedsvis.
 *
 * Nu kan flowet ikke startes UDEN at sige hvor man kommer fra, fordi
 * det er et argument. En fjerde indgang en dag kan ikke glemme det -
 * den kan ikke oversaette uden.
 *
 * Begge roerer appen gennem koeen og venter ikke paa noget.
 */

/* Leder efter anlaegget. Viser soegesiden og saetter scanningen i gang. */
void zs_setup_soeg_inverter(zs_inv_fra_t fra);

/*
 * Laver netvaerket om, altsaa HELE opsaetningen forfra: liste, kodeord,
 * soegning, valg af anlaeg. Appen vaelger selv foerste side, se
 * ZS_CMD_SETUP_RESTART.
 */
void zs_setup_skift_netvaerk(zs_inv_fra_t fra);

void zs_setup_set_wifi_list(const zs_ap_t *aps, int n);

/* Der kunne ikke soeges - en anden besked end den tomme liste.
 * Se noten i zs_screen_setup.c. */
void zs_setup_set_wifi_intet_svar(void);
void zs_setup_set_wifi_scanning(bool scanning);
void zs_setup_set_connect_status(const char *text, bool is_error, bool can_retry);
void zs_setup_set_scan_progress(int done, int total, int found);
void zs_setup_set_inverter_list(const zs_found_t *list, int n);

/* Skaermen har ingen adresse, saa soegningen kan ikke begynde. En
 * anden besked end den tomme liste - se noten i zs_screen_setup.c. */
void zs_setup_set_inverter_intet_net(void);

/* Siger hvor "tilbage" foerer hen fra siden med prisomraade. */


/* River opsaetningens sider ned. Se noten ved zs_screen_home_destroy. */
void zs_setup_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* ZS_SCREEN_SETUP_H */
