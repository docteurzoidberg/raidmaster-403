/*
 * Raidmaster 403 — tests unitaires de core/synth et core/jingles (Unity) : durée et niveau d'une
 * note, rendu indépendant de la taille des blocs, forme du jingle de démarrage, lecture par
 * hal_audio (sortie factice ci-dessous).
 *
 * Licence : GPL-3.0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/hal_audio.h"
#include "jingles.h"
#include "synth.h"
#include "unity.h"

#define MAX_SAMPLES (5U * SYNTH_RATE)
#define FULL        32767

static synth_t S;
static int16_t A[MAX_SAMPLES], B[MAX_SAMPLES];

/* hal_audio factice : garde le dernier son lancé. */
static hal_audio_fill_fn s_fill;
static void             *s_user;
static uint32_t          s_rate;
static bool              s_busy;

bool hal_audio_play(uint32_t rate, hal_audio_fill_fn fill, void *user)
{
    if (s_busy) return false;
    s_rate = rate;
    s_fill = fill;
    s_user = user;
    s_busy = true;
    return true;
}

bool hal_audio_busy(void)
{
    return s_busy;
}

void setUp(void)
{
    s_busy = false;
    s_fill = NULL;
}

void tearDown(void) {}

/* Rend toute la partition par blocs de `block` ; renvoie le nombre d'échantillons. */
static size_t render_all(const synth_score_t *score, int16_t *out, size_t block)
{
    synth_init(&S, score);
    size_t total = 0, got;
    do {
        got = synth_render(&S, out + total, block);
        total += got;
    } while (got == block && total + block <= MAX_SAMPLES);
    return total;
}

static int peak(const int16_t *x, size_t n)
{
    int p = 0;
    for (size_t i = 0; i < n; i++)
        if (abs(x[i]) > p) p = abs(x[i]);
    return p;
}

static void test_note_seule(void)
{
    static const synth_note_t note[] = {
        {.t_ms = 10, .d_ms = 100, .wave = SYNTH_SINE, .hz = 1000.0f, .level = 0.5f, .sustain = 1.0f},
    };
    static const synth_score_t score = {.notes = note, .n_notes = 1, .gain = 1.0f};
    const size_t n = render_all(&score, A, 4096);
    TEST_ASSERT_EQUAL_UINT32(SYNTH_RATE * 110U / 1000U, n);
    TEST_ASSERT_EQUAL_UINT32(n, synth_length(&S));
    for (size_t i = 0; i < SYNTH_RATE / 100U; i++) TEST_ASSERT_EQUAL_INT16(0, A[i]);    /* 10 ms de silence */
    TEST_ASSERT_INT_WITHIN(FULL / 100, FULL / 2, peak(A, n));
}

static void test_rendu_independant_des_blocs(void)
{
    const size_t n = render_all(&JINGLE_BOOT, A, MAX_SAMPLES);
    TEST_ASSERT_EQUAL(n, render_all(&JINGLE_BOOT, B, 37));
    TEST_ASSERT_EQUAL_MEMORY(A, B, n * sizeof(int16_t));
}

static void test_jingle_demarrage(void)
{
    const size_t n = render_all(&JINGLE_BOOT, A, 512);
    const int p = peak(A, n);
    size_t first = n, last = 0;
    for (size_t i = 0; i < n; i++) {
        if (abs(A[i]) > p / 50) {
            if (first == n) first = i;
            last = i;
        }
    }
    printf("jingle de démarrage : %.2f s audibles sur %.2f s, crête %d %%\n", (double)last / SYNTH_RATE,
           (double)n / SYNTH_RATE, p * 100 / FULL);
    TEST_ASSERT_TRUE(p > FULL / 2 && p < FULL * 97 / 100);     /* fort, sans écrêtage */
    TEST_ASSERT_LESS_THAN(SYNTH_RATE / 200U, first);             /* le son part tout de suite */
    TEST_ASSERT_TRUE(last > SYNTH_RATE * 26U / 10U && last < SYNTH_RATE * 33U / 10U);   /* le tour de 3 s */
    TEST_ASSERT_LESS_THAN(FULL / 100, peak(A + n - SYNTH_RATE / 10U, SYNTH_RATE / 10U)); /* fin dans le silence */
}

static void test_jingle_par_hal_audio(void)
{
    const size_t n = render_all(&JINGLE_BOOT, A, 512);
    TEST_ASSERT_TRUE(jingle_play(&JINGLE_BOOT));
    TEST_ASSERT_EQUAL_UINT32(SYNTH_RATE, s_rate);
    TEST_ASSERT_FALSE(jingle_play(&JINGLE_BOOT));               /* un son est en cours */
    size_t total = 0, got;
    do {
        got = s_fill(B + total, 256, s_user);
        total += got;
    } while (got == 256);
    TEST_ASSERT_EQUAL(n, total);
    TEST_ASSERT_EQUAL_MEMORY(A, B, n * sizeof(int16_t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_note_seule);
    RUN_TEST(test_rendu_independant_des_blocs);
    RUN_TEST(test_jingle_demarrage);
    RUN_TEST(test_jingle_par_hal_audio);
    return UNITY_END();
}
