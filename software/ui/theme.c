/*
 * Raidmaster 403 — thème LVGL "pauvre" basé sur les tokens.
 *
 * LVGL v9 cache la struct lv_theme_t dans un en-tête private (`lv_theme_private.h`), on
 * n'y touche pas depuis l'application. On reste donc sur le thème simple fourni par LVGL
 * (`lv_theme_simple_init`) qui donne un rendu neutre, et chaque écran applique lui-même ses
 * couleurs via les tokens UI_COLOR_LCD_* (voir screen_raid.c, screen_balade.c…).
 *
 * Ce module expose uniquement :
 *   - ui_theme_init(disp)    : installe le thème simple sur le display, mémorise le display.
 *   - ui_theme_toggle_night(): bascule un flag global ; les prochains refresh de screens
 *                              utilisent la palette nuit. Pour un effet immédiat on invalide
 *                              l'écran courant.
 *   - ui_theme_is_night()    : getter du flag.
 *
 * Pour le MVP les écrans sont statiques — ils ne lisent pas encore `ui_theme_is_night()`
 * pour re-skinner leurs widgets. Le mode nuit visuel sera finalisé en phase 2 une fois les
 * polices Share Tech Mono générées et les écrans peaufinés (TODO documenté dans
 * prompts/ui.md § 9).
 *
 * Licence : GPL-3.0.
 */

#include "theme.h"
#include "ui_tokens.h"

static bool          s_night_mode = false;
static lv_display_t *s_disp       = NULL;

void ui_theme_init(lv_display_t *disp)
{
    s_disp = disp;
    /* Thème parent neutre : les widgets interactifs (lv_list, lv_btn…) ont un rendu
     * minimal mais cohérent. Nos écrans surchargent background et text color via styles
     * locaux. */
    lv_theme_t *simple = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, simple);
}

void ui_theme_toggle_night(void)
{
    s_night_mode = !s_night_mode;
    if (s_disp != NULL) {
        lv_obj_t *scr = lv_screen_active();
        if (scr != NULL) {
            /* Si on est en mode nuit : fond noir pur ; sinon la couleur par défaut du screen.
             * Les écrans ré-appliqueront leur propre couleur à la prochaine création — pour le
             * MVP on force juste le fond du screen courant. */
            lv_color_t bg = s_night_mode ? UI_COLOR_NIGHT_BG : UI_COLOR_LCD_BG;
            lv_obj_set_style_bg_color(scr, bg, 0);
            lv_obj_invalidate(scr);
        }
    }
}

bool ui_theme_is_night(void)
{
    return s_night_mode;
}
