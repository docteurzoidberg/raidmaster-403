/*
 * Raidmaster 403 — orchestrateur UI top-level.
 *
 * Charge le thème, instancie les écrans à la demande, gère le switch courant, pousse les
 * ui_trip_state_t aux écrans concernés, enregistre un callback boutons par défaut qui
 * route +/−/menu vers l'écran courant (navigation menu), et câble les actions globales.
 *
 * Licence : GPL-3.0.
 */

#include "ui.h"
#include "theme.h"
#include "fonts.h"
#include "screen_boot.h"
#include "screen_raid.h"
#include "screen_balade.h"
#include "screen_menu.h"
#include "screen_calib.h"
#include "ui_demo.h"
#include <stdio.h>

static lv_obj_t      *s_screens[UI_SCREEN__COUNT] = {0};
static ui_screen_id_t s_current = UI_SCREEN_BOOT;
static lv_timer_t    *s_boot_timer = NULL;
static ui_trip_state_t s_last_state;
static bool           s_have_state = false;

static lv_obj_t *ensure_screen(ui_screen_id_t id)
{
    if (s_screens[id] != NULL) {
        return s_screens[id];
    }
    switch (id) {
        case UI_SCREEN_BOOT:   s_screens[id] = screen_boot_create();   break;
        case UI_SCREEN_RAID:   s_screens[id] = screen_raid_create();   break;
        case UI_SCREEN_BALADE: s_screens[id] = screen_balade_create(); break;
        case UI_SCREEN_MENU:   s_screens[id] = screen_menu_create();   break;
        case UI_SCREEN_CALIB:  s_screens[id] = screen_calib_create();  break;
        default: break;
    }
    return s_screens[id];
}

static void boot_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_boot_timer = NULL;
    ui_show_screen(UI_SCREEN_RAID);
    /* Le démarrage ne revient plus : on libère son écran (et l'animation, ~5,5 Mo + tampon). */
    if (s_screens[UI_SCREEN_BOOT] != NULL) {
        lv_obj_delete(s_screens[UI_SCREEN_BOOT]);
        s_screens[UI_SCREEN_BOOT] = NULL;
    }
    ui_demo_start();
}

static void ui_default_button_cb(ui_button_t btn, ui_button_event_t ev, void *user)
{
    (void)user;
    if (ev != UI_BTN_EV_PRESS && ev != UI_BTN_EV_LONGPRESS) return;

    /* Navigation menu dédiée. */
    if (s_current == UI_SCREEN_MENU) {
        if (ev == UI_BTN_EV_PRESS) {
            if (btn == UI_BTN_PLUS)       screen_menu_move(+1);
            else if (btn == UI_BTN_MINUS) screen_menu_move(-1);
            else if (btn == UI_BTN_MENU)  screen_menu_activate();
        } else if (ev == UI_BTN_EV_LONGPRESS && btn == UI_BTN_MENU) {
            ui_show_screen(UI_SCREEN_RAID);
        }
        return;
    }

    /* Autres écrans : long-press menu entre dans le menu. */
    if (ev == UI_BTN_EV_LONGPRESS && btn == UI_BTN_MENU) {
        ui_show_screen(UI_SCREEN_MENU);
        return;
    }

    /* Press court simple → log pour MVP (RAZ A/B, +/−, bascule trip actif). */
    static const char *names[UI_BTN__COUNT] = {"A", "B", "+", "-", "M"};
    if (ev == UI_BTN_EV_PRESS && btn < UI_BTN__COUNT) {
        printf("[ui] button %s pressed (screen=%d)\n", names[btn], (int)s_current);
    }
}

