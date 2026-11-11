/*
 * Raidmaster 403 — abstraction clavier/boutons pour l'UI (côté sim).
 *
 * Les 5 boutons physiques du MVP sont émulés côté sim par le
 * clavier (voir sim/main.c). Ce header expose l'enum stable et le prototype du callback
 * que l'UI (ou le futur core) peut enregistrer.
 *
 * TODO : aligner avec le futur hal_buttons.h (pilote des boutons de la carte) une fois ce
 * dernier écrit. Les enums BTN_* ci-dessous reprennent exactement les mêmes
 * valeurs pour éviter toute conversion.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_INPUT_H
#define RAIDMASTER_UI_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_BTN_A = 0,
    UI_BTN_B,
    UI_BTN_PLUS,
    UI_BTN_MINUS,
    UI_BTN_MENU,
    UI_BTN__COUNT,
} ui_button_t;

typedef enum {
    UI_BTN_EV_PRESS = 0,
    UI_BTN_EV_LONGPRESS,
    UI_BTN_EV_RELEASE,
} ui_button_event_t;

typedef void (*ui_button_cb_t)(ui_button_t btn, ui_button_event_t ev, void *user);

/* Enregistre un callback (un seul à la fois pour le MVP). NULL pour désinscrire. */
void ui_input_set_cb(ui_button_cb_t cb, void *user);

/* Dispatch utilisé par le binaire sim pour injecter les évènements clavier convertis. */
void ui_input_dispatch(ui_button_t btn, ui_button_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_INPUT_H */
