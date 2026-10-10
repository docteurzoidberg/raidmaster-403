/*
 * Raidmaster 403 — écran calibration auto distance.
 *
 * Maquette « Calibration auto distance » : titre amber, consigne, deux panneaux de lecture
 * (distance brute / distance réelle, cyan), cadre amber avec le facteur calculé.
 *
 * Les textes sont fournis par ui_app.c (procédure pilotée par le core : A au départ, A à
 * l'arrivée, saisie de la distance réelle, validation). Sans lui (démo, GIF, captures),
 * valeurs d'exemple de la maquette.
 *
 * Licence : GPL-3.0.
 */

#include "screen_calib.h"
#include "ui_tokens.h"
#include "ui_widgets.h"
#include <stdio.h>
#include <string.h>

#define X_L   UI_PAD_X
#define X_R   (UI_SCREEN_W - UI_PAD_X)
#define GAP   10

static lv_obj_t      *s_scr = NULL;
static ui_statusbar_t s_sb;
static lv_obj_t      *s_hint, *s_raw, *s_real, *s_factor, *s_panel_raw, *s_panel_real;

/* Textes courants (valeurs de la maquette par défaut). */
static char s_t_hint[200] = "Rouler sur une distance connue (balise rallye, borne routière). "
                            "Appuyer sur A au début, puis sur A à l'arrivée.";
static char s_t_raw[24] = "1,034";
static char s_t_real[24] = "1,000 km";
static char s_t_factor[24] = "−3,3 %";
static calib_focus_t s_focus = CALIB_FOCUS_NONE;

static lv_obj_t *reading_panel(lv_obj_t *scr, int32_t x, int32_t w, const char *k, const char *v,
                               lv_obj_t **val)
{
    lv_obj_t *p = ui_box(scr);
    lv_obj_set_pos(p, x, 150);
    lv_obj_set_size(p, w, 72);
    lv_obj_set_style_bg_color(p, UI_C_PANEL, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, UI_C_DIVIDER, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_radius(p, 5, 0);

    lv_obj_t *kl = ui_label(p, UI_FONT_SMALL, UI_C_MUTED, k);
    lv_obj_set_style_text_letter_space(kl, 1, 0);
    ui_place_baseline(kl, 9, 20, LV_TEXT_ALIGN_LEFT, 0);
    *val = ui_label(p, UI_FONT_VALUE_MED, UI_C_CYAN, v);
    ui_place_baseline(*val, 9, 58, LV_TEXT_ALIGN_LEFT, 0);
    return p;
}

static void apply_focus(void)
{
    lv_obj_set_style_border_color(s_panel_raw, s_focus == CALIB_FOCUS_RAW ? UI_C_CYAN : UI_C_DIVIDER, 0);
    lv_obj_set_style_border_color(s_panel_real, s_focus == CALIB_FOCUS_REAL ? UI_C_AMBER : UI_C_DIVIDER, 0);
}

static void on_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) == s_scr) s_scr = NULL;
}

lv_obj_t *screen_calib_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    s_scr = scr;
    lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, NULL);
    ui_statusbar_create(scr, &s_sb, false, false, "CALIB");

    lv_obj_t *title = ui_label(scr, UI_FONT_TITLE, UI_C_AMBER, "CALIBRATION AUTO · DISTANCE");
    lv_obj_set_style_text_letter_space(title, 1, 0);
    ui_place_baseline(title, X_L, 64, LV_TEXT_ALIGN_LEFT, 0);

    s_hint = ui_label(scr, ui_font_mono(13), UI_C_MUTED, s_t_hint);
    lv_obj_set_width(s_hint, X_R - X_L);
    lv_obj_set_style_text_line_space(s_hint, 4, 0);
    lv_obj_set_pos(s_hint, X_L, 78);

    int32_t w = (X_R - X_L - GAP) / 2;
    s_panel_raw = reading_panel(scr, X_L, w, "DISTANCE BRUTE", s_t_raw, &s_raw);
    s_panel_real = reading_panel(scr, X_L + w + GAP, w, "DISTANCE RÉELLE", s_t_real, &s_real);

    /* Résultat encadré amber. */
    lv_obj_t *res = ui_box(scr);
    lv_obj_set_pos(res, X_L, 234);
    lv_obj_set_size(res, X_R - X_L, 54);
    lv_obj_set_style_border_color(res, UI_C_AMBER, 0);
    lv_obj_set_style_border_width(res, 1, 0);
    lv_obj_set_style_radius(res, 5, 0);

    lv_obj_t *rk = ui_label(res, ui_font_mono(13), UI_C_AMBER, "FACTEUR CALCULÉ");
    ui_place_baseline(rk, 10, 34, LV_TEXT_ALIGN_LEFT, 0);
    s_factor = ui_label(res, UI_FONT_VALUE_RES, UI_C_AMBER, s_t_factor);
    ui_place_baseline(s_factor, X_R - X_L - 12, 36, LV_TEXT_ALIGN_RIGHT, 160);

    apply_focus();
    return scr;
}

static void set_text(char *dst, size_t cap, lv_obj_t *lbl, const char *src)
{
    if (src == NULL || strcmp(dst, src) == 0) return;
    snprintf(dst, cap, "%s", src);
    if (s_scr != NULL) lv_label_set_text(lbl, dst);
}

void screen_calib_set(const char *hint, const char *raw, const char *real, const char *factor,
                      calib_focus_t focus)
{
    set_text(s_t_hint, sizeof(s_t_hint), s_hint, hint);
    set_text(s_t_raw, sizeof(s_t_raw), s_raw, raw);
    set_text(s_t_real, sizeof(s_t_real), s_real, real);
    set_text(s_t_factor, sizeof(s_t_factor), s_factor, factor);
    if (focus != s_focus) {
        s_focus = focus;
        if (s_scr != NULL) apply_focus();
    }
}

void screen_calib_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;
    ui_statusbar_update(&s_sb, s);
}
