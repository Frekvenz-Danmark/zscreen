/*
 * ZDisplay - LVGL 8.3 oven paa et MIPI-DSI-panel.
 *
 * Samme kontrakt som zScreens lv_port.c (se firmware/kerne/hal/lv_port.h):
 * lv_port_init(), og en rekursiv laas om alt der roerer LVGL. Men
 * hardwaren under den er en anden, og derfor er filen sin egen.
 *
 * HVAD DER ER FORSKELLEN.
 *
 * Paa zScreen sidder panelet paa en RGB-bus, hvor ESP32'en selv klokker
 * hver pixel ud, og Seeeds bsp_lcd styrer det. Her er panelet paa
 * MIPI-DSI: P4'eren skriver ind i en billedbuffer i PSRAM, og en
 * selvstaendig blok laeser den buffer ud til skaermen 60 gange i
 * sekundet uden at spoerge om lov.
 *
 * Det betyder at "flush" ikke er en udsendelse, men en KOPI ind i den
 * buffer. Kopien laves af DMA2D og er derfor ikke faerdig naar
 * esp_lcd_panel_draw_bitmap() returnerer. Vi faar besked i
 * on_color_trans_done, og det er FOERST der at LVGL maa bruge sin
 * tegnebuffer igen.
 *
 * Det er maalt i ESP-IDF's egen kilde, ikke gaettet:
 * components/esp_lcd/dsi/esp_lcd_panel_dpi.c kalder on_color_trans_done
 * med det samme naar kopien laves af CPU'en, og fra DMA'ens egen
 * kvittering naar den laves af DMA2D. Seeeds panelopsaetning saetter
 * flags.use_dma2d = true, saa det er den sidste vej der gaelder. Kaldte
 * vi lv_disp_flush_ready() selv lige efter draw_bitmap, ville LVGL
 * begynde at tegne oven i en buffer der stadig blev laest ud af DMA'en,
 * og billedet ville rive i vilkaarlige striber.
 */

#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_touch.h"

#include "lvgl.h"
#include "lv_port.h"

#include "esp32_p4_re_terminal_d1001.h"
#include "display.h"
#include "touch.h"

#include "zs_display.h"
#include "zs_layout.h"

static const char *TAG = "lvgl_port";

/*
 * Tegnebufferens hoejde i linjer.
 *
 * 120 linjer x 800 px x 2 bytes = 192 KB, og der er to af dem i PSRAM.
 *
 * HVORFOR IKKE HELE SKAERMEN. En fuld buffer er 800 x 1280 x 2 = 2 MB,
 * og to af dem 4 MB. Der er 32 MB PSRAM, saa det KAN betales - men
 * hverken fladen eller tallene paa den aendrer sig en hel skaerm ad
 * gangen. Et nyt tal i et kort er nogle faa linjer, og saa er en kopi
 * paa 2 MB spildt arbejde 60 gange i sekundet.
 *
 * To buffere og ikke én, saa LVGL kan tegne i den ene mens DMA2D
 * kopierer den anden.
 */
#define BUF_LINJER      120
#define BUF_PIXELS      (ZS_G_SCR_WIDTH * BUF_LINJER)

/*
 * FLADEN OG PANELET SKAL VAERE ENIGE OM STOERRELSEN.
 *
 * ZS_G_SCR_WIDTH kommer fra byggeriet (se projektets CMakeLists), og
 * BSP_LCD_H_RES kommer fra Seeeds display.h. De to tal bruges til hver
 * sin ting: vores til at regne hele fladen ud, deres til at saette
 * touchens maksimum i bsp_touch_new().
 *
 * Og deres tal er BETINGET:
 *
 *     #if CONFIG_BSP_LCD_TYPE_1024_600   ->  1024 x 600
 *     #else                              ->   800 x 1280
 *
 * Den konstant findes ikke i deres Kconfig i dag, saa vi faar 800 x
 * 1280. Men tilfoejer Seeed det valg i en opdatering af submodulet, og
 * giver det en standard, saa flytter panelet sig under os: fladen ville
 * stadig regne i 800 x 1280, touchen ville blive skaleret til 1024 x
 * 600, og hvert tryk ville ramme et andet sted end der blev trykket.
 * Intet af det ville vaere en byggefejl.
 *
 * Derfor staar det her. Flytter tallene sig, falder byggeriet.
 */
