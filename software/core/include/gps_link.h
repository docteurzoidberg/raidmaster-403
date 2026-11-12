/*
 * Raidmaster 403 — liaison avec le récepteur GPS (core portable, PC et ESP32).
 *
 * Automate au-dessus du flux série du récepteur, commun aux deux variantes de la carte fille :
 * NEO-M9N soudé ou puck de drone NEO-M8N branché. Il ne connaît ni
 * l'UART ni l'horloge : le HAL lui donne les octets reçus et l'heure, il demande d'écrire et de
 * changer de débit par des fonctions de rappel.
 *
 *   RECHERCHE ── phrase ou trame valide ──► IDENTIFICATION ── MON-VER ──► configuration
 *      ▲  (débits essayés tour à tour,          (MON-VER redemandé,          (M8 : CFG-*, M9 :
 *      │   MON-VER envoyé à chaque débit)        réponses NMEA seules :         VALSET ; débit
 *      │                                          récepteur inconnu)            cible en dernier)
 *      │                                                                         │
 *      └── silence > absent_ms ◄── MARCHE ◄── données au nouveau débit ◄── VÉRIFICATION
 *
 * Les phrases NMEA à checksum juste sont transmises au core (app_gps_line) dans tous les états :
 * un récepteur inconnu ou non configurable sert quand même, à son débit et à sa cadence.
 *
 * Licence : GPL-3.0.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ubx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPS_LINK_SEARCH = 0,     /* aucun récepteur entendu au débit courant : on essaie le suivant */
    GPS_LINK_IDENTIFY,       /* récepteur entendu, MON-VER demandé */
    GPS_LINK_VERIFY,         /* configuration envoyée et débit changé : on attend ses données */
    GPS_LINK_RUN,            /* récepteur en marche (configuré, ou inconnu et laissé tel quel) */
} gps_link_state_t;

typedef struct {
    /* Écrit des octets vers le récepteur. */
    void (*write)(const uint8_t *data, size_t n, void *user);
    /* Change le débit de l'UART. Appelé après un write : le HAL doit attendre la fin de l'émission
     * en cours avant de changer de débit (sinon la commande de changement de débit est perdue). */
    void (*set_baud)(uint32_t baud, void *user);
    /* Phrase NMEA à checksum juste, terminée par \0, sans \r\n. */
    void (*nmea)(const char *line, size_t len, void *user);
    void *user;
} gps_link_io_t;

typedef struct {
    const uint32_t *bauds;   /* débits essayés, dans l'ordre (le cible en premier conseillé) */
    size_t   n_bauds;
    uint32_t target_baud;    /* débit voulu après configuration, 0 : garder celui trouvé */
    uint16_t meas_ms;        /* période de mesure demandée (100 ms = 10 Hz) */
    uint8_t  dyn_model;      /* modèle dynamique (UBX_DYN_AUTOMOTIVE) */
    uint32_t probe_ms;       /* durée d'écoute par débit en recherche */
    uint32_t ver_timeout_ms; /* attente d'une réponse MON-VER avant de la redemander */
    uint8_t  ver_tries;      /* demandes MON-VER avant de laisser le récepteur tel quel */
    uint32_t absent_ms;      /* silence au-delà duquel le récepteur est déclaré absent */
} gps_link_cfg_t;

/* Réglages par défaut : débits 115 200, 38 400 (pucks réglés pour APM), 9 600 (défaut usine des
 * M8), cible 115 200, 10 Hz, automobile. */
void gps_link_default_cfg(gps_link_cfg_t *cfg);

typedef struct {
    uint32_t searches;       /* passages en recherche (démarrage, perte du récepteur) */
    uint32_t baud_tries;     /* débits essayés */
    uint32_t configs;        /* configurations envoyées */
    uint32_t acks, naks;     /* UBX-ACK reçus */
    uint32_t losses;         /* récepteur perdu après avoir marché */
} gps_link_stats_t;

typedef struct {
    gps_link_cfg_t   cfg;
    gps_link_io_t    io;
    ubx_stream_t     stream;
    gps_link_state_t state;
    size_t           baud_idx;
    uint32_t         baud;           /* débit courant de l'UART */
    uint64_t         t_state_ms;     /* entrée dans l'état courant */
    uint64_t         t_rx_ms;        /* dernière phrase ou trame valide */
    uint64_t         t_poll_ms;      /* dernière demande MON-VER */
    uint8_t          polls;
    bool             heard;          /* au moins une donnée valide depuis le démarrage */
    bool             identified;     /* MON-VER reçu */
    ubx_version_t    version;
    uint64_t         now_ms;         /* heure du dernier appel (pour les rappels du découpage) */
    gps_link_stats_t stats;
} gps_link_t;

void gps_link_init(gps_link_t *l, const gps_link_cfg_t *cfg, const gps_link_io_t *io, uint64_t now_ms);

/* Octets reçus de l'UART. */
void gps_link_feed(gps_link_t *l, const uint8_t *data, size_t n, uint64_t now_ms);

/* À appeler régulièrement (boucle principale) : délais de recherche, de MON-VER, d'absence. */
void gps_link_tick(gps_link_t *l, uint64_t now_ms);

/* Récepteur présent : données valides reçues il y a moins de absent_ms. Faux au démarrage tant
 * que rien n'a été entendu (« module GPS absent » quand le puck est débranché). */
bool gps_link_present(const gps_link_t *l, uint64_t now_ms);

gps_link_state_t gps_link_state(const gps_link_t *l);
const char *gps_link_state_name(gps_link_state_t s);
uint32_t gps_link_baud(const gps_link_t *l);

/* Version du récepteur (gen UBX_GEN_UNKNOWN tant que MON-VER n'a pas répondu). */
const ubx_version_t *gps_link_version(const gps_link_t *l);
const gps_link_stats_t *gps_link_stats(const gps_link_t *l);
const ubx_stream_stats_t *gps_link_stream_stats(const gps_link_t *l);

/* Vérifie la somme de contrôle d'une phrase NMEA « $...*hh ». Exposé pour les tests. */
bool gps_nmea_checksum_ok(const char *line, size_t len);

#ifdef __cplusplus
}
#endif
