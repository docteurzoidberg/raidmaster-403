/*
 * Raidmaster 403 — briques communes aux écrans : labels, boîtes, séparateurs, placement sur
 * ligne de base, barre d'état.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include "lvgl.h"
#include "ui_state.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Écran racine plein cadre, fond palette, sans scroll. */
lv_obj_t *ui_screen_base(void);

/* Conteneur transparent sans style ni scroll. */
lv_obj_t *ui_box(lv_obj_t *parent);

/* Label avec police + couleur. */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);

/* Trait horizontal (séparateur) de 1 px aux couleurs de la palette. */
lv_obj_t *ui_hline(lv_obj_t *parent, int32_t x, int32_t y, int32_t w);

/* Place un label pour que sa ligne de base tombe sur `baseline_y`.
 * align = LV_TEXT_ALIGN_LEFT : `x` est le bord gauche ; LV_TEXT_ALIGN_RIGHT : `x` est le
 * bord droit (largeur `w` réservée, texte calé à droite) ; LV_TEXT_ALIGN_CENTER : `x` est
 * le centre (largeur `w`). */
void ui_place_baseline(lv_obj_t *lbl, int32_t x, int32_t baseline_y, lv_text_align_t align,
                       int32_t w);

/* Formate un nombre de km avec espace des milliers et `decimals` décimales à virgule,
 * comme sur la maquette (« 48 273 », « 12,3 »). */
void ui_fmt_km(char *buf, size_t len, double km, int decimals);

/* Barre d'état commune (maquette) : heure, [10Hz], pastille fix, jauge batterie,
 * [température], mode. */
typedef struct {
    lv_obj_t *time;
    lv_obj_t *hz;
    lv_obj_t *fix_dot;
    lv_obj_t *fix;
    lv_obj_t *batt_box;
    lv_obj_t *cells[4];
    lv_obj_t *batt;
    lv_obj_t *temp;
    lv_obj_t *mode;
} ui_statusbar_t;

void ui_statusbar_create(lv_obj_t *parent, ui_statusbar_t *sb, bool show_hz, bool show_temp,
                         const char *mode_text);
void ui_statusbar_update(ui_statusbar_t *sb, const ui_trip_state_t *s);

#ifdef __cplusplus
}
#endif
