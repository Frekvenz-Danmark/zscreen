#include "zs_keyboard.h"
#include "zs_theme.h"
#include "zs_nvs.h"      /* ZS_PASS_MAX */

#include "esp_log.h"

#include <string.h>

/* lv_async_call, se set_map_later nedenfor. */
#include "misc/lv_async.h"

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
    "-","/",":",";","(",")","$","&","@","\"","\n",
    "#+=",".",",","?","!","'","_","%",K_DEL,"\n",
    K_ABC, K_SPACE, K_OK, ""
};

/*
 * Bredde og opfoersel for hver tast.
 *
 * De TRE nederste bits er bredden, resten er flag. Bredden kan altsaa
 * hoejst vaere 7: skriver man 8, saetter man i stedet flaget
 * LV_BTNMATRIX_CTRL_HIDDEN og tasten forsvinder uden en fejl.
 *
 * De to flag er ikke pynt. Uden dem opfoerer tastaturet sig forkert,
 * og det er efterproevet paa hardwaren:
 *
 *   CLICK_TRIG  Uden det sender LVGL et tastetryk allerede naar
 *               fingeren rammer, OG igen hver gang fingeren glider hen
 *               over en ny tast. Et lille smut med tommelfingeren
 *               skrev derfor den tast man endte paa i stedet for den
 *               man ramte. Med flaget sendes tastetrykket foerst naar
 *               fingeren slippes, og kun hvis den slippes paa den
 *               samme tast.
 *
 *   NO_REPEAT   Uden det gentages tasten mens man holder den nede.
 *               Et sekunds toeven paa slettetasten ville rydde hele
 *               kodeordet.
 *
 * Kilde: lv_btnmatrix.c i LVGL 8.3.1, hvor VALUE_CHANGED sendes fra
 * baade LV_EVENT_PRESSED, LV_EVENT_PRESSING og
 * LV_EVENT_LONG_PRESSED_REPEAT naar flagene ikke er sat.
 */
#define KB_KEY(w)  ((lv_btnmatrix_ctrl_t)((w) \
                    | LV_BTNMATRIX_CTRL_CLICK_TRIG \
                    | LV_BTNMATRIX_CTRL_NO_REPEAT))

/* Skift og slet er dobbelt saa brede. De bliver ramt tit, og et
 * fejltryk paa dem koster mere end paa et bogstav: enten forsvinder et
 * tegn, eller ogsaa skifter hele tastaturet. */
static const lv_btnmatrix_ctrl_t ctrl_letters[] = {
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(2),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(2),
    KB_KEY(2),KB_KEY(7),KB_KEY(2)
};

static const lv_btnmatrix_ctrl_t ctrl_num[] = {
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(2),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),KB_KEY(1),
    KB_KEY(1),KB_KEY(1),KB_KEY(2),
    KB_KEY(2),KB_KEY(7),KB_KEY(2)
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

/*
 * Skifter layout SENERE, ikke midt i tastetrykket.
 *
 * lv_btnmatrix_set_map() bygger hele knapgitteret om: den taeller
 * knapperne igen og laegger nye omraader og kontrolbits ud. Goer man
 * det inde fra tryk-haandteringen, arbejder resten af LVGL's
 * haendelseskaede videre paa et gitter der er skiftet under den. I
 * praksis betoed det at tastaturet opfoerte sig som om en helt anden
 * tast var trykket, og at man skulle trykke flere gange foer der skete
 * noget.
 *
 * lv_async_call koerer funktionen paa naeste gennemloeb af LVGL's egen
 * opgave, altsaa efter at trykket er helt faerdigbehandlet.
 */
static void set_map_async(void *arg)
{
    set_map((zs_keyboard_t *)arg);
}

static void set_map_later(zs_keyboard_t *kb)
{
    /* Annuller et evt. ventende skift foerst, saa to hurtige tryk paa
     * skift ikke laegger to omgange i koe. */
    lv_async_call_cancel(set_map_async, kb);
    lv_async_call(set_map_async, kb);
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
        set_map_later(kb);
        return;
    }
    if (strcmp(txt, K_NUM) == 0) {
        kb->numeric = true;
        set_map_later(kb);
        return;
    }
    if (strcmp(txt, K_ABC) == 0) {
        kb->numeric = false;
        set_map_later(kb);
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
    refresh_target(kb);
    /* Efter det foerste store bogstav gaar vi tilbage til smaa, som
     * ethvert andet tastatur. Ellers skriver folk MITWIFI. */
    if (kb->upper && !kb->numeric) {
        kb->upper = false;
        set_map_later(kb);
    }
}

/* Taeller taster i et layout, praecis som LVGL selv goer det:
 * "\n" er en raekkeskifter og "" afslutter listen. */
static uint16_t count_keys(const char **map)
{
    uint16_t n = 0;
    for (const char **p = map; *p != NULL && (*p)[0] != '\0'; p++) {
        if (strcmp(*p, "\n") != 0) {
            n++;
        }
    }
    return n;
}

/*
 * Layout og bredder staar i hver sin liste, og LVGL laeser dem parvis.
 * Er de ikke lige lange, laeser LVGL bredder for taster der ikke
 * findes, eller giver de sidste taster bredde nul. Ingen af delene
 * giver en fejl, bare et tastatur der ser forkert ud.
 *
 * Vi taeller efter ved opstart. Det koster nogle mikrosekunder én gang
 * og fanger en fejl der ellers foerst ville blive set paa en skaerm
 * hos en kunde.
 */
static bool maps_are_consistent(void)
{
    struct { const char *navn; const char **map; size_t n_ctrl; } t[] = {
        { "smaa bogstaver", map_lower, sizeof(ctrl_letters) / sizeof(ctrl_letters[0]) },
        { "store bogstaver", map_upper, sizeof(ctrl_letters) / sizeof(ctrl_letters[0]) },
        { "tal og tegn",     map_num,   sizeof(ctrl_num) / sizeof(ctrl_num[0]) },
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        uint16_t n = count_keys(t[i].map);
        if (n != t[i].n_ctrl) {
            ESP_LOGE("keyboard", "layout \"%s\" har %u taster men %u bredder",
                     t[i].navn, (unsigned)n, (unsigned)t[i].n_ctrl);
            ok = false;
        }
    }
    return ok;
}

zs_keyboard_t *zs_keyboard_create(lv_obj_t *parent, lv_obj_t *target,
                                  lv_event_cb_t done_cb, void *user_data)
{
    if (!maps_are_consistent()) {
        return NULL;
    }
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
    /* Her er vi IKKE inde i et tastetryk, saa layoutet maa gerne
     * skiftes med det samme. */
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
