/*
 * Raidmaster 403 — écran mode raid (vue par défaut).
 *
 * Contenu : barre d'état, Trip A dominant (amber), Trip B secondaire (cyan),
 * V + CAP au milieu, footer ODO / MOY / CHRONO.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_RAID_H
#define RAIDMASTER_UI_SCREEN_RAID_H

#include "lvgl.h"
#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_raid_create(void);
void      screen_raid_update(const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_RAID_H */
