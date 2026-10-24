/*
 * Raidmaster 403 — hal_gps sur PC : rejeu d'un fichier .nmea ou trajet simulé.
 *
 * Rejeu : les lignes sont livrées au rythme de leur heure NMEA (2e champ hhmmss.ss), multiplié
 * par le facteur de vitesse ; les lignes sans heure partent avec la précédente. Fin de fichier :
 * arrêt, ou retour au début si loop. Signal coupé : les lignes sont jetées (le core voit le fix
 * se perdre).
 *
 * Trajet simulé (sans fichier) : core/gps_synth (le même que le GPS de démonstration du
 * firmware), parti à l'heure UTC de la machine.
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gps_synth.h"
#include "hal/hal_gps.h"
#include "hal/hal_time.h"
#include "hal_sim.h"

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
static gps_synth_t s_synth;

bool hal_gps_sim_config(const char *path, double speed, bool loop)
{
    s_speed = speed > 0.0 ? speed : 1.0;
    hal_time_sim_clock_rate(s_speed);      /* l'horloge de la carte suit le temps simulé */
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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    gps_synth_init(&s_synth, (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
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

static void synth_line(const char *line, size_t len, void *user)
{
    (void)user;
    if (s_cb != NULL) s_cb(line, len, s_user);
}

void hal_gps_poll(void)
{
    if (s_file != NULL) poll_replay();
    else                gps_synth_run(&s_synth, sim_elapsed_ms(), s_signal, synth_line, NULL);
}
