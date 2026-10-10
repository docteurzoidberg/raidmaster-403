/*
 * Raidmaster 403 — écran calibration auto distance (stub statique MVP).
 *
 * Maquette « Calibration auto distance » : titre amber, consigne, deux panneaux de lecture
 * (distance brute / distance réelle, cyan), cadre amber avec le facteur calculé. Valeurs
 * d'exemple ; le câblage sur le core (RAZ, calcul ±%, persistance NVS) est TODO cross-agent
 * firmware.
 *
 * Licence : GPL-3.0.
 */

#include "screen_calib.h"
#include "ui_tokens.h"
#include "ui_widgets.h"

#define X_L   UI_PAD_X
#define X_R   (UI_SCREEN_W - UI_PAD_X)
#define GAP   10

static lv_obj_t      *s_scr = NULL;
static ui_statusbar_t s_sb;

static lv_obj_t *reading_panel(lv_obj_t *scr, int32_t x, int32_t w, const char *k, const char *v)
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
    lv_obj_t *vl = ui_label(p, UI_FONT_VALUE_MED, UI_C_CYAN, v);
    ui_place_baseline(vl, 9, 58, LV_TEXT_ALIGN_LEFT, 0);
    return p;
}

lv_obj_t *screen_calib_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    s_scr = scr;
    ui_statusbar_create(scr, &s_sb, false, false, "CALIB");

    lv_obj_t *title = ui_label(scr, UI_FONT_TITLE, UI_C_AMBER, "CALIBRATION AUTO · DISTANCE");
    lv_obj_set_style_text_letter_space(title, 1, 0);
    ui_place_baseline(title, X_L, 64, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_t *step = ui_label(scr, ui_font_mono(13), UI_C_MUTED,
        "Rouler sur une distance connue (balise rallye, borne routière). "
        "Appuyer sur A au début, puis sur A à l'arrivée.");
    lv_obj_set_width(step, X_R - X_L);
    lv_obj_set_style_text_line_space(step, 4, 0);
    lv_obj_set_pos(step, X_L, 78);

    int32_t w = (X_R - X_L - GAP) / 2;
    reading_panel(scr, X_L, w, "DISTANCE BRUTE", "1,034");
    reading_panel(scr, X_L + w + GAP, w, "DISTANCE RÉELLE", "1,000 km");

    /* Résultat encadré amber. */
    lv_obj_t *res = ui_box(scr);
    lv_obj_set_pos(res, X_L, 234);
    lv_obj_set_size(res, X_R - X_L, 54);
    lv_obj_set_style_border_color(res, UI_C_AMBER, 0);
    lv_obj_set_style_border_width(res, 1, 0);
    lv_obj_set_style_radius(res, 5, 0);

    lv_obj_t *rk = ui_label(res, ui_font_mono(13), UI_C_AMBER, "FACTEUR CALCULÉ");
    ui_place_baseline(rk, 10, 34, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *rv = ui_label(res, UI_FONT_VALUE_RES, UI_C_AMBER, "−3,3 %");
    ui_place_baseline(rv, X_R - X_L - 12, 36, LV_TEXT_ALIGN_RIGHT, 160);

    return scr;
}

void screen_calib_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;
    ui_statusbar_update(&s_sb, s);
}
