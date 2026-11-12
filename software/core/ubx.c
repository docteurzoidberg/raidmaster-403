/*
 * Raidmaster 403 — protocole UBX. Voir core/include/ubx.h.
 *
 * Licence : GPL-3.0.
 */

#include "ubx.h"

#include <string.h>

/* ------------------------------------------------------------------ trames */

void ubx_checksum(const uint8_t *data, size_t n, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (uint8_t)(a + data[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

size_t ubx_frame(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t cap)
{
    if (len > UBX_MAX_PAYLOAD || cap < (size_t)len + UBX_OVERHEAD) return 0;
    out[0] = UBX_SYNC1;
    out[1] = UBX_SYNC2;
    out[2] = cls;
    out[3] = id;
    out[4] = (uint8_t)(len & 0xFF);
    out[5] = (uint8_t)(len >> 8);
    if (len > 0) memcpy(out + 6, payload, len);
    ubx_checksum(out + 2, (size_t)len + 4, &out[6 + len], &out[7 + len]);
    return (size_t)len + UBX_OVERHEAD;
}

/* ------------------------------------------------------------------ découpage du flux */

enum { S_IDLE, S_NMEA, S_SYNC2, S_HEAD, S_BODY };

void ubx_stream_init(ubx_stream_t *s, ubx_nmea_cb on_nmea, ubx_frame_cb on_frame, void *user)
{
    memset(s, 0, sizeof(*s));
    s->on_nmea = on_nmea;
    s->on_frame = on_frame;
    s->user = user;
}

void ubx_stream_reset(ubx_stream_t *s)
{
    s->state = S_IDLE;
    s->pos = 0;
}

static void start_byte(ubx_stream_t *s, uint8_t c)
{
    if (c == '$') {
        s->state = S_NMEA;
        s->nmea[0] = '$';
        s->pos = 1;
    } else if (c == UBX_SYNC1) {
        s->state = S_SYNC2;
    } else if (c != '\r' && c != '\n') {
        s->stats.junk++;
    }
}

void ubx_stream_feed(ubx_stream_t *s, const uint8_t *data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const uint8_t c = data[i];
        switch (s->state) {
        case S_IDLE:
            start_byte(s, c);
            break;

        case S_NMEA:
            if (c == '\r' || c == '\n') {
                s->nmea[s->pos] = '\0';
                s->stats.nmea_lines++;
                if (s->on_nmea != NULL) s->on_nmea(s->nmea, s->pos, s->user);
                s->state = S_IDLE;
            } else if (c == '$' || c == UBX_SYNC1 || c < 0x20 || c > 0x7E || s->pos >= UBX_NMEA_MAX) {
                /* phrase tronquée (débit faux, trame UBX au milieu) : on la jette */
                s->stats.junk += s->pos;
                s->state = S_IDLE;
                start_byte(s, c);
            } else {
                s->nmea[s->pos++] = (char)c;
            }
            break;

        case S_SYNC2:
            if (c == UBX_SYNC2) {
                s->state = S_HEAD;
                s->pos = 0;
            } else {
                s->stats.junk++;
                s->state = S_IDLE;
                start_byte(s, c);
            }
            break;

        case S_HEAD:
            s->buf[s->pos++] = c;
            if (s->pos == 4) {
                s->cls = s->buf[0];
                s->id = s->buf[1];
                s->len = (uint16_t)(s->buf[2] | (s->buf[3] << 8));
                if (s->len > UBX_MAX_PAYLOAD) {
                    s->stats.ubx_bad++;
                    s->state = S_IDLE;
                } else {
                    s->state = S_BODY;
                }
            }
            break;

        case S_BODY:
            s->buf[s->pos++] = c;
            if (s->pos == 4 + s->len + 2) {
                uint8_t a, b;
                ubx_checksum(s->buf, (size_t)4 + s->len, &a, &b);
                if (a == s->buf[4 + s->len] && b == s->buf[5 + s->len]) {
                    s->stats.ubx_frames++;
                    if (s->on_frame != NULL) s->on_frame(s->cls, s->id, s->buf + 4, s->len, s->user);
                } else {
                    s->stats.ubx_bad++;
                }
                s->state = S_IDLE;
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ UBX-MON-VER */

static void copy_field(char *dst, size_t cap, const uint8_t *src, size_t n)
{
    size_t k = 0;
    while (k < n && k + 1 < cap && src[k] != '\0') {
        dst[k] = (char)src[k];
        k++;
    }
    dst[k] = '\0';
}

static ubx_generation_t generation_from_hw(const char *hw)
{
    /* hwVersion : « 00080000 » M8, « 00190000 » M9, « 000A0000 » M10, « 00070000 » M7, « 00040007 » u-blox 6 */
    if (strcmp(hw, "00080000") == 0) return UBX_GEN_8;
    if (strcmp(hw, "00190000") == 0) return UBX_GEN_9;
    if (strcmp(hw, "000A0000") == 0) return UBX_GEN_10;
    if (strcmp(hw, "00070000") == 0) return UBX_GEN_7;
    if (strcmp(hw, "00040007") == 0) return UBX_GEN_6;
    return UBX_GEN_UNKNOWN;
}

static ubx_generation_t generation_from_module(const char *mod)
{
    /* « NEO-M8N », « NEO-M9N », « MAX-M10S »… : le chiffre après le « M » de la série */
    const char *m = strstr(mod, "-M");
    if (m == NULL) return UBX_GEN_UNKNOWN;
    m += 2;
    if (m[0] == '1' && m[1] == '0') return UBX_GEN_10;
    if (m[0] == '9') return UBX_GEN_9;
    if (m[0] == '8') return UBX_GEN_8;
    if (m[0] == '7') return UBX_GEN_7;
    return UBX_GEN_UNKNOWN;
}

bool ubx_mon_ver_decode(const uint8_t *payload, uint16_t len, ubx_version_t *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 40) return false;
    copy_field(out->sw, sizeof(out->sw), payload, 30);
    copy_field(out->hw, sizeof(out->hw), payload + 30, 10);
    for (uint16_t off = 40; off + 30 <= len; off += 30) {
        char ext[31];
        copy_field(ext, sizeof(ext), payload + off, 30);
        if (strncmp(ext, "MOD=", 4) == 0) {
            copy_field(out->module, sizeof(out->module), (const uint8_t *)ext + 4, 26);
        } else if (strncmp(ext, "PROTVER=", 8) == 0 || strncmp(ext, "PROTVER ", 8) == 0) {
            /* « PROTVER 18.00 » : anciens firmwares */
            copy_field(out->protver, sizeof(out->protver), (const uint8_t *)ext + 8, 22);
        }
    }
    out->gen = generation_from_hw(out->hw);
    if (out->gen == UBX_GEN_UNKNOWN && out->module[0] != '\0') out->gen = generation_from_module(out->module);
    return true;
}

const char *ubx_generation_name(ubx_generation_t gen)
{
    switch (gen) {
    case UBX_GEN_6:  return "u-blox 6";
    case UBX_GEN_7:  return "u-blox 7";
    case UBX_GEN_8:  return "u-blox M8";
    case UBX_GEN_9:  return "u-blox M9";
    case UBX_GEN_10: return "u-blox M10";
    default:         return "inconnu";
    }
}

/* ------------------------------------------------------------------ trames de configuration */

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

size_t ubx_build_mon_ver_poll(uint8_t *out, size_t cap)
{
    return ubx_frame(UBX_CLS_MON, UBX_ID_MON_VER, NULL, 0, out, cap);
}

/* Clés de configuration M9 (UBX-21022436 § 6) ; la taille de la valeur est dans les bits 28-30. */
#define KEY_RATE_MEAS        0x30210001u   /* U2, ms */
#define KEY_RATE_NAV         0x30210002u   /* U2, mesures par solution */
#define KEY_NAVSPG_DYNMODEL  0x20110021u   /* E1 */
#define KEY_UART1_BAUDRATE   0x40520001u   /* U4 */
#define KEY_UART1INPROT_UBX  0x10730001u   /* L */
#define KEY_UART1OUTPROT_NMEA 0x10740002u  /* L */
#define KEY_NMEA_GGA_UART1   0x209100bbu   /* U1, sorties par solution */
#define KEY_NMEA_RMC_UART1   0x209100acu
#define KEY_NMEA_GSV_UART1   0x209100c5u
#define KEY_NMEA_GSA_UART1   0x209100c0u
#define KEY_NMEA_GLL_UART1   0x209100cau
#define KEY_NMEA_VTG_UART1   0x209100b1u

static size_t value_size(uint32_t key)
{
    switch ((key >> 28) & 0x7) {
    case 1:  return 1;     /* L : un octet */
    case 2:  return 1;
    case 3:  return 2;
    case 4:  return 4;
    case 5:  return 8;
    default: return 0;
    }
}

static size_t put_kv(uint8_t *p, uint32_t key, uint32_t value)
{
    put_u32(p, key);
    const size_t n = value_size(key);
    for (size_t i = 0; i < n && i < 4; i++) p[4 + i] = (uint8_t)(value >> (8 * i));
    return 4 + n;
}

static size_t valset(const uint32_t (*kv)[2], size_t count, uint8_t *out, size_t cap)
{
    uint8_t pl[4 + 12 * 8];
    if (count > 12) return 0;
    pl[0] = 0;              /* version 0 : sans transaction */
    pl[1] = 0x01;           /* couche RAM seulement : la configuration est renvoyée à chaque démarrage */
    pl[2] = pl[3] = 0;
    size_t n = 4;
    for (size_t i = 0; i < count; i++) n += put_kv(pl + n, kv[i][0], kv[i][1]);
    return ubx_frame(UBX_CLS_CFG, UBX_ID_CFG_VALSET, pl, (uint16_t)n, out, cap);
}

size_t ubx_build_m9_config(const ubx_cfg_t *cfg, uint8_t *out, size_t cap)
{
    const uint32_t kv[][2] = {
        {KEY_RATE_MEAS, cfg->meas_ms},
        {KEY_RATE_NAV, 1},
        {KEY_NAVSPG_DYNMODEL, cfg->dyn_model},
        {KEY_UART1INPROT_UBX, 1},
        {KEY_UART1OUTPROT_NMEA, 1},
        {KEY_NMEA_RMC_UART1, 1},
        {KEY_NMEA_GGA_UART1, 1},
        {KEY_NMEA_GSV_UART1, 0},
        {KEY_NMEA_GSA_UART1, 0},
        {KEY_NMEA_GLL_UART1, 0},
        {KEY_NMEA_VTG_UART1, 0},
    };
    return valset(kv, sizeof(kv) / sizeof(kv[0]), out, cap);
}

size_t ubx_build_m9_baud(uint32_t baud, uint8_t *out, size_t cap)
{
    const uint32_t kv[][2] = {{KEY_UART1_BAUDRATE, baud}};
    return valset(kv, 1, out, cap);
}

static size_t cfg_msg(uint8_t nmea_id, uint8_t rate, uint8_t *out, size_t cap)
{
    const uint8_t pl[3] = {0xF0, nmea_id, rate};      /* classe NMEA standard 0xF0, débit sur le port courant */
    return ubx_frame(UBX_CLS_CFG, UBX_ID_CFG_MSG, pl, sizeof(pl), out, cap);
}

size_t ubx_build_m8_config(const ubx_cfg_t *cfg, uint8_t *out, size_t cap)
{
    size_t n = 0, k;
    uint8_t rate[6];
    put_u16(rate, cfg->meas_ms);
    put_u16(rate + 2, 1);              /* navRate */
    put_u16(rate + 4, 1);              /* timeRef : GPS */
    if ((k = ubx_frame(UBX_CLS_CFG, UBX_ID_CFG_RATE, rate, sizeof(rate), out + n, cap - n)) == 0) return 0;
    n += k;

    uint8_t nav5[36] = {0};
    put_u16(nav5, 0x0001);             /* mask : modèle dynamique seulement */
    nav5[2] = cfg->dyn_model;
    if ((k = ubx_frame(UBX_CLS_CFG, UBX_ID_CFG_NAV5, nav5, sizeof(nav5), out + n, cap - n)) == 0) return 0;
    n += k;

    /* NMEA standard : GGA 0x00, GLL 0x01, GSA 0x02, GSV 0x03, RMC 0x04, VTG 0x05 */
    static const uint8_t msgs[][2] = {{0x04, 1}, {0x00, 1}, {0x03, 0}, {0x02, 0}, {0x01, 0}, {0x05, 0}};
    for (size_t i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++) {
        if (cap - n < UBX_OVERHEAD + 3) return 0;
        n += cfg_msg(msgs[i][0], msgs[i][1], out + n, cap - n);
    }
    return n;
}

size_t ubx_build_m8_baud(uint32_t baud, uint8_t *out, size_t cap)
{
    uint8_t pl[20] = {0};
    pl[0] = 1;                         /* portID : UART1 */
    put_u32(pl + 4, 0x000008D0);       /* mode : 8 bits, sans parité, 1 bit de stop */
    put_u32(pl + 8, baud);
    put_u16(pl + 12, 0x0003);          /* inProtoMask : UBX + NMEA */
    put_u16(pl + 14, 0x0003);          /* outProtoMask : UBX + NMEA */
    return ubx_frame(UBX_CLS_CFG, UBX_ID_CFG_PRT, pl, sizeof(pl), out, cap);
}
