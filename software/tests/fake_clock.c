/*
 * Raidmaster 403 — hal_time (horloge de la carte) pour les tests du core : elle garde l'heure
 * réglée sans avancer, ce qui rend visible un réglage en trop.
 *
 * Licence : GPL-3.0.
 */

#include "fake_clock.h"

#include "hal/hal_time.h"

static int64_t s_utc_ms = -1;
unsigned fake_clock_sets;

void fake_clock_clear(void)
{
    s_utc_ms = -1;
    fake_clock_sets = 0;
}

int64_t hal_time_utc_ms(void)
{
    return s_utc_ms;
}

void hal_time_set_utc_ms(int64_t utc_ms)
{
    s_utc_ms = utc_ms;
    fake_clock_sets++;
}
