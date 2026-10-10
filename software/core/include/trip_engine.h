/*
 * Raidmaster 403 — moteur de distance : Trip A / Trip B / odomètre, vitesse, moyenne, max,
 * chrono, calibration (core portable, PC et ESP32).
 *
 * Distance = intégration de la vitesse Doppler du GPS entre deux fixes acceptés
 * (trapèzes sur le temps GPS). La vitesse Doppler n'est pas affectée par la gigue de position,
 * contrairement à la somme des écarts de position : sur une trace bruitée à 3 m, cette somme
 * surestime la distance de 60 % et plus.
 *   - Fix refusé (invalide, HDOP > seuil) : ignoré, l'intervalle suivant l'englobe.
 *   - Vitesse < seuil (2 km/h) : comptée comme 0 (pas de dérive à l'arrêt).
 *   - Trou > max_gap_s (perte de fix, tunnel) : comblé par la distance à vol d'oiseau entre le
 *     dernier fix accepté et le nouveau, si la vitesse implicite est plausible ; sinon rien.
 *   - Fix sans vitesse (GGA seule) : écart de position (haversine) à la place.
 *
 * Calibration : chaque incrément est multiplié par (1 + calib_pct / 100) au moment où il est
 * compté ; changer la calibration n'altère donc pas les distances déjà parcourues.
 *
 * Unités : distances en mètres (double), vitesses en km/h, temps en ms.
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRIP_CALIB_MIN_PCT  (-5.0f)
#define TRIP_CALIB_MAX_PCT  (5.0f)
#define TRIP_ADJUST_STEP_M  10.0     /* correction manuelle ±, bouton +/− */
#define TRIP_SPEED_WINDOW   16       /* échantillons gardés pour le lissage de vitesse */

typedef struct {
    float    min_speed_kmh;   /* en dessous : vitesse et distance nulles (défaut 2) */
    float    max_hdop;        /* au-dessus : fix refusé (défaut 2) */
    float    max_gap_s;       /* au-delà : trou comblé à vol d'oiseau (défaut 3) */
    float    max_bridge_kmh;  /* vitesse implicite max pour combler un trou (défaut 200) */
    uint32_t speed_avg_ms;    /* fenêtre de lissage de la vitesse affichée (défaut 500) */
    uint32_t fix_timeout_ms;  /* sans fix accepté depuis : fix perdu (défaut 2000) */
} trip_config_t;

/* Valeurs à persister (NVS / sim-storage.json). */
typedef struct {
    double trip_a_m;
    double trip_b_m;
    double odo_m;
    float  calib_pct;
} trip_persist_t;

/* État publié vers l'UI. */
typedef struct {
    double   trip_a_m, trip_b_m, odo_m;
    float    speed_kmh;       /* lissée, 0 sous le seuil ou sans fix */
    float    avg_kmh;         /* moyenne en mouvement depuis la RAZ Trip A */
    float    max_kmh;         /* max depuis la RAZ Trip A */
    float    course_deg;      /* cap GPS (route), NAN à l'arrêt ou sans fix */
    uint32_t moving_s;        /* temps en mouvement depuis la RAZ Trip A */
    uint32_t chrono_ms;
    bool     chrono_running;
    bool     fix_ok;          /* fix accepté reçu depuis moins de fix_timeout_ms */
    uint8_t  sats;
    float    hdop;
    int64_t  t_utc_ms;        /* heure GPS du dernier fix, -1 si inconnue */
    float    calib_pct;
} trip_state_t;

typedef struct {
    trip_config_t  cfg;
    trip_persist_t p;

    /* dernier fix accepté */
    bool    have_prev;
    int64_t prev_t_ms;
    double  prev_lat, prev_lon;
    float   prev_speed_kmh;     /* NAN si inconnue */

    /* vitesse lissée */
    int64_t sw_t[TRIP_SPEED_WINDOW];
    float   sw_v[TRIP_SPEED_WINDOW];
    int     sw_n, sw_head;

    /* segment depuis la RAZ Trip A */
    double  seg_moving_ms;
    float   seg_max_kmh;

    /* chrono (horloge monotone de l'appelant) */
    bool     chrono_running;
    uint64_t chrono_start_ms;
    uint64_t chrono_acc_ms;

    /* calibration automatique */
    bool    autocal_running;
    double  autocal_raw_m;      /* distance brute (non calibrée) depuis le début */

    /* dernier fix reçu (pour l'état publié) */
    uint64_t last_ok_mono_ms;
    bool     ever_ok;
    float    course_deg;
    uint8_t  sats;
    float    hdop;
    int64_t  t_utc_ms;
} trip_engine_t;

void trip_config_default(trip_config_t *cfg);

/* cfg NULL → valeurs par défaut ; persist NULL → tout à zéro. */
void trip_engine_init(trip_engine_t *e, const trip_config_t *cfg, const trip_persist_t *persist);

/* Donne un fix publié par gps_parser. mono_ms : horloge monotone de l'appelant (hal_time). */
void trip_engine_feed(trip_engine_t *e, const gps_fix_t *fix, uint64_t mono_ms);

void trip_engine_get_state(const trip_engine_t *e, uint64_t mono_ms, trip_state_t *out);
void trip_engine_get_persist(const trip_engine_t *e, trip_persist_t *out);

/* Boutons */
void trip_engine_reset_a(trip_engine_t *e);      /* RAZ Trip A + moyenne, max, temps en mouvement */
void trip_engine_reset_b(trip_engine_t *e);
void trip_engine_adjust_a(trip_engine_t *e, double delta_m);   /* ±10 m ; borné à 0 */
void trip_engine_adjust_b(trip_engine_t *e, double delta_m);
void trip_engine_reset_odo(trip_engine_t *e);    /* menu caché uniquement */

/* Calibration manuelle, bornée à [TRIP_CALIB_MIN_PCT, TRIP_CALIB_MAX_PCT]. */
void  trip_engine_set_calib(trip_engine_t *e, float pct);
float trip_engine_calib(const trip_engine_t *e);

/* Calibration automatique sur distance connue : start au repère de départ (distance brute
 * remise à 0), stop à l'arrivée (la distance brute est figée, lisible), puis finish avec la
 * distance réelle. finish renvoie false (calibration inchangée) si la distance brute est trop
 * courte (< 100 m) ou si le facteur sort des bornes ; *pct reçoit le facteur calculé. */
void   trip_engine_autocal_start(trip_engine_t *e);
void   trip_engine_autocal_stop(trip_engine_t *e);
bool   trip_engine_autocal_running(const trip_engine_t *e);
double trip_engine_autocal_raw_m(const trip_engine_t *e);
bool   trip_engine_autocal_finish(trip_engine_t *e, double real_m, float *pct);

/* Chrono d'étape (horloge monotone de l'appelant). */
void trip_engine_chrono_start(trip_engine_t *e, uint64_t mono_ms);
void trip_engine_chrono_stop(trip_engine_t *e, uint64_t mono_ms);
void trip_engine_chrono_reset(trip_engine_t *e);

/* Distance à vol d'oiseau (m), sphère de rayon moyen WGS84. Exposé pour les tests. */
double trip_haversine_m(double lat1, double lon1, double lat2, double lon2);

#ifdef __cplusplus
}
#endif
