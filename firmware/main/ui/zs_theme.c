/*
 * zScreen - designsystem. Se zs_theme.h for reglerne.
 */

#include "zs_theme.h"
#include "zs_config.h"

#include <string.h>

/*
 * Stilarterne er static og bliver genbrugt.
 *
 * LVGL kopierer ikke en stil naar man saetter den paa et objekt, den
 * gemmer en pegepind. Derfor skal de leve lige saa laenge som skaermen,
 * og derfor bliver de sat op én gang i stedet for at hvert kort bygger
 * sin egen. Med fire kort, en liste med tyve netvaerk og et tastatur
 * med tredive taster ville det ellers blive til mange hundrede
 * ens kopier i hukommelsen.
 */
static lv_style_t s_card;
static lv_style_t s_card_pressed;
static lv_style_t s_btn_primary;
static lv_style_t s_btn_primary_pressed;
static lv_style_t s_btn_secondary;
static lv_style_t s_btn_secondary_pressed;
static lv_style_t s_row;
static lv_style_t s_row_pressed;

static bool s_inited = false;

/*
 * De to paletter.
 *
 * Raekkefoelgen foelger zs_col_id_t. Der er en test der tjekker at
 * begge raekker har lige saa mange farver som der er navne, saa en
 * tilfoejet farve ikke kan blive glemt i den ene palet og give sort.
 *
 * Alle kombinationer er maalt mod WCAG. Se tests/host/test_theme.c for
 * de faktiske tal, og for hvilke der er tekst og hvilke der er grafik.
 */
static const uint32_t PALET[ZS_THEME_COUNT][ZS_ID_COUNT] = {
    [ZS_THEME_DARK] = {
        [ZS_ID_BG]           = 0x0E2A29,
        [ZS_ID_CARD]         = 0x16403E,
        [ZS_ID_CARD_PRESSED] = 0x1D504D,
        [ZS_ID_BORDER]       = 0x205A57,
        [ZS_ID_TEXT]         = 0xFFFFFF,
        [ZS_ID_TEXT_DIM]     = 0xB6D0CE,
        [ZS_ID_LABEL]        = 0x8FB3B1,
        /* Haevet fra 5C8280. Den gamle havde 2,70:1 mod kortet, og en
         * gammel maaling skal vaere daempet, ikke ulaeselig. Samme
         * kuloer, kun lysere. Se tools/check-colors.py. */
        [ZS_ID_STALE]        = 0x628B88,
        [ZS_ID_VALUE]        = ZS_BRAND_ORANGE,
        /* Maerkerne er lyse flader, saa skriften paa dem er bunden. */
        [ZS_ID_BADGE_TEXT]   = 0x0E2A29,
        [ZS_ID_ACCENT]       = ZS_BRAND_ORANGE,
        [ZS_ID_GOOD]         = 0x4ADE80,
        /* Haevet fra F87171: 4,13:1 mod kortet, under de 4,50 tekst
         * kraever. Samme roede, kun lysere. */
        [ZS_ID_BAD]          = 0xF97E7E,
        [ZS_ID_WARN]         = 0xFBBF24,
    },
    [ZS_THEME_LIGHT] = {
        [ZS_ID_BG]           = 0xF7FAF9,
        [ZS_ID_CARD]         = 0xFFFFFF,
        [ZS_ID_CARD_PRESSED] = 0xEAF1F0,
        [ZS_ID_BORDER]       = 0xCFDEDB,
        [ZS_ID_TEXT]         = 0x0E2A29,
        [ZS_ID_TEXT_DIM]     = 0x3D5F5D,
        [ZS_ID_LABEL]        = 0x4A6866,
        [ZS_ID_STALE]        = 0x699997,
        /* Tallene: brandets moerkegroenne, 9,95:1 mod hvid. */
        [ZS_ID_VALUE]        = ZS_BRAND_GREEN,
        [ZS_ID_BADGE_TEXT]   = 0xF7FAF9,
        /* Orangen toneret ned til 4,5:1. Brandets egen har 1,8:1 mod
         * hvid og kan ikke laeses. */
        [ZS_ID_ACCENT]       = 0x9E6803,
        [ZS_ID_GOOD]         = 0x15803D,
        [ZS_ID_BAD]          = 0xC02626,
        [ZS_ID_WARN]         = 0xA16207,
    },
    /*
     * Roedt. Bunden er #D73338, som den skal vaere.
     *
     * En maettet roed i den lysstyrke er et haardt laerred. Kun
     * naesten hvid tekst naar de 4,5:1 som smaa bogstaver kraever, saa
     * det her tema har mindre forskel mellem overskrift og etiket end
     * de to andre. Det er en foelge af farven, ikke en forglemmelse.
     *
     * Maerkerne er vendt om i forhold til de andre temaer: lyse flader
     * med moerk skrift. Moerke maerker ville ikke kunne skelnes fra
     * bunden, og lyse med lys skrift kunne ikke laeses.
     */
    [ZS_THEME_RED] = {
        [ZS_ID_BG]           = 0xD73338,
        [ZS_ID_CARD]         = 0xB82429,
        [ZS_ID_CARD_PRESSED] = 0xA81F24,
        [ZS_ID_BORDER]       = 0xE8686C,
        [ZS_ID_TEXT]         = 0xFFFFFF,
        [ZS_ID_TEXT_DIM]     = 0xFEF8F8,
        [ZS_ID_LABEL]        = 0xFEF8F8,
        [ZS_ID_STALE]        = 0xF0C3C5,
        [ZS_ID_VALUE]        = 0xFFFFFF,
        [ZS_ID_BADGE_TEXT]   = 0x5A1114,
        [ZS_ID_ACCENT]       = 0xFFFFFF,
        /*
         * Kun en anelse kuloer.
         *
         * Paa en maettet roed bund kan en raesonabel groen ikke laeses:
         * alt hvad der har farve nok til at ses som groent, falder
         * under de 4,5:1 som smaa bogstaver kraever. Derfor blege
         * toner, hvor ordet baerer betydningen og farven kun hvisker.
         *
         * Roed er helt hvid. En roed tone paa roed bund naar kun 4,44,
         * og den ville alligevel forsvinde i bunden i stedet for at
         * raabe op.
         */
        [ZS_ID_GOOD]         = 0xEDFFF4,
        [ZS_ID_BAD]          = 0xFFFFFF,
        [ZS_ID_WARN]         = 0xFFFBEA,
    },
};

