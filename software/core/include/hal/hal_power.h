/*
 * Raidmaster 403 — HAL alimentation : source d'énergie et batterie (PMIC AXP2101 sur l'ESP32,
 * état choisi au clavier sur le sim).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool   external;    /* alimentation externe présente (USB-C) */
    bool   battery;     /* batterie branchée */
    bool   charging;    /* batterie en charge */
    int8_t pct;         /* charge de la batterie 0..100, -1 si inconnue ou sans batterie */
} hal_power_t;

/* État courant ; false si le PMIC ne répond pas (*out est alors indéfini). Peu coûteux : à
 * appeler à chaque rafraîchissement de l'affichage. */
bool hal_power_read(hal_power_t *out);

#ifdef __cplusplus
}
#endif
