/*
 * Raidmaster 403 — moteur de distance. Voir core/include/trip_engine.h.
 *
 * Licence : GPL-3.0.
 */

#include "trip_engine.h"

#include <math.h>
#include <string.h>

#define EARTH_R_M   6371008.8      /* rayon moyen WGS84 (identique à tests/tools/compute_reference.py) */
#define DEG2RAD     (3.14159265358979323846 / 180.0)
#define AUTOCAL_MIN_M 100.0

void trip_config_default(trip_config_t *cfg)
{
    cfg->min_speed_kmh = 2.0f;
    cfg->max_hdop = 2.0f;
    cfg->max_gap_s = 3.0f;
    cfg->max_bridge_kmh = 200.0f;
    cfg->speed_avg_ms = 500;
    cfg->fix_timeout_ms = 2000;
}

double trip_haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * DEG2RAD, p2 = lat2 * DEG2RAD;
    const double dp = (lat2 - lat1) * DEG2RAD, dl = (lon2 - lon1) * DEG2RAD;
    const double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    return 2.0 * EARTH_R_M * asin(sqrt(a < 1.0 ? a : 1.0));
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void trip_engine_init(trip_engine_t *e, const trip_config_t *cfg, const trip_persist_t *persist)
{
    memset(e, 0, sizeof(*e));
    if (cfg)
        e->cfg = *cfg;
    else
        trip_config_default(&e->cfg);
    if (persist) {
        e->p = *persist;
        e->p.calib_pct = clampf(e->p.calib_pct, TRIP_CALIB_MIN_PCT, TRIP_CALIB_MAX_PCT);
    }
    e->prev_speed_kmh = NAN;
    e->course_deg = NAN;
    e->hdop = NAN;
    e->t_utc_ms = -1;
}

/* Ajoute une distance brute (non calibrée) aux compteurs. */
static void add_distance(trip_engine_t *e, double raw_m)
{
    if (!(raw_m > 0.0))
        return;
    const double m = raw_m * (1.0 + (double)e->p.calib_pct / 100.0);
    e->p.trip_a_m += m;
    e->p.trip_b_m += m;
    e->p.odo_m += m;
    if (e->autocal_running)
        e->autocal_raw_m += raw_m;
}

static void speed_window_push(trip_engine_t *e, int64_t t_ms, float v)
{
    e->sw_t[e->sw_head] = t_ms;
    e->sw_v[e->sw_head] = v;
    e->sw_head = (e->sw_head + 1) % TRIP_SPEED_WINDOW;
    if (e->sw_n < TRIP_SPEED_WINDOW)
        e->sw_n++;
}

/* Moyenne des vitesses de la fenêtre [t_last - speed_avg_ms, t_last]. */
static float speed_window_avg(const trip_engine_t *e)
{
    if (e->sw_n == 0)
        return 0.0f;
    const int last = (e->sw_head + TRIP_SPEED_WINDOW - 1) % TRIP_SPEED_WINDOW;
    const int64_t t_last = e->sw_t[last];
    float sum = 0.0f;
    int n = 0;
    for (int k = 0; k < e->sw_n; k++) {
        const int i = (last - k + TRIP_SPEED_WINDOW) % TRIP_SPEED_WINDOW;
        if (t_last - e->sw_t[i] > (int64_t)e->cfg.speed_avg_ms)
            break;
        sum += e->sw_v[i];
        n++;
    }
    return n ? sum / (float)n : 0.0f;
}

static bool fix_accepted(const trip_engine_t *e, const gps_fix_t *f)
{
    if (!f->valid || f->t_utc_ms < 0 || isnan(f->lat_deg) || isnan(f->lon_deg))
        return false;
    if (!isnan(f->hdop) && f->hdop > e->cfg.max_hdop)
        return false;
    return true;
}

void trip_engine_feed(trip_engine_t *e, const gps_fix_t *f, uint64_t mono_ms)
{
    if (!fix_accepted(e, f))
        return;

    float v = f->speed_kmh;                 /* NAN si GGA seule */
    if (!isnan(v) && v < e->cfg.min_speed_kmh)
        v = 0.0f;

    if (e->have_prev) {
        const double dt_s = (double)(f->t_utc_ms - e->prev_t_ms) / 1000.0;
        if (dt_s == 0.0) {
            return;                         /* fix en double : ignoré */
        } else if (dt_s < 0.0) {
            /* Retour arrière de l'heure GPS (rejeu en boucle, horloge du récepteur réinitialisée) :
             * on repart de ce point, sans distance. */
        } else if (dt_s > e->cfg.max_gap_s) {
            /* Trou : à vol d'oiseau si plausible. */
            const double d = trip_haversine_m(e->prev_lat, e->prev_lon, f->lat_deg, f->lon_deg);
            if (d / dt_s * 3.6 <= e->cfg.max_bridge_kmh) {
                add_distance(e, d);
                if (d / dt_s * 3.6 >= e->cfg.min_speed_kmh)
                    e->seg_moving_ms += dt_s * 1000.0;
            }
        } else if (!isnan(v) && !isnan(e->prev_speed_kmh)) {
            /* Cas normal : trapèze sur la vitesse Doppler. */
            const double d = ((double)v + (double)e->prev_speed_kmh) / 2.0 / 3.6 * dt_s;
            add_distance(e, d);
            if (v > 0.0f || e->prev_speed_kmh > 0.0f)
                e->seg_moving_ms += dt_s * 1000.0;
        } else {
            /* Vitesse inconnue d'un côté : écart de position, sauf à l'arrêt. */
            const double d = trip_haversine_m(e->prev_lat, e->prev_lon, f->lat_deg, f->lon_deg);
            if (d / dt_s * 3.6 >= e->cfg.min_speed_kmh && d / dt_s * 3.6 <= e->cfg.max_bridge_kmh) {
                add_distance(e, d);
                e->seg_moving_ms += dt_s * 1000.0;
            }
        }
    }

    e->have_prev = true;
    e->prev_t_ms = f->t_utc_ms;
    e->prev_lat = f->lat_deg;
    e->prev_lon = f->lon_deg;
    e->prev_speed_kmh = v;

    if (!isnan(v)) {
        speed_window_push(e, f->t_utc_ms, v);
        if (v > e->seg_max_kmh)
            e->seg_max_kmh = v;
    }
    e->course_deg = (!isnan(v) && v > 0.0f) ? f->course_deg : NAN;
    e->sats = f->sats;
    e->hdop = f->hdop;
    e->t_utc_ms = f->t_utc_ms;
    e->last_ok_mono_ms = mono_ms;
    e->ever_ok = true;
}

