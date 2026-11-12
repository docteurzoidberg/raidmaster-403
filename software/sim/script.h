/*
 * Raidmaster 403 — lecteur des scripts d'actions des scénarios (actions.txt), partagé par le sim
 * natif (sim/main.c) et le sim web (simweb/main.c).
 *
 * Une action par ligne, « # » pour commenter : « <ms> <action> » (temps depuis le lancement du sim)
 * ou « @<s> <action> » (instant <s> de la trace rejouée, = s × 1000 / vitesse de rejeu ms). Actions :
 * boutons a b plus minus m (majuscules : appui long), night, signal, gps, mag, spin, power_cut,
 * shot <nom>, expect <champ> <valeur> [tolérance], quit. Détail dans l'en-tête de sim/main.c.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* shot <nom> : capture de l'écran (obligatoire). */
    void (*shot)(const char *name, void *user);
    /* Résultat de chaque expect (facultatif, après la ligne imprimée). */
    void (*expect)(const char *field, double got, double want, double tol, bool ok, void *user);
    void *user;
} script_io_t;

/* Ouvre le script `path` ; speed : vitesse de rejeu de la trace (instants « @<s> »). Les actions
 * agissent sur `app` (expect, power_cut). Renvoie false si le fichier est illisible. */
bool script_open(const char *path, double speed, app_t *app, const script_io_t *io);

/* Joue les actions échues à t_ms (temps depuis le lancement) ; renvoie 0 pour quitter (quit ou
 * plus d'action), 1 sinon. */
int script_step(uint32_t t_ms);

/* Nombre de vérifications en échec (expect faux ou mal formé, champ inconnu). */
int script_failures(void);

#ifdef __cplusplus
}
#endif
