/*
 * Raidmaster 403 — jingles : partitions du synthétiseur (synth.h) et lecture par hal_audio.
 *
 * JINGLE_BOOT, « Grille néon » (choisie le 2026-10-24 sur la page d'écoute
 * docs/mockups/jingles-boot.html) : trois arpèges montants la m · fa · sol en doubles croches de
 * 85 ms (impulsion 25 %) avec un souffle de bruit qui monte, puis à 1,02 s un accord de La majeur
 * en dents de scie désaccordées et une note cristalline ; écho de 170 ms. Environ 2,9 s : il
 * accompagne le tour de la voiture de l'animation de démarrage (3 s).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>

#include "synth.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const synth_score_t JINGLE_BOOT;

/* Joue `score` par hal_audio, sans attendre ; false si un son est en cours ou sans sortie audio. */
bool jingle_play(const synth_score_t *score);

#ifdef __cplusplus
}
#endif
