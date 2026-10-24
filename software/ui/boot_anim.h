/*
 * Raidmaster 403 — animation de démarrage (fichier « RMBA » : palette 256 couleurs + images
 * indexées, brutes en v1 ou codées par différence en v2, voir ui/assets/boot_anim/build_boot_anim.py).
 *
 * Données lues par hal_asset (asset « boot_anim ») : fichier généré au build sur le sim,
 * partition flash boot_anim mappée en mémoire sur l'ESP32 (voir cmake/boot_anim.cmake).
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

/* Teinte monochrome (mode nuit) : chaque couleur devient sa luminance × (r, g, b).
 * on = false : couleurs d'origine. À appeler après boot_anim_open(). */
void boot_anim_set_tint(bool on, uint8_t r, uint8_t g, uint8_t b);

/* Écrit l'image `index` en RGB565 dans `dst` (largeur × hauteur pixels). En v2, `dst` doit
 * contenir l'image rendue juste avant (index − 1) pour que seuls les pixels qui changent soient
 * écrits ; sinon (premier appel, saut, autre tampon, teinte changée) l'animation est rejouée
 * depuis la première image. Lignes écrites dans [*y0, *y1] (y0 > y1 : aucune ; pointeurs NULL
 * acceptés). false si le fichier est illisible (dst est alors incomplet). */
bool boot_anim_render(uint16_t index, uint16_t *dst, uint16_t *y0, uint16_t *y1);

#ifdef __cplusplus
}
#endif
