/*
 * Raidmaster 403 — polices de l'écran.
 *
 * Toute l'interface LCD est en Share Tech Mono (cf. maquettes, artifact « Raidmaster 403 —
 * maquettes »), en polices bitmap LVGL compilées dans le binaire (ui/fonts/, générées par
 * gen_fonts.sh) : mêmes fichiers pour le simulateur et l'ESP32.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Point d'entrée conservé (aucun chargement nécessaire avec les polices compilées). */
void ui_fonts_init(void);

/* Sans effet (polices statiques). */
void ui_fonts_deinit(void);

/* Share Tech Mono à `px` pixels, parmi les tailles générées. Jamais NULL : une taille non
 * générée est signalée sur stderr et remplacée par la plus proche inférieure. */
const lv_font_t *ui_font_mono(uint16_t px);

#ifdef __cplusplus
}
#endif
