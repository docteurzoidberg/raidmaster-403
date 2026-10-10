/*
 * Raidmaster 403 — câblage de l'UI sur le core. Voir ui_app.h.
 *
 * Boutons (prompts/ui.md § 4.1) :
 *   écrans raid / balade : A RAZ Trip A, B RAZ Trip B, +/− ±10 m sur le trip actif (long : ±100 m),
 *                          Menu bascule le trip actif, Menu long entre dans le menu ;
 *   menu : + monte, − descend, Menu choisit (édition, bascule ou écran dédié), Menu long sort ;
 *          en édition, +/− changent la valeur et Menu (court ou long) termine ;
 *   calibration auto : A départ puis arrivée, +/− distance réelle (long : ±100 m), Menu valide,
 *                      B annule, Menu long revient au menu.
 *
 * Licence : GPL-3.0.
 */

#include "ui_app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/hal_time.h"
#include "screen_calib.h"
#include "screen_menu.h"
#include "theme.h"
#include "ui.h"
#include "ui_input.h"
#include "ui_widgets.h"

#define REFRESH_MS 100

typedef enum {
    CAL_IDLE = 0,       /* consigne, en attente de A */
    CAL_MEASURING,      /* distance brute qui défile, A à l'arrivée */
    CAL_REAL,           /* saisie de la distance réelle */
    CAL_DONE,           /* résultat affiché */
} cal_phase_t;

static app_t      *s_app;
static bool        s_editing;
static cal_phase_t s_cal;
static double      s_cal_real_m;
static float       s_cal_pct;
static bool        s_cal_ok;

/* ------------------------------------------------------------------ formats */

/* « +1,2 % », « −0,4 % », « 0,0 % » */
static void fmt_pct(char *b, size_t n, float pct)
{
    const int t = (int)lroundf(pct * 10.0f);
    const char *sign = t > 0 ? "+" : (t < 0 ? "−" : "");
    snprintf(b, n, "%s%d,%d %%", sign, abs(t) / 10, abs(t) % 10);
}

/* « UTC », « UTC+1 », « UTC−5:30 » */
static void fmt_tz(char *b, size_t n, int tz_min)
{
    if (tz_min == 0) {
        snprintf(b, n, "UTC");
        return;
    }
    const int a = abs(tz_min);
    if (a % 60)
        snprintf(b, n, "UTC%s%d:%02d", tz_min > 0 ? "+" : "−", a / 60, a % 60);
    else
        snprintf(b, n, "UTC%s%d", tz_min > 0 ? "+" : "−", a / 60);
}

/* ------------------------------------------------------------------ rafraîchissement */

static void refresh_menu(const app_state_t *st)
{
    char b[24];
    fmt_pct(b, sizeof(b), st->trip.calib_pct);
    screen_menu_set_value(MENU_CALIB, b);
    screen_menu_set_value(MENU_AUTOCAL, "›");
    fmt_tz(b, sizeof(b), st->set.tz_min);
    screen_menu_set_value(MENU_TZ, b);
    screen_menu_set_value(MENU_NIGHT, st->set.night ? "ON" : "OFF");
    screen_menu_set_value(MENU_VIEW, st->set.view == APP_VIEW_BALADE ? "BALADE" : "RAID");
    if (st->set.speed_alarm_kmh == 0)
        snprintf(b, sizeof(b), "OFF");
    else
        snprintf(b, sizeof(b), "%u km/h", (unsigned)st->set.speed_alarm_kmh);
    screen_menu_set_value(MENU_ALARM, b);
    screen_menu_set_value(MENU_GPX, st->set.gpx_log ? "ON" : "OFF");
    screen_menu_set_value(MENU_COMPASS, "›");
    screen_menu_set_editing(s_editing);
}