_Static_assert(ZS_G_SCR_WIDTH == BSP_LCD_H_RES,
               "fladens bredde og panelets bredde er ikke den samme. "
               "Se CONFIG_BSP_LCD_TYPE_1024_600 i Seeeds display.h");
_Static_assert(ZS_G_SCR_HEIGHT == BSP_LCD_V_RES,
               "fladens hoejde og panelets hoejde er ikke den samme. "
               "Se CONFIG_BSP_LCD_TYPE_1024_600 i Seeeds display.h");

#define TASK_DELAY_MS   5

#ifndef CONFIG_LCD_TASK_PRIORITY
#define CONFIG_LCD_TASK_PRIORITY    5
#endif

static lv_disp_drv_t            disp_drv;
static esp_lcd_panel_handle_t   s_panel = NULL;
static esp_lcd_touch_handle_t   s_touch = NULL;
static SemaphoreHandle_t        lvgl_mutex = NULL;
static TaskHandle_t             lvgl_task_handle = NULL;

/* Kvitteringen fra DMA2D: nu maa LVGL bruge tegnebufferen igen. */
static bool flush_faerdig(esp_lcd_panel_handle_t panel,
                          esp_lcd_dpi_panel_event_data_t *edata,
                          void *user_ctx)
{
    (void)panel; (void)edata; (void)user_ctx;
    lv_disp_flush_ready(&disp_drv);
    /* Ingen opgave af hoejere prioritet er vaekket herinde. */
    return false;
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)drv;
    /*
     * Bemaerk at der IKKE kaldes lv_disp_flush_ready() i det gode
     * tilfaelde. Det sker i flush_faerdig naar kopien er ovre. Se noten
     * oeverst i filen.
     */
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel,
                                              area->x1, area->y1,
                                              area->x2 + 1, area->y2 + 1,
                                              (const void *)color_p);
    if (err != ESP_OK) {
        /*
         * HVORFOR DER KVITTERES HER ALLIGEVEL.
         *
         * Gaar kopien ikke i gang, bliver on_color_trans_done aldrig
         * kaldt - og saa venter LVGL paa en kvittering der ikke kommer.
         * Ikke i et sekund: for altid. Skaermen staar med det billede
         * der var, uret gaar i staa, og alt andet i firmwaren koerer
         * videre som om intet var haendt. Det er den vaerste slags fejl
         * paa en vaeg: enheden ser taendt ud og viser gamle tal.
         *
         * ESP-IDF's dpi-panel afviser netop en tegning hvis den
         * FORRIGE kopi ikke er faerdig (ESP_ERR_INVALID_STATE,
         * esp_lcd_panel_dpi.c). Det skal ikke kunne ske, fordi LVGL
         * foerst tegner igen efter en kvittering - men "skal ikke kunne
         * ske" er ikke det samme som "kan ikke ske", og prisen for at
         * tage fejl er en frossen skaerm.
         *
         * Derfor kvitterer vi selv. Rammen gaar tabt, og den naeste
         * bliver tegnet. Et glip paa ét billede ser ingen; en frossen
         * skaerm ser alle.
         */
        /*
         * KUN DEN FOERSTE LOGLINJE, og saa et tal.
         *
         * Gaar kopien i staa, sker det ved HVER ramme - og LVGL tegner
         * op til 200 gange i sekundet. En advarsel pr. ramme ville
         * fylde serieporten helt og skubbe alt andet ud af loggen,
         * praecis naar man har mest brug for at kunne laese den.
         *
         * ESP-IDF's dpi-panel tager sin draw_sem FOER den kan fejle
         * (esp_lcd_panel_dpi.c), saa en fejl her betyder at semaforen er
         * tabt og at hver efterfoelgende ramme ogsaa fejler. Tallet
         * siger hvor laenge det har varet.
         */
        static uint32_t s_fejl;
        if (s_fejl++ == 0) {
            ESP_LOGW(TAG, "en ramme kunne ikke sendes til skaermen: %s. "
                          "Flere fejl af samme slags logges ikke.",
                     esp_err_to_name(err));
        } else if ((s_fejl % 600u) == 0u) {
            ESP_LOGW(TAG, "%u rammer er nu gaaet tabt", (unsigned)s_fejl);
        }
        lv_disp_flush_ready(&disp_drv);
    }
}

