/*
 * Raidmaster 403 — hal_gps sur PC : rejeu d'un fichier .nmea ou trajet simulé.
 *
 * Rejeu : les lignes sont livrées au rythme de leur heure NMEA (2e champ hhmmss.ss), multiplié
 * par le facteur de vitesse ; les lignes sans heure partent avec la précédente. Fin de fichier :
 * arrêt, ou retour au début si loop. Signal coupé : les lignes sont jetées (le core voit le fix
 * se perdre).
 *
 * Trajet simulé (sans fichier) : core/gps_synth (le même que le GPS de démonstration du
 * firmware), parti à l'heure UTC de la machine.
 *
 * Liaison : comme sur la carte, les lignes passent par core/gps_link, face à un NEO-M8N émulé
 * (octets à son débit, réponses à MON-VER et aux CFG-*, ACK). Il démarre déjà réglé à 115 200
 * bauds (ESP32 redémarré seul) ; débranché puis rebranché (touche `g`), il repart à 9 600 bauds,
 * débit d'usine : la liaison le cherche, le reconnaît et le règle de nouveau, sans données
 * pendant ce temps. Le vrai récepteur émet même sans fix ; le rejeu, qui jette les lignes quand le
 * signal est coupé, le remplace par une trame UBX par seconde.
 *
 * Licence : GPL-3.0.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gps_link.h"
#include "gps_synth.h"
#include "hal/hal_gps.h"
#include "hal/hal_time.h"
#include "hal_sim.h"

static hal_gps_line_cb s_cb;
static void           *s_user;
static double          s_speed = 1.0;
static bool            s_loop;
static bool            s_signal = true;
static uint64_t        s_t0_ms;           /* hal_time au démarrage (ou au dernier rebouclage) */

/* Rejeu */
static FILE    *s_file;
static char     s_pending[256];
static bool     s_have_pending;
static int64_t  s_file_t0 = -1;           /* heure NMEA de la première ligne datée (ms) */
static int64_t  s_last_tod = -1;
static int64_t  s_wrap_ms;                /* +24 h à chaque passage de minuit */

/* Trajet simulé */
static gps_synth_t s_synth;

/* Cap vrai du véhicule (pour la boussole émulée) */
static double s_heading = 90.0;

/* Liaison et récepteur émulé */
static gps_link_t   s_link;
static uint32_t     s_link_baud;
static bool         s_plugged = true;
static uint32_t     s_rx_baud = 115200;
static uint32_t     s_rx_new_baud;
static ubx_stream_t s_rx_in;
static uint8_t      s_rx_out[512];
static size_t       s_rx_n_out;
static uint64_t     s_next_beat_ms;

bool hal_gps_sim_config(const char *path, double speed, bool loop)
{
    s_speed = speed > 0.0 ? speed : 1.0;
    hal_time_sim_clock_rate(s_speed);      /* l'horloge de la carte suit le temps simulé */
    s_loop = loop;
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    if (path != NULL) {
        s_file = fopen(path, "r");
        if (s_file == NULL) return false;
    }
    return true;
}

void hal_gps_sim_set_signal(bool on)
{
    s_signal = on;
}

bool hal_gps_sim_signal(void)
{
    return s_signal;
}

/* ------------------------------------------------------------------ récepteur émulé */

static void rx_queue(const uint8_t *d, size_t n)
{
    if (s_rx_n_out + n <= sizeof(s_rx_out)) {
        memcpy(s_rx_out + s_rx_n_out, d, n);
        s_rx_n_out += n;
    }
}

static void rx_cmd(uint8_t cls, uint8_t id, const uint8_t *p, uint16_t len, void *user)
{
    (void)user;
    uint8_t f[160];
    if (cls == UBX_CLS_MON && id == UBX_ID_MON_VER && len == 0) {
        uint8_t pl[40 + 3 * 30] = {0};
        memcpy(pl, "ROM CORE 3.01 (107888)", 22);
        memcpy(pl + 30, "00080000", 8);
        memcpy(pl + 40, "FWVER=SPG 3.01", 14);
        memcpy(pl + 70, "PROTVER=18.00", 13);
        memcpy(pl + 100, "MOD=NEO-M8N (sim)", 17);
        rx_queue(f, ubx_frame(UBX_CLS_MON, UBX_ID_MON_VER, pl, sizeof(pl), f, sizeof(f)));
    } else if (cls == UBX_CLS_CFG) {
        if (id == UBX_ID_CFG_PRT && len >= 12 && p[0] == 1)
            s_rx_new_baud = (uint32_t)p[8] | (uint32_t)p[9] << 8 | (uint32_t)p[10] << 16 | (uint32_t)p[11] << 24;
        const uint8_t ack[2] = {cls, id};
        rx_queue(f, ubx_frame(UBX_CLS_ACK, UBX_ID_ACK_ACK, ack, 2, f, sizeof(f)));
    }
}

/* Octets du récepteur vers la liaison : lisibles au même débit, bruit sinon. */
static void rx_send(const uint8_t *d, size_t n)
{
    if (!s_plugged) return;
    if (s_rx_baud == s_link_baud) {
        gps_link_feed(&s_link, d, n, hal_time_ms());
    } else {
        uint8_t junk[32];
        for (size_t i = 0; i < sizeof(junk); i++) junk[i] = (uint8_t)(0x80u | (i * 37u));
        gps_link_feed(&s_link, junk, n < sizeof(junk) ? n : sizeof(junk), hal_time_ms());
    }
}

