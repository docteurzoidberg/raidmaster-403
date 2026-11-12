/*
 * Raidmaster 403 — HAL du simulateur PC : réglages propres au sim.
 *
 * Implémente hal/hal_storage.h (fichier JSON), hal/hal_gps.h (rejeu d'un .nmea ou trajet
 * simulé, par core/gps_link face à un M8N émulé), hal/hal_mag.h (boussole émulée),
 * hal/hal_time.h (CLOCK_MONOTONIC, horloge de la carte émulée), hal/hal_power.h
 * (état d'alimentation choisi au clavier) et hal/hal_audio.h (sortie audio SDL).
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stockage : charge `path` s'il existe (sinon part de zéro) ; chaque écriture réécrit le
 * fichier. path NULL : stockage en mémoire seulement. */
void hal_storage_sim_open(const char *path);

/* GPS : à appeler avant hal_gps_init. path NULL → trajet simulé (boucle de 2 min : départ,
 * route qui ondule entre 45 et 95 km/h, virages, freinage, arrêt). speed : facteur de temps
 * (1 = temps réel). loop : rejoue le fichier en boucle. Renvoie false si le fichier est
 * illisible. */
bool hal_gps_sim_config(const char *path, double speed, bool loop);

/* Coupe / rétablit le signal (tunnel) ; touche `d` du sim. */
void hal_gps_sim_set_signal(bool on);
bool hal_gps_sim_signal(void);

/* Débranche / rebranche le récepteur (puck) ; touche `g` du sim. Rebranché, il repart à 9 600
 * bauds (débit d'usine) : la liaison (core/gps_link) le cherche et le règle de nouveau. */
void hal_gps_sim_set_plugged(bool on);
bool hal_gps_sim_plugged(void);

/* Cap vrai du véhicule simulé (trajet simulé ou dernière RMC rejouée en mouvement), en degrés. */
double hal_gps_sim_heading(void);

/* Boussole émulée : champ terrestre (21 µT horizontal, 42 µT vertical) tourné selon le cap du
 * véhicule, fer dur de la voiture (8, −5, 3) µT, bruit de ±0,3 µT, 20 Hz. */
void hal_mag_sim_set_plugged(bool on);      /* touche `c` */
bool hal_mag_sim_plugged(void);
void hal_mag_sim_set_spin(bool on);         /* touche `r` : la voiture tourne sur place, 30°/s ;
                                              * à l'arrêt, elle retrouve le cap de son trajet */
bool hal_mag_sim_spin(void);

/* Horloge virtuelle : hal_time ne bouge plus que par hal_time_sim_advance_ms (mode script). */
void hal_time_sim_virtual(bool on);
void hal_time_sim_advance_ms(uint32_t ms);

/* Vitesse de l'horloge de la carte émulée (celle du GPS simulé, réglée par hal_gps_sim_config). */
void hal_time_sim_clock_rate(double rate);

/* Alimentation : passe à l'état suivant (touche `p` du sim) et renvoie son nom :
 * batterie 68 % → USB, batterie en charge → USB, batterie pleine → USB sans batterie
 * (comme la carte au banc) → batterie 8 %, faible → batterie 68 %… */
const char *hal_power_sim_cycle(void);

#ifdef __cplusplus
}
#endif
