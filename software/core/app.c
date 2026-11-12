/*
 * Raidmaster 403 — application. Voir core/include/app.h.
 *
 * Licence : GPL-3.0.
 */

#include "app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_gps.h"
#include "hal/hal_storage.h"
#include "hal/hal_time.h"

#define STORE_KEY   "state"
#define STORE_MAGIC 0x524D0001u     /* « RM », version 1 : changer à chaque évolution du format */
#define MAGCAL_KEY   "magcal"
#define MAGCAL_MAGIC 0x4D430001u    /* « MC », version 1 */
#define MAG_SMOOTH   0.25f          /* lissage exponentiel du champ horizontal (20 Hz : ~0,2 s) */

/* Bloc sauvegardé. Format interne (même compilateur, même cible) : une version différente ou
 * une taille inattendue fait repartir des valeurs par défaut. */
typedef struct {
    uint32_t  magic;
    mag_cal_t cal;
} magcal_store_t;

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

    mag_cal_identity(&a->mag.cal);
    magcal_store_t mc;
    len = sizeof(mc);
    if (hal_storage_get(MAGCAL_KEY, &mc, &len) == 0 && len == sizeof(mc) && mc.magic == MAGCAL_MAGIC &&
        isfinite(mc.cal.offset.x) && isfinite(mc.cal.offset.y) && isfinite(mc.cal.offset.z) &&
        mc.cal.scale.x > 0.0f && mc.cal.scale.y > 0.0f && mc.cal.scale.z > 0.0f) {
        a->mag.cal = mc.cal;
        a->mag.calibrated = true;
    }
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
    /* Premier fix depuis le démarrage : l'horloge de la carte prend l'heure GPS, une fois. */
    if (!a->clock_synced && fix.valid && fix.t_utc_ms >= 0) {
        hal_time_set_utc_ms(fix.t_utc_ms);
        a->clock_synced = true;
    }
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

/* ------------------------------------------------------------------ cap */

static bool mag_fresh(const app_t *a, uint64_t now_ms)
{
    return a->mag.have && now_ms - a->mag.t_ms < APP_MAG_STALE_MS;
}

static float mag_heading(const app_t *a)
{
    return mag_heading_deg((mag_vec_t){a->mag.hx, a->mag.hy, 0.0f});
}

/* Source du cap : boussole calibrée à basse vitesse, route GPS au-dessus, avec hystérésis. */
static void update_heading_src(app_t *a, uint64_t now_ms)
{
    trip_state_t t;
    trip_engine_get_state(&a->engine, now_ms, &t);
    const bool mag_ok = a->mag.calibrated && mag_fresh(a, now_ms);
    const bool gps_ok = t.fix_ok && !isnan(t.course_deg);
    const float v = t.fix_ok && !isnan(t.speed_kmh) ? t.speed_kmh : 0.0f;
    app_heading_src_t want;
    if (mag_ok && gps_ok) {
        if (v >= APP_HEAD_GPS_ABOVE_KMH)      want = APP_HEAD_GPS;
        else if (v < APP_HEAD_MAG_BELOW_KMH)  want = APP_HEAD_MAG;
        else want = a->head_src == APP_HEAD_NONE ? APP_HEAD_MAG : a->head_src;   /* entre les deux : on garde */
    } else if (mag_ok) {
        want = APP_HEAD_MAG;
    } else if (gps_ok) {
        want = APP_HEAD_GPS;
    } else {
        want = APP_HEAD_NONE;
    }
    a->head_src = want;
}

static unsigned popcount64(uint64_t v)
{
    unsigned n = 0;
    for (; v != 0; v &= v - 1) n++;
    return n;
}

void app_mag_sample(app_t *a, mag_vec_t raw, uint64_t now_ms)
{
    a->now_ms = now_ms;
    const float fx = (raw.x - a->mag.cal.offset.x) * a->mag.cal.scale.x;
    const float fy = (raw.y - a->mag.cal.offset.y) * a->mag.cal.scale.y;
    if (!a->mag.have || now_ms - a->mag.t_ms >= APP_MAG_STALE_MS) {
        a->mag.hx = fx;
        a->mag.hy = fy;
    } else {
        a->mag.hx += MAG_SMOOTH * (fx - a->mag.hx);
        a->mag.hy += MAG_SMOOTH * (fy - a->mag.hy);
    }
    a->mag.have = true;
    a->mag.t_ms = now_ms;
    if (a->mag.cal_running) {
        mag_cal_feed(&a->mag.acc, raw);
        app_mag_t *m = &a->mag;
        const uint16_t k = m->n_points;
        if (k < APP_MAGCAL_POINTS &&
            (k == 0 || fabsf(raw.x - m->px[k - 1]) + fabsf(raw.y - m->py[k - 1]) >= 1.5f)) {
            m->px[k] = raw.x;
            m->py[k] = raw.y;
            m->n_points = (uint16_t)(k + 1);
        }
    }
    update_heading_src(a, now_ms);
}

