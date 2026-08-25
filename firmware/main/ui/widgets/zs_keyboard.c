#include "zs_keyboard.h"
#include "zs_theme.h"
#include "zs_nvs.h"      /* ZS_PASS_MAX */

#include <string.h>

LV_FONT_DECLARE(zs_font_kb_24)

/* Taster med saerlig betydning. De staar som tekst i gitteret og
 * genkendes paa praecis den tekst, saa der ikke er tvivl om hvilken
 * tast der blev trykket. */
#define K_SHIFT   ZS_ICON_ARROW_UP
#define K_DEL     ZS_ICON_BACKSPACE
#define K_OK      ZS_ICON_CHECK
#define K_NUM     "123"
#define K_ABC     "abc"
#define K_SPACE   " "

struct zs_keyboard {
    lv_obj_t *matrix;
    lv_obj_t *target;      /* etiketten der viser det skrevne */
    char      text[ZS_PASS_MAX];
    bool      upper;
    bool      numeric;
    bool      hidden;      /* skjuler tegnene med prikker     */
    lv_event_cb_t done_cb;
    void     *user_data;
};

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

static const char *map_lower[] = {
    "q","w","e","r","t","y","u","i","o","p","å","\n",
    "a","s","d","f","g","h","j","k","l","æ","ø","\n",
    K_SHIFT,"z","x","c","v","b","n","m",K_DEL,"\n",
    K_NUM, K_SPACE, K_OK, ""
};

static const char *map_upper[] = {
    "Q","W","E","R","T","Y","U","I","O","P","Å","\n",
    "A","S","D","F","G","H","J","K","L","Æ","Ø","\n",
    K_SHIFT,"Z","X","C","V","B","N","M",K_DEL,"\n",
    K_NUM, K_SPACE, K_OK, ""
};

static const char *map_num[] = {
    "1","2","3","4","5","6","7","8","9","0","\n",
    "-","/",":",";","(",")","kr","&","@","\"","\n",
    "#+=",".",",","?","!","'","_","%",K_DEL,"\n",
    K_ABC, K_SPACE, K_OK, ""
};

/*
 * Bredder. LVGL's knapgitter fordeler bredden i hver raekke efter de
 * her tal, saa 1 er en almindelig tast og 2 er dobbelt saa bred.
 *
 * Skift og slet i tredje raekke er brede, fordi de bliver ramt tit og
 * er de eneste taster hvor et fejltryk koster brugeren noget: enten
 * forsvinder et tegn, eller ogsaa skifter hele tastaturet.
 */
static const lv_btnmatrix_ctrl_t ctrl_letters[] = {
    1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,
    2,1,1,1,1,1,1,1,2,
    2,7,2
};

static const lv_btnmatrix_ctrl_t ctrl_num[] = {
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    2,1,1,1,1,1,1,1,2,
    2,7,2
};

/* ------------------------------------------------------------------ */

static void refresh_target(zs_keyboard_t *kb)
{
    if (kb->target == NULL) {
        return;
    }
    if (!kb->hidden) {
        lv_label_set_text(kb->target, kb->text);
        return;
    }
    /* Prikker i stedet for tegn. Vi bygger dem i en lokal buffer og
     * skriver aldrig kodeordet ud nogen steder. */
    char dots[ZS_PASS_MAX * 3];
    size_t n = strlen(kb->text);
    size_t j = 0;
    for (size_t i = 0; i < n && j + 3 < sizeof(dots); i++) {
        /* U+2022 bullet, tre bytes i UTF-8 */
        dots[j++] = (char)0xE2;
        dots[j++] = (char)0x80;
        dots[j++] = (char)0xA2;
    }
    dots[j] = '\0';
    lv_label_set_text(kb->target, dots);
}

static void set_map(zs_keyboard_t *kb)
{
    if (kb->numeric) {
        lv_btnmatrix_set_map(kb->matrix, map_num);
        lv_btnmatrix_set_ctrl_map(kb->matrix, ctrl_num);
    } else {
        lv_btnmatrix_set_map(kb->matrix, kb->upper ? map_upper : map_lower);
        lv_btnmatrix_set_ctrl_map(kb->matrix, ctrl_letters);
    }
}

/* Tilfoejer et tegn hvis der er plads. Er der ikke, sker der
 * ingenting: det er bedre end at klippe kodeordet af i den ene ende
 * uden at brugeren opdager det. */
static void append(zs_keyboard_t *kb, const char *s)
{
    size_t have = strlen(kb->text);
    size_t add  = strlen(s);
    if (have + add >= sizeof(kb->text)) {
        return;
    }
    memcpy(kb->text + have, s, add + 1);
}

/* Fjerner det sidste tegn. Skal kunne haandtere at æ, ø og å fylder
 * TO bytes i UTF-8: sletter man kun én byte, staar der en halv
 * bogstav tilbage som resten af programmet ikke kan laese. */
