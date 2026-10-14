/*
 * zScreen - ventetrinnet. Titel, forklaring, bjaelke.
 *
 *     ┌────────────────────────────────┐
 *     │                                │
 *     │     Leder efter din inverter   │   zs_font_28
 *     │                                │
 *     │  Enheden gennemgaar netvaerket │   zs_font_16, daempet
 *     │                                │
 *     │  ████████░░░░░░░░░░░░░░░░░░░░  │   8 px bjaelke
 *     │                                │
 *     │          Fandt 2               │   zs_font_16, orange
 *     └────────────────────────────────┘
 *
 * HVORFOR DEN FINDES.
 *
 * To sider venter paa noget og viser det samme: inverter-soegningen og
 * tilmeldingens "Henter forbindelse". De stod skrevet hver sit sted,
 * og BEGGE filer havde en kommentar der sagde at maalene var de samme
 * som den andens. Det var skrevet i god tro, og det var ikke sandt:
 * titlen stod i to forskellige skriftstoerrelser, forklaringen i to
 * farver, og bjaelken var 24 px bredere paa den ene.
 *
 * Den slags kan en kommentar ikke holde styr paa. Da teksten paa
 * tilmeldingssiden blev kortet ned for at ligne de andre ventesider,
 * blev den LIGNENDE - og forskellene i maal blev siddende, fordi de
 * stod et andet sted i filen end den kommentar der lovede at de ikke
 * gjorde.
 *
 * Nu er der ét element og ét saet tal, og tallene staar i zs_layout.h
 * hvor der er vagter paa dem. To sider der ser ens ud, er to sider
 * montoeren ikke skal laere hver for sig.
 *
 * DEN EJER IKKE SIN FORAELDER.
 *
 * Elementet laegger sig oven paa den side det bliver givet, og det
 * flytter eller skjuler ikke noget andet paa den. En side der ogsaa har
 * andet indhold - som tilmeldingen har - skjuler selv sit eget og viser
 * trinnet. Gjorde elementet det for den, ville to sider skulle enes om
 * hvad "andet indhold" er, og det kan de ikke.
 */

#ifndef ZS_STEP_H
#define ZS_STEP_H

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *titel;     /* den store linje foeroven     */
    lv_obj_t *tekst;     /* forklaringen under den       */
    lv_obj_t *bjaelke;   /* fremdriften                  */
    lv_obj_t *note;      /* linjen under bjaelken        */
} zs_step_t;

/*
 * Bygger de fire ting paa forael. Positionerne og bredden kommer fra
 * ZS_G_STEP_* i zs_layout.h, ikke fra kalderen: kunne hver side give
 * sine egne, ville de to kunne skride igen, og det er netop det her
 * element findes for at forhindre.
 *
 * Noten er tom og bjaelken staar paa 0. Kald zs_step_saet() bagefter.
 */
void zs_step_create(zs_step_t *s, lv_obj_t *forael);

/* Titel og forklaring. NULL lader den staa som den er. */
void zs_step_saet(zs_step_t *s, const char *titel, const char *tekst);

/* 0 til 100. Uden for det klippes der, saa en regnefejl et andet sted
 * ikke bliver en bjaelke der tegner uden for sig selv. */
void zs_step_fremdrift(zs_step_t *s, int procent);

/* Linjen under bjaelken. NULL eller "" skjuler den ikke - den bliver
 * tom, saa resten ikke flytter sig. */
void zs_step_note(zs_step_t *s, const char *note);

/* Vis eller skjul alle fire paa én gang. En side der skifter tilstand,
 * skal ikke kunne glemme én af dem. */
void zs_step_vis(zs_step_t *s, bool paa);

#ifdef __cplusplus
}
#endif

#endif /* ZS_STEP_H */
