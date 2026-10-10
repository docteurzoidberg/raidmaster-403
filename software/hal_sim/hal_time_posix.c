/*
 * Raidmaster 403 — hal_time sur PC : CLOCK_MONOTONIC, ou horloge virtuelle avancée à la main
 * (scénarios : déterministes et sans attente réelle).
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 199309L

#include <time.h>

#include "hal/hal_time.h"
#include "hal_sim.h"

static bool     s_virtual;
static uint64_t s_virtual_us;

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
