/*
 * Raidmaster 403 — hal_time sur l'ESP32 : esp_timer (µs depuis le démarrage). L'horloge de la
 * carte (hal_time_utc_ms) est dans board_rtc.c.
 *
 * Licence : GPL-3.0.
 */

#include "esp_timer.h"

#include "hal/hal_time.h"

uint64_t hal_time_us(void)
{
    return (uint64_t)esp_timer_get_time();
}
