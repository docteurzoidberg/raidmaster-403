/*
 * Raidmaster 403 — écran AXS15231B (QSPI) et rétroéclairage de la carte Waveshare 3.5B, affichage
 * LVGL (voir board.h).
 *
 * LVGL dessine l'UI paysage 480 × 320 dans une image complète en PSRAM (mode DIRECT : seules les
 * zones modifiées sont redessinées). À la fin de chaque rafraîchissement, l'image est tournée vers
 * le panneau portrait 320 × 480, octets RGB565 permutés (le QSPI envoie l'octet fort d'abord), et
 * envoyée par bandes de lignes complètes, du haut vers le bas : en QSPI le contrôleur ignore
 * l'adresse de ligne (RASET) et continue l'écriture là où la précédente s'est arrêtée. Deux tampons
 * DMA en RAM interne alternent : on remplit l'un pendant que l'autre part.
 *
 * Licence : GPL-3.0. Séquence d'initialisation du panneau reprise des exemples ESP-IDF de Waveshare
 * (Apache-2.0).
 */

#include "board.h"

#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "board_display";

#define LCD_HOST        SPI2_HOST
#define CHUNK_ROWS      32                              /* lignes du panneau par envoi */
#define CHUNK_PX        (BOARD_LCD_W * CHUNK_ROWS)
#define CHUNK_COUNT     (BOARD_LCD_H / CHUNK_ROWS)

#define BL_TIMER        LEDC_TIMER_0
#define BL_CHANNEL      LEDC_CHANNEL_0
#define BL_MODE         LEDC_LOW_SPEED_MODE
#define BL_RES          LEDC_TIMER_10_BIT
#define BL_FREQ_HZ      5000

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t      s_free_chunks;            /* tampons DMA libres (0 à 2) */
static uint16_t              *s_chunk[2];
static uint16_t              *s_fb;                     /* image UI 480 × 320, RGB565 */
static uint32_t               s_frames;                 /* images envoyées (statistiques) */
static uint64_t               s_send_us;

