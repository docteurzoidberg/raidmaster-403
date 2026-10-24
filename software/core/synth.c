/*
 * Raidmaster 403 — petit synthétiseur (voir synth.h).
 *
 * Licence : GPL-3.0.
 */

#include "synth.h"

#include <math.h>
#include <string.h>

#define TWO_PI      6.28318530718f
#define ECHO_FLOOR  0.01f       /* traîne de l'écho : jusqu'à 1 % du son d'origine */

static uint32_t samples(uint32_t ms)
{
    return (uint32_t)((uint64_t)ms * SYNTH_RATE / 1000U);
}

void synth_init(synth_t *s, const synth_score_t *score)
{
    memset(s, 0, sizeof(*s));
    s->score = score;
    s->n_notes = score->n_notes > SYNTH_MAX_NOTES ? (uint8_t)SYNTH_MAX_NOTES : score->n_notes;
    s->noise = 403U;            /* graine fixe : même rendu à chaque lecture */
    uint32_t last = 0;
    for (uint8_t i = 0; i < s->n_notes; i++) {
        const synth_note_t *e = &score->notes[i];
        synth_voice_t *v = &s->v[i];
        const uint32_t release = samples(e->r_ms);
        v->start = samples(e->t_ms);
        v->attack = samples(e->a_ms);
        v->hold = samples(e->d_ms);
        v->len = v->hold + release;
        v->inv_attack = v->attack ? 1.0f / (float)v->attack : 0.0f;
        v->inv_release = release ? 1.0f / (float)release : 0.0f;
        v->k_dec = e->dec_ms ? expf(-1000.0f / ((float)e->dec_ms * (float)SYNTH_RATE)) : 0.0f;
        v->dec = 1.0f;
        v->voices = e->detune > 0.0f ? 3U : 1U;
        const float ratio[3] = {1.0f, 1.0f + e->detune, 1.0f - e->detune};
        for (uint8_t k = 0; k < v->voices; k++) v->inc[k] = e->hz * ratio[k] / (float)SYNTH_RATE;
        if (v->start + v->len > last) last = v->start + v->len;
    }
    if (score->echo_ms > 0 && score->echo_fb > 0.0f) {
        s->echo_len = samples(score->echo_ms);
        if (s->echo_len > SYNTH_ECHO_MAX) s->echo_len = SYNTH_ECHO_MAX;
        if (s->echo_len == 0) s->echo_len = 1;
        last += (uint32_t)ceilf(logf(ECHO_FLOOR) / logf(score->echo_fb)) * s->echo_len;
    }
    s->end = last;
}

uint32_t synth_length(const synth_t *s)
{
    return s->end;
}

/* Échantillon j (depuis le début de la note) de la note e. */
static float note_sample(synth_t *s, synth_voice_t *v, const synth_note_t *e, uint32_t j)
{
    float env;
    if (j < v->attack) {
        env = (float)j * v->inv_attack;
    } else {
        env = e->sustain + (1.0f - e->sustain) * v->dec;
        v->dec *= v->k_dec;
    }
    if (j >= v->hold) {
        const float r = 1.0f - (float)(j - v->hold) * v->inv_release;
        env *= r > 0.0f ? r : 0.0f;
    }

    float x = 0.0f;
    for (uint8_t k = 0; k < v->voices; k++) {
        v->ph[k] += v->inc[k];
        if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f;
        const float p = v->ph[k];
        switch ((synth_wave_t)e->wave) {
        case SYNTH_PULSE: x += (p < e->duty ? 1.0f : -1.0f) - (2.0f * e->duty - 1.0f); break;   /* sans composante continue */
        case SYNTH_SAW:   x += 2.0f * p - 1.0f; break;
        case SYNTH_SINE:  x += sinf(TWO_PI * p); break;
        case SYNTH_NOISE:
            s->noise = s->noise * 1664525U + 1013904223U;
            x += (float)(s->noise >> 8) / 8388608.0f - 1.0f;
            break;
        }
    }
    x /= (float)v->voices;
    if (e->lp > 0.0f) {
        v->lp += e->lp * (x - v->lp);
        x = v->lp;
    }
    return x * env * e->level;
}

size_t synth_render(synth_t *s, int16_t *out, size_t n)
{
    const synth_score_t *sc = s->score;
    size_t k = 0;
    for (; k < n && s->pos < s->end; k++, s->pos++) {
        float x = 0.0f;
        for (uint8_t i = 0; i < s->n_notes; i++) {
            synth_voice_t *v = &s->v[i];
            if (s->pos < v->start || s->pos - v->start >= v->len) continue;
            x += note_sample(s, v, &sc->notes[i], s->pos - v->start);
        }
        if (s->echo_len > 0) {
            float *d = &s->echo[s->echo_pos];
            x += sc->echo_fb * *d;
            *d = x;
            if (++s->echo_pos == s->echo_len) s->echo_pos = 0;
        }
        x *= sc->gain;
        if (x > 1.0f) x = 1.0f;
        else if (x < -1.0f) x = -1.0f;
        out[k] = (int16_t)lrintf(x * 32767.0f);
    }
    return k;
}
