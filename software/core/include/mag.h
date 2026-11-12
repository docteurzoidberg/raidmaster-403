/*
 * Raidmaster 403 — magnétomètre (boussole) sur I²C : détection, réglage, lecture, orientation,
 * calibration et cap (core portable, PC et ESP32).
 *
 * Boussoles des pucks de drone et du GY-271 : QMC5883L, IST8310, HMC5883L, LIS3MDL. La puce est
 * cherchée aux adresses connues et reconnue par ses registres d'identité, réglée en mesure
 * continue, puis lue à cadence fixe. Sans réponse (puck débranché), elle est déclarée absente et
 * recherchée de nouveau.
 *
 * Le module ne connaît pas le bus : le HAL lui donne deux fonctions (lecture de registres, écriture
 * d'un registre). Il ne connaît pas l'heure : mag_tick() la reçoit.
 *
 * Repère de sortie (« voiture ») : X vers l'avant (flèche du puck), Y vers la droite, Z vers le bas.
 * Cap = atan2(−Y, X), en degrés de 0 à 360, sens horaire depuis le nord magnétique, boîtier à plat
 * (compensation d'inclinaison par la centrale inertielle : plus tard).
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

/* Lit n registres à partir de reg (lecture en rafale). false : pas de réponse (NACK, bus). */
typedef bool (*mag_i2c_read_fn)(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n, void *user);
/* Écrit un registre. */
typedef bool (*mag_i2c_write_fn)(uint8_t addr, uint8_t reg, uint8_t val, void *user);

typedef struct {
    mag_i2c_read_fn  read;
    mag_i2c_write_fn write;
    void            *user;
} mag_bus_t;

typedef enum {
    MAG_CHIP_NONE = 0,
    MAG_CHIP_QMC5883L,
    MAG_CHIP_IST8310,
    MAG_CHIP_HMC5883L,
    MAG_CHIP_LIS3MDL,
} mag_chip_t;

typedef struct {
    float x, y, z;
} mag_vec_t;

/* Montage de la puce par rapport à la voiture, comme COMPASS_ORIENT d'ArduPilot réduit à l'utile :
 * rotation de yaw_quarter × 90° (sens horaire vu de dessus), puis retournement (puce à l'envers). */
typedef struct {
    uint8_t yaw_quarter;     /* 0 à 3 */
    bool    flipped;
} mag_orient_t;

/* Calibration : champ corrigé = (brut − offset) × scale, axe par axe (fer dur, fer doux simplifié). */
typedef struct {
    mag_vec_t offset;        /* µT */
    mag_vec_t scale;
} mag_cal_t;

/* Calibration par extrêmes : faire tourner la boussole sur elle-même (au moins un tour à plat). */
typedef struct {
    mag_vec_t min, max;
    uint32_t  n;
} mag_cal_acc_t;

typedef struct {
    uint32_t probes;         /* recherches complètes de la puce */
    uint32_t samples;        /* mesures lues */
    uint32_t errors;         /* lectures sans réponse */
    uint32_t losses;         /* puce perdue après avoir répondu */
} mag_stats_t;

typedef struct {
    mag_bus_t     bus;
    mag_orient_t  orient;
    mag_cal_t     cal;
    uint32_t      period_ms;     /* entre deux lectures (50 ms = 20 Hz) */
    uint32_t      probe_ms;      /* entre deux recherches quand la puce est absente */
    uint32_t      stale_ms;      /* sans mesure neuve au-delà : puce perdue */
    mag_chip_t    chip;
    uint8_t       addr;
    uint8_t       errors;        /* erreurs de suite */
    bool          have_sample;
    uint64_t      t_next_ms;
    uint64_t      t_found_ms;
    uint64_t      t_sample_ms;
    mag_vec_t     raw;           /* µT, repère voiture, non calibré */
    mag_vec_t     field;         /* µT, repère voiture, calibré */
    mag_stats_t   stats;
} mag_t;

void mag_init(mag_t *m, const mag_bus_t *bus, uint64_t now_ms);

/* À appeler souvent : recherche de la puce, lecture à cadence, perte. true : mesure neuve. */
bool mag_tick(mag_t *m, uint64_t now_ms);

/* Puce présente et mesure neuve depuis moins de stale_ms (« boussole absente » sinon). */
bool mag_present(const mag_t *m, uint64_t now_ms);

mag_chip_t  mag_chip(const mag_t *m);
uint8_t     mag_addr(const mag_t *m);
const char *mag_chip_name(mag_chip_t c);

/* Dernière mesure, µT, repère voiture : brute (pour la calibration) ou calibrée. */
mag_vec_t mag_raw(const mag_t *m);
mag_vec_t mag_field(const mag_t *m);

void mag_set_orient(mag_t *m, mag_orient_t o);
void mag_set_cal(mag_t *m, const mag_cal_t *cal);
const mag_stats_t *mag_stats(const mag_t *m);

/* Cap à plat d'un champ en repère voiture, degrés [0, 360). */
float mag_heading_deg(mag_vec_t v);

void mag_cal_reset(mag_cal_acc_t *a);
void mag_cal_feed(mag_cal_acc_t *a, mag_vec_t raw);
/* false tant que X et Y n'ont pas chacun une étendue d'au moins min_span_ut (tour incomplet). */
bool mag_cal_compute(const mag_cal_acc_t *a, float min_span_ut, mag_cal_t *out);

/* Calibration neutre (offset 0, échelle 1). */
void mag_cal_identity(mag_cal_t *c);

#ifdef __cplusplus
}
#endif