void trip_engine_get_state(const trip_engine_t *e, uint64_t mono_ms, trip_state_t *out)
{
    memset(out, 0, sizeof(*out));
    out->trip_a_m = e->p.trip_a_m;
    out->trip_b_m = e->p.trip_b_m;
    out->odo_m = e->p.odo_m;
    out->calib_pct = e->p.calib_pct;
    out->fix_ok = e->ever_ok && mono_ms - e->last_ok_mono_ms <= e->cfg.fix_timeout_ms;
    out->speed_kmh = out->fix_ok ? speed_window_avg(e) : 0.0f;
    out->course_deg = out->fix_ok ? e->course_deg : NAN;
    out->max_kmh = e->seg_max_kmh;
    out->moving_s = (uint32_t)(e->seg_moving_ms / 1000.0);
    out->avg_kmh = e->seg_moving_ms >= 1000.0
                       ? (float)(e->p.trip_a_m / (e->seg_moving_ms / 1000.0) * 3.6)
                       : 0.0f;
    out->chrono_running = e->chrono_running;
    out->chrono_ms = (uint32_t)(e->chrono_acc_ms +
                                (e->chrono_running ? mono_ms - e->chrono_start_ms : 0U));
    out->sats = e->sats;
    out->hdop = e->hdop;
    out->t_utc_ms = e->t_utc_ms;
}

void trip_engine_get_persist(const trip_engine_t *e, trip_persist_t *out)
{
    *out = e->p;
}

void trip_engine_reset_a(trip_engine_t *e)
{
    e->p.trip_a_m = 0.0;
    e->seg_moving_ms = 0.0;
    e->seg_max_kmh = 0.0f;
}

void trip_engine_reset_b(trip_engine_t *e)
{
    e->p.trip_b_m = 0.0;
}

void trip_engine_adjust_a(trip_engine_t *e, double delta_m)
{
    e->p.trip_a_m += delta_m;
    if (e->p.trip_a_m < 0.0)
        e->p.trip_a_m = 0.0;
}

void trip_engine_adjust_b(trip_engine_t *e, double delta_m)
{
    e->p.trip_b_m += delta_m;
    if (e->p.trip_b_m < 0.0)
        e->p.trip_b_m = 0.0;
}

void trip_engine_reset_odo(trip_engine_t *e)
{
    e->p.odo_m = 0.0;
}

void trip_engine_set_calib(trip_engine_t *e, float pct)
{
    /* arrondi au pas de 0,1 % */
    pct = roundf(pct * 10.0f) / 10.0f;
    e->p.calib_pct = clampf(pct, TRIP_CALIB_MIN_PCT, TRIP_CALIB_MAX_PCT);
}

float trip_engine_calib(const trip_engine_t *e)
{
    return e->p.calib_pct;
}

void trip_engine_autocal_start(trip_engine_t *e)
{
    e->autocal_running = true;
    e->autocal_raw_m = 0.0;
}

bool trip_engine_autocal_running(const trip_engine_t *e)
{
    return e->autocal_running;
}

double trip_engine_autocal_raw_m(const trip_engine_t *e)
{
    return e->autocal_raw_m;
}

bool trip_engine_autocal_finish(trip_engine_t *e, double real_m, float *pct)
{
    e->autocal_running = false;
    if (e->autocal_raw_m < AUTOCAL_MIN_M || !(real_m > 0.0)) {
        if (pct)
            *pct = NAN;
        return false;
    }
    const float p = (float)((real_m / e->autocal_raw_m - 1.0) * 100.0);
    if (pct)
        *pct = p;
    if (p < TRIP_CALIB_MIN_PCT || p > TRIP_CALIB_MAX_PCT)
        return false;
    trip_engine_set_calib(e, p);
    return true;
}

void trip_engine_autocal_stop(trip_engine_t *e)
{
    e->autocal_running = false;
}

void trip_engine_chrono_start(trip_engine_t *e, uint64_t mono_ms)
{
    if (e->chrono_running)
        return;
    e->chrono_running = true;
    e->chrono_start_ms = mono_ms;
}

void trip_engine_chrono_stop(trip_engine_t *e, uint64_t mono_ms)
{
    if (!e->chrono_running)
        return;
    e->chrono_acc_ms += mono_ms - e->chrono_start_ms;
    e->chrono_running = false;
}

void trip_engine_chrono_reset(trip_engine_t *e)
{
    e->chrono_running = false;
    e->chrono_acc_ms = 0;
}
