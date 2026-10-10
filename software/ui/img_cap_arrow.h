/*
 * Raidmaster 403 — sprites flèche de cap (style 07 isométrique 3D, 64×64 ARGB8888).
 * Pointe vers le haut (Nord) ; pivoter avec lv_image_set_rotation(img, cap * 10).
 * Données générées par ui/assets/gen_cap_arrow.py.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include "lvgl.h"

#define IMG_CAP_ARROW_SIZE 64

extern const lv_image_dsc_t img_cap_arrow;
extern const lv_image_dsc_t img_cap_arrow_shadow;
