/*
 * Raidmaster 403 — API top-level de l'UI LVGL (côté sim pour le MVP).
 *
 * Appels côté sim (voir sim/main.c) :
 *   ui_init(disp);                 // après création du display LVGL
 *   while (running) {              // boucle sim
 *     ui_tick(ms_elapsed);
 *     lv_timer_handler();
 *   }
 *
 * Les écrans sont instanciés paresseusement par ui_show_screen().
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_H
#define RAIDMASTER_UI_H

#include "lvgl.h"
#include "ui_state.h"
#include "ui_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_BOOT = 0,
    UI_SCREEN_RAID,
    UI_SCREEN_BALADE,
    UI_SCREEN_MENU,
    UI_SCREEN_CALIB,
    UI_SCREEN__COUNT,
} ui_screen_id_t;

/* Initialise toute l'UI : thème, écran courant (boot), input, démo auto. */
void ui_init(lv_display_t *disp);

/* À appeler périodiquement par le binaire sim pour nourrir le compteur de ticks LVGL.
 * `elapsed_ms` est le temps écoulé depuis le dernier appel. */
void ui_tick(uint32_t elapsed_ms);

/* Change l'écran courant. Si l'écran demandé n'existe pas encore, il est créé. */
void ui_show_screen(ui_screen_id_t id);

/* Renvoie l'écran courant. */
ui_screen_id_t ui_current_screen(void);

/* Cycle entre les écrans RAID → BALADE → MENU → RAID (helper pour la touche `v` du sim). */
void ui_cycle_screen(void);

/* Pousse un état trip au bon écran (raid ou balade selon celui qui est affiché). */
void ui_push_trip_state(const ui_trip_state_t *s);

/* Recrée les écrans avec la palette active (appelé au basculement jour / nuit). */
void ui_rebuild_screens(void);

/* Appelé à la fin du démarrage à la place de « écran raid + démo auto » : la source de
 * données (ui_app, core) affiche l'écran principal choisi. NULL : comportement par défaut. */
typedef void (*ui_boot_done_cb_t)(void);
void ui_set_boot_done_cb(ui_boot_done_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_H */
