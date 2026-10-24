/*
 * Raidmaster 403 — firmware ESP32-S3 (carte Waveshare ESP32-S3-Touch-LCD-3.5B).
 *
 * Même boucle que le simulateur (sim/main.c) : le core (core/app) reçoit les lignes NMEA de
 * hal_gps et sauvegarde par hal_storage, l'UI LVGL est câblée sur le core (ui_app). Tout tourne
 * dans la tâche app_main : LVGL n'a pas besoin de verrou.
 *
 * Première version : GPS de démonstration (trajet simulé, hal_esp32/hal_gps_demo.c), tactile
 * en guise de boutons (touch_buttons.c), réglages et distances en NVS. Toutes les 10 s, une
 * ligne de journal donne images/s, durée d'envoi d'une image et mémoire libre. Appui long sur
 * PWR : extinction propre (le PMIC rallume la carte au prochain appui).
 *
 * Réglages (tools/fw.sh menuconfig → « Raidmaster 403 ») : sens de l'écran, horloge QSPI,
 * rétroéclairage jour / nuit, tactile en guise de boutons.
 *
 * Licence : GPL-3.0.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "app.h"
#include "board.h"
#include "hal/hal_gps.h"
#include "hal/hal_time.h"
#include "theme.h"
#include "touch_buttons.h"
#include "ui.h"
#include "ui_app.h"
#include "ui_tokens.h"

static const char *TAG = "raidmaster";

#define STATS_PERIOD_MS 10000U

static app_t s_app;

static void on_gps_line(const char *line, size_t len, void *user)
{
    (void)len;
    app_gps_line((app_t *)user, line, hal_time_ms());
}

static uint8_t backlight_pct(bool night)
{
    return night ? CONFIG_RAIDMASTER_BACKLIGHT_NIGHT : CONFIG_RAIDMASTER_BACKLIGHT_DAY;
}

static void log_stats(void)
{
    uint32_t frames, send_us;
    board_display_stats(&frames, &send_us);
    const uint32_t fps10 = frames * 10000U / STATS_PERIOD_MS;      /* dixièmes d'image par seconde */
    ESP_LOGI(TAG, "%" PRIu32 ".%" PRIu32 " images/s, envoi %" PRIu32 " ms, libre : interne %u Ko, PSRAM %u Ko",
             fps10 / 10U, fps10 % 10U, send_us / 1000U, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));
}

/* Appui long sur PWR : distances sauvegardées, « EXTINCTION » à l'écran, rétroéclairage puis
 * PMIC coupés. Si le courant est toujours là une seconde après (PMIC muet), on reprend. */
static void power_off(lv_display_t *disp)
{
    ESP_LOGI(TAG, "extinction (appui long sur PWR)");
    app_save(&s_app);
    lv_obj_t *veil = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(veil);
    lv_obj_set_size(veil, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(veil, UI_C_BG, 0);
    lv_obj_set_style_bg_opa(veil, LV_OPA_COVER, 0);
    lv_obj_t *msg = lv_label_create(veil);
    lv_label_set_text(msg, "EXTINCTION");
    lv_obj_set_style_text_font(msg, UI_FONT_VALUE_SMALL, 0);   /* X, T, I ajoutés au 25 px pour ce message */
    lv_obj_set_style_text_color(msg, UI_C_AMBER, 0);
    lv_obj_set_style_text_letter_space(msg, 4, 0);
    lv_obj_center(msg);
    lv_refr_now(disp);
    vTaskDelay(pdMS_TO_TICKS(700));
    board_backlight_set(0);
    board_power_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGW(TAG, "le PMIC n'a pas coupé l'alimentation");
    lv_obj_delete(veil);
    board_backlight_set(backlight_pct(s_app.set.night));
}

void app_main(void)
{
    const board_config_t cfg = {
#if CONFIG_RAIDMASTER_LCD_ROTATION_270
        .rotation = BOARD_ROTATION_270,
#else
        .rotation = BOARD_ROTATION_90,
#endif
        .lcd_pclk_hz = CONFIG_RAIDMASTER_LCD_PCLK_MHZ * 1000000U,
    };
    ESP_ERROR_CHECK(board_init(&cfg));

    lv_init();
    lv_display_t *disp = board_display_create();
    if (disp == NULL) ESP_ERROR_CHECK(ESP_ERR_NO_MEM);

    /* Core avant l'UI : le mode nuit sauvegardé s'applique dès l'écran de démarrage (comme le sim). */
    app_init(&s_app, hal_time_ms());
    ui_theme_set_night(s_app.set.night);
    ui_init(disp);
    hal_gps_init(on_gps_line, &s_app);
    ui_app_attach(&s_app);

    /* Première image envoyée avant d'allumer le rétroéclairage (pas de flash d'image aléatoire). */
    lv_refr_now(disp);
    bool night = s_app.set.night;
    board_backlight_set(backlight_pct(night));
    ESP_LOGI(TAG, "démarré, libre : interne %u Ko, PSRAM %u Ko",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));

    uint64_t last_ms = hal_time_ms();
    uint64_t next_stats_ms = last_ms + STATS_PERIOD_MS;
    for (;;) {
        hal_gps_poll();
        const uint64_t now = hal_time_ms();
        app_tick(&s_app, now);
#if CONFIG_RAIDMASTER_TOUCH_BUTTONS
        touch_buttons_poll(now);
#endif
        if (board_pwr_key_long_press()) power_off(disp);
        if (s_app.set.night != night) {
            night = s_app.set.night;
            board_backlight_set(backlight_pct(night));
        }
        if (now > last_ms) {
            ui_tick((uint32_t)(now - last_ms));
            last_ms = now;
        }
        if (now >= next_stats_ms) {
            next_stats_ms = now + STATS_PERIOD_MS;
            log_stats();
        }

        uint32_t next = lv_timer_handler();
        if (next > 33U) next = 33U;     /* ~30 images/s au plus, comme le sim */
        if (next < 1U)  next = 1U;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}