/* Séquence du constructeur du panneau (exemples Waveshare, identique dans le firmware d'usine). */
static const axs15231b_lcd_init_cmd_t s_init_cmds[] = {
    {0xBB, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
    {0xA0, (uint8_t[]){0xC0, 0x10, 0x00, 0x02, 0x00, 0x00, 0x04, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 17, 0},
    {0xA2, (uint8_t[]){0x30, 0x3C, 0x24, 0x14, 0xD0, 0x20, 0xFF, 0xE0, 0x40, 0x19, 0x80, 0x80, 0x80, 0x20, 0xf9, 0x10, 0x02, 0xff, 0xff, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xE0, 0x20, 0x7F, 0xFF, 0x00, 0x5A}, 31, 0},
    {0xD0, (uint8_t[]){0xE0, 0x40, 0x51, 0x24, 0x08, 0x05, 0x10, 0x01, 0x20, 0x15, 0x42, 0xC2, 0x22, 0x22, 0xAA, 0x03, 0x10, 0x12, 0x60, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x8A, 0x20, 0x00, 0x03, 0x3A, 0x12}, 30, 0},
    {0xA3, (uint8_t[]){0xA0, 0x06, 0xAa, 0x00, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 22, 0},
    {0xC1, (uint8_t[]){0x31, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x00, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0d, 0x00, 0xFF, 0x40}, 30, 0},
    {0xC3, (uint8_t[]){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
    {0xC4, (uint8_t[]){0x00, 0x24, 0x33, 0x80, 0x00, 0xea, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
    {0xC5, (uint8_t[]){0x18, 0x00, 0x00, 0x03, 0xFE, 0x3A, 0x4A, 0x20, 0x30, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x3A, 0x4A, 0x20, 0x10, 0x10, 0x00}, 23, 0},
    {0xC6, (uint8_t[]){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x03, 0x00, 0x3F, 0x6A, 0x18, 0xC8, 0x22}, 20, 0},
    {0xC7, (uint8_t[]){0x50, 0x32, 0x28, 0x00, 0xa2, 0x80, 0x8f, 0x00, 0x80, 0xff, 0x07, 0x11, 0x9c, 0x67, 0xff, 0x24, 0x0c, 0x0d, 0x0e, 0x0f}, 20, 0},
    {0xC9, (uint8_t[]){0x33, 0x44, 0x44, 0x01}, 4, 0},
    {0xCF, (uint8_t[]){0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0x88, 0x00, 0x65, 0x09, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x08, 0x08, 0x12, 0xA0, 0x08}, 27, 0},
    {0xD5, (uint8_t[]){0x40, 0x8E, 0x8D, 0x01, 0x35, 0x04, 0x92, 0x74, 0x04, 0x92, 0x74, 0x04, 0x08, 0x6A, 0x04, 0x46, 0x03, 0x03, 0x03, 0x03, 0x82, 0x01, 0x03, 0x00, 0xE0, 0x51, 0xA1, 0x00, 0x00, 0x00}, 30, 0},
    {0xD6, (uint8_t[]){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x93, 0x00, 0x01, 0x83, 0x07, 0x07, 0x00, 0x07, 0x07, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x84, 0x00, 0x20, 0x01, 0x00}, 30, 0},
    {0xD7, (uint8_t[]){0x03, 0x01, 0x0b, 0x09, 0x0f, 0x0d, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19, 0x40, 0x8E, 0x04, 0x00, 0x20, 0xA0, 0x1F}, 19, 0},
    {0xD8, (uint8_t[]){0x02, 0x00, 0x0a, 0x08, 0x0e, 0x0c, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19}, 12, 0},
    {0xD9, (uint8_t[]){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDD, (uint8_t[]){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDF, (uint8_t[]){0x44, 0x73, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8, 0},
    {0xE0, (uint8_t[]){0x3B, 0x28, 0x10, 0x16, 0x0c, 0x06, 0x11, 0x28, 0x5c, 0x21, 0x0D, 0x35, 0x13, 0x2C, 0x33, 0x28, 0x0D}, 17, 0},
    {0xE1, (uint8_t[]){0x37, 0x28, 0x10, 0x16, 0x0b, 0x06, 0x11, 0x28, 0x5C, 0x21, 0x0D, 0x35, 0x14, 0x2C, 0x33, 0x28, 0x0F}, 17, 0},
    {0xE2, (uint8_t[]){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE3, (uint8_t[]){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x32, 0x2F, 0x0F}, 17, 0},
    {0xE4, (uint8_t[]){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE5, (uint8_t[]){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0F}, 17, 0},
    {0xA4, (uint8_t[]){0x85, 0x85, 0x95, 0x82, 0xAF, 0xAA, 0xAA, 0x80, 0x10, 0x30, 0x40, 0x40, 0x20, 0xFF, 0x60, 0x30}, 16, 0},
    {0xA4, (uint8_t[]){0x85, 0x85, 0x95, 0x85}, 4, 0},
    {0xBB, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {0x13, (uint8_t[]){0x00}, 0, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x2C, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4, 0},
};

/* Fin d'un envoi DMA (interruption) : le tampon redevient libre. */
static bool on_chunk_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    (void)io;
    (void)ev;
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_free_chunks, &woken);
    return woken == pdTRUE;
}

void board_backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = BL_MODE,
        .duty_resolution = BL_RES,
        .timer_num = BL_TIMER,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const ledc_channel_config_t ch = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = BL_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void board_backlight_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    const uint32_t duty = (uint32_t)percent * ((1U << BL_RES) - 1U) / 100U;
    ledc_set_duty(BL_MODE, BL_CHANNEL, duty);
    ledc_update_duty(BL_MODE, BL_CHANNEL);
}

void board_display_wait_idle(void)
{
    for (int i = 0; i < 2; i++) xSemaphoreTake(s_free_chunks, portMAX_DELAY);
    for (int i = 0; i < 2; i++) xSemaphoreGive(s_free_chunks);
}

/* Mémoire du panneau mise au noir : après le reset elle contient du bruit, que le panneau affiche
 * jusqu'à la première image (et encore quelques dizaines de ms, le temps que les cristaux suivent). */
static void clear_panel(void)
{
    for (int i = 0; i < 2; i++) memset(s_chunk[i], 0, CHUNK_PX * sizeof(uint16_t));
    for (int c = 0; c < CHUNK_COUNT; c++) {
        xSemaphoreTake(s_free_chunks, portMAX_DELAY);
        esp_lcd_panel_draw_bitmap(s_panel, 0, c * CHUNK_ROWS, BOARD_LCD_W, (c + 1) * CHUNK_ROWS, s_chunk[c & 1]);
    }
    board_display_wait_idle();
}

esp_err_t board_display_init(uint32_t pclk_hz)
{
    s_free_chunks = xSemaphoreCreateCounting(2, 2);
    for (int i = 0; i < 2; i++) {
        s_chunk[i] = heap_caps_malloc(CHUNK_PX * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_RETURN_ON_FALSE(s_chunk[i] != NULL, ESP_ERR_NO_MEM, TAG, "tampon DMA");
    }

    const spi_bus_config_t bus = AXS15231B_PANEL_BUS_QSPI_CONFIG(BOARD_LCD_SCLK, BOARD_LCD_D0, BOARD_LCD_D1,
                                                                  BOARD_LCD_D2, BOARD_LCD_D3,
                                                                  CHUNK_PX * sizeof(uint16_t));
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "bus QSPI");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = AXS15231B_PANEL_IO_QSPI_CONFIG(BOARD_LCD_CS, on_chunk_done, NULL);
    io_cfg.pclk_hz = pclk_hz;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io), TAG, "IO QSPI");

    const axs15231b_vendor_config_t vendor = {
        .init_cmds = s_init_cmds,
        .init_cmds_size = sizeof(s_init_cmds) / sizeof(s_init_cmds[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,               /* reset par EXIO1, déjà fait par board_init */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_axs15231b(io, &panel_cfg, &s_panel), TAG, "panneau");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset logiciel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init panneau");
    clear_panel();
    /* Pilote v1.0.x : le paramètre veut dire « éteindre » (false → DISPON), comme chez Waveshare. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, false), TAG, "allumage");
    return ESP_OK;
}

/* Remplit la bande de panneau [py0, py0 + CHUNK_ROWS) depuis l'image UI, tournée et permutée.
 * Rotation 90° : pixel panneau (px, py) = pixel UI (py, H − 1 − px) ; 270° : (W − 1 − py, px).
 * Boucle externe sur les lignes de l'UI : on lit 32 pixels consécutifs (64 octets) par ligne. */
static void fill_chunk(uint16_t *dst, int py0)
{
    const bool rot90 = board_rotation() == BOARD_ROTATION_90;
    for (int px = 0; px < BOARD_LCD_W; px++) {
        uint16_t *d = dst + px;
        if (rot90) {
            const uint16_t *src = s_fb + (BOARD_UI_H - 1 - px) * BOARD_UI_W + py0;
            for (int r = 0; r < CHUNK_ROWS; r++) {
                const uint16_t v = src[r];
                d[r * BOARD_LCD_W] = (uint16_t)((v >> 8) | (v << 8));
            }
        } else {
            const uint16_t *src = s_fb + px * BOARD_UI_W + (BOARD_UI_W - 1 - py0);
            for (int r = 0; r < CHUNK_ROWS; r++) {
                const uint16_t v = src[-r];
                d[r * BOARD_LCD_W] = (uint16_t)((v >> 8) | (v << 8));
            }
        }
    }
}

static void send_frame(void)
{
    const int64_t t0 = esp_timer_get_time();
    for (int c = 0; c < CHUNK_COUNT; c++) {
        xSemaphoreTake(s_free_chunks, portMAX_DELAY);
        uint16_t *buf = s_chunk[c & 1];
        const int py0 = c * CHUNK_ROWS;
        fill_chunk(buf, py0);
        esp_lcd_panel_draw_bitmap(s_panel, 0, py0, BOARD_LCD_W, py0 + CHUNK_ROWS, buf);
    }
    s_frames++;
    s_send_us += (uint64_t)(esp_timer_get_time() - t0);
}

void board_display_stats(uint32_t *frames, uint32_t *avg_send_us)
{
    *frames = s_frames;
    *avg_send_us = s_frames ? (uint32_t)(s_send_us / s_frames) : 0;
    s_frames = 0;
    s_send_us = 0;
}

/* Mode DIRECT : appelé pour chaque zone redessinée ; l'image part entière après la dernière. Les
 * bandes en cours d'envoi sont dans les tampons DMA : LVGL peut redessiner dès le retour. */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    if (lv_display_flush_is_last(disp)) send_frame();
    lv_display_flush_ready(disp);
}

lv_display_t *board_display_create(void)
{
    const size_t fb_size = BOARD_UI_W * BOARD_UI_H * sizeof(uint16_t);
    s_fb = heap_caps_aligned_alloc(64, fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_fb == NULL) {
        ESP_LOGE(TAG, "image de %u octets : mémoire insuffisante", (unsigned)fb_size);
        return NULL;
    }
    lv_display_t *disp = lv_display_create(BOARD_UI_W, BOARD_UI_H);
    if (disp == NULL) return NULL;
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_fb, NULL, fb_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    return disp;
}
