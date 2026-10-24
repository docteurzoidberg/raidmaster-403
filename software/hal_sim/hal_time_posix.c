/*
 * Raidmaster 403 — hal_time sur PC : CLOCK_MONOTONIC, ou horloge virtuelle avancée à la main
 * (scénarios : déterministes et sans attente réelle).
 *
 * Horloge de la carte émulée : pas réglée au lancement (comme une RTC sans pile après une
 * coupure), puis elle avance au rythme du GPS simulé (facteur --speed) : réglée au premier fix,
 * elle suit l'heure de la trace rejouée. Elle survit au « power_cut » des scénarios, qui ne
 * redémarre que le core.
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 199309L

#include <time.h>

#include "hal/hal_time.h"
#include "hal_sim.h"

static bool     s_virtual;
static uint64_t s_virtual_us;

static bool     s_utc_set;
static int64_t  s_utc_ms;          /* heure réglée… */
static uint64_t s_utc_at_us;       /* …à cet instant de hal_time_us */
static double   s_utc_rate = 1.0;

void hal_time_sim_virtual(bool on)
{
    s_virtual = on;
}

void hal_time_sim_advance_ms(uint32_t ms)
{
    s_virtual_us += (uint64_t)ms * 1000U;
}

uint64_t hal_time_us(void)
{
    if (s_virtual) return s_virtual_us;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U;
}

void hal_time_sim_clock_rate(double rate)
{
    s_utc_rate = rate > 0.0 ? rate : 1.0;
}

int64_t hal_time_utc_ms(void)
{
    if (!s_utc_set) return -1;
    return s_utc_ms + (int64_t)((double)(hal_time_us() - s_utc_at_us) * s_utc_rate / 1000.0);
}

void hal_time_set_utc_ms(int64_t utc_ms)
{
    s_utc_ms = utc_ms;
    s_utc_at_us = hal_time_us();
    s_utc_set = true;
}