static void refresh_calib(const app_state_t *st)
{
    char raw[24], real[24], factor[24];
    ui_fmt_km(raw, sizeof(raw), st->autocal_raw_m / 1000.0, 3);
    char rk[20];
    ui_fmt_km(rk, sizeof(rk), s_cal_real_m / 1000.0, 3);
    snprintf(real, sizeof(real), "%s km", rk);

    switch (s_cal) {
    case CAL_IDLE:
        screen_calib_set("Rouler sur une distance connue (balise rallye, borne routière). "
                         "Appuyer sur A au début, puis sur A à l'arrivée.",
                         "0,000", "-,--- km", "-,- %", CALIB_FOCUS_NONE);
        break;
    case CAL_MEASURING:
        screen_calib_set("Mesure en cours… Appuyer sur A à l'arrivée. B : annuler.",
                         raw, "-,--- km", "-,- %", CALIB_FOCUS_RAW);
        break;
    case CAL_REAL:
        screen_calib_set("Distance réelle : + / − (±10 m, appui long ±100 m), "
                         "MENU pour valider. B : annuler.",
                         raw, real, "-,- %", CALIB_FOCUS_REAL);
        break;
    case CAL_DONE:
        if (isnan(s_cal_pct))
            snprintf(factor, sizeof(factor), "-,- %%");
        else
            fmt_pct(factor, sizeof(factor), s_cal_pct);
        screen_calib_set(s_cal_ok ? "Facteur enregistré. A : nouvelle mesure, MENU long : retour."
                                  : "Non appliqué : distance trop courte (< 100 m) ou facteur "
                                    "hors bornes (±5 %). A : nouvelle mesure.",
                         raw, real, factor, CALIB_FOCUS_NONE);
        break;
    }
}

static void refresh(lv_timer_t *t)
{
    (void)t;
    const uint64_t now = hal_time_ms();
    app_state_t st;
    app_get_state(s_app, now, &st);

    ui_trip_state_t u;
    memset(&u, 0, sizeof(u));
    u.trip_a_km = st.trip.trip_a_m / 1000.0;
    u.trip_b_km = st.trip.trip_b_m / 1000.0;
    u.odo_km = st.trip.odo_m / 1000.0;
    u.speed_kmh = st.trip.speed_kmh;
    u.avg_kmh = st.trip.avg_kmh;
    /* Cap : route GPS en attendant la boussole (QMC5883L), inconnu à l'arrêt. */
    u.heading_unknown = isnan(st.trip.course_deg);
    u.heading_deg = u.heading_unknown ? 0 : (int)lroundf(st.trip.course_deg) % 360;
    u.chrono_s = st.trip.chrono_ms / 1000U;
    u.fix_type = st.trip.fix_ok ? UI_FIX_3D : UI_FIX_NONE;
    u.battery_pct = 100;            /* TODO firmware : AXP2101 */
    u.t_cabin_c = NAN;              /* TODO firmware : capteur de température */
    if (st.local_hh >= 0)
        snprintf(u.time_hhmm, sizeof(u.time_hhmm), "%02u:%02u", (unsigned)st.local_hh % 24U,
                 (unsigned)st.local_mm % 60U);
    u.active_trip = st.active == APP_TRIP_B ? 1 : 0;
    u.speed_alarm = st.speed_alarm;
    ui_push_trip_state(&u);

    refresh_menu(&st);
    refresh_calib(&st);
}

static void refresh_now(void)
{
    refresh(NULL);
}

/* ------------------------------------------------------------------ navigation */

static ui_screen_id_t view_screen(void)
{
    return s_app->set.view == APP_VIEW_BALADE ? UI_SCREEN_BALADE : UI_SCREEN_RAID;
}

static void apply_night(void)
{
    if ((bool)s_app->set.night != ui_theme_is_night())
        ui_theme_toggle_night();
}

static void open_calib(void)
{
    s_cal = CAL_IDLE;
    ui_show_screen(UI_SCREEN_CALIB);
}

static void close_calib(void)
{
    if (s_cal == CAL_MEASURING)
        app_autocal_stop(s_app);
    s_cal = CAL_IDLE;
    ui_show_screen(UI_SCREEN_MENU);
}

static void edit_selected(int dir)
{
    switch (screen_menu_selected()) {
    case MENU_CALIB:
        app_set_calib(s_app, trip_engine_calib(&s_app->engine) + 0.1f * (float)dir);
        break;
    case MENU_TZ:
        app_set_tz(s_app, s_app->set.tz_min + dir * APP_TZ_STEP_MIN);
        break;
    case MENU_ALARM:
        app_set_speed_alarm(s_app, app_speed_alarm_step(s_app->set.speed_alarm_kmh, dir));
        break;
    default:
        s_editing = false;
        break;
    }
}

