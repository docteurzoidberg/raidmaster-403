/* Raidmaster 403 — horloge de la carte figée pour les tests. Licence : GPL-3.0. */
#pragma once

#include <stdint.h>

extern unsigned fake_clock_sets;        /* nombre de réglages depuis le dernier clear */
void fake_clock_clear(void);            /* horloge pas réglée */
