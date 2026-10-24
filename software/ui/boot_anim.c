/*
 * Raidmaster 403 — animation de démarrage : lecture du fichier « RMBA » et conversion RGB565.
 * Deux formats (ui/assets/boot_anim/build_boot_anim.py, choix au build : cmake/boot_anim.cmake) :
 *   v1  images d'index brutes (5,5 Mo) : chaque image est convertie en entier ;
 *   v2  images codées par différence avec la précédente (sauts, aplats, copies ; 1,1 Mo, sans
 *       perte) : seuls les pixels qui changent sont écrits (~42 Ko par image au lieu de 154 Ko).
 *
 * Licence : GPL-3.0.
 */

#include "boot_anim.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_asset.h"

#define HEADER_SIZE   16
#define PALETTE_SIZE  (256 * 4)
#define LEN_ESC       63        /* v2 : longueur 63 = 63 + entier LEB128 qui suit */

enum { OP_SKIP = 0, OP_FILL = 1, OP_COPY = 2 };

static const uint8_t  *s_blob = NULL;
static const uint8_t  *s_data = NULL;      /* v1 : images ; v2 : codes */
static const uint8_t  *s_table = NULL;     /* v2 : (nb + 1) × u32, début des codes de chaque image */
static size_t          s_data_size;
static uint16_t        s_version;
static uint16_t        s_pal565[256];
static uint16_t        s_w, s_h, s_n, s_fps;
static const uint16_t *s_last_dst;         /* v2 : dernier rendu (tampon, image ; -1 si aucun) */
static int             s_last = -1;

static inline uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool boot_anim_open(void)
{
    if (s_blob != NULL) return true;
    size_t size = 0;
    s_blob = hal_asset_map("boot_anim", &size);
    if (s_blob == NULL) return false;

    const size_t base = HEADER_SIZE + PALETTE_SIZE;
    s_version = size >= base && memcmp(s_blob, "RMBA", 4) == 0 ? rd16(s_blob + 4) : 0;
    if (s_version != 1 && s_version != 2) {
        fprintf(stderr, "boot_anim : format inattendu, démarrage sans animation\n");
        boot_anim_close();
        return false;
    }
    s_w   = rd16(s_blob + 6);
    s_h   = rd16(s_blob + 8);
    s_n   = rd16(s_blob + 10);
    s_fps = rd16(s_blob + 12);
    bool ok = s_fps != 0 && s_n != 0 && s_w != 0 && s_h != 0;
    if (ok && s_version == 1) {
        ok = base + (size_t)s_n * s_w * s_h <= size;
        s_data = s_blob + base;
    } else if (ok) {
        const size_t table_size = ((size_t)s_n + 1U) * 4U;
        ok = base + table_size <= size && rd32(s_blob + base + table_size - 4U) <= size - base - table_size;
        s_table = s_blob + base;
        s_data = s_table + table_size;
        s_data_size = size - base - table_size;
    }
    if (!ok) {
        fprintf(stderr, "boot_anim : fichier tronqué\n");
        boot_anim_close();
        return false;
    }
    boot_anim_set_tint(false, 0, 0, 0);
    return true;
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
    s_last = -1;    /* v2 : les pixels déjà rendus ont les anciennes couleurs */
}

void boot_anim_close(void)
{
    hal_asset_unmap(s_blob);
    s_blob  = NULL;
    s_data  = NULL;
    s_table = NULL;
    s_last  = -1;
    s_version = 0;
    s_w = s_h = s_n = s_fps = 0;
}

uint16_t boot_anim_width(void)       { return s_w; }
uint16_t boot_anim_height(void)      { return s_h; }
uint16_t boot_anim_frame_count(void) { return s_n; }
uint16_t boot_anim_fps(void)         { return s_fps; }

/* v2 : applique les codes de l'image `index` à dst ; premier et dernier pixel écrits dans
 * *first, *last. false si les codes sont incohérents. */
static bool decode_v2(uint16_t index, uint16_t *dst, size_t *first, size_t *last)
{
    const uint32_t a = rd32(s_table + 4U * index), b = rd32(s_table + 4U * (index + 1U));
    if (a > b || b > s_data_size) return false;
    const uint8_t *p = s_data + a;
    const uint8_t *const end = s_data + b;
    const size_t px = (size_t)s_w * s_h;
    size_t pos = 0;
    while (p < end) {
        const uint8_t  c = *p++;
        const unsigned op = c >> 6;
        size_t n = c & 0x3FU;
        if (n == LEN_ESC) {
            size_t v = 0;
            unsigned sh = 0;
            uint8_t byte;
            do {
                if (p >= end || sh > 28) return false;
                byte = *p++;
                v |= (size_t)(byte & 0x7FU) << sh;
                sh += 7;
            } while (byte & 0x80U);
            n += v;
        }
        if (n > px - pos) return false;
        if (op == OP_FILL) {
            if (p >= end) return false;
            const uint16_t v = s_pal565[*p++];
            for (size_t k = 0; k < n; k++) dst[pos + k] = v;
        } else if (op == OP_COPY) {
            if ((size_t)(end - p) < n) return false;
            for (size_t k = 0; k < n; k++) dst[pos + k] = s_pal565[p[k]];
            p += n;
        } else if (op != OP_SKIP) {
            return false;
        }
        if (op != OP_SKIP && n > 0) {
            if (pos < *first) *first = pos;
            *last = pos + n - 1;
        }
        pos += n;
    }
    return pos == px;
}

bool boot_anim_render(uint16_t index, uint16_t *dst, uint16_t *y0, uint16_t *y1)
{
    if (y0 != NULL) *y0 = 1;
    if (y1 != NULL) *y1 = 0;
    if (s_data == NULL || dst == NULL) return false;
    index = (uint16_t)(index % s_n);

    if (s_version == 1) {
        const size_t   px  = (size_t)s_w * s_h;
        const uint8_t *src = s_data + (size_t)index * px;
        for (size_t i = 0; i < px; i++) dst[i] = s_pal565[src[i]];
        if (y0 != NULL) *y0 = 0;
        if (y1 != NULL) *y1 = (uint16_t)(s_h - 1U);
        return true;
    }

    const bool same_buf = dst == s_last_dst;
    if (same_buf && s_last == index) return true;
    /* Pas l'image qui suit le dernier rendu dans ce tampon : on repart de la première (complète). */
    const uint16_t from = (index != 0 && same_buf && s_last == index - 1) ? index : 0;
    size_t first = SIZE_MAX, last = 0;
    for (uint16_t i = from; i <= index; i++) {
        if (!decode_v2(i, dst, &first, &last)) {
            fprintf(stderr, "boot_anim : image %u illisible\n", i);
            s_last = -1;
            return false;
        }
    }
    s_last = index;
    s_last_dst = dst;
    if (first <= last) {
        if (y0 != NULL) *y0 = (uint16_t)(first / s_w);
        if (y1 != NULL) *y1 = (uint16_t)(last / s_w);
    }
    return true;
}