static void activate_selected(void)
{
    switch (screen_menu_selected()) {
    case MENU_CALIB:
    case MENU_TZ:
    case MENU_ALARM:
        s_editing = true;
        break;
    case MENU_AUTOCAL:
        open_calib();
        break;
    case MENU_NIGHT:
        app_set_night(s_app, !s_app->set.night);
        apply_night();
        break;
    case MENU_VIEW:
        app_set_view(s_app, s_app->set.view == APP_VIEW_BALADE ? APP_VIEW_RAID : APP_VIEW_BALADE);
        break;
    case MENU_GPX:
        app_set_gpx_log(s_app, !s_app->set.gpx_log);
        break;
    case MENU_COMPASS:
        printf("[ui] calibration boussole : à venir (QMC5883L)\n");
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ boutons */

static void on_menu_screen(ui_button_t btn, bool lng)
{
    if (s_editing) {
        if (btn == UI_BTN_PLUS)       edit_selected(+1);
        else if (btn == UI_BTN_MINUS) edit_selected(-1);
        else if (btn == UI_BTN_MENU)  s_editing = false;
        return;
    }
    if (btn == UI_BTN_MENU) {
        if (lng) ui_show_screen(view_screen());
        else     activate_selected();
    } else if (btn == UI_BTN_PLUS) {
        screen_menu_move(-1);
    } else if (btn == UI_BTN_MINUS) {
        screen_menu_move(+1);
    }
}

static void on_calib_screen(ui_button_t btn, bool lng)
{
    const double step = lng ? 100.0 : 10.0;
    switch (btn) {
    case UI_BTN_A:
        if (s_cal == CAL_MEASURING) {
            app_autocal_stop(s_app);
            /* Proposition : la distance brute arrondie aux 100 m (repères usuels). */
            s_cal_real_m = fmax(100.0, round(trip_engine_autocal_raw_m(&s_app->engine) / 100.0) * 100.0);
            s_cal = CAL_REAL;
        } else if (s_cal != CAL_REAL) {
            app_autocal_start(s_app);
            s_cal = CAL_MEASURING;
        }
        break;
    case UI_BTN_B:
        if (s_cal == CAL_MEASURING) app_autocal_stop(s_app);
        s_cal = CAL_IDLE;
        break;
    case UI_BTN_PLUS:
    case UI_BTN_MINUS:
        if (s_cal == CAL_REAL)
            s_cal_real_m = fmax(0.0, s_cal_real_m + (btn == UI_BTN_PLUS ? step : -step));
        break;
    case UI_BTN_MENU:
        if (lng) {
            close_calib();
        } else if (s_cal == CAL_REAL) {
            s_cal_ok = app_autocal_apply(s_app, s_cal_real_m, &s_cal_pct);
            s_cal = CAL_DONE;
        }
        break;
    default:
        break;
    }
}

static void on_view_screen(ui_button_t btn, bool lng)
{
    switch (btn) {
    case UI_BTN_A:     if (!lng) app_reset_trip(s_app, APP_TRIP_A); break;
    case UI_BTN_B:     if (!lng) app_reset_trip(s_app, APP_TRIP_B); break;
    case UI_BTN_PLUS:  app_adjust_active(s_app, lng ? +10 : +1); break;
    case UI_BTN_MINUS: app_adjust_active(s_app, lng ? -10 : -1); break;
    case UI_BTN_MENU:
        if (lng) {
            s_editing = false;
            ui_show_screen(UI_SCREEN_MENU);
        } else {
            app_toggle_active(s_app);
        }
        break;
    default:
        break;
    }
}

static void on_button(ui_button_t btn, ui_button_event_t ev, void *user)
{
    (void)user;
    if (ev != UI_BTN_EV_PRESS && ev != UI_BTN_EV_LONGPRESS) return;
    const bool lng = ev == UI_BTN_EV_LONGPRESS;
    switch (ui_current_screen()) {
    case UI_SCREEN_BOOT:   return;
    case UI_SCREEN_MENU:   on_menu_screen(btn, lng); break;
    case UI_SCREEN_CALIB:  on_calib_screen(btn, lng); break;
    default:               on_view_screen(btn, lng); break;
    }
    refresh_now();
}

static void on_boot_done(void)
{
    ui_show_screen(view_screen());
    refresh_now();
}

void ui_app_attach(app_t *app)
{
    s_app = app;
    ui_input_set_cb(on_button, NULL);
    ui_set_boot_done_cb(on_boot_done);
    apply_night();
    lv_timer_create(refresh, REFRESH_MS, NULL);
}

void ui_app_toggle_night(void)
{
    if (s_app == NULL) {
        ui_theme_toggle_night();
        return;
    }
    app_set_night(s_app, !s_app->set.night);
    apply_night();
    refresh_now();
}
