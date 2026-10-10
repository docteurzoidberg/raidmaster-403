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

typedef enum {
    CALIB_FOCUS_NONE = 0,
    CALIB_FOCUS_RAW,      /* mesure en cours : cadre de la distance brute en cyan */
    CALIB_FOCUS_REAL,     /* saisie de la distance réelle : cadre en amber */
} calib_focus_t;

lv_obj_t *screen_calib_create(void);

/* Textes de l'écran (NULL : inchangé). Conservés hors de l'écran : ils survivent à sa
 * reconstruction. Sans appel, valeurs d'exemple de la maquette. */
void screen_calib_set(const char *hint, const char *raw, const char *real, const char *factor,
                      calib_focus_t focus);

/* Rafraîchit la barre d'état. */
void screen_calib_update(const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_CALIB_H */
