/*
 * Raidmaster 403 — application : réunit le décodage NMEA, le moteur de distance, les
 * réglages et leur sauvegarde, et expose les actions des boutons et du menu.
 *
 * C portable, sans LVGL ni matériel : le sim et le firmware l'appellent de la même façon.
 *   app_init()          au démarrage (relit la sauvegarde via hal_storage)
 *   app_gps_line()      pour chaque ligne NMEA (callback de hal_gps)
 *   app_mag_sample()    pour chaque mesure de la boussole (hal_mag_poll)
 *   app_tick()          souvent (sauvegarde différée toutes les 10 s si quelque chose a bougé)
 *   app_get_state()     pour l'affichage
 *   app_<action>()      boutons et menu ; les réglages sont sauvegardés tout de suite
 *
 * Heure affichée : celle de l'horloge de la carte (hal_time_utc_ms), réglée sur l'heure GPS au
 * premier fix de chaque démarrage (pas de synchronisation permanente).
 *
 * Cap affiché : boussole calibrée à l'arrêt et à basse vitesse (sous 13 km/h, retour au-dessus de
 * 17 km/h), route GPS au-delà ; chaque source sert de repli à l'autre. Boussole non calibrée :
 * jamais utilisée (son cap peut être faux de plusieurs dizaines de degrés dans une voiture).
 * Déclinaison magnétique négligée (±2° en France et au Maroc).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_parser.h"
#include "mag.h"
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
typedef enum { APP_HEAD_NONE = 0, APP_HEAD_GPS, APP_HEAD_MAG } app_heading_src_t;

#define APP_HEAD_MAG_BELOW_KMH  13.0f   /* boussole sous cette vitesse… */
#define APP_HEAD_GPS_ABOVE_KMH  17.0f   /* …route GPS au-dessus de celle-ci (hystérésis) */
#define APP_MAG_STALE_MS        1000U   /* boussole muette au-delà */
#define APP_MAGCAL_SECTORS      36      /* tour de calibration : secteurs de 10° */
#define APP_MAGCAL_MIN_SECTORS  32      /* secteurs à parcourir pour valider */
#define APP_MAGCAL_MIN_SPAN_UT  20.0f   /* étendue minimale du champ en X et Y */
#define APP_MAGCAL_POINTS       256     /* points du tour gardés (espacés de 1,5 µT au moins) */

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
    bool           gps_present;     /* récepteur présent (hal_gps_present) */
    bool           mag_present;     /* boussole présente (mesure de moins d'une seconde) */
    bool           mag_calibrated;
    app_heading_src_t heading_src;
    float          heading_deg;     /* cap affiché 0..360, NAN si aucune source */
    bool           magcal_running;  /* tour de calibration de la boussole en cours */
    uint8_t        magcal_sectors;  /* secteurs de 10° parcourus (0..APP_MAGCAL_SECTORS) */
    uint64_t       magcal_mask;     /* bit i : secteur [10 i, 10 i + 10[ du cap du champ parcouru */
    bool           magcal_ready;    /* tour suffisant : app_magcal_apply possible */
} app_state_t;

/* Boussole : calibration, dernière mesure lissée, tour de calibration. */
typedef struct {
    mag_cal_t      cal;
    bool           calibrated;
    bool           have;            /* au moins une mesure */
    uint64_t       t_ms;            /* dernière mesure */
    float          hx, hy;          /* composantes horizontales calibrées, lissées */
    bool           cal_running;
    mag_cal_acc_t  acc;
    float          px[APP_MAGCAL_POINTS], py[APP_MAGCAL_POINTS];   /* champ horizontal brut */
    uint16_t       n_points;
} app_mag_t;

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
    app_mag_t      mag;
    app_heading_src_t head_src;     /* source du cap retenue (hystérésis) */
} app_t;

void app_settings_default(app_settings_t *s);

void app_init(app_t *a, uint64_t now_ms);
void app_gps_line(app_t *a, const char *line, uint64_t now_ms);
void app_mag_sample(app_t *a, mag_vec_t raw, uint64_t now_ms);
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

/* Calibration de la boussole : un tour complet sur place, lentement. apply enregistre la
 * calibration si le tour est suffisant (magcal_ready) et renvoie false sinon. */
void app_magcal_start(app_t *a);
void app_magcal_stop(app_t *a);
bool app_magcal_apply(app_t *a);

#ifdef __cplusplus
}
#endif