static void backspace(zs_keyboard_t *kb)
{
    size_t n = strlen(kb->text);
    if (n == 0) {
        return;
    }
    size_t i = n - 1;
    /* Gaa tilbage forbi alle fortsaettelses-bytes (0b10xxxxxx). */
    while (i > 0 && ((unsigned char)kb->text[i] & 0xC0) == 0x80) {
        i--;
    }
    kb->text[i] = '\0';
}

static void on_key(lv_event_t *e)
{
    zs_keyboard_t *kb = lv_event_get_user_data(e);
    lv_obj_t *m = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(m);
    const char *txt = lv_btnmatrix_get_btn_text(m, id);
    if (txt == NULL) {
        return;
    }

    if (strcmp(txt, K_SHIFT) == 0) {
        kb->upper = !kb->upper;
        set_map(kb);
        return;
    }
    if (strcmp(txt, K_NUM) == 0) {
        kb->numeric = true;
        set_map(kb);
        return;
    }
    if (strcmp(txt, K_ABC) == 0) {
        kb->numeric = false;
        set_map(kb);
        return;
    }
    if (strcmp(txt, K_DEL) == 0) {
        backspace(kb);
        refresh_target(kb);
        return;
    }
    if (strcmp(txt, K_OK) == 0) {
        if (kb->done_cb != NULL) {
            kb->done_cb(e);
        }
        return;
    }
    if (strcmp(txt, "#+=") == 0) {
        /* Reserveret til flere tegn. Indtil da sker der ingenting,
         * hellere end at tasten goer noget uventet. */
        return;
    }

    append(kb, txt);
    /* Efter det foerste store bogstav gaar vi tilbage til smaa, som
     * ethvert andet tastatur. Ellers skriver folk MITWIFI. */
    if (kb->upper && !kb->numeric) {
        kb->upper = false;
        set_map(kb);
    }
    refresh_target(kb);
}

zs_keyboard_t *zs_keyboard_create(lv_obj_t *parent, lv_obj_t *target,
                                  lv_event_cb_t done_cb, void *user_data)
{
    zs_keyboard_t *kb = lv_mem_alloc(sizeof(zs_keyboard_t));
    if (kb == NULL) {
        return NULL;
    }
    memset(kb, 0, sizeof(*kb));
    kb->target = target;
    kb->done_cb = done_cb;
    kb->user_data = user_data;
    kb->hidden = true;

    kb->matrix = lv_btnmatrix_create(parent);
    lv_obj_remove_style_all(kb->matrix);
    lv_obj_set_size(kb->matrix, ZS_SCR_WIDTH, ZS_KB_HEIGHT);
    lv_obj_set_pos(kb->matrix, 0, ZS_SCR_HEIGHT - ZS_KB_HEIGHT);
    lv_obj_set_style_pad_all(kb->matrix, 6, 0);
    lv_obj_set_style_pad_gap(kb->matrix, 4, 0);
    lv_obj_set_style_bg_color(kb->matrix, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(kb->matrix, LV_OPA_COVER, 0);

    /* Selve tasterne */
    lv_obj_set_style_bg_color(kb->matrix, lv_color_hex(ZS_C_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb->matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb->matrix, 8, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb->matrix, lv_color_hex(ZS_C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb->matrix, &zs_font_kb_24, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb->matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb->matrix, 0, LV_PART_ITEMS);

    /* Trykket skal kunne ses. Det er den eneste tilbagemelding
     * brugeren faar paa at tasten blev ramt. */
    lv_obj_set_style_bg_color(kb->matrix, lv_color_hex(ZS_C_ACCENT),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb->matrix, lv_color_hex(ZS_C_BG),
                                LV_PART_ITEMS | LV_STATE_PRESSED);

    set_map(kb);
    lv_obj_add_event_cb(kb->matrix, on_key, LV_EVENT_VALUE_CHANGED, kb);
    /* Ingen markering af "valgt tast" bagefter: tastaturet skal ikke
     * huske hvor fingeren sidst var. */
    lv_btnmatrix_set_one_checked(kb->matrix, false);

    refresh_target(kb);
    return kb;
}

const char *zs_keyboard_get_text(zs_keyboard_t *kb)
{
    return kb != NULL ? kb->text : "";
}

void zs_keyboard_clear(zs_keyboard_t *kb)
{
    if (kb == NULL) {
        return;
    }
    /* Nulstil hele bufferen, ikke bare det foerste tegn. Et gammelt
     * kodeord skal ikke ligge og vente i hukommelsen. */
    memset(kb->text, 0, sizeof(kb->text));
    kb->upper = false;
    kb->numeric = false;
    set_map(kb);
    refresh_target(kb);
}

void zs_keyboard_set_password_hidden(zs_keyboard_t *kb, bool hidden)
{
    if (kb == NULL) {
        return;
    }
    kb->hidden = hidden;
    refresh_target(kb);
}

bool zs_keyboard_get_password_hidden(zs_keyboard_t *kb)
{
    return kb != NULL && kb->hidden;
}
