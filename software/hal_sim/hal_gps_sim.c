/*
 * Raidmaster 403 — hal_gps sur PC : rejeu d'un fichier .nmea ou trajet simulé.
 *
 * Rejeu : les lignes sont livrées au rythme de leur heure NMEA (2e champ hhmmss.ss), multiplié
 * par le facteur de vitesse ; les lignes sans heure partent avec la précédente. Fin de fichier :
 * arrêt, ou retour au début si loop. Signal coupé : les lignes sont jetées (le core voit le fix
 * se perdre).
 *
 * Trajet simulé (sans fichier) : modèle cinématique déterministe à 10 Hz, boucle de 120 s
 * (accélération, route qui ondule, virages, freinage, 10 s d'arrêt), départ de Paris, heure UTC
 * de la machine. Signal coupé : phrases « sans fix » comme un récepteur u-blox (RMC V, GGA 0).
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hal/hal_gps.h"
#include "hal/hal_time.h"
#include "hal_sim.h"

#define PI        3.14159265358979323846
#define EARTH_R_M 6371008.8
#define SAMPLE_MS 100U
#define CYCLE_S   120.0

static hal_gps_line_cb s_cb;
static void           *s_user;
static double          s_speed = 1.0;
static bool            s_loop;
static bool            s_signal = true;
static uint64_t        s_t0_ms;           /* hal_time au démarrage (ou au dernier rebouclage) */

/* Rejeu */
static FILE    *s_file;
static char     s_pending[256];
static bool     s_have_pending;
static int64_t  s_file_t0 = -1;           /* heure NMEA de la première ligne datée (ms) */
static int64_t  s_last_tod = -1;
static int64_t  s_wrap_ms;                /* +24 h à chaque passage de minuit */

/* Trajet simulé */
static uint64_t s_next_sample_ms;         /* temps simulé du prochain échantillon */
static double   s_lat = 48.8566, s_lon = 2.3522, s_head = 90.0;
static int64_t  s_utc0_ms;                /* heure UTC de la machine au démarrage */

bool hal_gps_sim_config(const char *path, double speed, bool loop)
{
    s_speed = speed > 0.0 ? speed : 1.0;
    s_loop = loop;
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    if (path != NULL) {
        s_file = fopen(path, "r");
        if (s_file == NULL) return false;
    }
    return true;
}

void hal_gps_sim_set_signal(bool on)
{
    s_signal = on;
}

bool hal_gps_sim_signal(void)
{
    return s_signal;
}

void hal_gps_init(hal_gps_line_cb cb, void *user)
{
    s_cb = cb;
    s_user = user;
    s_t0_ms = hal_time_ms();
    s_next_sample_ms = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    s_utc0_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    s_utc0_ms -= s_utc0_ms % SAMPLE_MS;
}

static uint64_t sim_elapsed_ms(void)
{
    return (uint64_t)((double)(hal_time_ms() - s_t0_ms) * s_speed);
}

static void emit(const char *line)
{
    if (s_cb != NULL) s_cb(line, strlen(line), s_user);
}

/* ------------------------------------------------------------------ rejeu */

/* Heure NMEA de la ligne en ms depuis minuit, -1 si absente. */
static int64_t line_tod(const char *l)
{
    const char *c = strchr(l, ',');
    if (c == NULL) return -1;
    c++;
    for (int i = 0; i < 6; i++)
        if (!isdigit((unsigned char)c[i])) return -1;
    int64_t t = ((c[0] - '0') * 10 + (c[1] - '0')) * 3600000LL + ((c[2] - '0') * 10 + (c[3] - '0')) * 60000LL +
                ((c[4] - '0') * 10 + (c[5] - '0')) * 1000LL;
    if (c[6] == '.') {
        int scale = 100;
        for (int i = 7; isdigit((unsigned char)c[i]) && scale > 0; i++, scale /= 10)
            t += (c[i] - '0') * scale;
    }
    return t;
}

