/*
 * Raidmaster 403 — état affiché par les écrans (modèle de vue de l'UI).
 *
 * Rempli par ui_app.c depuis l'état du core (app_get_state), ou directement par la démo,
 * l'enregistrement du GIF et les captures du sim. Les écrans ne dépendent que de ce type :
 * ils restent indépendants du core. Valeurs déjà prêtes à afficher (km, km/h, heure locale).
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_STATE_H
#define RAIDMASTER_UI_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_FIX_NONE = 0,
    UI_FIX_2D,
    UI_FIX_3D,
} ui_fix_type_t;

typedef enum {
    UI_PWR_BATTERY = 0,     /* sur batterie */
    UI_PWR_EXTERNAL,        /* alimentation externe (USB-C), batterie absente ou pas en charge */
    UI_PWR_CHARGING,        /* alimentation externe, batterie en charge */
} ui_power_t;

/* Source du cap affiché. UNSET (démo, maquettes) : pas d'étiquette. */
typedef enum {
    UI_HEAD_SRC_UNSET = 0,
    UI_HEAD_SRC_GPS,        /* route GPS (en roulant) */
    UI_HEAD_SRC_MAG,        /* boussole calibrée (à l'arrêt, basse vitesse) */
} ui_heading_src_t;

typedef enum {
    UI_MAG_OK = 0,
    UI_MAG_ABSENT,          /* boussole absente (puck débranché, I²C muet) */
    UI_MAG_UNCAL,           /* présente mais pas calibrée (menu « Calibration boussole ») */
} ui_mag_state_t;

#define UI_BATT_UNKNOWN 0xFFu   /* battery_pct : pas de batterie, ou charge inconnue */

typedef struct {
    double   trip_a_km;      /* Trip A (distance partielle principale, point-à-point) */
    double   trip_b_km;      /* Trip B (distance partielle intermédiaire) */
    double   odo_km;         /* Odomètre total */
    float    speed_kmh;      /* Vitesse instantanée */
    float    avg_kmh;        /* Vitesse moyenne (hors arrêts) */
    int      heading_deg;    /* Cap affiché 0..359 (boussole ou route GPS, voir heading_src) */
    uint32_t chrono_s;       /* Chrono (s) */
    uint8_t  fix_type;       /* ui_fix_type_t */
    uint8_t  battery_pct;    /* 0..100, UI_BATT_UNKNOWN sans batterie ou si inconnue */
    uint8_t  power;          /* ui_power_t */
    float    t_cabin_c;      /* Température habitacle (°C), NAN si inconnue */
    char     time_hhmm[6];   /* "HH:MM" zéro-terminé, vide si heure inconnue */
    uint8_t  active_trip;    /* 0 : Trip A, 1 : Trip B (celui que corrigent +/−) */
    bool     heading_unknown;/* cap inconnu (ni boussole calibrée ni route GPS) */
    bool     speed_alarm;    /* vitesse au-dessus du seuil d'alarme */
    bool     gps_absent;     /* module GPS absent (récepteur muet) : « GPS × » dans la barre d'état */
    uint8_t  heading_src;    /* ui_heading_src_t */
    uint8_t  mag_state;      /* ui_mag_state_t */
} ui_trip_state_t;

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_STATE_H */
