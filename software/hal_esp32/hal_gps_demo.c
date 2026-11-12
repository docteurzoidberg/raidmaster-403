/*
 * Raidmaster 403 — hal_gps de démonstration sur l'ESP32 : le trajet simulé du sim
 * (core/gps_synth), en temps réel. Choix par défaut de menuconfig (« Source GPS ») ; le récepteur
 * réel (M9N ou puck M8N sur l'UART1) est dans hal_gps_uart.c.
 *
 * Heure de départ : horloge de la carte (RTC) si elle a l'heure, sinon date et heure du build
 * (heure locale de la machine de build, prise pour UTC+1 comme le fuseau par défaut du core).
 * Cette heure est inventée : elle ne doit pas régler l'horloge au premier fix
 * (board_clock_sync_enable(false)), qui reste « pas réglée » (--:--) sans pile ni GPS.
 *
 * Licence : GPL-3.0.
 */

#include "sdkconfig.h"

#if !CONFIG_RAIDMASTER_GPS_UART

#include "esp_log.h"

#include "board.h"
#include "gps_synth.h"
#include "hal/hal_gps.h"
#include "hal/hal_time.h"

static const char *TAG = "hal_gps";

#define TIME_2024_S 1704067200LL    /* 2024-01-01 00:00 UTC : en dessous, horloge non réglée */

static hal_gps_line_cb s_cb;
static void           *s_user;
static uint64_t        s_t0_ms;
static gps_synth_t     s_synth;

void hal_gps_init(hal_gps_line_cb cb, void *user)
{
    s_cb = cb;
    s_user = user;
    s_t0_ms = hal_time_ms();
    int64_t utc_ms = hal_time_utc_ms();
    if (utc_ms < 0) utc_ms = board_build_time_utc_ms();
    if (utc_ms < 0) utc_ms = TIME_2024_S * 1000;
    gps_synth_init(&s_synth, utc_ms);
    board_clock_sync_enable(false);
    ESP_LOGI(TAG, "GPS de démonstration : trajet simulé (aucun récepteur)");
}

void hal_gps_poll(void)
{
    gps_synth_run(&s_synth, hal_time_ms() - s_t0_ms, true, s_cb, s_user);
}

#endif /* !CONFIG_RAIDMASTER_GPS_UART */
