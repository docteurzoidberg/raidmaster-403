/*
 * Raidmaster 403 — petit synthétiseur : une partition (table de notes) rendue en PCM 16 bits mono
 * à 22 050 Hz, par blocs, sans fichier audio.
 *
 * Chaque note a sa forme d'onde (impulsion à rapport cyclique réglable, dents de scie, sinus,
 * bruit), son enveloppe (montée linéaire, décroissance exponentielle vers le niveau tenu,
 * relâchement linéaire) et, en option, deux voix désaccordées de part et d'autre ; un écho
 * (retard réinjecté) s'applique au mélange.
 *
 * C portable, sans matériel ni allocation : le rendu se fait dans le fil audio (tâche ESP32, rappel
 * SDL du sim) bloc par bloc, le résultat ne dépend pas de la taille des blocs.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTH_RATE        22050U
#define SYNTH_MAX_NOTES   32U
#define SYNTH_ECHO_MAX    4096U     /* retard de l'écho, en échantillons (186 ms) */

typedef enum {
    SYNTH_PULSE,        /* carré, rapport cyclique `duty` */
    SYNTH_SAW,
    SYNTH_SINE,
    SYNTH_NOISE,        /* bruit blanc, passe-bas `lp` en option */
} synth_wave_t;

typedef struct {
    uint16_t t_ms;      /* début, depuis le départ de la partition */
    uint16_t d_ms;      /* durée avant le relâchement */
    uint16_t a_ms;      /* montée */
    uint16_t dec_ms;    /* constante de temps de la décroissance vers `sustain` */
    uint16_t r_ms;      /* relâchement (descente linéaire jusqu'à 0) */
    uint8_t  wave;      /* synth_wave_t */
    float    hz;        /* hauteur, ignorée pour le bruit */
    float    level;     /* niveau de crête, 0 à 1 */
    float    sustain;   /* niveau tenu, en fraction de `level` */
    float    duty;      /* SYNTH_PULSE : part haute de la période */
    float    detune;    /* > 0 : trois voix à hz × (1, 1 + detune, 1 − detune) */
    float    lp;        /* SYNTH_NOISE : coefficient du passe-bas à un pôle (0 = sans) */
} synth_note_t;

typedef struct {
    const synth_note_t *notes;
    uint8_t  n_notes;   /* au plus SYNTH_MAX_NOTES */
    uint16_t echo_ms;   /* 0 : sans écho */
    float    echo_fb;   /* réinjection de l'écho, 0 à 0,9 */
    float    gain;      /* gain du mélange avant écrêtage à la pleine échelle */
} synth_score_t;

typedef struct {
    uint32_t start, attack, hold, len;  /* en échantillons : début, montée, fin de la tenue, durée totale */
    float    inv_attack, inv_release, k_dec;
    float    inc[3], ph[3];
    float    dec;                       /* part décroissante de l'enveloppe */
    float    lp;                        /* état du passe-bas */
    uint8_t  voices;
} synth_voice_t;

typedef struct {
    const synth_score_t *score;
    uint8_t  n_notes;
    uint32_t pos, end;                  /* échantillons rendus, fin de la partition écho compris */
    uint32_t noise;                     /* état du générateur de bruit */
    synth_voice_t v[SYNTH_MAX_NOTES];
    float    echo[SYNTH_ECHO_MAX];
    uint32_t echo_len, echo_pos;
} synth_t;

/* Prépare la lecture de `score` depuis le début. */
void synth_init(synth_t *s, const synth_score_t *score);

/* Durée totale de la partition, écho compris, en échantillons. */
uint32_t synth_length(const synth_t *s);

/* Rend jusqu'à n échantillons dans out ; renvoie le nombre écrit, < n à la fin de la partition. */
size_t synth_render(synth_t *s, int16_t *out, size_t n);

#ifdef __cplusplus
}
#endif
