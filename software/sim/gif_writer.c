/*
 * Raidmaster 403 — encodeur GIF animé minimal (voir gif_writer.h).
 *
 * Format : GIF89a, palette globale, extension NETSCAPE2.0 (boucle infinie), une extension de
 * contrôle graphique par image (délai, transparence, disposition « laisser en place »), données
 * compressées LZW à taille de code variable (9..12 bits, code de remise à zéro à 4096).
 *
 * Licence : GPL-3.0.
 */

#include "gif_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GIF_MAX_COLORS = 255 };     /* + 1 index réservé à la transparence */

struct gif_rec {
    int       w, h, scale;
    FILE     *tmp;                 /* images RGB565 brutes, dans l'ordre */
    int       nframes;
    int       cap;
    unsigned *delays;              /* délai de chaque image distincte (cs) */
    unsigned  total_cs;
    uint16_t *last;                /* dernière image reçue, pour fusionner les doublons */
    uint32_t *hist;                /* histogramme RGB565 (pixels × images) */
};

/* ------------------------------------------------------------------------- */
/* Couleurs. */

static void rgb565_to_888(uint16_t p, uint8_t out[3])
{
    out[0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
    out[1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
    out[2] = (uint8_t)((p & 0x1F) * 255 / 31);
}

/* Distance perceptuelle grossière (pondération 2/4/3), suffisante pour une UI à peu de teintes. */
static uint32_t color_dist(uint16_t a, uint16_t b)
{
    uint8_t ca[3], cb[3];
    rgb565_to_888(a, ca);
    rgb565_to_888(b, cb);
    int dr = (int)ca[0] - (int)cb[0];
    int dg = (int)ca[1] - (int)cb[1];
    int db = (int)ca[2] - (int)cb[2];
    return (uint32_t)(2 * dr * dr + 4 * dg * dg + 3 * db * db);
}

typedef struct {
    uint16_t color;
    uint32_t count;
} color_count_t;

static int cmp_count_desc(const void *pa, const void *pb)
{
    const color_count_t *a = pa;
    const color_count_t *b = pb;
    if (a->count != b->count) return a->count < b->count ? 1 : -1;
    return (int)a->color - (int)b->color;
}

/* Sélection gloutonne : parcourt les couleurs de la plus fréquente à la moins fréquente et garde
 * celles qui sont à plus de `thresh` de toutes les couleurs déjà gardées. Renvoie le nombre
 * gardé, ou max+1 dès que la palette déborde. */
static int greedy_pick(const color_count_t *cc, int n, uint32_t thresh,
                       uint16_t *pal, uint8_t *taken, int max)
{
    int np = 0;
    memset(taken, 0, (size_t)n);
    for (int i = 0; i < n; i++) {
        int keep = 1;
        for (int j = 0; j < np; j++) {
            if (color_dist(cc[i].color, pal[j]) <= thresh) { keep = 0; break; }
        }
        if (!keep) continue;
        if (np == max) return max + 1;
        taken[i] = 1;
        pal[np++] = cc[i].color;
    }
    return np;
}

/* Construit la palette (≤ GIF_MAX_COLORS entrées RGB565) et la table RGB565 → index. */
static int build_palette(const uint32_t *hist, uint16_t *pal, uint8_t *lut)
{
    int n = 0;
    for (uint32_t c = 0; c < 65536U; c++) {
        if (hist[c] != 0) n++;
    }
    color_count_t *cc = malloc((size_t)(n > 0 ? n : 1) * sizeof(*cc));
    uint8_t *taken    = malloc((size_t)(n > 0 ? n : 1));
    if (cc == NULL || taken == NULL) {
        free(cc);
        free(taken);
        return -1;
    }
    n = 0;
    for (uint32_t c = 0; c < 65536U; c++) {
        if (hist[c] != 0) {
            cc[n].color = (uint16_t)c;
            cc[n].count = hist[c];
            n++;
        }
    }
    qsort(cc, (size_t)n, sizeof(*cc), cmp_count_desc);

    int np;
    if (n <= GIF_MAX_COLORS) {
        /* Cas courant : palette exacte. */
        for (int i = 0; i < n; i++) pal[i] = cc[i].color;
        np = n;
    } else {
        /* Plus petit seuil pour lequel la sélection gloutonne tient dans la palette, puis on
         * complète avec les couleurs fréquentes restantes. */
        uint32_t lo = 0, hi = 600000U;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (greedy_pick(cc, n, mid, pal, taken, GIF_MAX_COLORS) <= GIF_MAX_COLORS) hi = mid;
            else lo = mid + 1;
        }
        np = greedy_pick(cc, n, lo, pal, taken, GIF_MAX_COLORS);
        for (int i = 0; i < n && np < GIF_MAX_COLORS; i++) {
            if (!taken[i]) pal[np++] = cc[i].color;
        }
    }

    /* Chaque couleur vue → entrée de palette la plus proche (identique si présente). */
    for (int i = 0; i < n; i++) {
        uint32_t best_d = UINT32_MAX;
        int best = 0;
        for (int j = 0; j < np && best_d != 0; j++) {
            uint32_t d = color_dist(cc[i].color, pal[j]);
            if (d < best_d) { best_d = d; best = j; }
        }
        lut[cc[i].color] = (uint8_t)best;
    }
    printf("gif : %d couleurs distinctes, palette de %d%s\n", n, np,
           n > np ? " (couleurs rares ramenées à la plus proche)" : " (exacte)");
    free(cc);
    free(taken);
    return np;
}

/* ------------------------------------------------------------------------- */
/* Écriture par sous-blocs de 255 octets + LZW. */

typedef struct {
    FILE    *f;
    uint8_t  block[255];
    int      nblock;
    uint32_t acc;
    int      nbits;
} bitw_t;

static void bw_byte(bitw_t *bw, uint8_t b)
{
    bw->block[bw->nblock++] = b;
    if (bw->nblock == 255) {
        fputc(255, bw->f);
        fwrite(bw->block, 1, 255, bw->f);
        bw->nblock = 0;
    }
}

static void bw_code(bitw_t *bw, unsigned code, int size)
{
    bw->acc |= (uint32_t)code << bw->nbits;
    bw->nbits += size;
    while (bw->nbits >= 8) {
        bw_byte(bw, (uint8_t)(bw->acc & 0xFF));
        bw->acc >>= 8;
        bw->nbits -= 8;
    }
}

static void bw_finish(bitw_t *bw)
{
    if (bw->nbits > 0) bw_byte(bw, (uint8_t)(bw->acc & 0xFF));
    if (bw->nblock > 0) {
        fputc(bw->nblock, bw->f);
        fwrite(bw->block, 1, (size_t)bw->nblock, bw->f);
    }
    fputc(0, bw->f);               /* bloc terminal */
}

enum { HBITS = 14, HSIZE = 1 << HBITS };
static uint32_t s_hkey[HSIZE];     /* clé + 1 (0 = libre) ; clé = préfixe << 8 | pixel */
static uint16_t s_hcode[HSIZE];

static unsigned hash_slot(uint32_t key)
{
    return (key * 2654435761U) >> (32 - HBITS);
}

static void lzw_encode(FILE *f, const uint8_t *px, size_t n)
{
    const unsigned clear = 256, eoi = 257;
    bitw_t bw = { .f = f };
    unsigned next = eoi + 1;
    int size = 9;

    fputc(8, f);                   /* taille minimale de code */
    memset(s_hkey, 0, sizeof(s_hkey));
    bw_code(&bw, clear, size);

    unsigned prefix = px[0];
    for (size_t i = 1; i < n; i++) {
        uint32_t key = ((uint32_t)prefix << 8) | px[i];
        unsigned slot = hash_slot(key);
        int found = 0;
        while (s_hkey[slot] != 0) {
            if (s_hkey[slot] == key + 1) { found = 1; break; }
            slot = (slot + 1) & (HSIZE - 1);
        }
        if (found) {
            prefix = s_hcode[slot];
            continue;
        }
        bw_code(&bw, prefix, size);
        if (next < 4096) {
            if (next == (1U << size)) size++;
            s_hkey[slot]  = key + 1;
            s_hcode[slot] = (uint16_t)next++;
        } else {
            bw_code(&bw, clear, size);
            memset(s_hkey, 0, sizeof(s_hkey));
            next = eoi + 1;
            size = 9;
        }
        prefix = px[i];
    }
    bw_code(&bw, prefix, size);
    bw_code(&bw, eoi, size);
    bw_finish(&bw);
}

/* ------------------------------------------------------------------------- */

gif_rec_t *gif_rec_create(int w, int h, int scale)
{
    if (w <= 0 || h <= 0 || scale < 1 || scale > 4) return NULL;
    gif_rec_t *rec = calloc(1, sizeof(*rec));
    if (rec == NULL) return NULL;
    rec->w = w;
    rec->h = h;
    rec->scale = scale;
    rec->tmp  = tmpfile();
    rec->last = malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    rec->hist = calloc(65536U, sizeof(uint32_t));
    if (rec->tmp == NULL || rec->last == NULL || rec->hist == NULL) {
        gif_rec_free(rec);
        return NULL;
    }
    return rec;
}

int gif_rec_add_frame(gif_rec_t *rec, const uint16_t *rgb565, unsigned delay_cs)
{
    size_t npx = (size_t)rec->w * (size_t)rec->h;
    rec->total_cs += delay_cs;
    if (rec->nframes > 0 && memcmp(rgb565, rec->last, npx * sizeof(uint16_t)) == 0) {
        rec->delays[rec->nframes - 1] += delay_cs;
        return 0;
    }
    if (rec->nframes == rec->cap) {
        int ncap = rec->cap ? rec->cap * 2 : 256;
        unsigned *nd = realloc(rec->delays, (size_t)ncap * sizeof(unsigned));
        if (nd == NULL) return -1;
        rec->delays = nd;
        rec->cap = ncap;
    }
    if (fwrite(rgb565, sizeof(uint16_t), npx, rec->tmp) != npx) return -1;
    for (size_t i = 0; i < npx; i++) rec->hist[rgb565[i]]++;
    memcpy(rec->last, rgb565, npx * sizeof(uint16_t));
    rec->delays[rec->nframes++] = delay_cs;
    return 0;
}

static void put_u16(FILE *f, unsigned v)
{
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

int gif_rec_write(gif_rec_t *rec, const char *path)
{
    if (rec->nframes == 0) return -1;
    const int w = rec->w, h = rec->h, s = rec->scale;
    const size_t npx = (size_t)w * (size_t)h;

    uint16_t pal[GIF_MAX_COLORS];
    static uint8_t lut[65536];
    int np = build_palette(rec->hist, pal, lut);
    if (np < 0) return -1;
    const uint8_t trans = (uint8_t)np;           /* index réservé, jamais une vraie couleur */
    int tab_bits = 1;
    while ((1 << tab_bits) < np + 1) tab_bits++;

    FILE *f = fopen(path, "wb");
    uint16_t *src  = malloc(npx * sizeof(uint16_t));
    uint8_t *cur   = malloc(npx);
    uint8_t *canvas = malloc(npx);
    uint8_t *sub   = malloc(npx * (size_t)s * (size_t)s);
    if (f == NULL || src == NULL || cur == NULL || canvas == NULL || sub == NULL) {
        fprintf(stderr, "gif : impossible d'écrire %s\n", path);
        if (f != NULL) fclose(f);
        free(src); free(cur); free(canvas); free(sub);
        return -1;
    }

    /* En-tête + descripteur d'écran + palette globale. */
    fwrite("GIF89a", 1, 6, f);
    put_u16(f, (unsigned)(w * s));
    put_u16(f, (unsigned)(h * s));
    fputc(0x80 | (7 << 4) | (tab_bits - 1), f);
    fputc(0, f);                                  /* fond : index 0 */
    fputc(0, f);                                  /* rapport d'aspect */
    for (int i = 0; i < (1 << tab_bits); i++) {
        uint8_t rgb[3] = {0, 0, 0};
        if (i < np) rgb565_to_888(pal[i], rgb);
        fwrite(rgb, 1, 3, f);
    }
    /* Boucle infinie. */
    fwrite("\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00", 1, 19, f);

    rewind(rec->tmp);
    long delay_pos = -1;
    unsigned delay_acc = 0;
    for (int fi = 0; fi < rec->nframes; fi++) {
        if (fread(src, sizeof(uint16_t), npx, rec->tmp) != npx) break;
        for (size_t i = 0; i < npx; i++) cur[i] = lut[src[i]];

        /* Rectangle modifié par rapport à ce qui est déjà affiché. */
        int x0 = 0, y0 = 0, x1 = w - 1, y1 = h - 1;
        if (fi > 0) {
            x0 = w; y0 = h; x1 = -1; y1 = -1;
            for (int y = 0; y < h; y++) {
                const uint8_t *a = &cur[(size_t)y * w];
                const uint8_t *b = &canvas[(size_t)y * w];
                for (int x = 0; x < w; x++) {
                    if (a[x] != b[x]) {
                        if (x < x0) x0 = x;
                        if (x > x1) x1 = x;
                        if (y < y0) y0 = y;
                        y1 = y;
                    }
                }
            }
            if (x1 < 0) {
                /* Identique après quantification : on allonge l'image précédente. */
                delay_acc += rec->delays[fi];
                long here = ftell(f);
                fseek(f, delay_pos, SEEK_SET);
                put_u16(f, delay_acc > 65535U ? 65535U : delay_acc);
                fseek(f, here, SEEK_SET);
                continue;
            }
        }

        /* Sous-image agrandie ×s ; pixels inchangés → transparents (meilleure compression). */
        const int sw = (x1 - x0 + 1) * s, sh = (y1 - y0 + 1) * s;
        size_t k = 0;
        for (int Y = 0; Y < sh; Y++) {
            size_t row = (size_t)(y0 + Y / s) * w;
            for (int X = 0; X < sw; X++) {
                size_t i = row + (size_t)(x0 + X / s);
                sub[k++] = (fi > 0 && cur[i] == canvas[i]) ? trans : cur[i];
            }
        }

        /* Extension de contrôle graphique : disposition 1 (laisser), transparence si fi > 0. */
        fputc(0x21, f); fputc(0xF9, f); fputc(4, f);
        fputc((1 << 2) | (fi > 0 ? 1 : 0), f);
        delay_pos = ftell(f);
        delay_acc = rec->delays[fi];
        put_u16(f, delay_acc > 65535U ? 65535U : delay_acc);
        fputc(trans, f);
        fputc(0, f);

        /* Descripteur d'image, sans palette locale. */
        fputc(0x2C, f);
        put_u16(f, (unsigned)(x0 * s));
        put_u16(f, (unsigned)(y0 * s));
        put_u16(f, (unsigned)sw);
        put_u16(f, (unsigned)sh);
        fputc(0, f);
        lzw_encode(f, sub, k);

        memcpy(canvas, cur, npx);
    }
    fputc(0x3B, f);                               /* fin de fichier */
    int err = ferror(f);
    if (fclose(f) != 0) err = 1;
    free(src); free(cur); free(canvas); free(sub);
    return err ? -1 : 0;
}

int gif_rec_frame_count(const gif_rec_t *rec)
{
    return rec->nframes;
}

unsigned gif_rec_duration_cs(const gif_rec_t *rec)
{
    return rec->total_cs;
}

void gif_rec_free(gif_rec_t *rec)
{
    if (rec == NULL) return;
    if (rec->tmp != NULL) fclose(rec->tmp);
    free(rec->delays);
    free(rec->last);
    free(rec->hist);
    free(rec);
}
