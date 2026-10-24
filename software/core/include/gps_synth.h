/*
 * Raidmaster 403 — trajet simulé : phrases NMEA d'un récepteur fictif.
 *
 * Modèle cinématique déterministe à 10 Hz, boucle de 120 s (accélération, route qui ondule entre
 * 45 et 95 km/h, virages, freinage, 10 s d'arrêt), départ de Paris. Signal coupé : phrases
 * « sans fix » comme un récepteur u-blox (RMC V, GGA 0).
 *
 * C portable, sans matériel : trajet par défaut du sim (hal_sim) et GPS de démonstration du
 * firmware tant qu'aucun récepteur n'est branché (hal_esp32).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPS_SYNTH_SAMPLE_MS 100U

typedef void (*gps_synth_line_cb)(const char *line, size_t len, void *user);

typedef struct {
    double   lat, lon, head;    /* position et cap courants */
    int64_t  utc0_ms;           /* heure UTC du départ (ms depuis 1970) */
    uint64_t next_ms;           /* temps simulé du prochain échantillon */
} gps_synth_t;

/* Départ à l'heure utc0_ms, arrondie au pas d'échantillonnage. */
void gps_synth_init(gps_synth_t *s, int64_t utc0_ms);

/* Émet (RMC puis GGA) tous les échantillons échus jusqu'au temps simulé t_ms depuis le
 * départ ; après une longue pause, pas plus de 10 s de rattrapage. */
void gps_synth_run(gps_synth_t *s, uint64_t t_ms, bool signal, gps_synth_line_cb cb, void *user);

#ifdef __cplusplus
}
#endif
