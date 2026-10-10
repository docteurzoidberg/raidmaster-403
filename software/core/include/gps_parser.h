/*
 * Raidmaster 403 — décodage NMEA → GpsFix (core portable, PC et ESP32).
 *
 * Décodage des phrases par minmea (core/third_party/minmea), regroupement par époque :
 * les phrases RMC et GGA qui portent la même heure UTC forment un seul GpsFix.
 * Un fix est publié dès que RMC et GGA de l'époque sont arrivés, ou, si l'une manque,
 * à l'arrivée de la première phrase de l'époque suivante.
 *
 * Coordonnées converties en double depuis les valeurs entières de minmea (un float ne
 * résout que ~0,4 m vers 45° de latitude, trop grossier pour intégrer une distance).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     valid;          /* position exploitable : RMC « A » et, si GGA reçue, qualité > 0 */
    bool     has_rmc;        /* champs RMC présents : vitesse, cap, date */
    bool     has_gga;        /* champs GGA présents : HDOP, satellites, altitude, qualité */
    double   lat_deg;        /* + nord */
    double   lon_deg;        /* + est */
    float    speed_kmh;      /* RMC, NAN si absent */
    float    course_deg;     /* RMC, cap vrai 0..360, NAN si absent (souvent vide à l'arrêt) */
    float    hdop;           /* GGA, NAN si absent */
    float    alt_m;          /* GGA, au-dessus du géoïde, NAN si absent */
    uint8_t  sats;           /* GGA, satellites utilisés */
    uint8_t  fix_quality;    /* GGA : 0 aucun, 1 GPS, 2 DGPS, 4/5 RTK, 6 estimé */
    uint32_t tod_ms;         /* heure UTC du jour en ms (identifiant d'époque) */
    int64_t  t_utc_ms;       /* date + heure UTC en ms depuis 1970, -1 si date inconnue */
} gps_fix_t;

typedef struct {
    uint32_t lines;          /* lignes reçues */
    uint32_t bad_checksum;   /* rejetées : « $ » manquant, checksum faux ou absent, caractère invalide */
    uint32_t parse_errors;   /* checksum bon mais champs illisibles */
    uint32_t ignored;        /* phrases valides non utilisées (GSV, VTG, propriétaires...) */
    uint32_t fixes;          /* GpsFix publiés */
} gps_parser_stats_t;

typedef struct {
    gps_fix_t          cur;          /* époque en cours d'assemblage */
    bool               cur_open;     /* au moins une phrase reçue pour cur */
    int32_t            last_date_days; /* dernière date RMC (jours depuis 1970), -1 si aucune */
    uint32_t           last_date_tod_ms;
    uint32_t           last_pub_tod;   /* heure de la dernière époque publiée */
    gps_parser_stats_t stats;
} gps_parser_t;

void gps_parser_init(gps_parser_t *p);

/* Donne une ligne NMEA (« $...*hh », avec ou sans \r\n final, terminée par \0).
 * Renvoie true et remplit *out quand un fix est publié. */
bool gps_parser_feed(gps_parser_t *p, const char *line, gps_fix_t *out);

/* Publie l'époque en cours sans attendre la suivante (fin de fichier, perte du flux).
 * Renvoie false s'il n'y a rien à publier. */
bool gps_parser_flush(gps_parser_t *p, gps_fix_t *out);

const gps_parser_stats_t *gps_parser_stats(const gps_parser_t *p);

/* Jours depuis le 1970-01-01 pour une date grégorienne (proleptique). Exposé pour les tests. */
int32_t gps_days_from_civil(int year, int month, int day);

#ifdef __cplusplus
}
#endif