/*
 * Beroeringen.
 *
 * Tjekket paa at koordinaterne ligger INDEN FOR skaermen er ikke
 * paranoia. Det er maalt paa zScreen: touch-kredsen der svarede 12204
 * og 65531 lige efter opstart, blev regnet om til et negativt tal, foldt
 * rundt i LVGL's int16, og landede som et tryk paa et vilkaarligt sted
 * paa skaermen. Skaermen startede demoen af sig selv halvandet sekund
 * efter opstart. Se den lange note i firmware/boards/zscreen/main/lv_port.c.
 *
 * Kredsen er en anden her (GSL3670 i stedet for FT5x06), og den har ikke
 * vist den opfoersel. Men prisen for tjekket er to sammenligninger, og
 * prisen for at mangle det er et tryk ingen har lavet.
 */
static void touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static lv_coord_t sidste_x = 0, sidste_y = 0;

    if (s_touch == NULL) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    /*
     * esp_lcd_touch_get_data og IKKE esp_lcd_touch_get_coordinates.
     *
     * Den sidste er udfaset i esp_lcd_touch 1.2 og forsvinder i 2.0.
     * Vi fandt det ved at CI faelder paa advarsler i vores egen kode -
     * byggeriet gik fint igennem, og advarslen laa midt i tusind linjer
     * hvor den var nem at se forbi.
     *
     * Forskellen er ikke kun navnet: den gamle gav true naar der var
     * et tryk, den nye giver ESP_OK og saetter antallet til nul. Et
     * tryk er altsaa ESP_OK OG mindst ét punkt.
     */
    esp_lcd_touch_point_data_t punkt[1] = { 0 };
    uint8_t antal = 0;

    /*
     * HVIS LAESNINGEN FEJLER, ER DER INGEN BEROERING.
     *
     * Her stod kaldet uden at se paa svaret. Fejler I2C-laesningen,
     * udleder driveren tilstanden af en buffer der ikke blev skrevet,
     * og saa kan et busglimt blive et RIGTIGT tryk paa et tilfaeldigt
     * punkt - som godt kan ligge inden for skaermen og dermed slippe
     * gennem tjekket nedenfor.
     *
     * Det er praecis den fejl zScreen har haft: en skaerm der startede
     * demoen af sig selv halvandet sekund efter opstart, fordi et
     * ugyldigt svar blev regnet om til et tryk. Dens port melder sluppet
     * og gaar hjem i det tilfaelde, og det goer den her nu ogsaa.
     */
    if (esp_lcd_touch_read_data(s_touch) != ESP_OK) {
        data->state   = LV_INDEV_STATE_REL;
        data->point.x = sidste_x;
        data->point.y = sidste_y;
        return;
    }

    esp_err_t err = esp_lcd_touch_get_data(s_touch, punkt, &antal, 1);

    bool gyldig = err == ESP_OK
               && antal > 0
               && punkt[0].x < ZS_G_SCR_WIDTH
               && punkt[0].y < ZS_G_SCR_HEIGHT;

    if (gyldig) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = (lv_coord_t)punkt[0].x;
        data->point.y = (lv_coord_t)punkt[0].y;
        sidste_x = data->point.x;
        sidste_y = data->point.y;
        zs_display_touch_wake();
    } else {
        data->state   = LV_INDEV_STATE_REL;
        data->point.x = sidste_x;
        data->point.y = sidste_y;
    }
}

static void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t disp_buf;

    const bsp_display_config_t cfg = { 0 };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_ERROR_CHECK(bsp_display_new(&cfg, &s_panel, &io));

    const esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = flush_faerdig,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(s_panel, &cbs, NULL));

    /*
     * Bufferne skal ligge i PSRAM, og de skal vaere linjejusterede.
     *
     * DMA2D laeser dem direkte, og cache-synkroniseringen i ESP-IDF
     * arbejder i hele cache-linjer. En buffer der starter midt i en
     * linje kan tage naboens data med over, og det ses som en stribe
     * skrald i kanten af det der blev tegnet. heap_caps_aligned_alloc
     * med cache-linjens stoerrelse fjerner det.
     */
    size_t bytes = BUF_PIXELS * sizeof(lv_color_t);
    void *buf1 = heap_caps_aligned_alloc(128, bytes, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_aligned_alloc(128, bytes, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, BUF_PIXELS);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ZS_G_SCR_WIDTH;
    disp_drv.ver_res  = ZS_G_SCR_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "skaerm klar, %d x %d, to buffere paa %u KB",
             (int)ZS_G_SCR_WIDTH, (int)ZS_G_SCR_HEIGHT,
             (unsigned)(bytes / 1024));
}

