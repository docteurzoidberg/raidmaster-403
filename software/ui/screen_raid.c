/*
 * Raidmaster 403 — écran mode raid.
 *
 * Reproduit la maquette « Mode raid » (artifact Raidmaster 403 — maquettes), 480×320 :
 *
 *   ┌──────────────────────────────────────────────────────────┐  34 px barre d'état
 *   │ 14:23  10Hz  ● 3D  ▮▮▮▯ 68%                   22°C  RAID │
 *   ├──────────────────────────────────────────────────────────┤
 *   │ A                                        12.34        km │  Trip A (amber, 72 px)
 *   ├──────────────────────────────────────────────────────────┤
 *   │ B                                         3.47        km │  Trip B (cyan, 44 px)
 *   ├──────────────────────────────────────────────────────────┤
 *   │ V           68  km/h  │ CAP   ➤      127°  SE            │  V / CAP (38 px) + flèche 3D,
 *   │                       │ MAG                              │  source du cap sous « CAP »
 *   ├──────────────────────────────────────────────────────────┤
 *   │ ODO 48 273 km          MOY 42 km/h       CHRONO 0:14:27  │
 *   └──────────────────────────────────────────────────────────┘
 *
 * Les positions viennent de la maquette (cqw × 4,8 px). Les textes sont calés sur leur ligne
 * de base (ui_place_baseline) pour que libellé, valeur et unité s'alignent comme en CSS.
 *
 * Licence : GPL-3.0.
 */

#include "screen_raid.h"
#include "ui_tokens.h"
#include "ui_widgets.h"
#include "theme.h"
#include "img_cap_arrow.h"
#include <stdio.h>

/* Géométrie (px). */
#define X_L          UI_PAD_X                     /* bord gauche du contenu */
#define X_R          (UI_SCREEN_W - UI_PAD_X)     /* bord droit du contenu */
#define TRIP_VAL_R   440                          /* bord droit des valeurs trip */
#define TRIP_VAL_W   400
#define TRIP_A_BASE  104
#define TRIP_A_DIV   124
#define TRIP_B_BASE  167
#define TRIP_B_DIV   183
#define MID_BASE     248                          /* ligne de base V / CAP */
#define MID_CENTER_Y 234                          /* centre visuel des chiffres V / CAP */
#define COL2_X       247                          /* début colonne CAP */
#define FOOT_DIV     286
#define FOOT_Y       293

static lv_obj_t      *s_scr = NULL;
static ui_statusbar_t s_sb;
static lv_obj_t      *s_lbl_trip_a;
static lv_obj_t      *s_lbl_trip_b;
static lv_obj_t      *s_lbl_speed;
static lv_obj_t      *s_lbl_cap;
static lv_obj_t      *s_lbl_card;
static lv_obj_t      *s_lbl_cap_src;
static lv_obj_t      *s_img_cap;
static lv_obj_t      *s_img_cap_shadow;
static lv_obj_t      *s_lbl_odo;
static lv_obj_t      *s_lbl_moy;
static lv_obj_t      *s_lbl_chrono;

static const char *cardinal_for_heading(int deg)
{
    static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    int idx = ((deg + 22) / 45) & 7;
    return dirs[idx];
}