/*
 * Hvilket logo hoerer til hvilket tema.
 *
 * Negativt logo er hvidt og skal paa moerk bund. Positivt er
 * moerkegroent og skal paa lys. Skal et tema en dag have sit helt eget
 * logo, er det den her tabel der udvides, og intet andet sted i koden
 * skal roeres.
 */
static const bool LOGO_NEGATIV[ZS_THEME_COUNT] = {
    [ZS_THEME_DARK]  = true,
    [ZS_THEME_LIGHT] = false,
    [ZS_THEME_RED]   = true,
};

/*
 * Lageret gemmer temaet som et tal og kender ikke opregningen her.
 * Grænsen staar i zs_config.h. Den her linje sikrer at de to foelges
 * ad: tilfoejer nogen et tema uden at rette graensen, oversaetter
 * firmwaren ikke, i stedet for at det nye tema stille bliver nulstillet
 * til moerkt ved hver genstart.
 */
_Static_assert(ZS_THEME_MAKS == ZS_THEME_COUNT - 1,
               "ZS_THEME_MAKS i zs_config.h passer ikke med antallet af temaer");

static zs_theme_mode_t s_mode = ZS_THEME_DARK;

uint32_t zs_col(zs_col_id_t id)
{
    if ((unsigned)id >= ZS_ID_COUNT) {
        /* Kan ikke ske med opregningen, men en farve ud af det blaa er
         * lettere at faa oeje paa end sort paa sort. */
        return 0xFF00FF;
    }
    return PALET[s_mode][id];
}

zs_theme_mode_t zs_theme_mode(void)
{
    return s_mode;
}

const char *zs_theme_name(zs_theme_mode_t m)
{
    switch (m) {
    case ZS_THEME_LIGHT: return "Lyst";
    case ZS_THEME_RED:   return "Rødt";
    default:             return "Mørkt";
    }
}

