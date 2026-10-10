/*
 * Raidmaster 403 — animation de démarrage : lecture du fichier « RMBA » et conversion RGB565.
 *
 * Licence : GPL-3.0.
 */

#include "boot_anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE   16
#define PALETTE_SIZE  (256 * 4)

static uint8_t       *s_blob = NULL;
static const uint8_t *s_frames = NULL;
static uint16_t       s_pal565[256];
static uint16_t       s_w, s_h, s_n, s_fps;

static inline uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool boot_anim_open(void)
{
    if (s_blob != NULL) return true;
#ifndef RAIDMASTER_BOOT_ANIM_PATH
    return false;
#else
    FILE *f = fopen(RAIDMASTER_BOOT_ANIM_PATH, "rb");
    if (f == NULL) {
        fprintf(stderr, "boot_anim : %s introuvable, démarrage sans animation\n",
                RAIDMASTER_BOOT_ANIM_PATH);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < HEADER_SIZE + PALETTE_SIZE) {
        fclose(f);
        return false;
    }
    s_blob = malloc((size_t)size);
    if (s_blob == NULL || fread(s_blob, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        boot_anim_close();
        return false;
    }
    fclose(f);

    if (memcmp(s_blob, "RMBA", 4) != 0 || rd16(s_blob + 4) != 1) {
        fprintf(stderr, "boot_anim : format inattendu\n");
        boot_anim_close();
        return false;
    }
    s_w   = rd16(s_blob + 6);
    s_h   = rd16(s_blob + 8);
    s_n   = rd16(s_blob + 10);
    s_fps = rd16(s_blob + 12);
    if (s_fps == 0 || s_n == 0 ||
        (long)HEADER_SIZE + PALETTE_SIZE + (long)s_n * s_w * s_h > size) {
        fprintf(stderr, "boot_anim : fichier tronqué\n");
        boot_anim_close();
        return false;
    }

    s_frames = s_blob + HEADER_SIZE + PALETTE_SIZE;
    boot_anim_set_tint(false, 0, 0, 0);
    return true;
#endif
}

void boot_anim_set_tint(bool on, uint8_t tr, uint8_t tg, uint8_t tb)
{
    if (s_blob == NULL) return;
    /* Palette B, G, R, A → RGB565 ; teinte : luminance de la couleur × couleur de teinte. */
    const uint8_t *pal = s_blob + HEADER_SIZE;
    for (int i = 0; i < 256; i++) {
        unsigned b = pal[4 * i], g = pal[4 * i + 1], r = pal[4 * i + 2];
        if (on) {
            const unsigned y = (299U * r + 587U * g + 114U * b) / 1000U;   /* 0..255 */
            r = tr * y / 255U;
            g = tg * y / 255U;
            b = tb * y / 255U;
        }
        s_pal565[i] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

void boot_anim_close(void)
{
    free(s_blob);
    s_blob   = NULL;
    s_frames = NULL;
    s_w = s_h = s_n = s_fps = 0;
}

uint16_t boot_anim_width(void)       { return s_w; }
uint16_t boot_anim_height(void)      { return s_h; }
uint16_t boot_anim_frame_count(void) { return s_n; }
uint16_t boot_anim_fps(void)         { return s_fps; }

void boot_anim_render(uint16_t index, uint16_t *dst)
{
    if (s_frames == NULL || dst == NULL) return;
    const size_t   px  = (size_t)s_w * s_h;
    const uint8_t *src = s_frames + (size_t)(index % s_n) * px;
    for (size_t i = 0; i < px; i++) {
        dst[i] = s_pal565[src[i]];
    }
}
