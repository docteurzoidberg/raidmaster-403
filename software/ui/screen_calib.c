/*
 * Raidmaster 403 — écran calibration auto distance (stub statique MVP).
 *
 * Reproduit visuellement l'étape 1 de la procédure : « Appuyer sur A au départ ». Les valeurs
 * sont mockées (distance brute, distance réelle attendue, pourcentage calculé). Le vrai
 * câblage sur le core (RAZ distance, calcul du facteur ±%, persistance NVS) viendra une fois
 * trip_engine.c écrit — TODO cross-agent firmware.
 *
 * Licence : GPL-3.0.
 */

#include "screen_calib.h"
#include "ui_tokens.h"

lv_obj_t *screen_calib_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "CALIBRATION  DISTANCE");
    lv_obj_set_style_text_font(title, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_LCD_AMBER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *step = lv_label_create(scr);
    lv_label_set_text(step,
        "Rouler sur une distance connue, puis :\n"
        "1. appuyer sur A au depart\n"
        "2. appuyer de nouveau sur A a l'arrivee\n"
        "3. valider le facteur +/- % propose");
    lv_obj_set_style_text_font(step, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(step, UI_COLOR_LCD_TEXT, 0);
    lv_obj_set_width(step, UI_SCREEN_W - 32);
    lv_obj_align(step, LV_ALIGN_TOP_LEFT, 16, 50);

    /* Bloc de valeurs mockées (statiques pour le MVP). */
    lv_obj_t *lbl_brute = lv_label_create(scr);
    lv_label_set_text(lbl_brute, "distance brute");
    lv_obj_set_style_text_font(lbl_brute, UI_FONT_LABEL_SMALL, 0);
    lv_obj_set_style_text_color(lbl_brute, UI_COLOR_LCD_MUTED, 0);
    lv_obj_set_pos(lbl_brute, 32, 180);

    lv_obj_t *val_brute = lv_label_create(scr);
    lv_label_set_text(val_brute, "4.962 km");
    lv_obj_set_style_text_font(val_brute, UI_FONT_DIGIT_SMALL, 0);
    lv_obj_set_style_text_color(val_brute, UI_COLOR_LCD_AMBER, 0);
    lv_obj_set_pos(val_brute, 32, 198);

    lv_obj_t *lbl_reel = lv_label_create(scr);
    lv_label_set_text(lbl_reel, "distance reelle");
    lv_obj_set_style_text_font(lbl_reel, UI_FONT_LABEL_SMALL, 0);
    lv_obj_set_style_text_color(lbl_reel, UI_COLOR_LCD_MUTED, 0);
    lv_obj_set_pos(lbl_reel, UI_SCREEN_W / 2 - 20, 180);

    lv_obj_t *val_reel = lv_label_create(scr);
    lv_label_set_text(val_reel, "5.000 km");
    lv_obj_set_style_text_font(val_reel, UI_FONT_DIGIT_SMALL, 0);
    lv_obj_set_style_text_color(val_reel, UI_COLOR_LCD_CYAN, 0);
    lv_obj_set_pos(val_reel, UI_SCREEN_W / 2 - 20, 198);

    lv_obj_t *facteur = lv_label_create(scr);
    lv_label_set_text(facteur, "facteur propose  :  +0.8 %");
    lv_obj_set_style_text_font(facteur, UI_FONT_FOOTER, 0);
    lv_obj_set_style_text_color(facteur, UI_COLOR_LCD_OK, 0);
    lv_obj_align(facteur, LV_ALIGN_BOTTOM_MID, 0, -24);

    return scr;
}
