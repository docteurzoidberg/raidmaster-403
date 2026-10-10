/*
 * Raidmaster 403 — HAL temps : horloge monotone (esp_timer sur l'ESP32, CLOCK_MONOTONIC sur
 * le sim). Abstraite pour permettre un temps simulé.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t hal_time_us(void);

static inline uint64_t hal_time_ms(void)
{
    return hal_time_us() / 1000U;
}

#ifdef __cplusplus
}
#endif
