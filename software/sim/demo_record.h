/*
 * Raidmaster 403 — mode enregistrement « démo » du simulateur.
 *
 * Joue un scénario scripté en temps virtuel (indépendant de la vitesse de la machine) et
 * enregistre l'écran à cadence fixe dans un GIF animé : démarrage complet, écran raid qui vit,
 * puis quelques secondes par page (balade, menu, calibration, raid de nuit).
 *
 * Activé par RAIDMASTER_RECORD=<fichier.gif> (avec SDL_VIDEODRIVER=dummy), agrandissement
 * optionnel RAIDMASTER_RECORD_SCALE=1..4. Commande à la demande : tools/demo_gif.sh.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_DEMO_RECORD_H
#define RAIDMASTER_DEMO_RECORD_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lance le scénario après ui_init(). `fb` est la copie plein écran RGB565 (w×h) tenue à jour
 * par le flush du display. Renvoie 0 si le GIF a été écrit. */
int demo_record_run(lv_display_t *disp, const uint16_t *fb, int w, int h,
                    const char *gif_path, int scale);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_DEMO_RECORD_H */
