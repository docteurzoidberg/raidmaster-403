/*
 * Raidmaster 403 — thème LVGL custom basé sur les tokens de ui_tokens.h.
 *
 * Deux modes : jour (`ui_theme_apply_day`) et nuit (`ui_theme_apply_night`), bascule par
 * `ui_theme_toggle_night()` (voir prompts/ui.md § 2.2 : bascule manuelle, pas de LDR).
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_THEME_H
#define RAIDMASTER_UI_THEME_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise et applique le thème sur un display donné. À appeler une fois après la création
 * du display LVGL, avant toute création d'écran. */
void ui_theme_init(lv_display_t *disp);

/* Bascule entre mode jour et nuit, met à jour tous les écrans actifs. */
void ui_theme_toggle_night(void);

/* Fixe le mode : avant ui_init (mode sauvegardé, appliqué dès l'écran de démarrage) ou après
 * (reconstruit les écrans si le mode change). */
void ui_theme_set_night(bool on);

/* Renvoie l'état courant. */
bool ui_theme_is_night(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_THEME_H */
