/*
 * Raidmaster 403 — adaptation de minmea (inclus par minmea.h, MINMEA_INCLUDE_COMPAT).
 *
 * minmea_gettime() appelle timegm(), absent de la libc en C11 strict et de newlib (ESP-IDF).
 * On le remplace par une version portable (core/gps_parser.c). Le core n'utilise pas
 * minmea_gettime(), mais minmea.c doit compiler et se lier sur les deux cibles.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <time.h>

time_t rm_timegm(struct tm *tm);
#define timegm rm_timegm