/* Rangée trip : libellé à gauche, valeur calée à droite, unité au bord droit, filet dessous. */
static lv_obj_t *build_trip_row(const char *name, const lv_font_t *val_font, lv_color_t val_color,
                                int32_t baseline, int32_t divider_y)
{
    lv_obj_t *lbl = ui_label(s_scr, UI_FONT_TIME, UI_C_MUTED, name);
    lv_obj_set_style_text_letter_space(lbl, 2, 0);
    ui_place_baseline(lbl, X_L, baseline, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_t *val = ui_label(s_scr, val_font, val_color, "0.00");
    ui_place_baseline(val, TRIP_VAL_R, baseline, LV_TEXT_ALIGN_RIGHT, TRIP_VAL_W);

    lv_obj_t *unit = ui_label(s_scr, UI_FONT_UNIT, UI_C_MUTED, "km");
    lv_obj_set_style_text_letter_space(unit, 1, 0);
    ui_place_baseline(unit, X_R, baseline, LV_TEXT_ALIGN_RIGHT, 30);

    ui_hline(s_scr, X_L, divider_y, X_R - X_L);
    return val;
}

static void build_row_mid(void)
{
    /* Cellule V : libellé | valeur calée à droite | unité. */
    lv_obj_t *v = ui_label(s_scr, UI_FONT_STATUS, UI_C_MUTED, "V");
    ui_place_baseline(v, X_L, MID_BASE, LV_TEXT_ALIGN_LEFT, 0);
    s_lbl_speed = ui_label(s_scr, UI_FONT_CELL, UI_C_TEXT, "0");
    ui_place_baseline(s_lbl_speed, 199, MID_BASE, LV_TEXT_ALIGN_RIGHT, 120);
    lv_obj_t *kmh = ui_label(s_scr, UI_FONT_SMALL, UI_C_MUTED, "km/h");
    ui_place_baseline(kmh, 207, MID_BASE, LV_TEXT_ALIGN_LEFT, 0);

    /* Cellule CAP : libellé | flèche 3D | valeur | point cardinal. */
    lv_obj_t *cap = ui_label(s_scr, UI_FONT_STATUS, UI_C_MUTED, "CAP");
    ui_place_baseline(cap, COL2_X, MID_BASE, LV_TEXT_ALIGN_LEFT, 0);
    /* Source du cap sous le libellé : MAG, GPS, MAG × ou MAG ?. */
    s_lbl_cap_src = ui_label(s_scr, UI_FONT_SMALL, UI_C_MUTED, "");
    ui_place_baseline(s_lbl_cap_src, COL2_X, MID_BASE + 18, LV_TEXT_ALIGN_LEFT, 0);
    s_lbl_cap = ui_label(s_scr, UI_FONT_CELL, UI_C_TEXT, "---°");
    ui_place_baseline(s_lbl_cap, 445, MID_BASE, LV_TEXT_ALIGN_RIGHT, 100);
    s_lbl_card = ui_label(s_scr, UI_FONT_SMALL, UI_C_MUTED, "N");
    ui_place_baseline(s_lbl_card, 453, MID_BASE, LV_TEXT_ALIGN_LEFT, 0);

    /* Flèche de cap (style 07 isométrique) dans le blanc entre « CAP » et la valeur, centrée
     * sur les chiffres. L'ombre est un sprite séparé, décalé et pivoté indépendamment : la
     * lumière reste fixe quand la flèche tourne. */
    const int32_t ax = 316 - IMG_CAP_ARROW_SIZE / 2;
    const int32_t ay = MID_CENTER_Y - IMG_CAP_ARROW_SIZE / 2;
    s_img_cap_shadow = lv_image_create(s_scr);
    lv_image_set_src(s_img_cap_shadow, &img_cap_arrow_shadow);
    lv_obj_set_pos(s_img_cap_shadow, ax + 3, ay + 3);
    lv_image_set_pivot(s_img_cap_shadow, IMG_CAP_ARROW_SIZE / 2, IMG_CAP_ARROW_SIZE / 2);

    s_img_cap = lv_image_create(s_scr);
    lv_image_set_src(s_img_cap, &img_cap_arrow);
    lv_obj_set_pos(s_img_cap, ax, ay);
    lv_image_set_pivot(s_img_cap, IMG_CAP_ARROW_SIZE / 2, IMG_CAP_ARROW_SIZE / 2);
    if (ui_theme_is_night()) {
        /* Nuit : flèche teintée orange profond, on garde un peu du modelé de la facette. */
        lv_obj_set_style_image_recolor(s_img_cap, UI_C_AMBER, 0);
        lv_obj_set_style_image_recolor_opa(s_img_cap, LV_OPA_80, 0);
    }
}

/* Élément du bas d'écran : « LIBELLÉ valeur ». */
static lv_obj_t *footer_item(lv_obj_t *row, const char *name, const char *init)
{
    lv_obj_t *g = ui_box(row);
    lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g, 4, 0);
    ui_label(g, UI_FONT_SMALL, UI_C_MUTED, name);
    return ui_label(g, UI_FONT_SMALL, UI_C_TEXT, init);
}

static void build_footer(void)
{
    ui_hline(s_scr, X_L, FOOT_DIV, X_R - X_L);

    lv_obj_t *row = ui_box(s_scr);
    lv_obj_set_size(row, X_R - X_L, LV_SIZE_CONTENT);
    lv_obj_set_pos(row, X_L, FOOT_Y);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_lbl_odo    = footer_item(row, "ODO", "0 km");
    s_lbl_moy    = footer_item(row, "MOY", "0 km/h");
    s_lbl_chrono = footer_item(row, "CHRONO", "0:00:00");
}

lv_obj_t *screen_raid_create(void)
{
    s_scr = ui_screen_base();
    ui_statusbar_create(s_scr, &s_sb, true, true, ui_theme_is_night() ? "NUIT" : "RAID");

    s_lbl_trip_a = build_trip_row("A", UI_FONT_TRIP_A, UI_C_AMBER, TRIP_A_BASE, TRIP_A_DIV);
    s_lbl_trip_b = build_trip_row("B", UI_FONT_TRIP_B, UI_C_CYAN, TRIP_B_BASE, TRIP_B_DIV);
    build_row_mid();
    build_footer();

    return s_scr;
}

void screen_raid_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) {
        return;
    }

    ui_statusbar_update(&s_sb, s);

    lv_label_set_text_fmt(s_lbl_trip_a, "%.2f", s->trip_a_km);
    lv_label_set_text_fmt(s_lbl_trip_b, "%.2f", s->trip_b_km);

    float sp = s->speed_kmh;
    if (sp < 0) sp = 0;
    lv_label_set_text_fmt(s_lbl_speed, "%d", (int)(sp + 0.5f));
    lv_obj_set_style_text_color(s_lbl_speed, s->speed_alarm ? UI_C_CRIT : UI_C_TEXT, 0);

    int cap = s->heading_deg % 360;
    if (cap < 0) cap += 360;
    if (s->heading_unknown) {
        lv_label_set_text(s_lbl_cap, "---°");
        lv_label_set_text(s_lbl_card, "");
    } else {
        lv_label_set_text_fmt(s_lbl_cap, "%03d°", cap);
        lv_label_set_text(s_lbl_card, cardinal_for_heading(cap));
    }
    lv_color_t tag_c;
    lv_label_set_text(s_lbl_cap_src, ui_heading_tag(s, &tag_c));
    lv_obj_set_style_text_color(s_lbl_cap_src, tag_c, 0);
    lv_image_set_rotation(s_img_cap, cap * 10);
    lv_image_set_rotation(s_img_cap_shadow, cap * 10);

    char odo[24];
    ui_fmt_km(odo, sizeof(odo), s->odo_km, 0);
    lv_label_set_text_fmt(s_lbl_odo, "%s km", odo);
    lv_label_set_text_fmt(s_lbl_moy, "%d km/h", (int)(s->avg_kmh + 0.5f));
    lv_label_set_text_fmt(s_lbl_chrono, "%u:%02u:%02u",
                          (unsigned)(s->chrono_s / 3600U),
                          (unsigned)((s->chrono_s / 60U) % 60U),
                          (unsigned)(s->chrono_s % 60U));
}
