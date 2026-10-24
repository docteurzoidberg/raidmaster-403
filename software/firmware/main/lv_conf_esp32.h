/*
 * Raidmaster 403 — configuration LVGL du firmware : celle du sim (ui/lv_conf.h), sauf l'allocateur.
 * Le tas intégré de LVGL (LV_MEM_SIZE = 256 Ko, tableau statique) ne tient pas en RAM interne :
 * on prend le malloc de la libc, qu'ESP-IDF sert en RAM interne pour les petits blocs et en PSRAM
 * au-delà de 16 Ko (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL).
 *
 * Inclus par LVGL via LV_CONF_PATH (firmware/CMakeLists.txt).
 *
 * Licence : GPL-3.0.
 */

#include "../../ui/lv_conf.h"

#undef  LV_USE_STDLIB_MALLOC
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