void ui_init(lv_display_t *disp)
{
    /* Police Share Tech Mono (FreeType) chargée AVANT la création des écrans — les macros
     * UI_FONT_* appellent ui_font_mono(). Repli Montserrat intégré si FreeType échoue. */
    ui_fonts_init();

    ui_theme_init(disp);
    ui_input_set_cb(ui_default_button_cb, NULL);

    /* On commence par l'écran boot. */
    ui_show_screen(UI_SCREEN_BOOT);

    /* Passage boot → raid après une rotation de l'animation (~1,2 s sans animation). */
    s_boot_timer = lv_timer_create(boot_timer_cb, screen_boot_duration_ms(), NULL);
    lv_timer_set_repeat_count(s_boot_timer, 1);
}

void ui_tick(uint32_t elapsed_ms)
{
    lv_tick_inc(elapsed_ms);
}

void ui_show_screen(ui_screen_id_t id)
{
    if (id >= UI_SCREEN__COUNT) return;
    lv_obj_t *scr = ensure_screen(id);
    if (scr == NULL) return;
    lv_screen_load(scr);
    s_current = id;
    /* Un écran créé à la volée reçoit tout de suite le dernier état connu. */
    if (s_have_state) {
        if (id == UI_SCREEN_RAID)   screen_raid_update(&s_last_state);
        if (id == UI_SCREEN_BALADE) screen_balade_update(&s_last_state);
        if (id == UI_SCREEN_MENU)   screen_menu_update(&s_last_state);
        if (id == UI_SCREEN_CALIB)  screen_calib_update(&s_last_state);
    }
}

ui_screen_id_t ui_current_screen(void)
{
    return s_current;
}

void ui_cycle_screen(void)
{
    /* Boucle : raid → balade → menu → raid, saute le boot. */
    switch (s_current) {
        case UI_SCREEN_BOOT:
        case UI_SCREEN_RAID:   ui_show_screen(UI_SCREEN_BALADE); break;
        case UI_SCREEN_BALADE: ui_show_screen(UI_SCREEN_MENU);   break;
        case UI_SCREEN_MENU:   ui_show_screen(UI_SCREEN_CALIB);  break;
        case UI_SCREEN_CALIB:  ui_show_screen(UI_SCREEN_RAID);   break;
        default:               ui_show_screen(UI_SCREEN_RAID);   break;
    }
}

void ui_rebuild_screens(void)
{
    /* Changement de palette : on recrée l'écran courant, on le charge, puis on détruit les
     * anciens (y compris l'ancien courant, une fois remplacé). Les autres écrans seront
     * recréés à la demande. */
    lv_obj_t *old[UI_SCREEN__COUNT];
    for (int i = 0; i < UI_SCREEN__COUNT; i++) {
        old[i] = s_screens[i];
        s_screens[i] = NULL;
    }
    /* Pendant le démarrage, on garde l'écran boot tel quel (animation en cours, ressources
     * uniques) : seuls les autres écrans prendront la nouvelle palette. */
    if (s_current == UI_SCREEN_BOOT) {
        s_screens[UI_SCREEN_BOOT] = old[UI_SCREEN_BOOT];
        old[UI_SCREEN_BOOT] = NULL;
    }
    ui_show_screen(s_current);
    for (int i = 0; i < UI_SCREEN__COUNT; i++) {
        if (old[i] != NULL) lv_obj_delete(old[i]);
    }
    if (s_have_state) ui_push_trip_state(&s_last_state);
}

void ui_push_trip_state(const ui_trip_state_t *s)
{
    if (s == NULL) return;
    s_last_state = *s;
    s_have_state = true;
    /* On met à jour tous les écrans "data" instanciés, pas seulement le courant : au prochain
     * switch les labels reflètent déjà l'état le plus récent. */
    if (s_screens[UI_SCREEN_RAID]   != NULL) screen_raid_update(s);
    if (s_screens[UI_SCREEN_BALADE] != NULL) screen_balade_update(s);
    if (s_screens[UI_SCREEN_MENU]   != NULL) screen_menu_update(s);
    if (s_screens[UI_SCREEN_CALIB]  != NULL) screen_calib_update(s);
}
