/*
 * Raidmaster 403 — écran mode balade.
 *
 * Maquette « Mode balade » : grand compteur de vitesse central (amber), ODO discret en haut à
 * gauche, unité en bas à droite de la zone compteur, puis trois cellules CAP / TRIP / T°
 * (libellé au-dessus, valeur cyan dessous).
 *
 * Licence : GPL-3.0.
 */

#include "screen_balade.h"
#include "ui_tokens.h"
#include "ui_widgets.h"
#include <math.h>

#define X_L         UI_PAD_X
#define X_R         (UI_SCREEN_W - UI_PAD_X)
#define SPEED_BASE  212
#define BTM_DIV     244
#define BTM_LBL     266
#define BTM_VAL     298
#define BTM_COL_W   ((X_R - X_L) / 3)

static lv_obj_t      *s_scr = NULL;
static ui_statusbar_t s_sb;
static lv_obj_t      *s_lbl_speed;
static lv_obj_t      *s_lbl_odo;
static lv_obj_t      *s_lbl_cap;
static lv_obj_t      *s_lbl_trip;
static lv_obj_t      *s_lbl_temp;
static lv_obj_t      *s_lbl_trip_name;

static const char *cardinal_for_heading(int deg)
{
    static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    return dirs[((deg + 22) / 45) & 7];
}

static lv_obj_t *btm_cell(int col, const char *name, const char *init, lv_obj_t **name_lbl)
{
    int32_t x = X_L + col * BTM_COL_W;
    lv_obj_t *l = ui_label(s_scr, UI_FONT_BTM_LABEL, UI_C_MUTED, name);
    if (name_lbl != NULL) *name_lbl = l;
    lv_obj_set_style_text_letter_space(l, 1, 0);
    ui_place_baseline(l, x, BTM_LBL, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *v = ui_label(s_scr, UI_FONT_VALUE_SMALL, UI_C_CYAN, init);
    ui_place_baseline(v, x, BTM_VAL, LV_TEXT_ALIGN_LEFT, 0);
    return v;
}

lv_obj_t *screen_balade_create(void)
{
    s_scr = ui_screen_base();
    ui_statusbar_create(s_scr, &s_sb, false, true, "BALADE");

    /* ODO discret en haut à gauche de la zone compteur. */
    lv_obj_t *odo_l = ui_label(s_scr, UI_FONT_STATUS, UI_C_MUTED, "ODO");
    lv_obj_set_style_text_letter_space(odo_l, 1, 0);
    ui_place_baseline(odo_l, X_L + 5, 64, LV_TEXT_ALIGN_LEFT, 0);
    s_lbl_odo = ui_label(s_scr, UI_FONT_STATUS, UI_C_TEXT, "0 km");
    ui_place_baseline(s_lbl_odo, X_L + 40, 64, LV_TEXT_ALIGN_LEFT, 0);

    /* Compteur central. */
    s_lbl_speed = ui_label(s_scr, UI_FONT_SPEEDO, UI_C_AMBER, "0");
    lv_obj_set_style_text_letter_space(s_lbl_speed, -4, 0);
    ui_place_baseline(s_lbl_speed, UI_SCREEN_W / 2, SPEED_BASE, LV_TEXT_ALIGN_CENTER, 440);

    lv_obj_t *unit = ui_label(s_scr, ui_font_mono(17), UI_C_MUTED, "km/h");
    lv_obj_set_style_text_letter_space(unit, 2, 0);
    ui_place_baseline(unit, X_R - 10, BTM_DIV - 14, LV_TEXT_ALIGN_RIGHT, 80);

    /* Trois cellules du bas. */
    ui_hline(s_scr, X_L, BTM_DIV, X_R - X_L);
    s_lbl_cap  = btm_cell(0, "CAP", "---°", NULL);
    s_lbl_trip = btm_cell(1, "TRIP", "0,0 km", &s_lbl_trip_name);
    s_lbl_temp = btm_cell(2, "T°", "--°C", NULL);

    return s_scr;
}

void screen_balade_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;

    ui_statusbar_update(&s_sb, s);

    float sp = s->speed_kmh;
    if (sp < 0) sp = 0;
    lv_label_set_text_fmt(s_lbl_speed, "%d", (int)(sp + 0.5f));

    char buf[24];
    ui_fmt_km(buf, sizeof(buf), s->odo_km, 0);
    lv_label_set_text_fmt(s_lbl_odo, "%s km", buf);

    lv_obj_set_style_text_color(s_lbl_speed, s->speed_alarm ? UI_C_CRIT : UI_C_AMBER, 0);

    int cap = s->heading_deg % 360;
    if (cap < 0) cap += 360;
    if (s->heading_unknown) lv_label_set_text(s_lbl_cap, "---°");
    else lv_label_set_text_fmt(s_lbl_cap, "%03d° %s", cap, cardinal_for_heading(cap));

    ui_fmt_km(buf, sizeof(buf), s->active_trip ? s->trip_b_km : s->trip_a_km, 1);
    lv_label_set_text_fmt(s_lbl_trip, "%s km", buf);
    lv_label_set_text(s_lbl_trip_name, s->active_trip ? "TRIP B" : "TRIP");
    if (isnan(s->t_cabin_c)) lv_label_set_text(s_lbl_temp, "--°C");
    else lv_label_set_text_fmt(s_lbl_temp, "%d°C", (int)lroundf(s->t_cabin_c));
}
