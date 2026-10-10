/*
 * Raidmaster 403 — écran calibration auto distance.
 *
 * Spec : prompts/ui.md § 3.4 — procédure guidée « rouler X km, A au départ, A à l'arrivée ».
 * Pour le MVP, stub statique avec valeurs mockées.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_CALIB_H
#define RAIDMASTER_UI_SCREEN_CALIB_H

#include "lvgl.h"
#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_calib_create(void);

/* Rafraîchit la barre d'état. */
void screen_calib_update(const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_CALIB_H */
