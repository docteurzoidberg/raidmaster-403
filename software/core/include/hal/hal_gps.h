/*
 * Raidmaster 403 — HAL GPS : livre les lignes NMEA brutes (UART1 NEO-M9N sur l'ESP32,
 * rejeu de fichier ou trajet simulé sur le sim). Le décodage est dans core/gps_parser.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* line : phrase NMEA terminée par \0 (avec ou sans \r\n). */
typedef void (*hal_gps_line_cb)(const char *line, size_t len, void *user);

void hal_gps_init(hal_gps_line_cb cb, void *user);

/* À appeler souvent depuis la boucle principale : livre les lignes disponibles. */
void hal_gps_poll(void);

#ifdef __cplusplus
}
#endif
