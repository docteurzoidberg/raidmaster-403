/*
 * Raidmaster 403 — HAL son : sortie PCM 16 bits mono tirée par un rappel (codec ES8311 + ampli
 * NS4150B en I²S sur l'ESP32, sortie audio SDL sur le sim).
 *
 * Le rappel est appelé depuis le fil audio (tâche FreeRTOS, fil SDL), pas depuis la boucle
 * principale : ce qu'il lit ne doit pas être modifié tant que hal_audio_busy() est vrai.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Écrit jusqu'à n échantillons dans buf et renvoie leur nombre ; moins de n = fin du son. */
typedef size_t (*hal_audio_fill_fn)(int16_t *buf, size_t n, void *user);

/* Lance la lecture d'un son à `rate` Hz, sans attendre ; false si un son est en cours ou si la
 * sortie audio manque (le son est alors simplement perdu). */
bool hal_audio_play(uint32_t rate, hal_audio_fill_fn fill, void *user);

/* Vrai du lancement d'un son jusqu'à sa fin. */
bool hal_audio_busy(void);

#ifdef __cplusplus
}
#endif