/* Secteurs de 10° couverts par les points du tour, autour du centre des extrêmes (le fer dur de la
 * voiture peut dépasser le champ terrestre : le cap brut, lui, ne ferait pas le tour). */
static uint64_t magcal_mask(const app_t *a)
{
    const app_mag_t *m = &a->mag;
    if (!m->cal_running || m->n_points < 2) return 0;
    const float cx = (m->acc.min.x + m->acc.max.x) * 0.5f;
    const float cy = (m->acc.min.y + m->acc.max.y) * 0.5f;
    uint64_t bits = 0;
    for (uint16_t i = 0; i < m->n_points; i++) {
        const float h = mag_heading_deg((mag_vec_t){m->px[i] - cx, m->py[i] - cy, 0.0f});
        bits |= 1ULL << ((unsigned)(h / (360.0f / APP_MAGCAL_SECTORS)) % APP_MAGCAL_SECTORS);
    }
    return bits;
}

static bool magcal_ready(const app_t *a, mag_cal_t *out)
{
    mag_cal_t c;
    return a->mag.cal_running && popcount64(magcal_mask(a)) >= APP_MAGCAL_MIN_SECTORS &&
           mag_cal_compute(&a->mag.acc, APP_MAGCAL_MIN_SPAN_UT, out != NULL ? out : &c);
}

void app_magcal_start(app_t *a)
{
    mag_cal_reset(&a->mag.acc);
    a->mag.n_points = 0;
    a->mag.cal_running = true;
}

void app_magcal_stop(app_t *a)
{
    a->mag.cal_running = false;
}

bool app_magcal_apply(app_t *a)
{
    mag_cal_t c;
    if (!magcal_ready(a, &c)) return false;
    a->mag.cal = c;
    a->mag.calibrated = true;
    a->mag.cal_running = false;
    a->mag.have = false;                    /* lissage repris sur la nouvelle calibration */
    const magcal_store_t mc = {.magic = MAGCAL_MAGIC, .cal = c};
    if (hal_storage_set(MAGCAL_KEY, &mc, sizeof(mc)) != 0)
        fprintf(stderr, "app : échec de la sauvegarde de la calibration de la boussole\n");
    return true;
}

/* ------------------------------------------------------------------ boucle */

void app_tick(app_t *a, uint64_t now_ms)
{
    a->now_ms = now_ms;
    update_heading_src(a, now_ms);
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
    const int64_t utc_ms = hal_time_utc_ms();
    if (utc_ms >= 0) {
        int64_t m = (utc_ms / 60000 + a->set.tz_min) % (24 * 60);
        if (m < 0)
            m += 24 * 60;
        out->local_hh = (int8_t)(m / 60);
        out->local_mm = (int8_t)(m % 60);
    }
    out->autocal_running = trip_engine_autocal_running(&a->engine);
    out->autocal_raw_m = trip_engine_autocal_raw_m(&a->engine);

    out->gps_present = hal_gps_present();
    out->mag_present = mag_fresh(a, now_ms);
    out->mag_calibrated = a->mag.calibrated;
    out->heading_src = a->head_src;
    if (a->head_src == APP_HEAD_MAG && out->mag_present && a->mag.calibrated) {
        out->heading_deg = mag_heading(a);
    } else if (a->head_src == APP_HEAD_GPS && out->trip.fix_ok && !isnan(out->trip.course_deg)) {
        out->heading_deg = out->trip.course_deg;
    } else {
        out->heading_src = APP_HEAD_NONE;
        out->heading_deg = NAN;
    }
    out->magcal_running = a->mag.cal_running;
    out->magcal_mask = magcal_mask(a);
    out->magcal_sectors = (uint8_t)popcount64(out->magcal_mask);
    out->magcal_ready = magcal_ready(a, NULL);
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
