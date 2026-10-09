/*
 * Raidmaster 403 — implémentation de l'abstraction boutons côté UI.
 * Licence : GPL-3.0.
 */

#include "ui_input.h"
#include <stddef.h>

static ui_button_cb_t s_cb   = NULL;
static void          *s_user = NULL;

void ui_input_set_cb(ui_button_cb_t cb, void *user)
{
    s_cb   = cb;
    s_user = user;
}

void ui_input_dispatch(ui_button_t btn, ui_button_event_t ev)
{
    if (btn >= UI_BTN__COUNT) {
        return;
    }
    if (s_cb != NULL) {
        s_cb(btn, ev, s_user);
    }
}
