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
/* Typographie                                                               */
/* ------------------------------------------------------------------------- */

/* Fallback Montserrat embarqué LVGL — à remplacer par Share Tech Mono / Inter
 * en phase 2 via lv_font_conv (TODO documenté dans prompts/ui.md § 9). */
#define UI_FONT_DIGIT_LARGE     (&lv_font_montserrat_48)  /* ~ font_digit_96 */
#define UI_FONT_DIGIT_MED       (&lv_font_montserrat_36)  /* ~ font_digit_54 */
#define UI_FONT_DIGIT_SMALL     (&lv_font_montserrat_28)  /* ~ font_digit_32 */

#define UI_FONT_LABEL           (&lv_font_montserrat_14)
#define UI_FONT_LABEL_SMALL     (&lv_font_montserrat_12)
#define UI_FONT_STATUS          (&lv_font_montserrat_16)
#define UI_FONT_FOOTER          (&lv_font_montserrat_18)
#define UI_FONT_TITLE           (&lv_font_montserrat_24)

/* ------------------------------------------------------------------------- */
/* Dimensions clés du layout 480×320                                         */
/* ------------------------------------------------------------------------- */

#define UI_SCREEN_W             480
#define UI_SCREEN_H             320
#define UI_STATUS_BAR_H         28
#define UI_FOOTER_H             40

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_TOKENS_H */
