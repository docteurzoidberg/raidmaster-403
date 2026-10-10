/*
 * Raidmaster 403 — HAL stockage clé / valeur (NVS sur l'ESP32, sim-storage.json sur le sim).
 *
 * Valeurs binaires opaques, clés courtes (≤ 15 caractères, contrainte NVS).
 * Licence : GPL-3.0.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lit la valeur de `key` dans buf (capacité *len). Renvoie 0 et met la taille lue dans *len,
 * ou -1 si la clé est absente ou plus grande que buf. */
int hal_storage_get(const char *key, void *buf, size_t *len);

/* Écrit (remplace) la valeur de `key`. Renvoie 0, ou -1 en cas d'échec. */
int hal_storage_set(const char *key, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif
