/*
 * Raidmaster 403 — protocole UBX des récepteurs u-blox (core portable, PC et ESP32).
 *
 * - Découpage d'un flux d'octets du récepteur, où se mêlent phrases NMEA (« $...\r\n ») et trames
 *   UBX (B5 62 classe id longueur charge CK_A CK_B), en lignes NMEA et trames UBX vérifiées.
 * - Construction des trames à envoyer : interrogation UBX-MON-VER et configuration, en messages
 *   historiques pour les M8 (CFG-PRT, CFG-MSG, CFG-RATE, CFG-NAV5, puck NEO-M8N) ou en clés de
 *   configuration pour les M9 (UBX-CFG-VALSET, NEO-M9N de la carte fille).
 * - Lecture de UBX-MON-VER : génération du récepteur (M8, M9, M10…), module, version du protocole.
 *
 * Sources : u-blox M9 SPG 4.04 Interface description (UBX-21022436, clés CFG-*, VALSET, MON-VER) ;
 * u-blox 8 / M8 Receiver description (messages CFG historiques).
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

#define UBX_SYNC1        0xB5
#define UBX_SYNC2        0x62
#define UBX_OVERHEAD     8          /* 2 sync + classe + id + 2 longueur + 2 checksum */
#define UBX_MAX_PAYLOAD  512        /* MON-VER : 40 + 30 × extensions (une quinzaine au plus) */
#define UBX_NMEA_MAX     128        /* NMEA 0183 : 82 caractères ; marge pour les phrases u-blox */

/* Classes et identifiants utilisés. */
#define UBX_CLS_ACK      0x05
#define UBX_ID_ACK_NAK   0x00
#define UBX_ID_ACK_ACK   0x01
#define UBX_CLS_CFG      0x06
#define UBX_ID_CFG_PRT   0x00
#define UBX_ID_CFG_MSG   0x01
#define UBX_ID_CFG_RATE  0x08
#define UBX_ID_CFG_NAV5  0x24
#define UBX_ID_CFG_VALSET 0x8A
#define UBX_CLS_MON      0x0A
#define UBX_ID_MON_VER   0x04

/* ------------------------------------------------------------------ trames */

/* Somme de contrôle UBX (Fletcher 8 bits) sur classe, id, longueur et charge. */
void ubx_checksum(const uint8_t *data, size_t n, uint8_t *ck_a, uint8_t *ck_b);

/* Écrit dans out une trame complète ; renvoie sa longueur (len + 8), 0 si cap est trop petit
 * ou len trop grand. payload peut être NULL si len vaut 0. */
size_t ubx_frame(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t cap);

/* ------------------------------------------------------------------ découpage du flux */

typedef void (*ubx_nmea_cb)(const char *line, size_t len, void *user);
typedef void (*ubx_frame_cb)(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, void *user);

typedef struct {
    uint32_t nmea_lines;     /* phrases NMEA livrées (« $ » … fin de ligne ; checksum vérifié par le parseur) */
    uint32_t ubx_frames;     /* trames UBX à checksum juste */
    uint32_t ubx_bad;        /* trames UBX à checksum faux ou trop longues */
    uint32_t junk;           /* octets hors de toute phrase ou trame (débit faux, bruit) */
} ubx_stream_stats_t;

typedef struct {
    ubx_nmea_cb        on_nmea;
    ubx_frame_cb       on_frame;
    void              *user;
    uint8_t            state;
    uint16_t           pos;
    uint16_t           len;
    uint8_t            cls, id;
    char               nmea[UBX_NMEA_MAX + 1];
    uint8_t            buf[4 + UBX_MAX_PAYLOAD + 2];   /* classe, id, longueur, charge, CK */
    ubx_stream_stats_t stats;
} ubx_stream_t;

void ubx_stream_init(ubx_stream_t *s, ubx_nmea_cb on_nmea, ubx_frame_cb on_frame, void *user);

/* Donne des octets reçus ; appelle on_nmea pour chaque phrase complète (terminée par \0, sans
 * \r\n) et on_frame pour chaque trame UBX valide. */
void ubx_stream_feed(ubx_stream_t *s, const uint8_t *data, size_t n);

/* Remet le découpage à zéro (changement de débit : ce qui était en cours est perdu). */
void ubx_stream_reset(ubx_stream_t *s);

/* ------------------------------------------------------------------ UBX-MON-VER */

typedef enum {
    UBX_GEN_UNKNOWN = 0,
    UBX_GEN_6,
    UBX_GEN_7,
    UBX_GEN_8,           /* NEO-M8N des pucks de drone */
    UBX_GEN_9,           /* NEO-M9N de la carte fille */
    UBX_GEN_10,
} ubx_generation_t;

typedef struct {
    ubx_generation_t gen;
    char  sw[31];        /* swVersion, ex. « ROM SPG 4.04 (7f89f7) » */
    char  hw[11];        /* hwVersion, ex. « 00190000 » */
    char  module[31];    /* extension « MOD= », ex. « NEO-M9N », vide si absente */
    char  protver[11];   /* extension « PROTVER= », ex. « 32.01 », vide si absente */
} ubx_version_t;

/* Décode la charge d'une réponse MON-VER ; false si elle est trop courte. */
bool ubx_mon_ver_decode(const uint8_t *payload, uint16_t len, ubx_version_t *out);

const char *ubx_generation_name(ubx_generation_t gen);

/* ------------------------------------------------------------------ trames de configuration */

/* Réglages communs aux deux générations. */
typedef struct {
    uint16_t meas_ms;    /* période de mesure : 100 ms = 10 Hz */
    uint32_t baud;       /* débit de l'UART1 du récepteur après configuration */
    uint8_t  dyn_model;  /* modèle dynamique : 4 = automobile */
} ubx_cfg_t;

#define UBX_DYN_AUTOMOTIVE 4

/* Interrogation MON-VER (trame sans charge). */
size_t ubx_build_mon_ver_poll(uint8_t *out, size_t cap);

/* M9 et suivants : un UBX-CFG-VALSET en RAM (renvoyé à chaque démarrage) avec cadence, modèle
 * dynamique, sorties NMEA de l'UART1 (RMC et GGA seules) ; le débit n'y est pas (voir
 * ubx_build_m9_baud, à envoyer en dernier puisque le récepteur change de débit aussitôt). */
size_t ubx_build_m9_config(const ubx_cfg_t *cfg, uint8_t *out, size_t cap);
size_t ubx_build_m9_baud(uint32_t baud, uint8_t *out, size_t cap);

/* M8 : suite de messages historiques écrits bout à bout dans out (CFG-RATE, CFG-NAV5, CFG-MSG
 * pour RMC et GGA à 1 et GSV, GSA, GLL, VTG à 0) ; le débit est à part (ubx_build_m8_baud :
 * CFG-PRT de l'UART1, UBX et NMEA en entrée et en sortie, 8N1). */
size_t ubx_build_m8_config(const ubx_cfg_t *cfg, uint8_t *out, size_t cap);
size_t ubx_build_m8_baud(uint32_t baud, uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif
