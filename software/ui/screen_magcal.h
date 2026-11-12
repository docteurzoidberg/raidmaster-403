/*
 * Raidmaster 403 — écran calibration de la boussole.
 *
 * Procédure pilotée par le core (core/app : app_magcal_*) depuis ui_app.c : un tour complet sur
 * place, l'anneau de 36 secteurs de 10° se remplit à mesure que le champ tourne, puis MENU
 * enregistre. Sans ui_app (démo, GIF, captures) : valeurs d'exemple.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_MAGCAL_H
#define RAIDMASTER_UI_SCREEN_MAGCAL_H

#include <stdint.h>

#include "lvgl.h"
#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAGCAL_TONE_INFO = 0,   /* consigne (amber) */
    MAGCAL_TONE_OK,         /* tour complet, calibration enregistrée (vert) */
    MAGCAL_TONE_ALERT,      /* boussole absente, tour incomplet (rouge) */
} magcal_tone_t;

lv_obj_t *screen_magcal_create(void);

/* Contenu de l'écran (textes NULL : inchangés). mask : bit i = secteur [10 i, 10 i + 10[ du cap
 * parcouru (N en haut, sens horaire). Conservé hors de l'écran : survit à sa reconstruction. */
void screen_magcal_set(const char *hint, uint64_t mask, const char *cap, const char *status,
                       magcal_tone_t tone);

/* Rafraîchit la barre d'état. */
void screen_magcal_update(const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_MAGCAL_H */
