/*
 * Raidmaster 403 — boutons de banc : le tactile découpé en 5 bandes verticales (A | B | − | + |
 * Menu), en attendant la carte boutons. Option RAIDMASTER_TOUCH_BUTTONS (menuconfig).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdint.h>

/* À appeler souvent depuis la boucle principale (interroge le tactile toutes les 20 ms). */
void touch_buttons_poll(uint64_t now_ms);
