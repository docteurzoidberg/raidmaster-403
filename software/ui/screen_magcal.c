/*
 * Raidmaster 403 — écran calibration de la boussole. Voir screen_magcal.h.
 *
 *   ┌──────────────────────────────────────────────────────────┐  barre d'état « CALIB »
 *   │ CALIBRATION · BOUSSOLE                                   │  titre amber
 *   │ Sur place, faire un tour complet, lentement…             │  consigne
 *   │      ╭─────╮        ┌────────────────────────────────┐   │
 *   │    ╱  78 %  ╲       │ CAP BOUSSOLE            127°   │   │  anneau de 36 secteurs
 *   │   │  (36 ×   │      └────────────────────────────────┘   │  (cyan : parcourus)
 *   │    ╲  10°)  ╱       ┌────────────────────────────────┐   │
 *   │      ╰─────╯        │ TOUR COMPLET · MENU : ENREG.   │   │  état (couleur selon le ton)
 *   └──────────────────────────────────────────────────────────┘
 *
 * Licence : GPL-3.0.
 */

#include "screen_magcal.h"

#include <stdio.h>
#include <string.h>

#include "ui_tokens.h"
#include "ui_widgets.h"

#define X_L       UI_PAD_X
#define X_R       (UI_SCREEN_W - UI_PAD_X)
#define SECTORS   36
#define RING_D    156
#define RING_X    (X_L + 8)
#define RING_Y    136
#define RING_W    14
#define PANEL_X   (RING_X + RING_D + 26)

static lv_obj_t      *s_scr = NULL;
static ui_statusbar_t s_sb;
static lv_obj_t      *s_hint, *s_pct, *s_cap, *s_status, *s_status_box;
static lv_obj_t      *s_seg[SECTORS];

static char          s_t_hint[200] = "Sur place, faire un tour complet, lentement (volant braqué). "
                                     "A : commencer, B : annuler.";
static uint64_t      s_mask = 0x0003FFFFFFFULL;      /* exemple : 30 secteurs sur 36 */
static char          s_t_cap[16] = "127°";
static char          s_t_status[64] = "TOURNER ENCORE";
static magcal_tone_t s_tone = MAGCAL_TONE_INFO;

static lv_color_t tone_color(magcal_tone_t t)
{
    return t == MAGCAL_TONE_OK ? UI_C_OK : (t == MAGCAL_TONE_ALERT ? UI_C_CRIT : UI_C_AMBER);
}

static unsigned popcount(uint64_t v)
{
    unsigned n = 0;
    for (; v != 0; v &= v - 1) n++;
    return n;
}

static void apply(void)
{
    for (int i = 0; i < SECTORS; i++)
        lv_obj_set_style_arc_color(s_seg[i], (s_mask >> i) & 1U ? UI_C_CYAN : UI_C_DIVIDER, LV_PART_INDICATOR);
    lv_label_set_text_fmt(s_pct, "%u%%", popcount(s_mask) * 100U / SECTORS);
    lv_label_set_text(s_hint, s_t_hint);
    lv_label_set_text(s_cap, s_t_cap);
    lv_label_set_text(s_status, s_t_status);
    lv_obj_set_style_text_color(s_status, tone_color(s_tone), 0);
    lv_obj_set_style_border_color(s_status_box, tone_color(s_tone), 0);
}

static void on_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) == s_scr) s_scr = NULL;
}

static lv_obj_t *panel(lv_obj_t *scr, int32_t y, int32_t h)
{
    lv_obj_t *p = ui_box(scr);
    lv_obj_set_pos(p, PANEL_X, y);
    lv_obj_set_size(p, X_R - PANEL_X, h);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_radius(p, 5, 0);
    return p;
}

