/*
 * Raidmaster 403 — écran mode raid.
 *
 * Layout 480×320 landscape, selon prompts/ui.md § 3.1 :
 *
 *   ┌──────────────────────────────────────────────────────────┐  28 px status
 *   │ 14:03  3D  10Hz  ■■■□  25°C                        RAID  │
 *   ├──────────────────────────────────────────────────────────┤
 *   │                                                          │
 *   │          12.345  km       (Trip A — amber, 48 px)        │
 *   │                                                          │
 *   │             3.47  km      (Trip B — cyan, 36 px)         │
 *   │                                                          │
 *   │       V  85  km/h           CAP  127°  SE                │  row-mid
 *   │                                                          │
 *   ├──────────────────────────────────────────────────────────┤  40 px footer
 *   │  ODO  12354.1 km   MOY  42 km/h   CHRONO  2:14:08        │
 *   └──────────────────────────────────────────────────────────┘
 *
 * Les widgets sont statiques (une seule instance d'écran) : screen_raid_update() patch les
 * labels existants.
 *
 * Licence : GPL-3.0.
 */

#include "screen_raid.h"
#include "ui_tokens.h"
#include <stdio.h>
#include <math.h>

static lv_obj_t *s_scr      = NULL;
static lv_obj_t *s_lbl_time;
static lv_obj_t *s_lbl_fix;
static lv_obj_t *s_lbl_batt;
static lv_obj_t *s_lbl_temp;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_trip_a;
static lv_obj_t *s_lbl_trip_a_unit;
static lv_obj_t *s_lbl_trip_b;
static lv_obj_t *s_lbl_trip_b_unit;
static lv_obj_t *s_lbl_speed;
static lv_obj_t *s_lbl_speed_unit;
static lv_obj_t *s_lbl_cap;
static lv_obj_t *s_lbl_odo;
static lv_obj_t *s_lbl_moy;
static lv_obj_t *s_lbl_chrono;

static const char *cardinal_for_heading(int deg)
{
    static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    int idx = ((deg + 22) / 45) & 7;
    return dirs[idx];
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                            const char *init_text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, init_text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static lv_obj_t *build_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_LCD_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_time = make_label(bar, UI_FONT_STATUS, UI_COLOR_LCD_TEXT, "--:--");
    lv_obj_align(s_lbl_time, LV_ALIGN_LEFT_MID, 8, 0);

    s_lbl_fix = make_label(bar, UI_FONT_LABEL, UI_COLOR_LCD_WARN, "no fix");
    lv_obj_align(s_lbl_fix, LV_ALIGN_LEFT_MID, 80, 0);

    s_lbl_batt = make_label(bar, UI_FONT_LABEL, UI_COLOR_LCD_OK, "---%");
    lv_obj_align(s_lbl_batt, LV_ALIGN_RIGHT_MID, -120, 0);

    s_lbl_temp = make_label(bar, UI_FONT_LABEL, UI_COLOR_LCD_MUTED, "--°C");
    lv_obj_align(s_lbl_temp, LV_ALIGN_RIGHT_MID, -70, 0);

    s_lbl_mode = make_label(bar, UI_FONT_LABEL, UI_COLOR_LCD_AMBER, "RAID");
    lv_obj_align(s_lbl_mode, LV_ALIGN_RIGHT_MID, -8, 0);

    /* Ligne séparatrice fine en bas de la barre. */
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, UI_SCREEN_W, 1);
    lv_obj_set_pos(sep, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LCD_DIVIDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    return bar;
}

static void build_trip_block(lv_obj_t *parent)
{
    /* Trip A : "12.345 km" aligné à droite, label "A" en préfixe. */
    lv_obj_t *label_a = make_label(parent, UI_FONT_LABEL, UI_COLOR_LCD_MUTED, "A");
    lv_obj_set_pos(label_a, 16, 42);

    s_lbl_trip_a = make_label(parent, UI_FONT_DIGIT_LARGE, UI_COLOR_LCD_AMBER, "0.000");
    lv_obj_align(s_lbl_trip_a, LV_ALIGN_TOP_RIGHT, -70, 36);

    s_lbl_trip_a_unit = make_label(parent, UI_FONT_FOOTER, UI_COLOR_LCD_AMBER, "km");
    lv_obj_align(s_lbl_trip_a_unit, LV_ALIGN_TOP_RIGHT, -20, 54);

    /* Trip B : "3.47 km". */
    lv_obj_t *label_b = make_label(parent, UI_FONT_LABEL, UI_COLOR_LCD_MUTED, "B");
    lv_obj_set_pos(label_b, 16, 112);

    s_lbl_trip_b = make_label(parent, UI_FONT_DIGIT_MED, UI_COLOR_LCD_CYAN, "0.00");
    lv_obj_align(s_lbl_trip_b, LV_ALIGN_TOP_RIGHT, -70, 108);

    s_lbl_trip_b_unit = make_label(parent, UI_FONT_LABEL, UI_COLOR_LCD_CYAN, "km");
    lv_obj_align(s_lbl_trip_b_unit, LV_ALIGN_TOP_RIGHT, -24, 118);
}