static void poll_replay(void)
{
    const uint64_t elapsed = sim_elapsed_ms();
    for (int budget = 2000; budget > 0; budget--) {
        if (!s_have_pending) {
            if (fgets(s_pending, sizeof(s_pending), s_file) == NULL) {
                if (!s_loop) return;
                rewind(s_file);
                s_file_t0 = -1;
                s_last_tod = -1;
                s_wrap_ms = 0;
                s_t0_ms = hal_time_ms();
                return;
            }
            s_have_pending = true;
        }
        int64_t tod = line_tod(s_pending);
        if (tod >= 0) {
            if (s_last_tod >= 0 && tod + 12 * 3600000LL < s_last_tod) s_wrap_ms += 24 * 3600000LL;
            s_last_tod = tod;
            tod += s_wrap_ms;
            if (s_file_t0 < 0) s_file_t0 = tod;
            if ((uint64_t)(tod - s_file_t0) > elapsed) return;     /* pas encore l'heure */
        }
        if (s_signal) emit(s_pending);
        s_have_pending = false;
    }
}

/* ------------------------------------------------------------------ trajet simulé */

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

static void emit_synth_sample(uint64_t t_ms)
{
    const double t = (double)t_ms / 1000.0;
    double v, turn;
    profile(t, &v, &turn);
    const double dt = SAMPLE_MS / 1000.0;
    s_head = fmod(s_head + turn * dt + 360.0, 360.0);
    const double d = v / 3.6 * dt;
    s_lat += d * cos(s_head * PI / 180.0) / EARTH_R_M * 180.0 / PI;
    s_lon += d * sin(s_head * PI / 180.0) / (EARTH_R_M * cos(s_lat * PI / 180.0)) * 180.0 / PI;

    const time_t sec = (time_t)((s_utc0_ms + (int64_t)t_ms) / 1000);
    const unsigned cs = (unsigned)((s_utc0_ms + (int64_t)t_ms) % 1000 / 10);
    struct tm tm;
    gmtime_r(&sec, &tm);
    char hms[32], date[32], la[24], lo[24], body[160], line[200];
    snprintf(hms, sizeof(hms), "%02d%02d%02d.%02u", tm.tm_hour, tm.tm_min, tm.tm_sec, cs);
    snprintf(date, sizeof(date), "%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);
    nmea_coord(la, sizeof(la), s_lat, true);
    nmea_coord(lo, sizeof(lo), s_lon, false);

    if (s_signal) {
        snprintf(body, sizeof(body), "GNRMC,%s,A,%s,%s,%.2f,%.1f,%s,,,A", hms, la, lo, v / 1.852,
                 v > 1.0 ? s_head : 0.0, date);
        sentence(line, sizeof(line), body);
        emit(line);
        snprintf(body, sizeof(body), "GNGGA,%s,%s,%s,1,12,0.8,92.0,M,46.0,M,,", hms, la, lo);
    } else {
        snprintf(body, sizeof(body), "GNRMC,%s,V,,,,,,,%s,,,N", hms, date);
        sentence(line, sizeof(line), body);
        emit(line);
        snprintf(body, sizeof(body), "GNGGA,%s,,,,,0,00,99.99,,,,,,", hms);
    }
    sentence(line, sizeof(line), body);
    emit(line);
}

static void poll_synth(void)
{
    const uint64_t elapsed = sim_elapsed_ms();
    /* Après une longue pause (débogueur), on ne rattrape pas plus de 10 s d'un coup. */
    if (elapsed > s_next_sample_ms + 10000U) s_next_sample_ms = elapsed - 10000U;
    while (s_next_sample_ms <= elapsed) {
        emit_synth_sample(s_next_sample_ms);
        s_next_sample_ms += SAMPLE_MS;
    }
}

void hal_gps_poll(void)
{
    if (s_file != NULL) poll_replay();
    else                poll_synth();
}
