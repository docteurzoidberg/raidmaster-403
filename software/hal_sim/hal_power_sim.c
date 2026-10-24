/*
 * Raidmaster 403 — hal_power sur PC : états d'alimentation fixes, parcourus à la touche `p`.
 *
 * Licence : GPL-3.0.
 */

#include "hal/hal_power.h"
#include "hal_sim.h"

static const struct {
    hal_power_t st;
    const char *name;
} s_states[] = {
    {{.external = false, .battery = true, .charging = false, .pct = 68}, "batterie 68 %"},
    {{.external = true, .battery = true, .charging = true, .pct = 68}, "USB, batterie en charge (68 %)"},
    {{.external = true, .battery = true, .charging = false, .pct = 100}, "USB, batterie pleine"},
    {{.external = true, .battery = false, .charging = false, .pct = -1}, "USB sans batterie"},
    {{.external = false, .battery = true, .charging = false, .pct = 8}, "batterie 8 %"},
};
#define N_STATES ((int)(sizeof(s_states) / sizeof(s_states[0])))

static int s_cur;

const char *hal_power_sim_cycle(void)
{
    s_cur = (s_cur + 1) % N_STATES;
    return s_states[s_cur].name;
}

bool hal_power_read(hal_power_t *out)
{
    *out = s_states[s_cur].st;
    return true;
}
