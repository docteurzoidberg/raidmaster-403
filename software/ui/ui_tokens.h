/*
 * Raidmaster 403 — tokens de design partagés par tous les écrans LVGL.
 *
 * Valeurs issues de prompts/ui.md § 2.1 (palette LCD jour) et § 2.2 (palette LCD nuit).
 * Toute évolution doit être répliquée dans prompts/ui.md et dans la maquette publiée (lien § 1).
 *
 * Licence : GPL-3.0 — voir LICENSE à la racine.
 */

#ifndef RAIDMASTER_UI_TOKENS_H
#define RAIDMASTER_UI_TOKENS_H

#include "lvgl.h"
#include "fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Palette LCD jour                                                          */
/* ------------------------------------------------------------------------- */

#define UI_COLOR_LCD_BG         lv_color_hex(0x0A0E14)  /* fond principal (noir bleuté) */
#define UI_COLOR_LCD_PANEL      lv_color_hex(0x141821)  /* panneau barre d'état */
#define UI_COLOR_LCD_DIVIDER    lv_color_hex(0x202836)  /* séparateurs fins */
#define UI_COLOR_LCD_TEXT       lv_color_hex(0xE8ECF0)  /* texte primaire */
#define UI_COLOR_LCD_MUTED      lv_color_hex(0x6B7585)  /* texte secondaire / labels */
#define UI_COLOR_LCD_AMBER      lv_color_hex(0xFFB84D)  /* chiffres dominants (Trip A, speedo) */
#define UI_COLOR_LCD_CYAN       lv_color_hex(0x4DD0E1)  /* chiffres secondaires (Trip B, data) */
#define UI_COLOR_LCD_OK         lv_color_hex(0x66BB6A)  /* fix GPS, batterie OK */
#define UI_COLOR_LCD_WARN       lv_color_hex(0xFFA726)  /* HDOP > 2, batterie faible */
#define UI_COLOR_LCD_CRIT       lv_color_hex(0xEF5350)  /* perte fix, alarme vitesse */

/* ------------------------------------------------------------------------- */
/* Palette LCD nuit                                                          */
/* ------------------------------------------------------------------------- */

#define UI_COLOR_NIGHT_BG       lv_color_hex(0x000000)  /* noir pur */
#define UI_COLOR_NIGHT_FG       lv_color_hex(0xFF3B00)  /* orange profond, scotopique */
#define UI_COLOR_NIGHT_MUTED    lv_color_hex(0x803000)
#define UI_COLOR_NIGHT_DIVIDER  lv_color_hex(0x220800)

/* ------------------------------------------------------------------------- */
/* Palette active (jour / nuit)                                              */
/* ------------------------------------------------------------------------- */

/* Les écrans prennent leurs couleurs dans la palette active : au basculement jour/nuit,
 * ui.c reconstruit les écrans (voir ui_theme_toggle_night()). En nuit, toutes les couleurs
 * d'accent passent en orange profond, comme sur la maquette. */
typedef struct {
    lv_color_t bg, panel, divider, text, muted, amber, cyan, ok, warn, crit, on_amber;
} ui_palette_t;

const ui_palette_t *ui_pal(void);       /* défini dans theme.c */

#define UI_C_BG        (ui_pal()->bg)
#define UI_C_PANEL     (ui_pal()->panel)
#define UI_C_DIVIDER   (ui_pal()->divider)
#define UI_C_TEXT      (ui_pal()->text)
#define UI_C_MUTED     (ui_pal()->muted)
#define UI_C_AMBER     (ui_pal()->amber)
#define UI_C_CYAN      (ui_pal()->cyan)
#define UI_C_OK        (ui_pal()->ok)
#define UI_C_WARN      (ui_pal()->warn)
#define UI_C_CRIT      (ui_pal()->crit)
#define UI_C_ON_AMBER  (ui_pal()->on_amber)    /* texte sur fond amber (sélection menu) */

/* ------------------------------------------------------------------------- */
/* Typographie — tout l'écran en Share Tech Mono                             */
/* ------------------------------------------------------------------------- */

/* Tailles reprises de la maquette (unités cqw × 4,8 px pour un écran de 480 px). */
#define UI_FONT_TRIP_A        ui_font_mono(72)   /* Trip A, 15 cqw */
#define UI_FONT_TRIP_B        ui_font_mono(44)   /* Trip B, 9 cqw */
#define UI_FONT_CELL          ui_font_mono(38)   /* V / CAP, 8 cqw */
#define UI_FONT_SPEEDO        ui_font_mono(190)  /* vitesse mode balade, ~40 cqw */
#define UI_FONT_LOGO          ui_font_mono(134)  /* « 403 » écran de démarrage, 28 cqw */
#define UI_FONT_VALUE_MED     ui_font_mono(34)   /* lectures calibration, 7 cqw */
#define UI_FONT_VALUE_RES     ui_font_mono(29)   /* facteur calibration, 6 cqw */
#define UI_FONT_VALUE_SMALL   ui_font_mono(25)   /* valeurs bas d'écran balade, 5,2 cqw */
#define UI_FONT_TITLE         ui_font_mono(16)   /* titres menu / calib, items menu */
#define UI_FONT_TIME          ui_font_mono(15)   /* heure barre d'état, labels A / B */
#define UI_FONT_UNIT          ui_font_mono(14)   /* unité trip, nom de marque boot */
#define UI_FONT_BTM_LABEL     ui_font_mono(13)   /* labels bas d'écran balade */
#define UI_FONT_STATUS        ui_font_mono(13)   /* barre d'état, labels V / CAP */
#define UI_FONT_SMALL         ui_font_mono(12)   /* bas d'écran raid, unités, version */

/* ------------------------------------------------------------------------- */
/* Dimensions clés du layout 480×320                                         */
/* ------------------------------------------------------------------------- */

#define UI_SCREEN_W             480
#define UI_SCREEN_H             320
#define UI_STATUS_BAR_H         34
#define UI_PAD_X                14     /* marge latérale du contenu, 3 cqw */
#define UI_PAD_Y                10     /* marge haute / basse du contenu, 2 cqw */
#define UI_GAP                  7      /* espacement entre rangées, 1,4 cqw */

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_TOKENS_H */
