/*
 * Raidmaster 403 — HAL boussole : mesures brutes du magnétomètre (boussole du puck sur l'I2C1 de
 * l'ESP32, par core/mag ; boussole émulée sur le sim). La calibration, le lissage et le choix de
 * la source du cap sont dans core/app.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>

#include "mag.h"

#ifdef __cplusplus
extern "C" {
#endif

void hal_mag_init(void);

/* À appeler souvent depuis la boucle principale. true : mesure neuve dans *raw, en µT, repère
 * voiture (X avant, Y droite, Z bas), non calibrée. */
bool hal_mag_poll(mag_vec_t *raw);

/* Boussole présente (mesure neuve depuis moins d'une seconde). Faux : boussole absente. */
bool hal_mag_present(void);

#ifdef __cplusplus
}
#endif
