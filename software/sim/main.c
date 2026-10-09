/*
 * Raidmaster 403 — simulateur PC.
 *
 * Étape 1 du plan (voir PROMPT.md § 6) : ouvrir une fenêtre SDL 480×320 landscape, résolution
 * exacte du ST7796 de la carte Waveshare ESP32-S3-Touch-LCD-3.5. Fond blanc, sortie sur ÉCHAP ou
 * SDL_QUIT. Aucun code métier, aucun LVGL : ils arrivent aux étapes 2 et 3.
 *
 * Licence : GPL-3.0 — voir LICENSE à la racine.
 */

#include <SDL2/SDL.h>
#include <stdio.h>

enum {
    TRIPMASTER_SCREEN_W = 480,
    TRIPMASTER_SCREEN_H = 320,
};

int main(int argc, char *argv[])
{
    /* argc/argv ignorés en étape 1 : la CLI (--replay, --scenario, --speed) arrive avec le HAL
     * sim, cf. prompts/simulation.md § 1. On marque les paramètres comme utilisés pour que
     * -Wextra ne râle pas. */
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init a échoué : %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Raidmaster 403 — sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow a échoué : %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        /* Fallback software : certaines config WSL n'ont pas d'accélération GL. */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer a échoué : %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN &&
                       ev.key.keysym.sym == SDLK_ESCAPE) {
                running = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