lv_obj_t *screen_magcal_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    s_scr = scr;
    lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, NULL);
    ui_statusbar_create(scr, &s_sb, false, false, "CALIB");

    lv_obj_t *title = ui_label(scr, UI_FONT_TITLE, UI_C_AMBER, "CALIBRATION · BOUSSOLE");
    lv_obj_set_style_text_letter_space(title, 1, 0);
    ui_place_baseline(title, X_L, 64, LV_TEXT_ALIGN_LEFT, 0);

    s_hint = ui_label(scr, ui_font_mono(13), UI_C_MUTED, "");
    lv_obj_set_width(s_hint, X_R - X_L);
    lv_obj_set_style_text_line_space(s_hint, 4, 0);
    lv_obj_set_pos(s_hint, X_L, 78);

    /* Anneau : un arc par secteur de 10° (N en haut, sens horaire ; 0° LVGL = 3 h). */
    for (int i = 0; i < SECTORS; i++) {
        lv_obj_t *a = lv_arc_create(scr);
        lv_obj_remove_style_all(a);
        lv_obj_set_size(a, RING_D, RING_D);
        lv_obj_set_pos(a, RING_X, RING_Y);
        lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_bg_angles(a, 0, 0);
        lv_arc_set_angles(a, (lv_value_precise_t)(i * 10 - 90 + 1), (lv_value_precise_t)(i * 10 - 90 + 9));
        lv_obj_set_style_arc_width(a, RING_W, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
        s_seg[i] = a;
    }
    lv_obj_t *n = ui_label(scr, UI_FONT_SMALL, UI_C_MUTED, "N");
    ui_place_baseline(n, RING_X + RING_D / 2, RING_Y + RING_W + 16, LV_TEXT_ALIGN_CENTER, 20);
    s_pct = ui_label(scr, UI_FONT_VALUE_RES, UI_C_CYAN, "0%");
    ui_place_baseline(s_pct, RING_X + RING_D / 2, RING_Y + RING_D / 2 + 11, LV_TEXT_ALIGN_CENTER, 120);

    /* Cap de la boussole. */
    lv_obj_t *pc = panel(scr, RING_Y + 4, 64);
    lv_obj_set_style_bg_color(pc, UI_C_PANEL, 0);
    lv_obj_set_style_bg_opa(pc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pc, UI_C_DIVIDER, 0);
    lv_obj_t *kl = ui_label(pc, UI_FONT_SMALL, UI_C_MUTED, "CAP BOUSSOLE");
    lv_obj_set_style_text_letter_space(kl, 1, 0);
    ui_place_baseline(kl, 9, 37, LV_TEXT_ALIGN_LEFT, 0);
    s_cap = ui_label(pc, UI_FONT_CELL, UI_C_TEXT, "");
    ui_place_baseline(s_cap, X_R - PANEL_X - 10, 46, LV_TEXT_ALIGN_RIGHT, 130);

    /* État de la calibration. */
    s_status_box = panel(scr, RING_Y + 84, 64);
    s_status = ui_label(s_status_box, UI_FONT_TIME, UI_C_AMBER, "");
    lv_obj_set_width(s_status, X_R - PANEL_X - 20);
    lv_obj_set_style_text_line_space(s_status, 3, 0);
    lv_obj_align(s_status, LV_ALIGN_LEFT_MID, 10, 0);

    apply();
    return scr;
}

void screen_magcal_set(const char *hint, uint64_t mask, const char *cap, const char *status,
                       magcal_tone_t tone)
{
    bool changed = mask != s_mask || tone != s_tone;
    if (hint != NULL && strcmp(hint, s_t_hint) != 0) { snprintf(s_t_hint, sizeof(s_t_hint), "%s", hint); changed = true; }
    if (cap != NULL && strcmp(cap, s_t_cap) != 0) { snprintf(s_t_cap, sizeof(s_t_cap), "%s", cap); changed = true; }
    if (status != NULL && strcmp(status, s_t_status) != 0) {
        snprintf(s_t_status, sizeof(s_t_status), "%s", status);
        changed = true;
    }
    s_mask = mask;
    s_tone = tone;
    if (changed && s_scr != NULL) apply();
}

void screen_magcal_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;
    ui_statusbar_update(&s_sb, s);
}