const char *zs_theme_hint(zs_theme_mode_t m)
{
    switch (m) {
    case ZS_THEME_LIGHT:
        return "Lys bund. Bedst i et rum med meget dagslys.";
    case ZS_THEME_RED:
        return "Rød bund med hvid skrift. Ses tydeligt på afstand.";
    default:
        return "Mørk grøn bund. Lyser ikke rummet op om aftenen.";
    }
}

const lv_img_dsc_t *zs_logo_zmark(void)
{
    return LOGO_NEGATIV[s_mode] ? &zs_img_zmark : &zs_img_zmark_pos;
}

const lv_img_dsc_t *zs_logo_wordmark(void)
{
    return LOGO_NEGATIV[s_mode] ? &zs_img_wordmark : &zs_img_wordmark_pos;
}

void zs_style_text(lv_obj_t *obj, const lv_font_t *font, uint32_t color)
{
    if (font != NULL) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

/* Faellestraek for alt der ligner et kort eller en knap: ingen skygge,
 * intet forloeb, ingen kant vi ikke selv har bedt om. */
/*
 * lv_style_init nulstiller stilen. Det maa kun ske FOERSTE gang: paa en
 * stil der allerede er i brug ville den smide de vaerdier vaek som
 * objekterne regner med, og efterlade dem uden baggrund. Anden gang
 * skriver vi bare de nye farver oven i.
 */
static bool s_foerste = true;

static void nulstil_foerste_gang(lv_style_t *st)
{
    if (s_foerste) {
        lv_style_init(st);
    }
}

static void base_surface(lv_style_t *st, uint32_t bg, uint32_t border,
                         lv_coord_t radius, lv_coord_t border_w)
{
    nulstil_foerste_gang(st);
    lv_style_set_bg_color(st, lv_color_hex(bg));
    lv_style_set_bg_opa(st, LV_OPA_COVER);
    lv_style_set_radius(st, radius);
    lv_style_set_border_width(st, border_w);
    if (border_w > 0) {
        lv_style_set_border_color(st, lv_color_hex(border));
        lv_style_set_border_opa(st, LV_OPA_COVER);
    }
    /* Ingen skygge og intet forloeb. Det er en bevidst beslutning, ikke
     * en forglemmelse: begge dele er de foerste ting der faar en flade
     * til at ligne noget der er genereret frem for tegnet. */
    lv_style_set_shadow_width(st, 0);
    lv_style_set_bg_grad_dir(st, LV_GRAD_DIR_NONE);
    lv_style_set_outline_width(st, 0);
}

/*
 * Fylder farver i de delte stilarter.
 *
 * Kaldes baade ved opstart og hver gang temaet skifter. lv_style_set_*
 * paa en stil der allerede er sat op skriver bare vaerdien om, saa der
 * er ingen grund til at rive dem ned og bygge dem op igen. De objekter
 * der peger paa dem beholder deres pegepind.
 */
static void fyld_stilarter(void)
{
    base_surface(&s_card, ZS_C_CARD, ZS_C_BORDER, ZS_CARD_RADIUS, 1);
    lv_style_set_pad_all(&s_card, ZS_CARD_PAD);

    nulstil_foerste_gang(&s_card_pressed);
    lv_style_set_bg_color(&s_card_pressed, lv_color_hex(ZS_C_CARD_PRESSED));

    base_surface(&s_btn_primary, ZS_C_ACCENT, 0, 14, 0);
    lv_style_set_pad_hor(&s_btn_primary, 22);
    lv_style_set_text_color(&s_btn_primary, lv_color_hex(ZS_C_BG));
    lv_style_set_text_font(&s_btn_primary, &zs_font_20);

    nulstil_foerste_gang(&s_btn_primary_pressed);
    /* Nedtonet i stedet for at skifte farve. Et tryk skal ses, ikke
     * fejres, og en knap der skifter kuloer virker som en anden knap. */
    lv_style_set_bg_opa(&s_btn_primary_pressed, LV_OPA_80);

    base_surface(&s_btn_secondary, ZS_C_BG, ZS_C_BORDER, 14, 1);
    lv_style_set_bg_opa(&s_btn_secondary, LV_OPA_TRANSP);
    lv_style_set_pad_hor(&s_btn_secondary, 22);
    lv_style_set_text_color(&s_btn_secondary, lv_color_hex(ZS_C_TEXT));
    lv_style_set_text_font(&s_btn_secondary, &zs_font_20);

    nulstil_foerste_gang(&s_btn_secondary_pressed);
    lv_style_set_bg_color(&s_btn_secondary_pressed, lv_color_hex(ZS_C_CARD));
    lv_style_set_bg_opa(&s_btn_secondary_pressed, LV_OPA_COVER);

    base_surface(&s_row, ZS_C_CARD, ZS_C_BORDER, 14, 1);
    lv_style_set_pad_hor(&s_row, 16);
    lv_style_set_pad_ver(&s_row, 0);

    nulstil_foerste_gang(&s_row_pressed);
    lv_style_set_bg_color(&s_row_pressed, lv_color_hex(ZS_C_CARD_PRESSED));

    /* Skaermens bund. Saettes her saa ingen skaerm glemmer det og
     * efterlader en sort kant der ikke passer til resten. */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

void zs_theme_init(void)
{
    if (s_inited) {
        return;
    }
    fyld_stilarter();
    s_foerste = false;
    s_inited  = true;
}

void zs_theme_set_mode(zs_theme_mode_t m)
{
    if ((unsigned)m >= ZS_THEME_COUNT) {
        return;
    }
    if (m == s_mode) {
        return;
    }
    s_mode = m;
    if (!s_inited) {
        /* Temaet blev valgt foer brugerfladen blev bygget. Saa er der
         * ingen stilarter at rette, og den bygges rigtigt fra start. */
        return;
    }
    fyld_stilarter();
    /* Fortael LVGL at de delte stilarter har aendret sig, saa alt der
     * bruger dem bliver tegnet om. Det daekker kort, knapper og rader.
     * Farver der er sat direkte paa et enkelt objekt sidder fast, og
     * dem tager zs_ui_set_theme() sig af ved at bygge siderne om. */
    lv_obj_report_style_change(NULL);
}

lv_obj_t *zs_card_create(lv_obj_t *parent, bool pressable)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &s_card, 0);
    if (pressable) {
        lv_obj_add_style(card, &s_card_pressed, LV_STATE_PRESSED);
    } else {
        /* Ikke bare uden tryk-stil: kortet maa heller ikke TAGE imod
         * tryk. Ellers spiser det bevaegelsen fra fingeren, og siden
         * kan ikke traekkes til side ovenpaa et kort. */
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    }
    /* Kortene skal ikke kunne rulles. Uden dette kan en finger der
     * glider en anelse under et tryk faa hele kortet til at vippe. */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    return card;
}