static void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    /*
     * TRE FORSOEG, OG DET ER MAALT AT DET HJAELPER.
     *
     * GSL3670'eren tager ikke altid sin firmware i foerste forsoeg.
     * Den svarer paa I2C, alle 4356 firmware-writes bliver kvitteret,
     * og register 0xb0 - som chippen SELV skriver 5a5a5a5a i naar den
     * koerer koden - staar alligevel paa nul. Saa er skaermen doev:
     * fladen tegner alt rigtigt, men der kommer ingen tryk ind. Den ser
     * frossen ud.
     *
     * Maalt 14. oktober 2026, to gange:
     *
     *   * Én mislykket init lige efter en flashning. De fire naeste
     *     opstarter virkede alle. Altsaa periodisk, og et forsoeg mere
     *     havde klaret den.
     *   * Og én gang hvor chippen havde LAAST sig: seks nulstillinger
     *     i traek fejlede, og kun en stroemafbrydelse ryddede det.
     *     Reset-benet gOEr det ikke - kortet holder sin egen 3V3 via
     *     BSP_PWR_HOLD og har batteri.
     *
     * Et forsoeg mere koster omkring 4,3 sekunder, og KUN naar det
     * ellers var gaaet galt. En doev skaerm koster et besoeg.
     *
     * Hjaelper tre forsoeg ikke, er det den laaste tilstand, og den
     * kan firmwaren ikke komme ud af. Derfor siger beskeden hvad man
     * saa skal goere, i stedet for bare at sige at det gik galt.
     */
    const bsp_touch_config_t cfg = { 0 };
    esp_err_t err = ESP_FAIL;
    for (int forsoeg = 1; forsoeg <= 3; forsoeg++) {
        err = bsp_touch_new(&cfg, &s_touch);
        if (err == ESP_OK) {
            if (forsoeg > 1) {
                ESP_LOGW(TAG, "touch kom op i forsoeg %d af 3", forsoeg);
            }
            break;
        }
        s_touch = NULL;     /* driveren skriver en frigjort pegepind */
        ESP_LOGW(TAG, "touch svarede ikke i forsoeg %d af 3 (%s)",
                 forsoeg, esp_err_to_name(err));
    }
    if (err != ESP_OK) {
        /*
         * En skaerm uden touch er ikke en doed skaerm: den viser stadig
         * de fire tal, og det er det vigtigste den laver. Derfor
         * standser opstarten ikke her.
         */
        ESP_LOGE(TAG, "touch svarede ikke i tre forsoeg, skaermen kan kun "
                      "ses. Chippen er sandsynligvis laast: hold "
                      "taend-knappen inde i 3 sekunder, traek USB ud, "
                      "vent 10 sekunder og taend igen. En nulstilling "
                      "alene rydder det IKKE.");
        s_touch = NULL;
        return;
    }

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

static void lv_tick_inc_cb(void *data)
{
    uint32_t ms = *((uint32_t *)data);
    lv_tick_inc(ms);
}

static void lv_port_tick_init(void)
{
    static uint32_t tick_ms = 2;
    const esp_timer_create_args_t args = {
        .callback = lv_tick_inc_cb,
        .name = "lv_tick",
        .arg = &tick_ms,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t t;
    ESP_ERROR_CHECK(esp_timer_create(&args, &t));
    ESP_ERROR_CHECK(esp_timer_start_periodic(t, tick_ms * 1000));
}

static void lvgl_task(void *args)
{
    (void)args;
    for (;;) {
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
        lv_task_handler();
        xSemaphoreGiveRecursive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

void lv_port_init(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_tick_init();

    /* Rekursiv, af samme grund som paa zScreen: app_main tager laasen
     * og kalder zs_ui_init(), som inde i sig tager den igen. Med en
     * almindelig mutex venter opgaven paa sig selv, og skaermen staar
     * stille uden en eneste fejlmeddelelse. */
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mutex);
    xTaskCreate(lvgl_task, "lvgl_task", 4096 * 4, NULL,
                CONFIG_LCD_TASK_PRIORITY, &lvgl_task_handle);
}

void lv_port_sem_take(void)
{
    if (lvgl_task_handle != xTaskGetCurrentTaskHandle()) {
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    }
}

void lv_port_sem_give(void)
{
    if (lvgl_task_handle != xTaskGetCurrentTaskHandle()) {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}
