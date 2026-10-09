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
 * UI-agent exception: integration LVGL (voir prompts/ui.md).
 *
 * Licence : GPL-3.0.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "lvgl.h"
#include "ui.h"
#include "theme.h"
#include "ui_demo.h"
#include "ui_input.h"

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

    /* On ne fait un RenderPresent qu'au dernier chunk pour éviter le tearing. */
    if (lv_display_flush_is_last(disp)) {
        SDL_RenderClear(s_renderer);
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);
    }

    lv_display_flush_ready(disp);
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
    int running = 1;
    uint32_t last_ms = SDL_GetTicks();
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
