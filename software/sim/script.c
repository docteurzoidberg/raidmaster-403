/*
 * Raidmaster 403 — lecteur des scripts d'actions des scénarios (voir script.h).
 *
 * Extrait de sim/main.c sans changement de comportement (2026-11-12) pour servir aussi au sim
 * web (simweb/main.c).
 *
 * Licence : GPL-3.0.
 */

#include "script.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_time.h"
#include "hal_sim.h"
#include "ui_app.h"
#include "ui_input.h"

static FILE             *s_script;
static app_t            *s_app;
static script_io_t       s_io;
static uint32_t          s_script_at;
static char              s_script_cmd[160];
static bool              s_script_pending;
static int               s_script_failures;
static double            s_script_speed = 1.0;    /* --speed, pour les instants « @<s> » */

/* Comparaison sans casse, ASCII (comme SDL_strcasecmp). */
static int ascii_casecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        const int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || ca == '\0') return ca - cb;
    }
}

/* expect <champ> <valeur> [tolérance] sur l'état du core. */
static void script_expect(const char *args)
{
    char field[32];
    double want, tol = 0.0;
    int n = sscanf(args, "%31s %lf %lf", field, &want, &tol);
    if (n < 2) {
        fprintf(stderr, "script : expect mal formé « %s »\n", args);
        s_script_failures++;
        return;
    }
    app_state_t st;
    app_get_state(s_app, hal_time_ms(), &st);
    double got;
    if      (strcmp(field, "trip_a") == 0) got = st.trip.trip_a_m / 1000.0;
    else if (strcmp(field, "trip_b") == 0) got = st.trip.trip_b_m / 1000.0;
    else if (strcmp(field, "odo") == 0)    got = st.trip.odo_m / 1000.0;
    else if (strcmp(field, "speed") == 0)  got = st.trip.speed_kmh;
    else if (strcmp(field, "calib") == 0)  got = st.trip.calib_pct;
    else if (strcmp(field, "active") == 0) got = st.active;
    else if (strcmp(field, "fix") == 0)    got = st.trip.fix_ok;
    else if (strcmp(field, "night") == 0)  got = st.set.night;
    else if (strcmp(field, "view") == 0)   got = st.set.view;
    else if (strcmp(field, "avg") == 0)    got = st.trip.avg_kmh;
    else if (strcmp(field, "max") == 0)    got = st.trip.max_kmh;
    else if (strcmp(field, "hhmm") == 0)   got = st.local_hh < 0 ? -1 : st.local_hh * 100 + st.local_mm;
    else if (strcmp(field, "alarm") == 0)  got = st.speed_alarm;
    else if (strcmp(field, "chrono") == 0) got = st.trip.chrono_running;
    else if (strcmp(field, "gps_err") == 0)
        got = s_app->parser.stats.bad_checksum + s_app->parser.stats.parse_errors;
    else if (strcmp(field, "gps") == 0)      got = st.gps_present;
    else if (strcmp(field, "mag") == 0)      got = st.mag_present;
    else if (strcmp(field, "mag_cal") == 0)  got = st.mag_calibrated;
    else if (strcmp(field, "head_src") == 0) got = st.heading_src;
    else if (strcmp(field, "heading") == 0)  got = isnan(st.heading_deg) ? -1.0 : st.heading_deg;
    else if (strcmp(field, "magcal") == 0)   got = st.magcal_sectors;
    else {
        fprintf(stderr, "script : champ inconnu « %s »\n", field);
        s_script_failures++;
        return;
    }
    const bool ok = fabs(got - want) <= tol + 1e-9;
    printf("expect %-7s %10.4f (attendu %.4f ± %g) %s\n", field, got, want, tol, ok ? "ok" : "ÉCHEC");
    if (!ok) s_script_failures++;
    if (s_io.expect != NULL) s_io.expect(field, got, want, tol, ok, s_io.user);
}

static bool script_next(void)
{
    char line[200];
    while (fgets(line, sizeof(line), s_script) != NULL) {
        char *h = strchr(line, '#');
        if (h != NULL) *h = '\0';
        unsigned at;
        double at_s;
        int n = 0;
        if (sscanf(line, " @%lf %n", &at_s, &n) == 1 && line[n] != '\0' && line[n] != '\n') {
            s_script_at = (uint32_t)llround(at_s * 1000.0 / s_script_speed);
            snprintf(s_script_cmd, sizeof(s_script_cmd), "%s", line + n);
            s_script_cmd[strcspn(s_script_cmd, "\r\n")] = '\0';
            return true;
        }
        n = 0;
        if (sscanf(line, "%u %n", &at, &n) == 1 && line[n] != '\0' && line[n] != '\n') {
            s_script_at = at;
            snprintf(s_script_cmd, sizeof(s_script_cmd), "%s", line + n);
            s_script_cmd[strcspn(s_script_cmd, "\r\n")] = '\0';
            return true;
        }
    }
    return false;
}

static void dispatch_named(const char *name, bool *ok)
{
    static const struct { const char *n; ui_button_t b; } map[] = {
        {"a", UI_BTN_A}, {"b", UI_BTN_B}, {"plus", UI_BTN_PLUS}, {"minus", UI_BTN_MINUS}, {"m", UI_BTN_MENU},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].n) == 0) {
            ui_input_dispatch(map[i].b, UI_BTN_EV_PRESS);
            return;
        }
        if (ascii_casecmp(name, map[i].n) == 0) {
            ui_input_dispatch(map[i].b, UI_BTN_EV_LONGPRESS);
            return;
        }
    }
    *ok = false;
}

bool script_open(const char *path, double speed, app_t *app, const script_io_t *io)
{
    s_script = fopen(path, "r");
    if (s_script == NULL) return false;
    s_app = app;
    s_io = *io;
    s_script_failures = 0;
    s_script_speed = speed;
    s_script_pending = script_next();
    return true;
}

int script_failures(void)
{
    return s_script_failures;
}

/* Joue les actions échues ; renvoie 0 pour quitter. */
int script_step(uint32_t t_ms)
{
    while (s_script_pending && t_ms >= s_script_at) {
        char verb[32] = "", arg[128] = "";
        sscanf(s_script_cmd, "%31s %127s", verb, arg);
        bool ok = true;
        if (strcmp(verb, "quit") == 0) {
            return 0;
        } else if (strcmp(verb, "shot") == 0 && arg[0] != '\0') {
            s_io.shot(arg, s_io.user);
        } else if (strcmp(verb, "expect") == 0) {
            script_expect(s_script_cmd + strlen("expect"));
        } else if (strcmp(verb, "night") == 0) {
            ui_app_toggle_night();
        } else if (strcmp(verb, "signal") == 0) {
            hal_gps_sim_set_signal(!hal_gps_sim_signal());
        } else if (strcmp(verb, "gps") == 0) {
            hal_gps_sim_set_plugged(!hal_gps_sim_plugged());
        } else if (strcmp(verb, "mag") == 0) {
            hal_mag_sim_set_plugged(!hal_mag_sim_plugged());
        } else if (strcmp(verb, "spin") == 0) {
            hal_mag_sim_set_spin(!hal_mag_sim_spin());
        } else if (strcmp(verb, "power_cut") == 0) {
            app_init(s_app, hal_time_ms());
        } else {
            dispatch_named(verb, &ok);
        }
        if (!ok) fprintf(stderr, "script : action inconnue « %s »\n", s_script_cmd);
        s_script_pending = script_next();
    }
    return s_script_pending ? 1 : 0;
}
