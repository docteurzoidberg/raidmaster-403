/*
 * Raidmaster 403 — hal_gps_present pour les tests du core : récepteur présent, sauf si le test
 * le débranche.
 *
 * Licence : GPL-3.0.
 */

#include <stdbool.h>

#include "hal/hal_gps.h"

bool fake_gps_present = true;

bool hal_gps_present(void)
{
    return fake_gps_present;
}
