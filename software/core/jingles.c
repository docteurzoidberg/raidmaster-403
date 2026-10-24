/*
 * Raidmaster 403 — jingles (voir jingles.h).
 *
 * Licence : GPL-3.0.
 */

#include "jingles.h"

#include "hal/hal_audio.h"

/* Hauteurs (La 440 Hz, gamme tempérée). */
#define A3   220.00f
#define F4   349.23f
#define G4   392.00f
#define A4   440.00f
#define B4   493.88f
#define C5   523.25f
#define CS5  554.37f
#define D5   587.33f
#define E5   659.26f
#define F5   698.46f
#define G5   783.99f
#define A5   880.00f
#define A6  1760.00f

/* Note d'arpège : impulsion 25 %, 65 ms, attaque sèche et petite décroissance. */
#define ARP(t, f) {.t_ms = (t), .d_ms = 65, .a_ms = 3, .dec_ms = 60, .r_ms = 40, .wave = SYNTH_PULSE, \
                   .hz = (f), .level = 0.32f, .sustain = 0.55f, .duty = 0.25f}
/* Note de l'accord final : trois dents de scie désaccordées de ±0,6 %. */
#define CHORD(f) {.t_ms = 1020, .d_ms = 1150, .a_ms = 20, .dec_ms = 350, .r_ms = 600, .wave = SYNTH_SAW, \
                  .hz = (f), .level = 0.16f, .sustain = 0.65f, .detune = 0.006f}

static const synth_note_t s_boot[] = {
    ARP(0, A4),   ARP(85, C5),  ARP(170, E5), ARP(255, A5),     /* la mineur */
    ARP(340, F4), ARP(425, A4), ARP(510, C5), ARP(595, F5),     /* fa */
    ARP(680, G4), ARP(765, B4), ARP(850, D5), ARP(935, G5),     /* sol */
    /* Souffle qui monte pendant les arpèges et se coupe sur l'accord. */
    {.t_ms = 400, .d_ms = 620, .a_ms = 600, .r_ms = 40, .wave = SYNTH_NOISE, .level = 0.10f, .sustain = 1.0f,
     .lp = 0.18f},
    CHORD(A3), CHORD(A4), CHORD(CS5), CHORD(E5), CHORD(A5),     /* La majeur */
    /* Note cristalline à l'ouverture de l'accord. */
    {.t_ms = 1020, .d_ms = 50, .a_ms = 2, .dec_ms = 250, .r_ms = 300, .wave = SYNTH_SINE, .hz = A6, .level = 0.22f},
};

const synth_score_t JINGLE_BOOT = {
    .notes = s_boot,
    .n_notes = (uint8_t)(sizeof(s_boot) / sizeof(s_boot[0])),
    .echo_ms = 170,
    .echo_fb = 0.33f,
    .gain = 1.0f,       /* crête à ~83 % de la pleine échelle (test_synth) */
};

static synth_t s_synth;

static size_t fill(int16_t *buf, size_t n, void *user)
{
    return synth_render((synth_t *)user, buf, n);
}

bool jingle_play(const synth_score_t *score)
{
    if (hal_audio_busy()) return false;     /* s_synth est encore lu par le fil audio */
    synth_init(&s_synth, score);
    return hal_audio_play(SYNTH_RATE, fill, &s_synth);
}
