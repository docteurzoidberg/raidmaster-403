/*
 * Raidmaster 403 — animation de démarrage (fichier « RMBA » : palette 256 couleurs + images
 * indexées, voir ui/assets/boot_anim/build_boot_anim.py).
 *
 * Sim : le fichier est lu depuis RAIDMASTER_BOOT_ANIM_PATH (généré au build).
 * ESP32 (TODO firmware) : même fichier dans une partition flash dédiée, mappée en mémoire
 * (esp_partition_mmap) ; seule boot_anim_open() changera.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Charge l'animation. Renvoie false si elle est absente ou invalide (l'écran de démarrage
 * retombe alors sur sa version sans animation). */
bool boot_anim_open(void);

/* Libère les données chargées. */
void boot_anim_close(void);

uint16_t boot_anim_width(void);
uint16_t boot_anim_height(void);
uint16_t boot_anim_frame_count(void);
uint16_t boot_anim_fps(void);

/* Écrit l'image `index` en RGB565 dans `dst` (largeur × hauteur pixels). */
void boot_anim_render(uint16_t index, uint16_t *dst);

#ifdef __cplusplus
}
#endif
