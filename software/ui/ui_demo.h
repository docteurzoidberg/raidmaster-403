/*
 * Raidmaster 403 — démo auto : fait vivre les valeurs du ui_trip_state_t sans vrai GPS.
 *
 * Utilise un timer LVGL périodique (100 ms) qui incrémente progressivement vitesse / Trip A /
 * chrono / cap, pousse l'état à l'écran courant via ui_push_trip_state().
 *
 * Permet de voir l'UI vivre avant que le simulateur rejoue de vraies traces NMEA (étape 4
 * du plan, voir le tableau « État » du README).
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_UI_DEMO_H
#define RAIDMASTER_UI_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Démarre le timer. Idempotent. */
void ui_demo_start(void);

/* Arrête le timer. Idempotent. */
void ui_demo_stop(void);

/* Toggle pause/reprise. */
void ui_demo_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_UI_DEMO_H */
