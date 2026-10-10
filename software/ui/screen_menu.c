/*
 * Raidmaster 403 — menu réglages.
 *
 * Maquette « Menu réglages » : barre d'état, titre RÉGLAGES amber souligné, liste de réglages
 * « libellé … valeur », sélection surlignée en amber. Liste scrollable si elle dépasse.
 *
 * Les actions sont câblées par ui_app.c (core) : il fournit les valeurs (screen_menu_set_value)
 * et l'état d'édition. Sans lui (démo, GIF, captures), valeurs d'exemple de la maquette, sauf
 * « Mode nuit » qui reflète le thème courant.
 *
 * Licence : GPL-3.0.
 */

#include "screen_menu.h"
#include "ui_tokens.h"
#include "ui_widgets.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

#define X_L        UI_PAD_X
#define X_R        (UI_SCREEN_W - UI_PAD_X)
#define TITLE_BASE 62
#define TITLE_DIV  74
#define LIST_Y     80
#define ROW_H      30

typedef struct {
    const char *name;
    const char *value;
} menu_item_t;

static const menu_item_t s_items[MENU__COUNT] = {
    {"Calibration ±%",            "+1,2 %"},
    {"Calibration auto distance", "›"},
    {"Fuseau horaire",            "UTC+1"},
    {"Mode nuit",                 NULL},      /* reflète ui_theme_is_night() */
    {"Affichage",                 "RAID"},
    {"Alarme vitesse",            "OFF"},
    {"Log GPX sur SD",            "ON"},
    {"Calibration boussole",      "›"},
};
#define S_ITEM_COUNT ((int)(sizeof(s_items) / sizeof(s_items[0])))

static lv_obj_t *s_scr  = NULL;
static ui_statusbar_t s_sb;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_rows[S_ITEM_COUNT];
static lv_obj_t *s_names[S_ITEM_COUNT];
static lv_obj_t *s_vals[S_ITEM_COUNT];
static int       s_selected = 0;
static char      s_values[S_ITEM_COUNT][24];   /* valeurs fournies (vide : exemple maquette) */
static bool      s_editing = false;

static const char *value_text(int i)
{
    if (s_values[i][0] != '\0') return s_values[i];
    if (s_items[i].value != NULL) return s_items[i].value;
    return ui_theme_is_night() ? "ON" : "OFF";
}

static void refresh_highlight(void)
{
    for (int i = 0; i < S_ITEM_COUNT; i++) {
        bool sel = (i == s_selected);
        lv_obj_set_style_bg_opa(s_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(s_rows[i], sel && s_editing ? UI_C_CYAN : UI_C_AMBER, 0);
        lv_obj_set_style_text_color(s_names[i], sel ? UI_C_ON_AMBER : UI_C_TEXT, 0);
        lv_obj_set_style_text_color(s_vals[i], sel ? UI_C_ON_AMBER : UI_C_TEXT, 0);
    }
    lv_obj_scroll_to_view(s_rows[s_selected], LV_ANIM_OFF);
}

/* Écran détruit (bascule jour / nuit) : plus de mise à jour tant qu'il n'est pas recréé. */
static void on_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) == s_scr) s_scr = NULL;
}

lv_obj_t *screen_menu_create(void)
{
    s_scr = ui_screen_base();
    lv_obj_add_event_cb(s_scr, on_delete, LV_EVENT_DELETE, NULL);
    ui_statusbar_create(s_scr, &s_sb, false, false, "MENU");

    lv_obj_t *title = ui_label(s_scr, UI_FONT_TITLE, UI_C_AMBER, "RÉGLAGES");
    lv_obj_set_style_text_letter_space(title, 2, 0);
    ui_place_baseline(title, X_L, TITLE_BASE, LV_TEXT_ALIGN_LEFT, 0);
    ui_hline(s_scr, X_L, TITLE_DIV, X_R - X_L);

    /* Liste scrollable (sans barre de défilement, la sélection suffit à se repérer). */
    s_list = ui_box(s_scr);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_list, X_L, LIST_Y);
    lv_obj_set_size(s_list, X_R - X_L, UI_SCREEN_H - LIST_Y - UI_PAD_Y);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 0, 0);

    for (int i = 0; i < S_ITEM_COUNT; i++) {
        lv_obj_t *row = ui_box(s_list);
        lv_obj_set_size(row, LV_PCT(100), ROW_H);
        lv_obj_set_style_radius(row, 3, 0);
        lv_obj_set_style_bg_color(row, UI_C_AMBER, 0);
        lv_obj_set_style_pad_hor(row, 7, 0);

        s_names[i] = ui_label(row, UI_FONT_TITLE, UI_C_TEXT, s_items[i].name);
        lv_obj_align(s_names[i], LV_ALIGN_LEFT_MID, 0, 0);

        s_vals[i] = ui_label(row, UI_FONT_TITLE, UI_C_TEXT, value_text(i));
        lv_obj_align(s_vals[i], LV_ALIGN_RIGHT_MID, 0, 0);
        s_rows[i] = row;
    }

    refresh_highlight();
    return s_scr;
}

void screen_menu_move(int delta)
{
    if (s_scr == NULL) return;
    s_selected += delta;
    if (s_selected < 0) s_selected = 0;
    if (s_selected >= S_ITEM_COUNT) s_selected = S_ITEM_COUNT - 1;
    refresh_highlight();
}

int screen_menu_selected(void)
{
    return s_selected;
}

void screen_menu_set_value(int item, const char *text)
{
    if (item < 0 || item >= S_ITEM_COUNT || text == NULL) return;
    if (strcmp(s_values[item], text) == 0) return;
    snprintf(s_values[item], sizeof(s_values[item]), "%s", text);
    if (s_scr != NULL) lv_label_set_text(s_vals[item], s_values[item]);
}

void screen_menu_set_editing(bool on)
{
    if (s_editing == on) return;
    s_editing = on;
    if (s_scr != NULL) refresh_highlight();
}

void screen_menu_activate(void)
{
    if (s_scr == NULL) return;
    /* Stub : impression console seulement — le câblage effectif est TODO cross-agent. */
    printf("[ui] menu activate -> %s\n", s_items[s_selected].name);
}

void screen_menu_update(const ui_trip_state_t *s)
{
    if (s_scr == NULL || s == NULL) return;
    ui_statusbar_update(&s_sb, s);
}
