/*
 * Raidmaster 403 — application : réunit le décodage NMEA, le moteur de distance, les
 * réglages et leur sauvegarde, et expose les actions des boutons et du menu.
 *
 * C portable, sans LVGL ni matériel : le sim et le firmware l'appellent de la même façon.
 *   app_init()          au démarrage (relit la sauvegarde via hal_storage)
 *   app_gps_line()      pour chaque ligne NMEA (callback de hal_gps)
 *   app_tick()          souvent (sauvegarde différée toutes les 10 s si quelque chose a bougé)
 *   app_get_state()     pour l'affichage
 *   app_<action>()      boutons et menu ; les réglages sont sauvegardés tout de suite
 *
 * Heure affichée : celle de l'horloge de la carte (hal_time_utc_ms), réglée sur l'heure GPS au
 * premier fix de chaque démarrage (pas de synchronisation permanente).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_parser.h"
#include "trip_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SAVE_PERIOD_MS  10000U
#define APP_TZ_STEP_MIN     30
#define APP_ALARM_MIN_KMH   30
#define APP_ALARM_MAX_KMH   130
#define APP_ALARM_STEP_KMH  10
#define APP_TZ_MIN_MIN      (-12 * 60)
#define APP_TZ_MAX_MIN      (14 * 60)

typedef enum { APP_TRIP_A = 0, APP_TRIP_B } app_trip_t;
typedef enum { APP_VIEW_RAID = 0, APP_VIEW_BALADE } app_view_t;

/* Réglages du menu (sauvegardés). La calibration est dans trip_persist_t. */
typedef struct {
    int16_t  tz_min;            /* décalage de l'heure locale sur UTC, pas de 30 min (défaut +60) */
    uint8_t  night;             /* mode nuit */
    uint8_t  view;              /* app_view_t : écran principal */
    uint16_t speed_alarm_kmh;   /* 0 = alarme coupée */
    uint8_t  gpx_log;           /* log GPX sur SD (firmware) */
} app_settings_t;

typedef struct {
    trip_state_t   trip;
    app_settings_t set;
    app_trip_t     active;          /* trip que corrigent +/− */
    bool           speed_alarm;     /* vitesse au-dessus du seuil (seuil actif) */
    int8_t         local_hh;        /* heure locale (horloge de la carte), -1 si pas réglée */
    int8_t         local_mm;
    bool           autocal_running;
    double         autocal_raw_m;
} app_state_t;

typedef struct {
    gps_parser_t   parser;
    trip_engine_t  engine;
    app_settings_t set;
    app_trip_t     active;
    bool           chrono_armed;    /* chrono remis à 0, démarre au premier mouvement */
    bool           clock_synced;    /* horloge de la carte réglée sur le GPS depuis le démarrage */
    bool           dirty;           /* distances à sauvegarder */
    uint64_t       last_save_ms;
    uint64_t       now_ms;
} app_t;

void app_settings_default(app_settings_t *s);

void app_init(app_t *a, uint64_t now_ms);
void app_gps_line(app_t *a, const char *line, uint64_t now_ms);
void app_tick(app_t *a, uint64_t now_ms);
void app_save(app_t *a);
void app_get_state(const app_t *a, uint64_t now_ms, app_state_t *out);

/* Boutons */
void app_reset_trip(app_t *a, app_trip_t t);    /* RAZ A : aussi moyenne, max et chrono */
void app_adjust_active(app_t *a, int steps);    /* ± steps × 10 m sur le trip actif */
void app_toggle_active(app_t *a);

/* Menu */
void app_set_calib(app_t *a, float pct);
void app_set_tz(app_t *a, int tz_min);
void app_set_night(app_t *a, bool on);
void app_set_view(app_t *a, app_view_t v);
void app_set_speed_alarm(app_t *a, uint16_t kmh);
void app_set_gpx_log(app_t *a, bool on);

/* Seuil d'alarme suivant / précédent : OFF, puis 30 à 130 km/h par pas de 10 (couvre les
 * limitations françaises 30/50/70/80/90/110/130 et marocaines 30/60/80/100/120). */
uint16_t app_speed_alarm_step(uint16_t kmh, int dir);

/* Calibration automatique (voir trip_engine.h). apply enregistre le facteur s'il est valide. */
void app_autocal_start(app_t *a);
void app_autocal_stop(app_t *a);
bool app_autocal_apply(app_t *a, double real_m, float *pct);

#ifdef __cplusplus
}
#endif