static void build_row_mid(lv_obj_t *parent)
{
    /* V (vitesse) à gauche, CAP à droite. Séparateur vertical. */
    lv_obj_t *v_label = make_label(parent, UI_FONT_LABEL, UI_COLOR_LCD_MUTED, "V");
    lv_obj_set_pos(v_label, 24, 170);

    s_lbl_speed = make_label(parent, UI_FONT_DIGIT_SMALL, UI_COLOR_LCD_TEXT, "0");
    lv_obj_set_pos(s_lbl_speed, 52, 162);

    s_lbl_speed_unit = make_label(parent, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "km/h");
    lv_obj_set_pos(s_lbl_speed_unit, 150, 192);

    /* Séparateur vertical. */
    lv_obj_t *vdiv = lv_obj_create(parent);
    lv_obj_remove_style_all(vdiv);
    lv_obj_set_size(vdiv, 1, 56);
    lv_obj_set_pos(vdiv, UI_SCREEN_W / 2, 162);
    lv_obj_set_style_bg_color(vdiv, UI_COLOR_LCD_DIVIDER, 0);
    lv_obj_set_style_bg_opa(vdiv, LV_OPA_COVER, 0);

    lv_obj_t *cap_label = make_label(parent, UI_FONT_LABEL, UI_COLOR_LCD_MUTED, "CAP");
    lv_obj_set_pos(cap_label, UI_SCREEN_W / 2 + 20, 170);

    s_lbl_cap = make_label(parent, UI_FONT_DIGIT_SMALL, UI_COLOR_LCD_TEXT, "---°  N");
    lv_obj_set_pos(s_lbl_cap, UI_SCREEN_W / 2 + 70, 162);
}

static void build_footer(lv_obj_t *parent)
{
    /* Ligne séparatrice. */
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, UI_SCREEN_W, 1);
    lv_obj_set_pos(sep, 0, UI_SCREEN_H - UI_FOOTER_H);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LCD_DIVIDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    int row_y = UI_SCREEN_H - UI_FOOTER_H + 10;

    lv_obj_t *odo_l = make_label(parent, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "ODO");
    lv_obj_set_pos(odo_l, 16, row_y);
    s_lbl_odo = make_label(parent, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "0.0 km");
    lv_obj_set_pos(s_lbl_odo, 56, row_y - 3);

    lv_obj_t *moy_l = make_label(parent, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "MOY");
    lv_obj_set_pos(moy_l, UI_SCREEN_W / 2 - 60, row_y);
    s_lbl_moy = make_label(parent, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "0 km/h");
    lv_obj_set_pos(s_lbl_moy, UI_SCREEN_W / 2 - 20, row_y - 3);

    lv_obj_t *ch_l = make_label(parent, UI_FONT_LABEL_SMALL, UI_COLOR_LCD_MUTED, "CHRONO");
    lv_obj_set_pos(ch_l, UI_SCREEN_W - 160, row_y);
    s_lbl_chrono = make_label(parent, UI_FONT_FOOTER, UI_COLOR_LCD_TEXT, "0:00:00");
    lv_obj_set_pos(s_lbl_chrono, UI_SCREEN_W - 90, row_y - 3);
}

lv_obj_t *screen_raid_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, UI_COLOR_LCD_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_scr, UI_SCREEN_W, UI_SCREEN_H);

    build_status_bar(s_scr);
    build_trip_block(s_scr);
    build_row_mid(s_scr);
    build_footer(s_scr);

    return s_scr;
}

void screen_raid_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) {
        return;
    }

    /* Barre d'état. */
    lv_label_set_text_fmt(s_lbl_time, "%s", s->time_hhmm[0] ? s->time_hhmm : "--:--");

    const char *fix_txt = "no fix";
    lv_color_t fix_col = UI_COLOR_LCD_CRIT;
    if (s->fix_type == 1) { fix_txt = "2D"; fix_col = UI_COLOR_LCD_WARN; }
    else if (s->fix_type == 2) { fix_txt = "3D"; fix_col = UI_COLOR_LCD_OK; }
    lv_label_set_text(s_lbl_fix, fix_txt);
    lv_obj_set_style_text_color(s_lbl_fix, fix_col, 0);

    lv_label_set_text_fmt(s_lbl_batt, "%d%%", (int)s->battery_pct);
    if (s->battery_pct > 30) {
        lv_obj_set_style_text_color(s_lbl_batt, UI_COLOR_LCD_OK, 0);
    } else if (s->battery_pct > 10) {
        lv_obj_set_style_text_color(s_lbl_batt, UI_COLOR_LCD_WARN, 0);
    } else {
        lv_obj_set_style_text_color(s_lbl_batt, UI_COLOR_LCD_CRIT, 0);
    }

    lv_label_set_text_fmt(s_lbl_temp, "%d°C", (int)(s->t_cabin_c + 0.5f));

    /* Trip A / B. */
    lv_label_set_text_fmt(s_lbl_trip_a, "%6.3f", s->trip_a_km);
    lv_label_set_text_fmt(s_lbl_trip_b, "%5.2f", s->trip_b_km);

    /* Vitesse / cap. */
    float sp = s->speed_kmh;
    if (sp < 0) sp = 0;
    lv_label_set_text_fmt(s_lbl_speed, "%3d", (int)(sp + 0.5f));

    int cap = s->heading_deg % 360;
    if (cap < 0) cap += 360;
    lv_label_set_text_fmt(s_lbl_cap, "%03d°  %s", cap, cardinal_for_heading(cap));

    /* Footer. */
    lv_label_set_text_fmt(s_lbl_odo, "%.1f km", s->odo_km);
    lv_label_set_text_fmt(s_lbl_moy, "%d km/h", (int)(s->avg_kmh + 0.5f));
    lv_label_set_text_fmt(s_lbl_chrono, "%u:%02u:%02u",
                          (unsigned)(s->chrono_s / 3600U),
                          (unsigned)((s->chrono_s / 60U) % 60U),
                          (unsigned)(s->chrono_s % 60U));
}
