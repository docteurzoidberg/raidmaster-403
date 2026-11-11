/*
 * Raidmaster 403 — écran de démarrage (splash).
 *
 * Contenu : logo 403 grand format, dots de progression, ~1-2 s puis switch
 * vers l'écran raid. Pour le MVP on reste textuel (silhouette SVG en TODO phase 2).
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_SCREEN_BOOT_H
#define RAIDMASTER_UI_SCREEN_BOOT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Crée l'écran. Le screen n'est pas chargé automatiquement — c'est ui_show_screen() qui le fait. */
lv_obj_t *screen_boot_create(void);

/* Durée d'affichage du démarrage avant de passer à l'écran raid : une rotation complète de
 * l'animation si elle est disponible, sinon ~1,2 s. Valable après screen_boot_create(). */
uint32_t screen_boot_duration_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_SCREEN_BOOT_H */
