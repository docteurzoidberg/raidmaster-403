/*
 * Raidmaster 403 — HAL temps : horloge monotone (esp_timer sur l'ESP32, CLOCK_MONOTONIC sur
 * le sim), abstraite pour permettre un temps simulé ; et horloge de la carte (RTC PCF85063 sur
 * l'ESP32, émulée sur le sim), qui donne l'heure affichée.
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

/* Horloge de la carte : heure UTC en ms depuis 1970, -1 si elle n'est pas réglée (RTC arrêtée
 * faute d'alimentation : ni USB, ni batterie, ni pile de sauvegarde). */
int64_t hal_time_utc_ms(void);

/* Règle l'horloge de la carte (le core le fait au premier fix GPS de chaque démarrage). */
void hal_time_set_utc_ms(int64_t utc_ms);

#ifdef __cplusplus
}
#endif
