/*
 * Raidmaster 403 — menu réglages.
 *
 * `lv_list` LVGL scrollable, items du MVP figés (voir prompts/ui.md § 3.3). La sélection
 * est maintenue en interne (indice courant) et surlignée en amber.
 *
 * Les actions ne sont pas câblées : à ce stade elles bougent la sélection ou impriment un
 * log pour le MVP. Le câblage effectif (NVS, calibration, log GPX…) viendra une fois le
 * core écrit par l'agent firmware.
 *
 * Licence : GPL-3.0.
 */

#include "screen_menu.h"
#include "ui_tokens.h"
#include <stdio.h>

static const char *const s_items[] = {
    "Calibration +/- %",
    "Calibration auto distance",
    "Fuseau horaire",
    "Mode nuit",
    "Mode d'affichage (RAID/BALADE)",
    "Alarme vitesse",
    "Log GPX sur SD",
    "Calibration boussole",
};
#define S_ITEM_COUNT ((int)(sizeof(s_items) / sizeof(s_items[0])))

static lv_obj_t *s_scr      = NULL;
static lv_obj_t *s_list     = NULL;
static lv_obj_t *s_rows[S_ITEM_COUNT];
static int       s_selected = 0;

static void refresh_highlight(void)
{
    for (int i = 0; i < S_ITEM_COUNT; i++) {
        if (s_rows[i] == NULL) continue;
        if (i == s_selected) {
            lv_obj_set_style_bg_color(s_rows[i], UI_COLOR_LCD_AMBER, 0);
            lv_obj_set_style_bg_opa(s_rows[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_rows[i], UI_COLOR_LCD_BG, 0);
        } else {
            lv_obj_set_style_bg_color(s_rows[i], UI_COLOR_LCD_PANEL, 0);
            lv_obj_set_style_bg_opa(s_rows[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(s_rows[i], UI_COLOR_LCD_TEXT, 0);
        }
    }
    if (s_list != NULL && s_rows[s_selected] != NULL) {
        lv_obj_scroll_to_view(s_rows[s_selected], LV_ANIM_OFF);
    }
}

lv_obj_t *screen_menu_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_scr, UI_SCREEN_W, UI_SCREEN_H);

    /* Titre. */
    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, "MENU");
    lv_obj_set_style_text_font(title, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_LCD_AMBER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 10);

    lv_obj_t *hint = lv_label_create(s_scr);
    lv_label_set_text(hint, "+/- : nav   M : valider   long M : sortir");
    lv_obj_set_style_text_font(hint, UI_FONT_LABEL_SMALL, 0);
    lv_obj_set_style_text_color(hint, UI_COLOR_LCD_MUTED, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -16, 18);

    /* Liste scrollable. */
    s_list = lv_list_create(s_scr);
    lv_obj_set_size(s_list, UI_SCREEN_W - 32, UI_SCREEN_H - 56);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(s_list, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 2, 0);

    for (int i = 0; i < S_ITEM_COUNT; i++) {
        lv_obj_t *btn = lv_list_add_button(s_list, NULL, s_items[i]);
        lv_obj_set_style_bg_color(btn, UI_COLOR_LCD_PANEL, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(btn, UI_COLOR_LCD_TEXT, 0);
        lv_obj_set_style_text_font(btn, UI_FONT_LABEL, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        s_rows[i] = btn;
    }

    s_selected = 0;
    refresh_highlight();

    return s_scr;
}

void screen_menu_move(int delta)
{
    if (s_scr == NULL) return;
    s_selected += delta;
    if (s_selected < 0) s_selected = 0;
    if (s_selected >= S_ITEM_COUNT) s_selected = S_ITEM_COUNT - 1;
    refresh_highlight();
}

void screen_menu_activate(void)
{
    if (s_scr == NULL) return;
    /* Stub : impression console seulement — le câblage effectif est TODO cross-agent. */
    printf("[ui] menu activate -> %s\n", s_items[s_selected]);
}
