/*
 * Raidmaster 403 — écran mode balade.
 *
 * Compteur de vitesse central énorme (amber), ODO discret en haut à gauche, footer 3 cellules.
 *
 * Licence : GPL-3.0.
 */

#include "screen_balade.h"
#include "ui_tokens.h"

static lv_obj_t *s_scr        = NULL;
static lv_obj_t *s_lbl_speed;
static lv_obj_t *s_lbl_odo;
static lv_obj_t *s_lbl_cap;
static lv_obj_t *s_lbl_trip;
static lv_obj_t *s_lbl_temp;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_time;

static lv_obj_t *mk(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, t);
    lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_text_color(o, c, 0);
    return o;
}

lv_obj_t *screen_balade_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_scr, UI_SCREEN_W, UI_SCREEN_H);

    /* Barre d'état minimaliste : horloge + mode. */
    lv_obj_t *bar = lv_obj_create(s_scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_LCD_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_time = mk(bar, UI_FONT_STATUS, UI_COLOR_LCD_TEXT, "--:--");
    lv_obj_align(s_lbl_time, LV_ALIGN_LEFT_MID, 8, 0);

    s_lbl_mode = mk(bar, UI_FONT_LABEL, UI_COLOR_LCD_CYAN, "BALADE");
    lv_obj_align(s_lbl_mode, LV_ALIGN_RIGHT_MID, -8, 0);

    /* ODO discret haut-gauche sous la barre. */
    lv_obj_t *odo_lab = mk(s_scr, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "ODO");
    lv_obj_set_pos(odo_lab, 16, 36);
    s_lbl_odo = mk(s_scr, UI_FONT_LABEL, UI_COLOR_LCD_TEXT, "0.0 km");
    lv_obj_set_pos(s_lbl_odo, 48, 34);

    /* Compteur de vitesse central énorme. */
    s_lbl_speed = mk(s_scr, UI_FONT_DIGIT_LARGE, UI_COLOR_LCD_AMBER, "0");
    lv_obj_align(s_lbl_speed, LV_ALIGN_CENTER, -20, -10);

    lv_obj_t *unit = mk(s_scr, UI_FONT_FOOTER, UI_COLOR_LCD_MUTED, "km/h");
    lv_obj_align_to(unit, s_lbl_speed, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, -8);

    /* Footer 3 cellules : CAP / TRIP / T°. */
    int row_y = UI_SCREEN_H - UI_FOOTER_H + 10;
    lv_obj_t *sep = lv_obj_create(s_scr);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, UI_SCREEN_W, 1);
    lv_obj_set_pos(sep, 0, UI_SCREEN_H - UI_FOOTER_H);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LCD_DIVIDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    lv_obj_t *cap_l = mk(s_scr, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "CAP");
    lv_obj_set_pos(cap_l, 24, row_y);
    s_lbl_cap = mk(s_scr, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "---°");
    lv_obj_set_pos(s_lbl_cap, 60, row_y - 3);

    lv_obj_t *tr_l = mk(s_scr, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "TRIP");
    lv_obj_set_pos(tr_l, UI_SCREEN_W / 2 - 60, row_y);
    s_lbl_trip = mk(s_scr, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "0.0 km");
    lv_obj_set_pos(s_lbl_trip, UI_SCREEN_W / 2 - 20, row_y - 3);

    lv_obj_t *t_l = mk(s_scr, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "T°");
    lv_obj_set_pos(t_l, UI_SCREEN_W - 110, row_y);
    s_lbl_temp = mk(s_scr, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "--°C");
    lv_obj_set_pos(s_lbl_temp, UI_SCREEN_W - 80, row_y - 3);

    return s_scr;
}

void screen_balade_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;
    float sp = s->speed_kmh;
    if (sp < 0) sp = 0;
    lv_label_set_text_fmt(s_lbl_speed, "%d", (int)(sp + 0.5f));
    lv_label_set_text_fmt(s_lbl_odo, "%.1f km", s->odo_km);

    int cap = s->heading_deg % 360;
    if (cap < 0) cap += 360;
    lv_label_set_text_fmt(s_lbl_cap, "%03d°", cap);
    lv_label_set_text_fmt(s_lbl_trip, "%.2f km", s->trip_a_km);
    lv_label_set_text_fmt(s_lbl_temp, "%d°C", (int)(s->t_cabin_c + 0.5f));
    lv_label_set_text(s_lbl_time, s->time_hhmm[0] ? s->time_hhmm : "--:--");
}
