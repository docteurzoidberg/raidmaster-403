/*
 * Raidmaster 403 — écran mode balade.
 *
 * Contenu : compteur de vitesse central énorme, ODO discret, footer CAP/TRIP/T°.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_BALADE_H
#define RAIDMASTER_UI_SCREEN_BALADE_H

#include "lvgl.h"
#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_balade_create(void);
void      screen_balade_update(const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_BALADE_H */
