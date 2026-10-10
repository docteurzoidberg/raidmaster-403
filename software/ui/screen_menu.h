/*
 * Raidmaster 403 — menu réglages.
 *
 * Spec : prompts/ui.md § 3.3 — liste scrollable, surbrillance amber, navigation +/−, entrée menu.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_MENU_H
#define RAIDMASTER_UI_SCREEN_MENU_H

#include "lvgl.h"
#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_menu_create(void);

/* Rafraîchit la barre d'état. */
void screen_menu_update(const ui_trip_state_t *s);

/* Déplace la sélection d'un cran vers le bas (+) ou le haut (−). */
void screen_menu_move(int delta);

/* Active l'item sélectionné (pour la touche menu courte). */
void screen_menu_activate(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_MENU_H */
