/*
 * Raidmaster 403 — hal_audio sur PC : sortie audio SDL (mono 16 bits), ouverte au premier son.
 *
 * Le rappel SDL tire les échantillons du son en cours puis complète de silence ; le
 * périphérique reste ouvert (silence) jusqu'à la fin du sim. Sans carte son (ou avec
 * SDL_AUDIODRIVER=dummy), les sons sont perdus sans erreur.
 *
 * Licence : GPL-3.0.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_audio.h"

static SDL_AudioDeviceID s_dev;
static uint32_t          s_rate;
static bool              s_failed;      /* sortie indisponible : on ne réessaie pas */
static hal_audio_fill_fn s_fill;        /* son en cours (lu dans le fil SDL) */
static void             *s_user;
static SDL_atomic_t      s_busy;

static void callback(void *unused, Uint8 *stream, int len)
{
    (void)unused;
    int16_t *buf = (int16_t *)(void *)stream;
    const size_t n = (size_t)len / sizeof(int16_t);
    size_t got = 0;
    if (s_fill != NULL) {
        got = s_fill(buf, n, s_user);
        if (got < n) {
            s_fill = NULL;
            SDL_AtomicSet(&s_busy, 0);
        }
    }
    memset(buf + got, 0, (n - got) * sizeof(int16_t));
}

static bool open_device(uint32_t rate)
{
    if (s_dev != 0 && s_rate == rate) return true;
    if (s_dev != 0) SDL_CloseAudioDevice(s_dev);
    s_dev = 0;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[sim] son indisponible : %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want = {
        .freq = (int)rate,
        .format = AUDIO_S16SYS,
        .channels = 1,
        .samples = 512,
        .callback = callback,
    };
    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);   /* SDL convertit vers la sortie réelle */
    if (s_dev == 0) {
        fprintf(stderr, "[sim] son indisponible : %s\n", SDL_GetError());
        return false;
    }
    s_rate = rate;
    return true;
}

bool hal_audio_play(uint32_t rate, hal_audio_fill_fn fill, void *user)
{
    if (s_failed || SDL_AtomicGet(&s_busy)) return false;
    if (!open_device(rate)) {
        s_failed = true;
        return false;
    }
    SDL_LockAudioDevice(s_dev);
    s_fill = fill;
    s_user = user;
    SDL_AtomicSet(&s_busy, 1);
    SDL_UnlockAudioDevice(s_dev);
    SDL_PauseAudioDevice(s_dev, 0);
    return true;
}

bool hal_audio_busy(void)
{
    return SDL_AtomicGet(&s_busy) != 0;
}
