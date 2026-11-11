/*
 * Raidmaster 403 — menu réglages.
 *
 * Contenu : liste scrollable, surbrillance amber, navigation +/−, entrée menu.
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

/* Entrées du menu, dans l'ordre d'affichage. */
typedef enum {
    MENU_CALIB = 0,     /* Calibration ±% (édition +/−) */
    MENU_AUTOCAL,       /* Calibration auto distance → écran dédié */
    MENU_TZ,            /* Fuseau horaire (édition +/−) */
    MENU_NIGHT,         /* Mode nuit (bascule) */
    MENU_VIEW,          /* Affichage RAID / BALADE (bascule) */
    MENU_ALARM,         /* Alarme vitesse (édition +/−) */
    MENU_GPX,           /* Log GPX sur SD (bascule) */
    MENU_COMPASS,       /* Calibration boussole → écran dédié (à venir) */
    MENU__COUNT,
} menu_item_id_t;

lv_obj_t *screen_menu_create(void);

/* Entrée sélectionnée (menu_item_id_t). */
int screen_menu_selected(void);

/* Texte de la valeur d'une entrée. Conservé hors de l'écran : il survit à sa reconstruction
 * (bascule jour / nuit). Sans appel, valeurs d'exemple de la maquette. */
void screen_menu_set_value(int item, const char *text);

/* Édition de l'entrée sélectionnée : la ligne passe en cyan, +/− changent la valeur. */
void screen_menu_set_editing(bool on);

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
