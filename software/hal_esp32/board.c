/*
 * Raidmaster 403 — carte Waveshare 3.5B : bus I²C, expandeur TCA9554 et séquence de démarrage
 * (voir board.h).
 *
 * Licence : GPL-3.0.
 */

#include "board.h"

#include "esp_check.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

static i2c_master_bus_handle_t  s_i2c;
static esp_io_expander_handle_t s_exio;
static board_rotation_t         s_rotation = BOARD_ROTATION_90;

i2c_master_bus_handle_t board_i2c_bus(void)
{
    return s_i2c;
}

board_rotation_t board_rotation(void)
{
    return s_rotation;
}

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_i2c);
}

/* Reset commun écran + tactile par EXIO1 (bas 100 ms, puis 200 ms avant de leur parler), comme
 * le firmware d'usine. Les autres broches de l'expandeur restent en entrée (état au démarrage). */
static esp_err_t lcd_reset(void)
{
    const uint32_t rst = 1U << BOARD_EXIO_LCD_RST;
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_tca9554(s_i2c, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &s_exio),
                        TAG, "TCA9554");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_exio, rst, IO_EXPANDER_OUTPUT), TAG, "EXIO1 en sortie");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_exio, rst, 0), TAG, "reset écran");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_exio, rst, 1), TAG, "fin du reset écran");
    vTaskDelay(pdMS_TO_TICKS(200));
    return ESP_OK;
}

esp_err_t board_init(const board_config_t *cfg)
{
    s_rotation = cfg->rotation;
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "bus I²C");
    ESP_RETURN_ON_ERROR(lcd_reset(), TAG, "reset écran");
    ESP_RETURN_ON_ERROR(board_display_init(cfg->lcd_pclk_hz), TAG, "écran");
    /* Le tactile, la RTC et le PMIC ne sont pas indispensables : la démo tourne sans. */
    if (board_touch_init() != ESP_OK) ESP_LOGW(TAG, "tactile absent");
    if (board_rtc_init() != ESP_OK) ESP_LOGW(TAG, "RTC absente");
    if (board_pmu_init() != ESP_OK) ESP_LOGW(TAG, "PMIC absent");
    ESP_LOGI(TAG, "carte prête (UI tournée de %d°, QSPI %lu MHz)",
             s_rotation == BOARD_ROTATION_90 ? 90 : 270, (unsigned long)(cfg->lcd_pclk_hz / 1000000U));
    return ESP_OK;
}
