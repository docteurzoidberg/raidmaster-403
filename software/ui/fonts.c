/*
 * Raidmaster 403 — polices de l'écran : Share Tech Mono en bitmap LVGL compilé.
 *
 * Les fichiers ui/fonts/stm_<px>.c sont générés par ui/fonts/gen_fonts.sh (lv_font_conv,
 * 4 bpp, non compressés) aux seules tailles utilisées ; ils sont identiques pour le
 * simulateur et l'ESP32 (rien n'est chargé à l'exécution).
 *
 * Licence : GPL-3.0.
 */

#include "fonts.h"
#include <stdio.h>

#define FONTS(X) X(12) X(13) X(14) X(15) X(16) X(17) X(25) X(29) X(34) X(38) X(44) X(72) X(134) X(190)

#define DECLARE(px) extern const lv_font_t stm_##px;
FONTS(DECLARE)
#undef DECLARE

typedef struct {
    uint16_t         px;
    const lv_font_t *font;
} font_entry_t;

#define ENTRY(px) {px, &stm_##px},
static const font_entry_t s_fonts[] = { FONTS(ENTRY) };
#undef ENTRY

#define FONT_COUNT ((int)(sizeof(s_fonts) / sizeof(s_fonts[0])))

void ui_fonts_init(void)
{
    /* Rien à charger : polices compilées. Conservé pour garder le point d'entrée. */
}

void ui_fonts_deinit(void)
{
}

const lv_font_t *ui_font_mono(uint16_t px)
{
    for (int i = 0; i < FONT_COUNT; i++) {
        if (s_fonts[i].px == px) return s_fonts[i].font;
    }
    /* Taille non générée : erreur de développement, on prend la plus proche inférieure. */
    fprintf(stderr, "ui_fonts : taille %u px non générée (voir ui/fonts/gen_fonts.sh)\n",
            (unsigned)px);
    const lv_font_t *best = s_fonts[0].font;
    for (int i = 0; i < FONT_COUNT; i++) {
        if (s_fonts[i].px <= px) best = s_fonts[i].font;
    }
    return best;
}
