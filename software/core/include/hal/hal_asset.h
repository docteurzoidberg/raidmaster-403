/*
 * Raidmaster 403 — HAL données embarquées en lecture seule (animation de démarrage…) :
 * partition flash de même nom mappée en mémoire sur l'ESP32, fichier <nom>.bin généré au build
 * sur le sim.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Renvoie les données de l'asset `name` (nom de partition, ≤ 15 caractères) et leur taille
 * dans *size, ou NULL s'il est absent. Sur l'ESP32, la taille est celle de la partition : le
 * format des données doit porter sa propre longueur. */
const void *hal_asset_map(const char *name, size_t *size);

/* Libère un asset obtenu par hal_asset_map (NULL accepté). */
void hal_asset_unmap(const void *data);

#ifdef __cplusplus
}
#endif
