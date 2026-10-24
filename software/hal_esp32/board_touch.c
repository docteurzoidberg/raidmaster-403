/*
 * Raidmaster 403 — tactile de l'AXS15231B (I²C 0x3B) de la carte Waveshare 3.5B (voir board.h).
 *
 * Lecture brute comme le firmware d'usine : commande de 11 octets, réponse de 8 octets pour un
 * point (geste, nombre de points, puis X et Y sur 12 bits ; bits 7-6 de l'octet 2 = évènement,
 * 1 = relevé). L'interruption (EXIO2) n'est pas utilisée : on interroge depuis la boucle. Le
 * pilote esp_lcd_touch du composant ne signale pas le relâchement, d'où cette version.
 * Coordonnées du panneau (portrait 320 × 480) ramenées à l'UI paysage selon la rotation.
 *
 * Licence : GPL-3.0.
 */

#include "board.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "board_touch";

#define TOUCH_ADDR    0x3B
#define TOUCH_EV_UP   1

static i2c_master_dev_handle_t s_dev;

esp_err_t board_touch_init(void)
{
    ESP_RETURN_ON_ERROR(i2c_master_probe(board_i2c_bus(), TOUCH_ADDR, 50), TAG, "pas de réponse en 0x%02X",
                        TOUCH_ADDR);
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDR,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(board_i2c_bus(), &cfg, &s_dev);
}

bool board_touch_read(int32_t *x, int32_t *y)
{
    static const uint8_t cmd[11] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
    uint8_t d[8] = {0};
    if (s_dev == NULL || i2c_master_transmit_receive(s_dev, cmd, sizeof(cmd), d, sizeof(d), 20) != ESP_OK)
        return false;
    ESP_LOGD(TAG, "%02X %02X %02X %02X %02X %02X %02X %02X", d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    if (d[1] == 0 || d[1] > 2 || (d[2] >> 6) == TOUCH_EV_UP) return false;
    const int32_t tx = ((d[2] & 0x0F) << 8) | d[3];
    const int32_t ty = ((d[4] & 0x0F) << 8) | d[5];
    if (tx >= BOARD_LCD_W || ty >= BOARD_LCD_H) return false;
    if (board_rotation() == BOARD_ROTATION_90) {
        *x = ty;
        *y = BOARD_UI_H - 1 - tx;
    } else {
        *x = BOARD_UI_W - 1 - ty;
        *y = tx;
    }
    return true;
}