static void link_write(const uint8_t *d, size_t n, void *user)
{
    (void)user;
    if (!s_plugged || s_rx_baud != s_link_baud) return;
    ubx_stream_feed(&s_rx_in, d, n);
    if (s_rx_new_baud != 0) {              /* nouveau débit après la réponse, comme les u-blox */
        s_rx_baud = s_rx_new_baud;
        s_rx_new_baud = 0;
    }
}

static void link_set_baud(uint32_t baud, void *user)
{
    (void)user;
    s_link_baud = baud;
}

static void link_nmea(const char *line, size_t len, void *user)
{
    (void)user;
    if (s_cb != NULL) s_cb(line, len, s_user);
}

void hal_gps_sim_set_plugged(bool on)
{
    if (on && !s_plugged) {
        s_rx_baud = 9600;                  /* rebranché : débit d'usine, réglages perdus */
        s_rx_new_baud = 0;
        s_rx_n_out = 0;
        ubx_stream_reset(&s_rx_in);
    }
    s_plugged = on;
}

bool hal_gps_sim_plugged(void)
{
    return s_plugged;
}

bool hal_gps_present(void)
{
    return gps_link_present(&s_link, hal_time_ms());
}

double hal_gps_sim_heading(void)
{
    return s_heading;
}

/* Cap vrai d'une RMC rejouée (champ 8, si la vitesse dépasse 1 nœud). */
static void heading_from_rmc(const char *l)
{
    if (strncmp(l + 3, "RMC", 3) != 0) return;
    const char *f[10] = {0};
    int k = 0;
    for (const char *c = l; *c && k < 10; c++)
        if (*c == ',') f[k++] = c + 1;
    if (k < 9 || f[6][0] == ',' || f[7][0] == ',') return;
    if (atof(f[6]) > 1.0) s_heading = atof(f[7]);
}

/* ------------------------------------------------------------------ init */

void hal_gps_init(hal_gps_line_cb cb, void *user)
{
    s_cb = cb;
    s_user = user;
    s_t0_ms = hal_time_ms();
    ubx_stream_init(&s_rx_in, NULL, rx_cmd, NULL);
    const gps_link_io_t io = {.write = link_write, .set_baud = link_set_baud, .nmea = link_nmea};
    gps_link_init(&s_link, NULL, &io, hal_time_ms());
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    gps_synth_init(&s_synth, (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint64_t sim_elapsed_ms(void)
{
    return (uint64_t)((double)(hal_time_ms() - s_t0_ms) * s_speed);
}

/* Ligne émise par le récepteur émulé. */
static void emit(const char *line)
{
    heading_from_rmc(line);
    rx_send((const uint8_t *)line, strlen(line));
}

/* ------------------------------------------------------------------ rejeu */

/* Heure NMEA de la ligne en ms depuis minuit, -1 si absente. */
static int64_t line_tod(const char *l)
{
    const char *c = strchr(l, ',');
    if (c == NULL) return -1;
    c++;
    for (int i = 0; i < 6; i++)
        if (!isdigit((unsigned char)c[i])) return -1;
    int64_t t = ((c[0] - '0') * 10 + (c[1] - '0')) * 3600000LL + ((c[2] - '0') * 10 + (c[3] - '0')) * 60000LL +
                ((c[4] - '0') * 10 + (c[5] - '0')) * 1000LL;
    if (c[6] == '.') {
        int scale = 100;
        for (int i = 7; isdigit((unsigned char)c[i]) && scale > 0; i++, scale /= 10)
            t += (c[i] - '0') * scale;
    }
    return t;
}

static void poll_replay(void)
{
    const uint64_t elapsed = sim_elapsed_ms();
    for (int budget = 2000; budget > 0; budget--) {
        if (!s_have_pending) {
            if (fgets(s_pending, sizeof(s_pending), s_file) == NULL) {
                if (!s_loop) return;
                rewind(s_file);
                s_file_t0 = -1;
                s_last_tod = -1;
                s_wrap_ms = 0;
                s_t0_ms = hal_time_ms();
                return;
            }
            s_have_pending = true;
        }
        int64_t tod = line_tod(s_pending);
        if (tod >= 0) {
            if (s_last_tod >= 0 && tod + 12 * 3600000LL < s_last_tod) s_wrap_ms += 24 * 3600000LL;
            s_last_tod = tod;
            tod += s_wrap_ms;
            if (s_file_t0 < 0) s_file_t0 = tod;
            if ((uint64_t)(tod - s_file_t0) > elapsed) return;     /* pas encore l'heure */
        }
        if (s_signal) emit(s_pending);
        s_have_pending = false;
    }
}

/* ------------------------------------------------------------------ trajet simulé */

static void synth_line(const char *line, size_t len, void *user)
{
    (void)user;
    (void)len;
    emit(line);
}

void hal_gps_poll(void)
{
    if (s_rx_n_out > 0) {                   /* réponses UBX en attente */
        const size_t n = s_rx_n_out;
        s_rx_n_out = 0;
        rx_send(s_rx_out, n);
    }
    if (s_file != NULL) {
        poll_replay();
        const uint64_t now = hal_time_ms();
        if (!s_signal && now >= s_next_beat_ms) {   /* signal coupé : le récepteur reste vivant */
            s_next_beat_ms = now + 1000;
            uint8_t f[16];
            rx_send(f, ubx_frame(0x01, 0x03, NULL, 0, f, sizeof(f)));
        }
    } else {
        gps_synth_run(&s_synth, sim_elapsed_ms(), s_signal, synth_line, NULL);
        s_heading = s_synth.head;
    }
    gps_link_tick(&s_link, hal_time_ms());
}
