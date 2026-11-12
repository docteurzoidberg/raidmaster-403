/*
 * Raidmaster 403 — hal_mag sur PC : boussole émulée. Voir hal_sim.h.
 *
 * Le champ suit le cap vrai du véhicule simulé (hal_gps_sim_heading), plus la rotation sur place
 * (touche `r`) ; mesures à 20 Hz du temps du sim, bruit pseudo-aléatoire déterministe (les
 * scénarios donnent toujours le même résultat).
 *
 * Licence : GPL-3.0.
 */

#include <math.h>

#include "hal/hal_mag.h"
#include "hal/hal_time.h"
#include "hal_sim.h"

#define PERIOD_MS  50U
#define FIELD_H    21.0f
#define FIELD_Z    42.0f
#define SPIN_DPS   30.0
#define PI_D       3.14159265358979323846

static const mag_vec_t HARD_IRON = {8.0f, -5.0f, 3.0f};

static bool     s_plugged = true;
static bool     s_spin;
static double   s_spin_deg;
static uint64_t s_last_ms;
static uint64_t s_next_ms;
static uint64_t s_t_sample_ms;
static bool     s_have;
static uint32_t s_rng = 12345u;

static float noise(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return ((float)(s_rng >> 8) / 16777216.0f - 0.5f) * 0.6f;
}

void hal_mag_sim_set_plugged(bool on)
{
    s_plugged = on;
}

bool hal_mag_sim_plugged(void)
{
    return s_plugged;
}

/* Fin de la rotation : la voiture retrouve le cap de son trajet (simplification du sim). */
void hal_mag_sim_set_spin(bool on)
{
    s_spin = on;
    if (!on) s_spin_deg = 0.0;
}

bool hal_mag_sim_spin(void)
{
    return s_spin;
}

void hal_mag_init(void)
{
    s_last_ms = s_next_ms = hal_time_ms();
}

bool hal_mag_poll(mag_vec_t *raw)
{
    const uint64_t now = hal_time_ms();
    if (s_spin) s_spin_deg = fmod(s_spin_deg + SPIN_DPS * (double)(now - s_last_ms) / 1000.0, 360.0);
    s_last_ms = now;
    if (!s_plugged || now < s_next_ms) return false;
    s_next_ms = now + PERIOD_MS;
    const double h = (hal_gps_sim_heading() + s_spin_deg) * PI_D / 180.0;
    /* repère voiture : X avant, Y droite, Z bas ; nord à droite quand on va vers l'ouest */
    raw->x = HARD_IRON.x + FIELD_H * (float)cos(h) + noise();
    raw->y = HARD_IRON.y - FIELD_H * (float)sin(h) + noise();
    raw->z = HARD_IRON.z + FIELD_Z + noise();
    s_have = true;
    s_t_sample_ms = now;
    return true;
}

bool hal_mag_present(void)
{
    return s_plugged && s_have && hal_time_ms() - s_t_sample_ms < 1000U;
}
