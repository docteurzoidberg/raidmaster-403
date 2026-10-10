/*
 * Raidmaster 403 — simulateur PC.
 *
 * Fenêtre SDL 480×320 landscape (résolution exacte du ST7796 de la carte Waveshare
 * ESP32-S3-Touch-LCD-3.5), avec LVGL v9 dessinant dedans via un buffer software + SDL_Texture.
 *
 * Pourquoi ce chemin plutôt que LV_USE_SDL=1 : on garde le contrôle total des SDL_Event, ce qui
 * permet de router les touches a/b/+/-/m vers notre ui_input_dispatch() sans combat avec le
 * driver clavier interne de LVGL. Fallback acceptable d'après prompts/ui.md (dérogation UI-agent).
 *
 * Touches clavier (voir prompts/ui.md § 4 et prompts/simulation.md § 1) :
 *   a / b / + ou = / - / m        → boutons physiques A, B, +, -, Menu (appui court)
 *   Shift + m                     → long-press menu (entrée/sortie menu réglages)
 *   n ou Tab                      → bascule mode nuit (toggle thème global)
 *   v                             → cycle entre écrans raid / balade / menu / calib
 *   d                             → bascule démo auto
 *   Échap                         → quitter
 *
 * Mode captures (vérification visuelle sans interaction) : RAIDMASTER_SHOTS=<dossier>
 * enchaîne chaque écran avec un état fixe repris de la maquette, écrit <dossier>/<écran>.ppm
 * puis quitte. Ex. : SDL_VIDEODRIVER=dummy RAIDMASTER_SHOTS=/tmp/shots ./raidmaster-sim
 *
 * UI-agent exception: integration LVGL (voir prompts/ui.md).
 *
 * Licence : GPL-3.0.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui.h"
#include "theme.h"
#include "ui_demo.h"
#include "ui_input.h"
#include "screen_menu.h"

enum {
    TRIPMASTER_SCREEN_W = 480,
    TRIPMASTER_SCREEN_H = 320,
};

/* Buffer LVGL : partiel (10 lignes), format RGB565 (LV_COLOR_DEPTH 16). On en met un seul,
 * c'est largement suffisant côté PC. */
#define BUF_LINES 40
static uint16_t s_lv_buf[TRIPMASTER_SCREEN_W * BUF_LINES];

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture  *s_texture  = NULL;

/* Copie complète de l'écran (RGB565), tenue à jour au flush, pour le mode captures. */
static uint16_t s_fb[TRIPMASTER_SCREEN_W * TRIPMASTER_SCREEN_H];

/* ------------------------------------------------------------------------- */
/* LVGL display flush : LVGL nous donne un rectangle à copier dans la texture SDL,
 * puis on présente. On fait un UpdateTexture ciblé par rectangle. */
static void sim_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    SDL_Rect r;
    r.x = area->x1;
    r.y = area->y1;
    r.w = area->x2 - area->x1 + 1;
    r.h = area->y2 - area->y1 + 1;

    /* px_map est au format RGB565, stride = r.w * 2. */
    SDL_UpdateTexture(s_texture, &r, px_map, r.w * (int)sizeof(uint16_t));
    const uint16_t *src = (const uint16_t *)px_map;
    for (int y = 0; y < r.h; y++) {
        memcpy(&s_fb[(r.y + y) * TRIPMASTER_SCREEN_W + r.x], &src[y * r.w],
               (size_t)r.w * sizeof(uint16_t));
    }

    /* On ne fait un RenderPresent qu'au dernier chunk pour éviter le tearing. */
    if (lv_display_flush_is_last(disp)) {
        SDL_RenderClear(s_renderer);
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);
    }

    lv_display_flush_ready(disp);
}

