/*
 * Raidmaster 403 — structure d'état UI transitoire.
 *
 * TODO cross-agent (firmware) : aligner avec core/trip_engine.h quand l'agent firmware écrira
 * le core (voir prompts/firmware.md § 5 et § 6). Ce type est volontairement préfixé `ui_`
 * pour marquer qu'il est temporaire côté UI : il sert à nourrir les écrans et la démo auto
 * tant que la vraie TripState du core n'existe pas.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_STATE_H
#define RAIDMASTER_UI_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_FIX_NONE = 0,
    UI_FIX_2D,
    UI_FIX_3D,
} ui_fix_type_t;

typedef struct {
    double   trip_a_km;      /* Trip A (distance partielle principale, point-à-point) */
    double   trip_b_km;      /* Trip B (distance partielle intermédiaire) */
    double   odo_km;         /* Odomètre total */
    float    speed_kmh;      /* Vitesse instantanée */
    float    avg_kmh;        /* Vitesse moyenne (hors arrêts) */
    int      heading_deg;    /* Cap boussole 0..359 */
    uint32_t chrono_s;       /* Chrono (s) */
    uint8_t  fix_type;       /* ui_fix_type_t */
    uint8_t  battery_pct;    /* 0..100 */
    float    t_cabin_c;      /* Température habitacle (°C) */
    char     time_hhmm[6];   /* "HH:MM" zéro-terminé */
} ui_trip_state_t;

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_STATE_H */
