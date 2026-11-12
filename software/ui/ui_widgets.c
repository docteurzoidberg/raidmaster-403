/*
 * Raidmaster 403 — briques communes aux écrans.
 *
 * Licence : GPL-3.0.
 */

#include "ui_widgets.h"
#include "ui_tokens.h"
#include "theme.h"
#include <stdio.h>
#include <math.h>

lv_obj_t *ui_screen_base(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, UI_C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

lv_obj_t *ui_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

lv_obj_t *ui_hline(lv_obj_t *parent, int32_t x, int32_t y, int32_t w)
{
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, w, 1);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, UI_C_DIVIDER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

void ui_place_baseline(lv_obj_t *lbl, int32_t x, int32_t baseline_y, lv_text_align_t align,
                       int32_t w)
{
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    int32_t top = baseline_y - (f->line_height - f->base_line);
    lv_obj_set_style_text_align(lbl, align, 0);
    if (align == LV_TEXT_ALIGN_RIGHT) {
        lv_obj_set_width(lbl, w);
        lv_obj_set_pos(lbl, x - w, top);
    } else if (align == LV_TEXT_ALIGN_CENTER) {
        lv_obj_set_width(lbl, w);
        lv_obj_set_pos(lbl, x - w / 2, top);
    } else {
        lv_obj_set_pos(lbl, x, top);
    }
}

void ui_fmt_km(char *buf, size_t len, double km, int decimals)
{
    if (km < 0) km = 0;
    if (decimals < 0) decimals = 0;
    if (decimals > 3) decimals = 3;
    double scale = pow(10.0, decimals);
    unsigned long long total = (unsigned long long)llround(km * scale);
    unsigned long long ip    = total / (unsigned long long)scale;
    unsigned long long fp    = total % (unsigned long long)scale;

    /* Partie entière groupée par 3 avec espaces. */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%llu", ip);
    char grouped[48];
    size_t n = 0, digits = 0;
    for (size_t i = 0; tmp[i] != '\0'; i++) digits++;
    for (size_t i = 0; tmp[i] != '\0' && n + 2 < sizeof(grouped); i++) {
        if (i > 0 && (digits - i) % 3 == 0) grouped[n++] = ' ';
        grouped[n++] = tmp[i];
    }
    grouped[n] = '\0';

    if (decimals > 0) {
        char frac[8];
        snprintf(frac, sizeof(frac), "%03u", (unsigned)(fp % 1000U));
        snprintf(buf, len, "%s,%s", grouped, frac + (3 - decimals));
    } else {
        snprintf(buf, len, "%s", grouped);
    }
}

/* ------------------------------------------------------------------------- */
/* Barre d'état                                                              */

#define CHARGE_STEP_MS 500U     /* animation de charge : une cellule de plus toutes les 500 ms */

static lv_obj_t *sb_label(lv_obj_t *parent, const char *text)
{
    return ui_label(parent, UI_FONT_STATUS, UI_C_MUTED, text);
}

void ui_statusbar_create(lv_obj_t *parent, ui_statusbar_t *sb, bool show_hz, bool show_temp,
                         const char *mode_text)
{
    lv_obj_t *bar = ui_box(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_C_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_set_style_pad_column(bar, 12, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_hline(parent, 0, UI_STATUS_BAR_H, UI_SCREEN_W);

    sb->time = ui_label(bar, UI_FONT_TIME, UI_C_TEXT, "--:--");
    sb->hz   = show_hz ? sb_label(bar, "10Hz") : NULL;

    /* Pastille de fix + texte. */
    lv_obj_t *fixg = ui_box(bar);
    lv_obj_set_size(fixg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fixg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fixg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fixg, 4, 0);
    sb->fix_dot = ui_box(fixg);
    lv_obj_set_size(sb->fix_dot, 7, 7);
    lv_obj_set_style_radius(sb->fix_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(sb->fix_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(sb->fix_dot, UI_C_CRIT, 0);
    if (!ui_theme_is_night()) {
        lv_obj_set_style_shadow_width(sb->fix_dot, 8, 0);
        lv_obj_set_style_shadow_opa(sb->fix_dot, LV_OPA_50, 0);
    }
    sb->fix = sb_label(fixg, "--");

    /* Jauge batterie 4 cellules + pourcentage. */
    lv_obj_t *battg = ui_box(bar);
    lv_obj_set_size(battg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(battg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(battg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(battg, 4, 0);
    sb->batt_box = ui_box(battg);
    lv_obj_set_size(sb->batt_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(sb->batt_box, 1, 0);
    lv_obj_set_style_border_color(sb->batt_box, UI_C_MUTED, 0);
    lv_obj_set_style_radius(sb->batt_box, 2, 0);
    lv_obj_set_style_pad_all(sb->batt_box, 2, 0);
    lv_obj_set_style_pad_column(sb->batt_box, 1, 0);
    lv_obj_set_flex_flow(sb->batt_box, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < 4; i++) {
        sb->cells[i] = ui_box(sb->batt_box);
        lv_obj_set_size(sb->cells[i], 5, 9);
        lv_obj_set_style_bg_color(sb->cells[i], UI_C_MUTED, 0);
        lv_obj_set_style_bg_opa(sb->cells[i], LV_OPA_COVER, 0);
    }
    /* Éclair par-dessus la jauge (alimentation externe) : symbole de la police LVGL, détouré à la
     * couleur du panneau par quatre copies décalées d'un pixel pour rester lisible sur les
     * cellules pleines. Il déborde de la jauge en hauteur. */
    lv_obj_add_flag(battg, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(sb->batt_box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    sb->bolt = ui_box(sb->batt_box);
    lv_obj_add_flag(sb->bolt, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_OVERFLOW_VISIBLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(sb->bolt, 14, 18);
    lv_obj_align(sb->bolt, LV_ALIGN_CENTER, 0, 0);
    static const int8_t halo[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int i = 0; i <= 4; i++) {
        lv_obj_t *l = ui_label(sb->bolt, &lv_font_montserrat_14, i < 4 ? UI_C_PANEL : UI_C_TEXT,
                               LV_SYMBOL_CHARGE);
        lv_obj_align(l, LV_ALIGN_CENTER, i < 4 ? halo[i][0] : 0, i < 4 ? halo[i][1] : 0);
    }
    sb->batt = sb_label(battg, "--%");

    /* Poussoir : le reste est calé à droite. */
    lv_obj_t *spacer = ui_box(bar);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    sb->temp = show_temp ? sb_label(bar, "--°C") : NULL;
    sb->mode = sb_label(bar, mode_text);
}

void ui_statusbar_update(ui_statusbar_t *sb, const ui_trip_state_t *s)
{
    lv_label_set_text(sb->time, s->time_hhmm[0] ? s->time_hhmm : "--:--");

    const char *fix_txt = "--";
    lv_color_t  dot     = UI_C_CRIT;
    if (s->gps_absent)                 fix_txt = "GPS ×";     /* module GPS absent */
    else if (s->fix_type == UI_FIX_2D) { fix_txt = "2D"; dot = UI_C_WARN; }
    else if (s->fix_type == UI_FIX_3D) { fix_txt = "3D"; dot = UI_C_OK; }
    lv_label_set_text(sb->fix, fix_txt);
    lv_obj_set_style_text_color(sb->fix, s->gps_absent ? UI_C_CRIT : UI_C_MUTED, 0);
    lv_obj_set_style_bg_color(sb->fix_dot, dot, 0);
    lv_obj_set_style_shadow_color(sb->fix_dot, dot, 0);

    /* Jauge : une cellule par tranche de 25 %, arrondi au supérieur dès qu'il reste du jus.
     * Sur batterie, couleur d'alerte sous 30 % / 10 %, neutre sinon (maquette). En charge : vert,
     * et les cellules au-dessus du niveau s'allument une à une (au moins la dernière clignote).
     * Alimentation externe : éclair par-dessus ; sans batterie, jauge vide et « USB ». */
    const bool known = s->battery_pct <= 100;
    const bool ext   = s->power != UI_PWR_BATTERY;
    const int  pct   = known ? s->battery_pct : 0;
    int on = (pct + 24) / 25;
    lv_color_t c = UI_C_MUTED;
    if (s->power == UI_PWR_CHARGING) {
        const int base = on < 3 ? on : 3;
        on = base + (int)(lv_tick_get() / CHARGE_STEP_MS % (uint32_t)(4 - base + 1));
        c = UI_C_OK;
    } else if (!ext && known) {
        if (pct <= 10)      c = UI_C_CRIT;
        else if (pct <= 30) c = UI_C_WARN;
    }
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_color(sb->cells[i], c, 0);
        lv_obj_set_style_bg_opa(sb->cells[i], i < on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    lv_obj_set_style_border_color(sb->batt_box, c, 0);
    lv_obj_set_style_text_color(sb->batt, c, 0);
    if (known)    lv_label_set_text_fmt(sb->batt, "%d%%", pct);
    else if (ext) lv_label_set_text(sb->batt, "USB");
    else          lv_label_set_text(sb->batt, "--%");
    if (lv_obj_has_flag(sb->bolt, LV_OBJ_FLAG_HIDDEN) == ext) {
        if (ext) lv_obj_remove_flag(sb->bolt, LV_OBJ_FLAG_HIDDEN);
        else     lv_obj_add_flag(sb->bolt, LV_OBJ_FLAG_HIDDEN);
    }

    if (sb->temp != NULL) {
        if (isnan(s->t_cabin_c)) lv_label_set_text(sb->temp, "--°C");
        else lv_label_set_text_fmt(sb->temp, "%d°C", (int)lroundf(s->t_cabin_c));
    }
}

const char *ui_heading_tag(const ui_trip_state_t *s, lv_color_t *color)
{
    *color = UI_C_MUTED;
    if (s->heading_src == UI_HEAD_SRC_MAG) return "MAG";
    if (s->heading_src == UI_HEAD_SRC_GPS) return "GPS";
    if (!s->heading_unknown) return "";
    if (s->mag_state == UI_MAG_ABSENT) {
        *color = UI_C_CRIT;
        return "MAG ×";
    }
    if (s->mag_state == UI_MAG_UNCAL) {
        *color = UI_C_WARN;
        return "MAG ?";
    }
    return "";
}
