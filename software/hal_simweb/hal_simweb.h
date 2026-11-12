/*
 * Raidmaster 403 — HAL du simulateur web (WebAssembly) : ce qui diffère du sim PC.
 *
 * Seuls le stockage (localStorage du navigateur au lieu de sim-storage.json) et le son (Web Audio
 * au lieu de SDL) sont propres au web. GPS, boussole, horloge, alimentation et assets reprennent
 * tels quels les fichiers de hal_sim/ (voir CMakeLists.txt) ; leurs réglages restent dans
 * hal_sim.h.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>

#include "hal_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stockage : persistent, chaque clé est gardée dans le localStorage du navigateur
 * (« raidmaster-403.nvs.<clé> » = valeur en hexadécimal, comme sim-storage.json), relue au
 * démarrage suivant ; sinon en mémoire seulement (comme --no-storage, pour les scénarios joués avec
 * leurs actions). */
void hal_storage_web_open(bool persistent);

#ifdef __cplusplus
}
#endif