lv_obj_t *zs_label_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text != NULL ? text : "");
    zs_style_text(lbl, &zs_font_13, ZS_C_LABEL);
    /* Lidt luft mellem bogstaverne. Versaler i smaa stoerrelser bliver
     * ellers til en klump man skal laese to gange. */
    lv_obj_set_style_text_letter_space(lbl, 1, 0);
    return lbl;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                             lv_style_t *base, lv_style_t *pressed)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, base, 0);
    lv_obj_add_style(btn, pressed, LV_STATE_PRESSED);
    lv_obj_set_height(btn, ZS_BTN_HEIGHT);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text != NULL ? text : "");
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *zs_btn_primary_create(lv_obj_t *parent, const char *text)
{
    return make_button(parent, text, &s_btn_primary, &s_btn_primary_pressed);
}

lv_obj_t *zs_btn_secondary_create(lv_obj_t *parent, const char *text)
{
    return make_button(parent, text, &s_btn_secondary, &s_btn_secondary_pressed);
}

lv_obj_t *zs_column_create(lv_obj_t *parent, lv_coord_t gap)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, ZS_CONTENT_WIDTH);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, gap, 0);
    lv_obj_set_style_pad_ver(col, ZS_EDGE, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

void zs_row_create(zs_row_t *out, lv_obj_t *parent, const char *icon,
                   const char *title, const char *value, bool chevron)
{
    memset(out, 0, sizeof(*out));

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &s_row, 0);
    lv_obj_add_style(row, &s_row_pressed, LV_STATE_PRESSED);
    lv_obj_set_height(row, ZS_ROW_HEIGHT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    out->row = row;

    lv_coord_t x = 0;

    if (icon != NULL) {
        out->icon = lv_label_create(row);
        lv_label_set_text(out->icon, icon);
        lv_obj_set_style_text_font(out->icon, &zs_icons_20, 0);
        lv_obj_set_style_text_color(out->icon, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(out->icon, LV_ALIGN_LEFT_MID, 0, 0);
        x = ZS_ROW_ICON_W;
    }

    if (title != NULL) {
        out->title = lv_label_create(row);
        lv_label_set_text(out->title, title);
        zs_style_text(out->title, &zs_font_20, ZS_C_TEXT);
        lv_label_set_long_mode(out->title, LV_LABEL_LONG_DOT);
        lv_obj_align(out->title, LV_ALIGN_LEFT_MID, x, 0);

        /*
         * Bredden skal saettes, ellers vokser etiketten ud over raden
         * og et langt netvaerksnavn skriver hen over vaerdien til
         * hoejre. Vi traekker fra hvad der er optaget: ikonet til
         * venstre, og vaerdien og pilen til hoejre.
         */
        lv_coord_t right = 0;
        if (chevron)       { right += 30; }
        if (value != NULL) { right += 100; }
        lv_obj_set_width(out->title,
                         ZS_CONTENT_WIDTH - 2 * 16 - x - right);
    }

    lv_coord_t rx = 0;
    if (chevron) {
        out->chevron = lv_label_create(row);
        lv_label_set_text(out->chevron, ZS_ICON_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(out->chevron, &zs_icons_20, 0);
        lv_obj_set_style_text_color(out->chevron, lv_color_hex(ZS_C_LABEL), 0);
        lv_obj_align(out->chevron, LV_ALIGN_RIGHT_MID, 0, 0);
        rx = -30;
    }

    if (value != NULL) {
        out->value = lv_label_create(row);
        lv_label_set_text(out->value, value);
        zs_style_text(out->value, &zs_font_16, ZS_C_LABEL);
        lv_obj_align(out->value, LV_ALIGN_RIGHT_MID, rx, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Sidens ramme                                                        */
/* ------------------------------------------------------------------ */

void zs_page_create(zs_page_t *p, const char *title,
                    lv_event_cb_t back_cb, void *user_data,
                    bool with_footer)
{
    memset(p, 0, sizeof(*p));

    p->root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(p->root);
    lv_obj_set_size(p->root, ZS_SCR_WIDTH, ZS_SCR_HEIGHT);
    lv_obj_set_pos(p->root, 0, 0);
    lv_obj_clear_flag(p->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(p->root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(p->root, lv_color_hex(ZS_C_BG), 0);
    lv_obj_set_style_bg_opa(p->root, LV_OPA_COVER, 0);

    /* ── hoved ── */
    p->head = lv_obj_create(p->root);
    lv_obj_remove_style_all(p->head);
    lv_obj_set_size(p->head, ZS_SCR_WIDTH, ZS_PAGE_HEAD_HEIGHT);
    lv_obj_set_pos(p->head, 0, 0);
    lv_obj_clear_flag(p->head, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t title_x = ZS_EDGE + 4;

    if (back_cb != NULL) {
        /* Hele knappen er 44 x 44 selvom pilen er 20 px. Se noten om
         * fingerflader i zs_theme.h. */
        p->back = lv_btn_create(p->head);
        lv_obj_remove_style_all(p->back);
        lv_obj_set_size(p->back, ZS_TOUCH_MIN, ZS_TOUCH_MIN);
        lv_obj_set_pos(p->back, 4, (ZS_PAGE_HEAD_HEIGHT - ZS_TOUCH_MIN) / 2);
        lv_obj_add_event_cb(p->back, back_cb, LV_EVENT_CLICKED, user_data);

        lv_obj_t *ic = lv_label_create(p->back);
        lv_label_set_text(ic, ZS_ICON_ARROW_LEFT);
        lv_obj_set_style_text_font(ic, &zs_icons_20, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(ZS_C_TEXT), 0);
        lv_obj_center(ic);

        title_x = 4 + ZS_TOUCH_MIN + 8;
    }

    p->title = lv_label_create(p->head);
    lv_label_set_text(p->title, title != NULL ? title : "");
    zs_style_text(p->title, &zs_font_28, ZS_C_TEXT);
    lv_label_set_long_mode(p->title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(p->title, ZS_SCR_WIDTH - title_x - ZS_EDGE);
    lv_obj_align(p->title, LV_ALIGN_LEFT_MID, title_x, 0);

    /* ── fod ── */
    lv_coord_t foot_h = with_footer ? ZS_PAGE_FOOT_HEIGHT : 0;
    if (with_footer) {
        p->footer = lv_obj_create(p->root);
        lv_obj_remove_style_all(p->footer);
        lv_obj_set_size(p->footer, ZS_SCR_WIDTH, ZS_PAGE_FOOT_HEIGHT);
        lv_obj_set_pos(p->footer, 0, ZS_SCR_HEIGHT - ZS_PAGE_FOOT_HEIGHT);
        lv_obj_clear_flag(p->footer, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(p->footer, ZS_EDGE, 0);
    }

    /* ── indhold ── */
    p->content = lv_obj_create(p->root);
    lv_obj_remove_style_all(p->content);
    lv_obj_set_size(p->content, ZS_SCR_WIDTH,
                    ZS_SCR_HEIGHT - ZS_PAGE_HEAD_HEIGHT - foot_h);
    lv_obj_set_pos(p->content, 0, ZS_PAGE_HEAD_HEIGHT);
    /* Ingen luft i siderne. Se noten ved ZS_CONTENT_WIDTH: alt paa en
     * side bruger skaermens egne koordinater, saa der kun er ét sted at
     * regne fra. */
    lv_obj_set_style_pad_hor(p->content, 0, 0);
    lv_obj_set_style_pad_ver(p->content, 0, 0);
    /* Kun lodret rulning. Vandret ville betyde at et uheldigt strejf
     * kunne skubbe hele indholdet ud til siden. */
    lv_obj_set_scroll_dir(p->content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p->content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(p->content, LV_OPA_TRANSP, 0);

    /* Rullebjaelken skal kunne ses paa den moerke bund uden at fylde. */
    lv_obj_set_style_bg_color(p->content, lv_color_hex(ZS_C_BORDER), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(p->content, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(p->content, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(p->content, 2, LV_PART_SCROLLBAR);
}

void zs_page_set_hidden(zs_page_t *p, bool hidden)
{
    if (p == NULL || p->root == NULL) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *zs_choice_create(lv_obj_t *parent, const char *titel,
                           const char *forklaring, lv_coord_t y, bool valgt,
                           lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, ZS_CONTENT_WIDTH, ZS_CHOICE_HEIGHT);
    lv_obj_set_pos(b, ZS_EDGE, y);   /* samme kant som alt andet */
    lv_obj_set_style_bg_color(b, lv_color_hex(ZS_C_CARD), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 16, 0);

    /*
     * Det valgte faar en tykkere kant i accentfarven.
     *
     * Kanten og ikke baggrunden, for en fyldt flade ville se ud som en
     * knap der er trykket ned lige nu. Tykkelsen er 2 mod 1, hvilket er
     * nok til at oejet finder den uden at raekken hopper: kanten tegnes
     * indad i LVGL, saa knappen fylder det samme.
     */
    lv_obj_set_style_border_width(b, valgt ? 2 : 1, 0);
    lv_obj_set_style_border_color(b,
        lv_color_hex(valgt ? ZS_C_ACCENT : ZS_C_BORDER), 0);
    lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);

    lv_obj_set_style_bg_color(b, lv_color_hex(ZS_C_CARD_PRESSED),
                              LV_STATE_PRESSED);
    if (cb != NULL) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t *t = lv_label_create(b);
    lv_label_set_text(t, titel);
    zs_style_text(t, &zs_font_28, ZS_C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 14);

    if (forklaring != NULL && forklaring[0] != '\0') {
        lv_obj_t *f = lv_label_create(b);
        lv_label_set_text(f, forklaring);
        zs_style_text(f, &zs_font_16, ZS_C_LABEL);
        lv_label_set_long_mode(f, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(f, ZS_CONTENT_WIDTH - 32);
        lv_obj_align(f, LV_ALIGN_TOP_LEFT, 16, 50);
    }
    return b;
}
