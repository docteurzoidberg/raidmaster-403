/*
 * Raidmaster 403 — thème LVGL et palettes jour / nuit.
 *
 * LVGL v9 cache la struct lv_theme_t dans un en-tête private (`lv_theme_private.h`), on
 * n'y touche pas depuis l'application. On reste donc sur le thème simple fourni par LVGL
 * (`lv_theme_simple_init`) et chaque écran applique ses couleurs depuis la palette active
 * (`ui_pal()`, macros UI_C_* de ui_tokens.h).
 *
 * Bascule jour / nuit : on change de palette puis ui.c reconstruit les écrans
 * (ui_rebuild_screens()), ce qui est plus simple et plus sûr que de re-skinner chaque widget.
 *
 * Licence : GPL-3.0.
 */

#include "theme.h"
#include "ui.h"
#include "ui_tokens.h"

static bool          s_night_mode = false;
static lv_display_t *s_disp       = NULL;

static ui_palette_t s_day;
static ui_palette_t s_night;

static void palettes_init(void)
{
    s_day = (ui_palette_t){
        .bg = UI_COLOR_LCD_BG,       .panel = UI_COLOR_LCD_PANEL, .divider = UI_COLOR_LCD_DIVIDER,
        .text = UI_COLOR_LCD_TEXT,   .muted = UI_COLOR_LCD_MUTED, .amber = UI_COLOR_LCD_AMBER,
        .cyan = UI_COLOR_LCD_CYAN,   .ok = UI_COLOR_LCD_OK,       .warn = UI_COLOR_LCD_WARN,
        .crit = UI_COLOR_LCD_CRIT,   .on_amber = UI_COLOR_LCD_BG,
    };
    /* Nuit : fond noir pur, tout en orange profond ; le cyan est atténué (80 %) pour garder
     * la hiérarchie Trip A / Trip B de la maquette. */
    s_night = (ui_palette_t){
        .bg = UI_COLOR_NIGHT_BG,     .panel = UI_COLOR_NIGHT_BG,  .divider = UI_COLOR_NIGHT_DIVIDER,
        .text = UI_COLOR_NIGHT_FG,   .muted = UI_COLOR_NIGHT_MUTED, .amber = UI_COLOR_NIGHT_FG,
        .cyan = lv_color_mix(UI_COLOR_NIGHT_FG, UI_COLOR_NIGHT_BG, LV_OPA_80),
        .ok = UI_COLOR_NIGHT_FG,     .warn = UI_COLOR_NIGHT_FG,   .crit = UI_COLOR_NIGHT_FG,
        .on_amber = UI_COLOR_NIGHT_BG,
    };
}

const ui_palette_t *ui_pal(void)
{
    return s_night_mode ? &s_night : &s_day;
}

void ui_theme_init(lv_display_t *disp)
{
    s_disp = disp;
    palettes_init();
    lv_theme_t *simple = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, simple);
}

void ui_theme_toggle_night(void)
{
    s_night_mode = !s_night_mode;
    ui_rebuild_screens();
}

bool ui_theme_is_night(void)
{
    return s_night_mode;
}
