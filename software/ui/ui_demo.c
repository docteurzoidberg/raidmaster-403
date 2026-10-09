/*
 * Raidmaster 403 — démo auto.
 *
 * Petit modèle cinématique : la vitesse oscille sinusoïdalement entre 30 et 110 km/h, le trip A
 * s'incrémente de speed*dt, le trip B suit (plus lentement). Le cap tourne lentement (0.5 °/s).
 *
 * Licence : GPL-3.0.
 */

#include "ui_demo.h"
#include "ui.h"
#include "ui_state.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static lv_timer_t      *s_timer = NULL;
static ui_trip_state_t  s_state;
static uint32_t         s_elapsed_ms = 0;

static void demo_cb(lv_timer_t *t)
{
    (void)t;
    const uint32_t dt_ms = 100;
    s_elapsed_ms += dt_ms;

    float ts = (float)s_elapsed_ms / 1000.0f;

    /* Vitesse : 70 ± 40 km/h, période ~45 s. */
    s_state.speed_kmh = 70.0f + 40.0f * sinf(ts * 2.0f * 3.14159265f / 45.0f);
    if (s_state.speed_kmh < 0) s_state.speed_kmh = 0;

    /* Distance : v (km/h) * dt (h). */
    double step_km = (double)s_state.speed_kmh * (double)dt_ms / 3600000.0;
    s_state.trip_a_km += step_km;
    s_state.trip_b_km += step_km * 0.6;
    s_state.odo_km    += step_km;

    /* Moyenne. */
    if (s_elapsed_ms > 0) {
        s_state.avg_kmh = (float)(s_state.trip_a_km / ((double)s_elapsed_ms / 3600000.0));
    }

    /* Chrono = elapsed. */
    s_state.chrono_s = s_elapsed_ms / 1000U;

    /* Cap qui tourne. */
    int cap = (int)(ts * 0.5f);
    s_state.heading_deg = ((cap % 360) + 360) % 360;

    /* Horloge mockée (statique pour la démo). */
    snprintf(s_state.time_hhmm, sizeof(s_state.time_hhmm), "14:03");

    ui_push_trip_state(&s_state);
}

static void demo_init_state(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.fix_type    = 2;        /* 3D */
    s_state.battery_pct = 78;
    s_state.t_cabin_c   = 24.0f;
    snprintf(s_state.time_hhmm, sizeof(s_state.time_hhmm), "14:03");
}

void ui_demo_start(void)
{
    if (s_timer != NULL) return;
    demo_init_state();
    s_elapsed_ms = 0;
    s_timer = lv_timer_create(demo_cb, 100, NULL);
}

void ui_demo_stop(void)
{
    if (s_timer == NULL) return;
    lv_timer_delete(s_timer);
    s_timer = NULL;
}

void ui_demo_toggle(void)
{
    if (s_timer == NULL) ui_demo_start();
    else                 ui_demo_stop();
}
