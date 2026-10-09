/*
 * Raidmaster 403 — écran de démarrage.
 *
 * Logo texte « 403 » amber + label « RAIDMASTER » + version. Timer LVGL auto-switch vers
 * l'écran raid après ~1200 ms (voir ui.c pour le timer).
 *
 * Licence : GPL-3.0.
 */

#include "screen_boot.h"
#include "ui_tokens.h"

lv_obj_t *screen_boot_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Logo « 403 » central. */
    lv_obj_t *logo = lv_label_create(scr);
    lv_label_set_text(logo, "403");
    lv_obj_set_style_text_font(logo, UI_FONT_DIGIT_LARGE, 0);
    lv_obj_set_style_text_color(logo, UI_COLOR_LCD_AMBER, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -24);

    /* Sous-titre. */
    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "RAIDMASTER");
    lv_obj_set_style_text_font(subtitle, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(subtitle, UI_COLOR_LCD_TEXT, 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 36);

    /* Version. */
    lv_obj_t *version = lv_label_create(scr);
    lv_label_set_text(version, "v0.1  -  sim");
    lv_obj_set_style_text_font(version, UI_FONT_LABEL_SMALL, 0);
    lv_obj_set_style_text_color(version, UI_COLOR_LCD_MUTED, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 72);

    /* Dots de progression (statiques pour le MVP). */
    lv_obj_t *dots = lv_label_create(scr);
    lv_label_set_text(dots, ". . .");
    lv_obj_set_style_text_font(dots, UI_FONT_FOOTER, 0);
    lv_obj_set_style_text_color(dots, UI_COLOR_LCD_CYAN, 0);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -20);

    return scr;
}
