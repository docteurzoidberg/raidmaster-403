/*
 * Raidmaster 403 — liaison avec le récepteur GPS. Voir core/include/gps_link.h.
 *
 * Licence : GPL-3.0.
 */

#include "gps_link.h"

#include <string.h>

static const uint32_t DEFAULT_BAUDS[] = {115200, 38400, 9600};

void gps_link_default_cfg(gps_link_cfg_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->bauds = DEFAULT_BAUDS;
    cfg->n_bauds = sizeof(DEFAULT_BAUDS) / sizeof(DEFAULT_BAUDS[0]);
    cfg->target_baud = 115200;
    cfg->meas_ms = 100;
    cfg->dyn_model = UBX_DYN_AUTOMOTIVE;
    cfg->probe_ms = 1500;          /* > 1 s : un récepteur à 1 Hz émet au moins une phrase */
    cfg->ver_timeout_ms = 600;
    cfg->ver_tries = 3;
    cfg->absent_ms = 3000;
}

bool gps_nmea_checksum_ok(const char *line, size_t len)
{
    if (len < 4 || line[0] != '$') return false;
    uint8_t x = 0;
    size_t i = 1;
    for (; i < len && line[i] != '*'; i++) x ^= (uint8_t)line[i];
    if (i + 3 > len) return false;          /* « * » absent ou moins de deux chiffres après */
    unsigned v = 0;
    for (size_t k = i + 1; k <= i + 2; k++) {
        const char c = line[k];
        v <<= 4;
        if (c >= '0' && c <= '9')      v |= (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else return false;
    }
    return v == x;
}

/* ------------------------------------------------------------------ émission */

static void send(gps_link_t *l, const uint8_t *data, size_t n)
{
    if (n > 0 && l->io.write != NULL) l->io.write(data, n, l->io.user);
}

static void poll_version(gps_link_t *l)
{
    uint8_t f[UBX_OVERHEAD];
    send(l, f, ubx_build_mon_ver_poll(f, sizeof(f)));
    l->t_poll_ms = l->now_ms;
    l->polls++;
}

static void set_baud(gps_link_t *l, uint32_t baud)
{
    l->baud = baud;
    ubx_stream_reset(&l->stream);
    if (l->io.set_baud != NULL) l->io.set_baud(baud, l->io.user);
}

static void enter(gps_link_t *l, gps_link_state_t s)
{
    l->state = s;
    l->t_state_ms = l->now_ms;
    l->polls = 0;
}

/* Essaie un débit de la liste : changement de débit puis MON-VER (un récepteur qui n'émet que de
 * l'UBX se signale ainsi). */
static void try_baud(gps_link_t *l, size_t idx)
{
    l->baud_idx = idx % (l->cfg.n_bauds > 0 ? l->cfg.n_bauds : 1);
    l->stats.baud_tries++;
    if (l->cfg.n_bauds > 0) set_baud(l, l->cfg.bauds[l->baud_idx]);
    enter(l, GPS_LINK_SEARCH);
    poll_version(l);
}

static void start_search(gps_link_t *l, uint32_t first_baud)
{
    l->stats.searches++;
    l->identified = false;
    memset(&l->version, 0, sizeof(l->version));
    size_t idx = 0;
    for (size_t i = 0; i < l->cfg.n_bauds; i++)
        if (l->cfg.bauds[i] == first_baud) idx = i;
    try_baud(l, idx);
}

/* Configuration selon la génération, puis débit cible en dernier. */
static void configure(gps_link_t *l)
{
    const ubx_cfg_t c = {.meas_ms = l->cfg.meas_ms, .baud = l->cfg.target_baud, .dyn_model = l->cfg.dyn_model};
    uint8_t buf[256];
    const bool m9 = l->version.gen >= UBX_GEN_9;
    const bool legacy = l->version.gen >= UBX_GEN_6 && l->version.gen <= UBX_GEN_8;
    if (!m9 && !legacy) {               /* inconnu : laissé tel quel, NMEA à son débit */
        enter(l, GPS_LINK_RUN);
        return;
    }
    l->stats.configs++;
    send(l, buf, m9 ? ubx_build_m9_config(&c, buf, sizeof(buf)) : ubx_build_m8_config(&c, buf, sizeof(buf)));
    if (l->cfg.target_baud != 0 && l->cfg.target_baud != l->baud) {
        send(l, buf, m9 ? ubx_build_m9_baud(l->cfg.target_baud, buf, sizeof(buf))
                        : ubx_build_m8_baud(l->cfg.target_baud, buf, sizeof(buf)));
        set_baud(l, l->cfg.target_baud);
        enter(l, GPS_LINK_VERIFY);
    } else {
        enter(l, GPS_LINK_RUN);
    }
}

/* ------------------------------------------------------------------ réception */

static void heard(gps_link_t *l)
{
    l->t_rx_ms = l->now_ms;
    l->heard = true;
    if (l->state == GPS_LINK_SEARCH) {
        enter(l, GPS_LINK_IDENTIFY);
        poll_version(l);
    } else if (l->state == GPS_LINK_VERIFY) {
        enter(l, GPS_LINK_RUN);
    }
}

static void on_nmea(const char *line, size_t len, void *user)
{
    gps_link_t *l = user;
    if (!gps_nmea_checksum_ok(line, len)) return;
    heard(l);
    if (l->io.nmea != NULL) l->io.nmea(line, len, l->io.user);
}

static void on_frame(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, void *user)
{
    gps_link_t *l = user;
    const gps_link_state_t before = l->state;
    heard(l);
    if (cls == UBX_CLS_ACK) {
        if (id == UBX_ID_ACK_ACK) l->stats.acks++;
        else if (id == UBX_ID_ACK_NAK) l->stats.naks++;
    } else if (cls == UBX_CLS_MON && id == UBX_ID_MON_VER && !l->identified) {
        if (ubx_mon_ver_decode(payload, len, &l->version)) {
            l->identified = true;
            if (before == GPS_LINK_SEARCH || l->state == GPS_LINK_IDENTIFY) configure(l);
        }
    }
}

/* ------------------------------------------------------------------ API */

void gps_link_init(gps_link_t *l, const gps_link_cfg_t *cfg, const gps_link_io_t *io, uint64_t now_ms)
{
    memset(l, 0, sizeof(*l));
    if (cfg != NULL) l->cfg = *cfg;
    else             gps_link_default_cfg(&l->cfg);
    if (io != NULL) l->io = *io;
    l->now_ms = now_ms;
    ubx_stream_init(&l->stream, on_nmea, on_frame, l);
    start_search(l, l->cfg.n_bauds > 0 ? l->cfg.bauds[0] : 0);
}

void gps_link_feed(gps_link_t *l, const uint8_t *data, size_t n, uint64_t now_ms)
{
    l->now_ms = now_ms;
    ubx_stream_feed(&l->stream, data, n);
}

void gps_link_tick(gps_link_t *l, uint64_t now_ms)
{
    l->now_ms = now_ms;
    const uint64_t in_state = now_ms - l->t_state_ms;
    switch (l->state) {
    case GPS_LINK_SEARCH:
        if (in_state >= l->cfg.probe_ms) try_baud(l, l->baud_idx + 1);
        break;
    case GPS_LINK_IDENTIFY:
        if (now_ms - l->t_rx_ms >= l->cfg.absent_ms) {
            start_search(l, l->baud);
        } else if (now_ms - l->t_poll_ms >= l->cfg.ver_timeout_ms) {
            if (l->polls < l->cfg.ver_tries) poll_version(l);
            else enter(l, GPS_LINK_RUN);         /* pas de réponse UBX : NMEA seul, tel quel */
        }
        break;
    case GPS_LINK_VERIFY:
        if (in_state >= l->cfg.probe_ms) start_search(l, l->baud);   /* le nouveau débit n'a pas pris */
        break;
    case GPS_LINK_RUN:
        if (now_ms - l->t_rx_ms >= l->cfg.absent_ms) {
            l->stats.losses++;
            start_search(l, l->baud);            /* débranché : on le cherche, au dernier débit d'abord */
        }
        break;
    }
}

bool gps_link_present(const gps_link_t *l, uint64_t now_ms)
{
    return l->heard && l->state != GPS_LINK_SEARCH && now_ms - l->t_rx_ms < l->cfg.absent_ms;
}

gps_link_state_t gps_link_state(const gps_link_t *l)
{
    return l->state;
}

const char *gps_link_state_name(gps_link_state_t s)
{
    switch (s) {
    case GPS_LINK_SEARCH:   return "recherche";
    case GPS_LINK_IDENTIFY: return "identification";
    case GPS_LINK_VERIFY:   return "vérification";
    case GPS_LINK_RUN:      return "marche";
    default:                return "?";
    }
}

uint32_t gps_link_baud(const gps_link_t *l)
{
    return l->baud;
}

const ubx_version_t *gps_link_version(const gps_link_t *l)
{
    return &l->version;
}

const gps_link_stats_t *gps_link_stats(const gps_link_t *l)
{
    return &l->stats;
}

const ubx_stream_stats_t *gps_link_stream_stats(const gps_link_t *l)
{
    return &l->stream.stats;
}
