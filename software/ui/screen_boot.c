/*
 * Raidmaster 403 — écran de démarrage.
 *
 * Maquette « Écran de démarrage » : « 403 » amber géant, silhouette de 403 Familiale au trait
 * cyan, marque RAIDMASTER, ligne de version, et cinq points de progression qui s'allument
 * pendant le boot (~1,2 s, voir le timer dans ui.c).
 *
 * Licence : GPL-3.0.
 */

#include "screen_boot.h"
#include "ui_tokens.h"
#include "ui_widgets.h"

#define SIL_X      96      /* silhouette : 60 % de la largeur, centrée */
#define SIL_Y      164
#define SIL_SCALE  1.2f    /* viewBox 240×60 → 288×72 px */
#define DOT_COUNT  5

/* Silhouette (repris du SVG de la maquette, viewBox 240×60). La courbe Q du pavillon
 * (58,22)→(108,18), contrôle (82,16), est échantillonnée. */
static const float s_body_vb[][2] = {
    {4, 46}, {18, 42}, {32, 30}, {58, 22},
    {70.6f, 19.2f}, {83.4f, 17.6f}, {96, 17.4f}, {108, 18},
    {156, 22}, {188, 30}, {218, 36}, {228, 42}, {234, 46}, {220, 48},
};
#define BODY_N ((int)(sizeof(s_body_vb) / sizeof(s_body_vb[0])))

static lv_point_precise_t s_body[BODY_N];
static lv_point_precise_t s_floor[2];
static lv_obj_t          *s_dots[DOT_COUNT];
static lv_timer_t        *s_dot_timer = NULL;
static int                s_dots_on   = 0;

static lv_obj_t *sil_line(lv_obj_t *parent, lv_point_precise_t *pts, uint32_t n)
{
    lv_obj_t *l = lv_line_create(parent);
    lv_line_set_points(l, pts, n);
    lv_obj_set_style_line_color(l, UI_C_CYAN, 0);
    lv_obj_set_style_line_width(l, 2, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_set_pos(l, SIL_X, SIL_Y);
    return l;
}

static void wheel(lv_obj_t *parent, float cx_vb)
{
    const int32_t cx = SIL_X + (int32_t)(cx_vb * SIL_SCALE);
    const int32_t cy = SIL_Y + (int32_t)(48 * SIL_SCALE);
    const int32_t radii[2] = {8, 4};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *c = ui_box(parent);
        lv_obj_set_size(c, radii[i] * 2 + 1, radii[i] * 2 + 1);
        lv_obj_set_pos(c, cx - radii[i], cy - radii[i]);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(c, UI_C_CYAN, 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_bg_color(c, UI_C_PANEL, 0);
        lv_obj_set_style_bg_opa(c, i == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void dot_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_dots_on < DOT_COUNT) {
        lv_obj_set_style_bg_color(s_dots[s_dots_on], UI_C_AMBER, 0);
        s_dots_on++;
    }
    if (s_dots_on >= DOT_COUNT) {
        lv_timer_delete(s_dot_timer);
        s_dot_timer = NULL;
    }
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    if (s_dot_timer != NULL) {
        lv_timer_delete(s_dot_timer);
        s_dot_timer = NULL;
    }
}

lv_obj_t *screen_boot_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, NULL);

    lv_obj_t *logo = ui_label(scr, UI_FONT_LOGO, UI_C_AMBER, "403");
    lv_obj_set_style_text_letter_space(logo, -5, 0);
    ui_place_baseline(logo, UI_SCREEN_W / 2, 146, LV_TEXT_ALIGN_CENTER, 400);

    for (int i = 0; i < BODY_N; i++) {
        s_body[i].x = s_body_vb[i][0] * SIL_SCALE;
        s_body[i].y = s_body_vb[i][1] * SIL_SCALE;
    }
    s_floor[0] = (lv_point_precise_t){34 * SIL_SCALE, 48 * SIL_SCALE};
    s_floor[1] = (lv_point_precise_t){228 * SIL_SCALE, 48 * SIL_SCALE};
    sil_line(scr, s_body, BODY_N);
    sil_line(scr, s_floor, 2);
    wheel(scr, 52);
    wheel(scr, 198);

    lv_obj_t *brand = ui_label(scr, UI_FONT_UNIT, UI_C_CYAN, "RAIDMASTER");
    lv_obj_set_style_text_letter_space(brand, 4, 0);
    ui_place_baseline(brand, UI_SCREEN_W / 2, 266, LV_TEXT_ALIGN_CENTER, 300);

    lv_obj_t *ver = ui_label(scr, UI_FONT_SMALL, UI_C_MUTED, "v0.1.0 · SIM · LVGL 9.2");
    lv_obj_set_style_text_letter_space(ver, 3, 0);
    ui_place_baseline(ver, UI_SCREEN_W / 2, 290, LV_TEXT_ALIGN_CENTER, 440);

    /* Points de progression. */
    const int32_t d = 8, gap = 5;
    const int32_t x0 = (UI_SCREEN_W - (DOT_COUNT * d + (DOT_COUNT - 1) * gap)) / 2;
    for (int i = 0; i < DOT_COUNT; i++) {
        s_dots[i] = ui_box(scr);
        lv_obj_set_size(s_dots[i], d, d);
        lv_obj_set_pos(s_dots[i], x0 + i * (d + gap), UI_SCREEN_H - 14 - d);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dots[i], UI_C_MUTED, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
    }
    s_dots_on = 0;
    if (s_dot_timer == NULL) {
        s_dot_timer = lv_timer_create(dot_timer_cb, 220, NULL);
    }

    return scr;
}
