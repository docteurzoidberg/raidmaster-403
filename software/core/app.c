/*
 * Raidmaster 403 — application. Voir core/include/app.h.
 *
 * Licence : GPL-3.0.
 */

#include "app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_storage.h"

#define STORE_KEY   "state"
#define STORE_MAGIC 0x524D0001u     /* « RM », version 1 : changer à chaque évolution du format */

/* Bloc sauvegardé. Format interne (même compilateur, même cible) : une version différente ou
 * une taille inattendue fait repartir des valeurs par défaut. */
typedef struct {
    uint32_t       magic;
    trip_persist_t trip;
    app_settings_t set;
    uint8_t        active;
} store_t;


void app_settings_default(app_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->tz_min = 60;
    s->view = APP_VIEW_RAID;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void settings_sanitize(app_settings_t *s)
{
    int tz = clampi(s->tz_min, APP_TZ_MIN_MIN, APP_TZ_MAX_MIN);
    s->tz_min = (int16_t)(tz / APP_TZ_STEP_MIN * APP_TZ_STEP_MIN);
    s->night = s->night ? 1 : 0;
    s->view = s->view == APP_VIEW_BALADE ? APP_VIEW_BALADE : APP_VIEW_RAID;
    s->gpx_log = s->gpx_log ? 1 : 0;
    const unsigned k = s->speed_alarm_kmh;
    if (k < APP_ALARM_MIN_KMH || k > APP_ALARM_MAX_KMH || k % APP_ALARM_STEP_KMH != 0)
        s->speed_alarm_kmh = 0;
}

void app_init(app_t *a, uint64_t now_ms)
{
    memset(a, 0, sizeof(*a));
    gps_parser_init(&a->parser);
    app_settings_default(&a->set);

    store_t st;
    size_t len = sizeof(st);
    const trip_persist_t *persist = NULL;
    if (hal_storage_get(STORE_KEY, &st, &len) == 0) {
        if (len == sizeof(st) && st.magic == STORE_MAGIC && isfinite(st.trip.trip_a_m) &&
            isfinite(st.trip.trip_b_m) && isfinite(st.trip.odo_m) && isfinite(st.trip.calib_pct)) {
            persist = &st.trip;
            a->set = st.set;
            settings_sanitize(&a->set);
            a->active = st.active == APP_TRIP_B ? APP_TRIP_B : APP_TRIP_A;
        } else {
            fprintf(stderr, "app : sauvegarde illisible (format changé ?), valeurs par défaut\n");
        }
    }
    trip_engine_init(&a->engine, NULL, persist);
    a->chrono_armed = true;
    a->now_ms = now_ms;
    a->last_save_ms = now_ms;
}

void app_save(app_t *a)
{
    store_t st;
    memset(&st, 0, sizeof(st));       /* octets de bourrage déterministes */
    st.magic = STORE_MAGIC;
    trip_engine_get_persist(&a->engine, &st.trip);
    st.set = a->set;
    st.active = (uint8_t)a->active;
    if (hal_storage_set(STORE_KEY, &st, sizeof(st)) != 0)
        fprintf(stderr, "app : échec de la sauvegarde\n");
    a->dirty = false;
    a->last_save_ms = a->now_ms;
}

void app_gps_line(app_t *a, const char *line, uint64_t now_ms)
{
    gps_fix_t fix;
    a->now_ms = now_ms;
    if (!gps_parser_feed(&a->parser, line, &fix))
        return;
    const double odo_before = a->engine.p.odo_m;
    trip_engine_feed(&a->engine, &fix, now_ms);
    if (a->engine.p.odo_m != odo_before)
        a->dirty = true;

    /* Chrono d'étape : démarre au premier mouvement après une RAZ de Trip A. */
    if (a->chrono_armed && fix.valid && !isnan(fix.speed_kmh) &&
        fix.speed_kmh >= a->engine.cfg.min_speed_kmh) {
        trip_engine_chrono_start(&a->engine, now_ms);
        a->chrono_armed = false;
    }
}

void app_tick(app_t *a, uint64_t now_ms)
{
    a->now_ms = now_ms;
    if (a->dirty && now_ms - a->last_save_ms >= APP_SAVE_PERIOD_MS)
        app_save(a);
}

void app_get_state(const app_t *a, uint64_t now_ms, app_state_t *out)
{
    memset(out, 0, sizeof(*out));
    trip_engine_get_state(&a->engine, now_ms, &out->trip);
    out->set = a->set;
    out->active = a->active;
    out->speed_alarm = a->set.speed_alarm_kmh > 0 && out->trip.fix_ok &&
                       out->trip.speed_kmh > (float)a->set.speed_alarm_kmh;
    out->local_hh = out->local_mm = -1;
    if (out->trip.t_utc_ms >= 0) {
        int64_t m = (out->trip.t_utc_ms / 60000 + a->set.tz_min) % (24 * 60);
        if (m < 0)
            m += 24 * 60;
        out->local_hh = (int8_t)(m / 60);
        out->local_mm = (int8_t)(m % 60);
    }
    out->autocal_running = trip_engine_autocal_running(&a->engine);
    out->autocal_raw_m = trip_engine_autocal_raw_m(&a->engine);
}

void app_reset_trip(app_t *a, app_trip_t t)
{
    if (t == APP_TRIP_A) {
        trip_engine_reset_a(&a->engine);
        trip_engine_chrono_reset(&a->engine);
        a->chrono_armed = true;
    } else {
        trip_engine_reset_b(&a->engine);
    }
    app_save(a);
}

void app_adjust_active(app_t *a, int steps)
{
    const double d = steps * TRIP_ADJUST_STEP_M;
    if (a->active == APP_TRIP_A)
        trip_engine_adjust_a(&a->engine, d);
    else
        trip_engine_adjust_b(&a->engine, d);
    a->dirty = true;
}

void app_toggle_active(app_t *a)
{
    a->active = a->active == APP_TRIP_A ? APP_TRIP_B : APP_TRIP_A;
    app_save(a);
}

void app_set_calib(app_t *a, float pct)
{
    trip_engine_set_calib(&a->engine, pct);
    app_save(a);
}

void app_set_tz(app_t *a, int tz_min)
{
    a->set.tz_min = (int16_t)tz_min;
    settings_sanitize(&a->set);
    app_save(a);
}

void app_set_night(app_t *a, bool on)
{
    a->set.night = on ? 1 : 0;
    app_save(a);
}

void app_set_view(app_t *a, app_view_t v)
{
    a->set.view = (uint8_t)v;
    settings_sanitize(&a->set);
    app_save(a);
}

void app_set_speed_alarm(app_t *a, uint16_t kmh)
{
    a->set.speed_alarm_kmh = kmh;
    settings_sanitize(&a->set);
    app_save(a);
}

void app_set_gpx_log(app_t *a, bool on)
{
    a->set.gpx_log = on ? 1 : 0;
    app_save(a);
}

uint16_t app_speed_alarm_step(uint16_t kmh, int dir)
{
    int k = kmh < APP_ALARM_MIN_KMH || kmh > APP_ALARM_MAX_KMH ? 0 : kmh / APP_ALARM_STEP_KMH * APP_ALARM_STEP_KMH;
    if (dir > 0)
        k = k == 0 ? APP_ALARM_MIN_KMH : clampi(k + APP_ALARM_STEP_KMH, 0, APP_ALARM_MAX_KMH);
    else if (dir < 0)
        k = k <= APP_ALARM_MIN_KMH ? 0 : k - APP_ALARM_STEP_KMH;
    return (uint16_t)k;
}

void app_autocal_start(app_t *a)
{
    trip_engine_autocal_start(&a->engine);
}

void app_autocal_stop(app_t *a)
{
    trip_engine_autocal_stop(&a->engine);
}

bool app_autocal_apply(app_t *a, double real_m, float *pct)
{
    const bool ok = trip_engine_autocal_finish(&a->engine, real_m, pct);
    if (ok)
        app_save(a);
    return ok;
}
