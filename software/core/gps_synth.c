/*
 * Raidmaster 403 — trajet simulé (voir gps_synth.h).
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 200809L    /* gmtime_r */

#include "gps_synth.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PI        3.14159265358979323846
#define EARTH_R_M 6371008.8
#define CYCLE_S   120.0

void gps_synth_init(gps_synth_t *s, int64_t utc0_ms)
{
    s->lat = 48.8566;
    s->lon = 2.3522;
    s->head = 90.0;
    s->utc0_ms = utc0_ms - utc0_ms % GPS_SYNTH_SAMPLE_MS;
    s->next_ms = 0;
}

static void sentence(char *out, size_t n, const char *body)
{
    unsigned cs = 0;
    for (const char *c = body; *c; c++) cs ^= (unsigned char)*c;
    snprintf(out, n, "$%s*%02X\r\n", body, cs);
}

/* Vitesse (km/h) et vitesse de virage (°/s) à l'instant t (s) de la boucle. */
static void profile(double t, double *v, double *turn)
{
    const double c = fmod(t, CYCLE_S);
    double cruise = 70.0 + 20.0 * sin(2 * PI * t / 37.0) + 5.0 * sin(2 * PI * t / 7.0);
    if (c < 10.0)        *v = cruise * c / 10.0;
    else if (c < 100.0)  *v = cruise;
    else if (c < 110.0)  *v = cruise * (110.0 - c) / 10.0;
    else                 *v = 0.0;
    *turn = *v > 1.0 ? 9.0 * sin(2 * PI * t / 23.0) : 0.0;
}

static void nmea_coord(char *out, size_t n, double deg, bool lat)
{
    const char hemi = lat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
    deg = fabs(deg);
    int d = (int)deg;
    double m = (deg - d) * 60.0;
    if (m >= 59.999995) { d++; m = 0.0; }
    snprintf(out, n, lat ? "%02d%08.5f,%c" : "%03d%08.5f,%c", d, m, hemi);
}

static void emit(const char *line, gps_synth_line_cb cb, void *user)
{
    if (cb != NULL) cb(line, strlen(line), user);
}

static void emit_sample(gps_synth_t *s, uint64_t t_ms, bool signal, gps_synth_line_cb cb, void *user)
{
    const double t = (double)t_ms / 1000.0;
    double v, turn;
    profile(t, &v, &turn);
    const double dt = GPS_SYNTH_SAMPLE_MS / 1000.0;
    s->head = fmod(s->head + turn * dt + 360.0, 360.0);
    const double d = v / 3.6 * dt;
    s->lat += d * cos(s->head * PI / 180.0) / EARTH_R_M * 180.0 / PI;
    s->lon += d * sin(s->head * PI / 180.0) / (EARTH_R_M * cos(s->lat * PI / 180.0)) * 180.0 / PI;

    const time_t sec = (time_t)((s->utc0_ms + (int64_t)t_ms) / 1000);
    const unsigned cs = (unsigned)((s->utc0_ms + (int64_t)t_ms) % 1000 / 10);
    struct tm tm;
    gmtime_r(&sec, &tm);
    char hms[32], date[32], la[24], lo[24], body[160], line[200];
    snprintf(hms, sizeof(hms), "%02d%02d%02d.%02u", tm.tm_hour, tm.tm_min, tm.tm_sec, cs);
    snprintf(date, sizeof(date), "%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);
    nmea_coord(la, sizeof(la), s->lat, true);
    nmea_coord(lo, sizeof(lo), s->lon, false);

    if (signal) {
        snprintf(body, sizeof(body), "GNRMC,%s,A,%s,%s,%.2f,%.1f,%s,,,A", hms, la, lo, v / 1.852,
                 v > 1.0 ? s->head : 0.0, date);
        sentence(line, sizeof(line), body);
        emit(line, cb, user);
        snprintf(body, sizeof(body), "GNGGA,%s,%s,%s,1,12,0.8,92.0,M,46.0,M,,", hms, la, lo);
    } else {
        snprintf(body, sizeof(body), "GNRMC,%s,V,,,,,,,%s,,,N", hms, date);
        sentence(line, sizeof(line), body);
        emit(line, cb, user);
        snprintf(body, sizeof(body), "GNGGA,%s,,,,,0,00,99.99,,,,,,", hms);
    }
    sentence(line, sizeof(line), body);
    emit(line, cb, user);
}

void gps_synth_run(gps_synth_t *s, uint64_t t_ms, bool signal, gps_synth_line_cb cb, void *user)
{
    /* Après une longue pause (débogueur), on ne rattrape pas plus de 10 s d'un coup. */
    if (t_ms > s->next_ms + 10000U) s->next_ms = t_ms - 10000U;
    while (s->next_ms <= t_ms) {
        emit_sample(s, s->next_ms, signal, cb, user);
        s->next_ms += GPS_SYNTH_SAMPLE_MS;
    }
}
