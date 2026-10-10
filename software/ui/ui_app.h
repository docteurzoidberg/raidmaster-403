/*
 * Raidmaster 403 — câblage de l'UI sur le core (core/app).
 *
 * ui_app_attach() après ui_init() :
 *   - toutes les 100 ms, l'état du core est converti en ui_trip_state_t et poussé aux écrans,
 *     avec les valeurs du menu et de l'écran de calibration ;
 *   - les boutons pilotent le core (prompts/ui.md § 4.1) ;
 *   - à la fin du démarrage, l'écran principal choisi au menu (raid ou balade) s'affiche ;
 *   - le mode nuit sauvegardé est appliqué.
 *
 * Sans ui_app_attach (démo, GIF, captures), l'UI garde son comportement autonome.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_app_attach(app_t *app);

/* Bascule jour / nuit en gardant le réglage du core à jour (touche `n` du sim). */
void ui_app_toggle_night(void);

#ifdef __cplusplus
}
#endif