/* ------------------------------------------------------------------------- */
/* Mode captures. */
static void save_ppm(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "capture : impossible d'écrire %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    for (int i = 0; i < TRIPMASTER_SCREEN_W * TRIPMASTER_SCREEN_H; i++) {
        uint16_t p = s_fb[i];
        uint8_t rgb[3] = {
            (uint8_t)(((p >> 11) & 0x1F) * 255 / 31),
            (uint8_t)(((p >> 5) & 0x3F) * 255 / 63),
            (uint8_t)((p & 0x1F) * 255 / 31),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("capture : %s\n", path);
}

/* État fixe = valeurs de la maquette mode raid jour. */
static void push_mockup_state(void)
{
    ui_trip_state_t st;
    memset(&st, 0, sizeof(st));
    st.trip_a_km   = 12.345;
    st.trip_b_km   = 3.47;
    st.odo_km      = 48273.2;
    st.speed_kmh   = 68.0f;
    st.avg_kmh     = 42.0f;
    st.heading_deg = 127;
    st.chrono_s    = 14U * 60U + 27U;
    st.fix_type    = UI_FIX_3D;
    st.battery_pct = 68;
    st.t_cabin_c   = 22.0f;
    snprintf(st.time_hhmm, sizeof(st.time_hhmm), "14:23");
    ui_push_trip_state(&st);
}

/* Avance le scénario de captures ; renvoie 0 quand c'est fini. */
static int shots_step(const char *dir, uint32_t t_ms)
{
    static int step = 0;
    static uint32_t next = 700;
    if (t_ms < next) return 1;
    switch (step++) {
        case 0:  save_ppm(dir, "1-boot"); break;
        case 1:  ui_demo_stop(); ui_show_screen(UI_SCREEN_RAID); push_mockup_state(); break;
        case 2:  save_ppm(dir, "2-raid"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 3:  save_ppm(dir, "3-balade"); ui_show_screen(UI_SCREEN_MENU);
                 screen_menu_move(+1); break;
        case 4:  save_ppm(dir, "4-menu"); ui_show_screen(UI_SCREEN_CALIB); break;
        case 5:  save_ppm(dir, "5-calib"); ui_show_screen(UI_SCREEN_RAID);
                 ui_theme_toggle_night(); break;
        case 6:  save_ppm(dir, "6-raid-nuit"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 7:  save_ppm(dir, "7-balade-nuit"); return 0;
        default: return 0;
    }
    /* Après le boot, on attend la fin du timer boot→raid (1200 ms, qui lance la démo) avant
     * de figer l'état. */
    next = t_ms + (step == 1 ? 900 : step == 2 ? 600 : 300);
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Routing clavier SDL → ui_input_dispatch(). */
static void handle_key(const SDL_KeyboardEvent *k, int *running)
{
    bool is_long = (k->keysym.mod & KMOD_SHIFT) != 0;
    SDL_Keycode sym = k->keysym.sym;

    switch (sym) {
        case SDLK_ESCAPE:
            *running = 0;
            break;
        case SDLK_a:
            ui_input_dispatch(UI_BTN_A, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_b:
            ui_input_dispatch(UI_BTN_B, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            ui_input_dispatch(UI_BTN_PLUS, UI_BTN_EV_PRESS);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            ui_input_dispatch(UI_BTN_MINUS, UI_BTN_EV_PRESS);
            break;
        case SDLK_m:
            ui_input_dispatch(UI_BTN_MENU, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_n:
        case SDLK_TAB:
            ui_theme_toggle_night();
            break;
        case SDLK_v:
            ui_cycle_screen();
            break;
        case SDLK_d:
            ui_demo_toggle();
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init a echoue : %s\n", SDL_GetError());
        return 1;
    }

    s_window = SDL_CreateWindow(
        "Raidmaster 403 - sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (s_window == NULL) {
        fprintf(stderr, "SDL_CreateWindow a echoue : %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    s_renderer = SDL_CreateRenderer(
        s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (s_renderer == NULL) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (s_renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer a echoue : %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    if (s_texture == NULL) {
        fprintf(stderr, "SDL_CreateTexture a echoue : %s\n", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    /* ------- LVGL init ------- */
    lv_init();

    lv_display_t *disp = lv_display_create(TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    if (disp == NULL) {
        fprintf(stderr, "lv_display_create a echoue\n");
        return 1;
    }
    lv_display_set_flush_cb(disp, sim_disp_flush);
    lv_display_set_buffers(disp, s_lv_buf, NULL,
                           sizeof(s_lv_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    /* UI init (thème, écran boot, timer boot→raid, démo). */
    ui_init(disp);

    /* ------- Boucle principale ------- */
    const char *shots_dir = getenv("RAIDMASTER_SHOTS");
    int running = 1;
    uint32_t start_ms = SDL_GetTicks();
    uint32_t last_ms = start_ms;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                handle_key(&ev.key, &running);
            }
        }

        uint32_t now = SDL_GetTicks();
        uint32_t dt  = now - last_ms;
        last_ms      = now;
        if (dt > 0) {
            ui_tick(dt);
        }

        if (shots_dir != NULL && !shots_step(shots_dir, now - start_ms)) {
            running = 0;
        }

        uint32_t next = lv_timer_handler();
        if (next > 33U) next = 33U;    /* cap à ~30 FPS côté veille. */
        if (next < 1U)  next = 1U;
        SDL_Delay(next);
    }

    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    return 0;
}
