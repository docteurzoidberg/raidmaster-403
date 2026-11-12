/*
 * Raidmaster 403 — hal_audio du sim web : Web Audio.
 *
 * Le son est calculé d'un bloc (jusqu'à 30 s) puis confié au navigateur en un AudioBuffer. La
 * page fournit le contexte audio (Module.audioCtx, créé au premier clic : les navigateurs
 * refusent le son avant un geste de l'utilisateur) ; sans lui (Node, page pas encore cliquée), le
 * son est perdu sans erreur, comme sur un PC sans carte son.
 *
 * Licence : GPL-3.0.
 */

#include <emscripten.h>
#include <stdlib.h>

#include "hal/hal_audio.h"

#define MAX_SECONDS 30U
#define CHUNK       512U

/* clang-format off */
EM_JS(int, js_audio_play, (const int16_t *pcm, int n, int rate), {
    const ctx = Module.audioCtx;
    if (!ctx || n <= 0) return 0;
    const buf = ctx.createBuffer(1, n, rate);
    const ch = buf.getChannelData(0);
    const base = pcm >> 1;
    for (let i = 0; i < n; i++) ch[i] = HEAP16[base + i] / 32768;
    const src = ctx.createBufferSource();
    src.buffer = buf;
    src.connect(Module.audioOut || ctx.destination);
    src.start();
    Module.audioEnd = ctx.currentTime + n / rate;
    return 1;
})

EM_JS(int, js_audio_busy, (void), {
    const ctx = Module.audioCtx;
    return ctx && Module.audioEnd && ctx.currentTime < Module.audioEnd ? 1 : 0;
})
/* clang-format on */

bool hal_audio_play(uint32_t rate, hal_audio_fill_fn fill, void *user)
{
    if (js_audio_busy()) return false;
    const size_t cap = (size_t)rate * MAX_SECONDS;
    int16_t *pcm = malloc(cap * sizeof(int16_t));
    if (pcm == NULL) return false;
    size_t n = 0;
    while (n < cap) {
        const size_t want = cap - n < CHUNK ? cap - n : CHUNK;
        const size_t got = fill(pcm + n, want, user);
        n += got;
        if (got < want) break;
    }
    const bool ok = js_audio_play(pcm, (int)n, (int)rate) != 0;
    free(pcm);
    return ok;
}

bool hal_audio_busy(void)
{
    return js_audio_busy() != 0;
}
