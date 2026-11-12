/*
 * Raidmaster 403 — carte Waveshare ESP32-S3-Touch-LCD-3.5B : brochage et pilotes de base (bus
 * I²C, expandeur TCA9554, écran AXS15231B en QSPI, rétroéclairage, tactile).
 *
 * Brochage lu sur le schéma de Waveshare
 * (https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3.5B-Schematic.pdf),
 * recoupé avec les exemples ESP-IDF de Waveshare (dépôt waveshareteam/ESP32-S3-Touch-LCD-3.5B).
 *
 * Écran : panneau natif portrait 320 × 480 ; l'UI est en paysage 480 × 320, tournée par logiciel
 * à l'envoi. Le contrôleur en QSPI n'accepte que des lignes complètes écrites dans l'ordre depuis
 * le haut : chaque rafraîchissement envoie l'image entière.
 *
 * Usage (firmware/main/main.c) :
 *   board_init(&cfg);              // I²C, TCA9554, reset et init de l'écran, tactile
 *   lv_init();
 *   disp = board_display_create(); // affichage LVGL 480 × 320
 *   lv_refr_now(disp);             // première image…
 *   board_display_wait_idle();     // …envoyée en entier, puis laissée au panneau le temps de l'afficher
 *   board_backlight_set(100);
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bus I²C principal (I2C0), partagé : tactile 0x3B, TCA9554 0x20, AXP2101 0x34, PCF85063 0x51,
 * QMI8658 0x6B, ES8311 0x18 (et QMC5883L 0x0D sur la carte fille). */
#define BOARD_I2C_SDA       8
#define BOARD_I2C_SCL       7

/* Écran AXS15231B en QSPI (SPI2). */
#define BOARD_LCD_CS        12
#define BOARD_LCD_SCLK      5
#define BOARD_LCD_D0        1
#define BOARD_LCD_D1        2
#define BOARD_LCD_D2        3
#define BOARD_LCD_D3        4
#define BOARD_LCD_BL        6       /* rétroéclairage : transistor S8050, PWM LEDC */

/* Broches du TCA9554 (EXIO0 à EXIO7). */
#define BOARD_EXIO_CAM_PWDN 0
#define BOARD_EXIO_LCD_RST  1       /* reset commun écran + tactile */
#define BOARD_EXIO_TP_INT   2
#define BOARD_EXIO_SD_CS    3
#define BOARD_EXIO_RTC_INT  4
#define BOARD_EXIO_AXP_IRQ  5
#define BOARD_EXIO_PWR_KEY  6       /* bouton PWR (SYS_OUT, actif haut) */
#define BOARD_EXIO_PA_CTRL  7       /* ampli audio NS4150B */

/* Son : codec ES8311 (I²C 0x18, alimenté par ALDO1 de l'AXP2101) en I²S standard, ampli NS4150B
 * activé par EXIO7. Le micro (DIN) ne sert pas. */
#define BOARD_I2S_MCLK      44
#define BOARD_I2S_BCLK      13
#define BOARD_I2S_WS        15
#define BOARD_I2S_DOUT      16      /* vers le codec */

/* Panneau natif (portrait) et UI (paysage). */
#define BOARD_LCD_W         320
#define BOARD_LCD_H         480
#define BOARD_UI_W          480
#define BOARD_UI_H          320

/* Sens de rotation de l'UI sur le panneau (dépend du montage dans le boîtier). */
typedef enum {
    BOARD_ROTATION_90 = 0,          /* UI tournée de 90° dans le sens horaire */
    BOARD_ROTATION_270,
} board_rotation_t;

typedef struct {
    board_rotation_t rotation;
    uint32_t         lcd_pclk_hz;   /* horloge QSPI de l'écran (40 MHz chez Waveshare) */
} board_config_t;

/* Bus I²C, expandeur, reset et initialisation de l'écran (éteint, rétroéclairage à 0), tactile,
 * horloge (RTC), PMIC ; ampli audio coupé. */
esp_err_t board_init(const board_config_t *cfg);

/* Pour les pilotes à venir (PMIC, IMU, RTC, audio) sur le même bus. */
i2c_master_bus_handle_t board_i2c_bus(void);
board_rotation_t        board_rotation(void);

/* Affichage LVGL 480 × 320 (RGB565) sur l'écran ; à appeler après lv_init(). NULL si la mémoire
 * manque. */
lv_display_t *board_display_create(void);

/* Images envoyées depuis l'appel précédent et durée moyenne d'un envoi (rotation + QSPI, la
 * dernière bande part en DMA après), puis remise à zéro. */
void board_display_stats(uint32_t *frames, uint32_t *avg_send_us);

/* Attend la fin de l'envoi en cours (dernières bandes en DMA). */
void board_display_wait_idle(void);

/* Rétroéclairage, 0 à 100 %. */
void board_backlight_set(uint8_t percent);

/* Lit le tactile ; true si appuyé, avec la position en coordonnées de l'UI (paysage). */
bool board_touch_read(int32_t *x, int32_t *y);

/* Réglage de l'horloge de la carte par le core (premier fix GPS) : ignoré si off. Le GPS de
 * démonstration le coupe, son heure étant inventée. */
void board_clock_sync_enable(bool on);

/* Bouton PWR, lu sur l'AXP2101 (qui rallume seul la carte éteinte) : true une fois par appui long
 * (1,5 s, durée réglée dans le PMIC) ; les appuis courts sont ignorés. Lu au plus toutes les 50 ms. */
bool board_pwr_key_long_press(void);

/* Coupe l'alimentation (extinction logicielle du PMIC) ; un appui sur PWR rallume la carte. */
void board_power_off(void);

/* Ampli audio NS4150B (EXIO7) : allumé seulement pendant les sons (souffle et consommation). */
esp_err_t board_amp_enable(bool on);

/* Date et heure de compilation du firmware (esp_app_desc, heure locale de la machine de build
 * prise pour UTC+1) en ms UTC, -1 si illisibles. Borne basse de toute heure valable. */
int64_t board_build_time_utc_ms(void);

/* Fonctions internes au composant. */
void      board_backlight_init(void);   /* PWM à 0 % : rétroéclairage éteint */
esp_err_t board_display_init(uint32_t pclk_hz);
esp_err_t board_touch_init(void);
esp_err_t board_rtc_init(void);     /* RTC PCF85063 → heure système si elle n'a pas perdu l'heure */
esp_err_t board_pmu_init(void);     /* AXP2101, lecture seule */
esp_err_t board_pmu_codec_power(void);  /* ALDO1 à 3,3 V et allumé (codec ES8311) */

#ifdef __cplusplus
}
#endif
