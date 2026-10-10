/*
 * Raidmaster 403 — écran de démarrage.
 *
 * Version animée (par défaut) : la 403 HD tourne sur une grille cyan lumineuse (rendu Blender,
 * ui/assets/boot_anim/), plein écran ; « 403 » amber dans le ciel noir, puis en bas, sur un
 * dégradé sombre, RAIDMASTER, les points de progression et la version. Le démarrage dure une
 * rotation complète.
 *
 * Version de secours (animation absente) : maquette « Écran de démarrage », « 403 » amber géant,
 * silhouette au trait cyan, RAIDMASTER, version, points de progression (~1,2 s).
 *
 * Licence : GPL-3.0.
 */

#include "screen_boot.h"
#include "ui_tokens.h"
#include "ui_widgets.h"
#include "boot_anim.h"
#include <stdlib.h>
#include <string.h>

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
static uint32_t           s_dot_period_ms = 220;

/* Animation : une image RGB565 réécrite à chaque pas, affichée par un lv_image. */
static lv_timer_t        *s_anim_timer = NULL;
static lv_obj_t          *s_anim_img   = NULL;
static lv_image_dsc_t     s_anim_dsc;
static uint16_t          *s_anim_buf   = NULL;
static uint16_t           s_anim_frame = 0;
static uint32_t           s_duration_ms = 1200;

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
    if (s_anim_timer != NULL) {
        lv_timer_delete(s_anim_timer);
        s_anim_timer = NULL;
    }
    s_anim_img = NULL;
    free(s_anim_buf);
    s_anim_buf = NULL;
    boot_anim_close();
}

static void anim_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_anim_img == NULL || s_anim_buf == NULL) return;
    s_anim_frame = (uint16_t)((s_anim_frame + 1) % boot_anim_frame_count());
    boot_anim_render(s_anim_frame, s_anim_buf);
    lv_obj_invalidate(s_anim_img);
}

static void build_dots(lv_obj_t *scr, int32_t y)
{
    const int32_t d = 8, gap = 5;
    const int32_t x0 = (UI_SCREEN_W - (DOT_COUNT * d + (DOT_COUNT - 1) * gap)) / 2;
    for (int i = 0; i < DOT_COUNT; i++) {
        s_dots[i] = ui_box(scr);
        lv_obj_set_size(s_dots[i], d, d);
        lv_obj_set_pos(s_dots[i], x0 + i * (d + gap), y);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dots[i], UI_C_MUTED, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
    }
    s_dots_on = 0;
    if (s_dot_timer == NULL) {
        s_dot_timer = lv_timer_create(dot_timer_cb, s_dot_period_ms, NULL);
    }
}

/* Version animée. Renvoie false si l'animation n'est pas disponible. */
static bool build_animated(lv_obj_t *scr)
{
    if (!boot_anim_open()) return false;
    const uint16_t w = boot_anim_width(), h = boot_anim_height();
    s_anim_buf = malloc((size_t)w * h * sizeof(uint16_t));
    if (s_anim_buf == NULL) {
        boot_anim_close();
        return false;
    }
    s_anim_frame = 0;
    boot_anim_render(0, s_anim_buf);
    memset(&s_anim_dsc, 0, sizeof(s_anim_dsc));
    s_anim_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    s_anim_dsc.header.cf     = LV_COLOR_FORMAT_RGB565;
    s_anim_dsc.header.w      = w;
    s_anim_dsc.header.h      = h;
    s_anim_dsc.header.stride = (uint32_t)w * 2U;
    s_anim_dsc.data_size     = (uint32_t)w * h * 2U;
    s_anim_dsc.data          = (const uint8_t *)s_anim_buf;

    s_anim_img = lv_image_create(scr);
    lv_image_set_src(s_anim_img, &s_anim_dsc);
    lv_obj_align(s_anim_img, LV_ALIGN_CENTER, 0, 0);

    const uint16_t n = boot_anim_frame_count(), fps = boot_anim_fps();
    s_anim_timer = lv_timer_create(anim_timer_cb, 1000U / fps, NULL);
    s_duration_ms = (uint32_t)n * 1000U / fps;

    /* « 403 » dans le ciel noir. */
    lv_obj_t *logo = ui_label(scr, UI_FONT_TRIP_A, UI_C_AMBER, "403");
    lv_obj_set_style_text_letter_space(logo, -2, 0);
    ui_place_baseline(logo, UI_SCREEN_W / 2, 70, LV_TEXT_ALIGN_CENTER, 300);

    /* Bas d'écran : dégradé sombre sous la marque, les points et la version. */
    lv_obj_t *band = ui_box(scr);
    lv_obj_set_size(band, UI_SCREEN_W, 96);
    lv_obj_set_pos(band, 0, UI_SCREEN_H - 96);
    lv_obj_set_style_bg_color(band, UI_C_BG, 0);
    lv_obj_set_style_bg_grad_color(band, UI_C_BG, 0);
    lv_obj_set_style_bg_grad_dir(band, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_opa(band, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_grad_opa(band, LV_OPA_90, 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);

    lv_obj_t *brand = ui_label(scr, UI_FONT_UNIT, UI_C_CYAN, "RAIDMASTER");
    lv_obj_set_style_text_letter_space(brand, 4, 0);
    ui_place_baseline(brand, UI_SCREEN_W / 2, 282, LV_TEXT_ALIGN_CENTER, 300);

    lv_obj_t *ver = ui_label(scr, UI_FONT_SMALL, UI_C_MUTED, "v0.1.0 · SIM");
    lv_obj_set_style_text_letter_space(ver, 2, 0);
    ui_place_baseline(ver, UI_SCREEN_W - 10, UI_SCREEN_H - 8, LV_TEXT_ALIGN_RIGHT, 200);

    /* Les points s'allument régulièrement sur toute la durée. */
    s_dot_period_ms = s_duration_ms / DOT_COUNT;
    build_dots(scr, 294);
    return true;
}

uint32_t screen_boot_duration_ms(void)
{
    return s_duration_ms;
}

lv_obj_t *screen_boot_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, NULL);

    if (build_animated(scr)) {
        return scr;
    }

    /* Secours : écran statique de la maquette. */
    s_duration_ms   = 1200;
    s_dot_period_ms = 220;

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

    build_dots(scr, UI_SCREEN_H - 22);

    return scr;
}
