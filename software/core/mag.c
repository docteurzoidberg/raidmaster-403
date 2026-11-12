/*
 * Raidmaster 403 — magnétomètre sur I²C. Voir core/include/mag.h.
 *
 * Registres d'après les fiches techniques : QMC5883L (QST, v1.0), IST8310 (iSentek), HMC5883L
 * (Honeywell), LIS3MDL (ST). Toutes les puces ont un repère direct, Z vers le haut du boîtier.
 *
 * Licence : GPL-3.0.
 */

#include "mag.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979f

/* Adresses essayées : QMC5883L 0x0D, IST8310 0x0C à 0x0F (0x0E sur les pucks), LIS3MDL 0x1C / 0x1E,
 * HMC5883L 0x1E. */
static const uint8_t ADDRS[] = {0x0D, 0x0E, 0x0C, 0x0F, 0x1E, 0x1C};

/* ------------------------------------------------------------------ registres */

#define QMC_DATA      0x00
#define QMC_STATUS    0x06      /* bit 0 DRDY, bit 1 OVL */
#define QMC_CTRL1     0x09
#define QMC_SETRESET  0x0B
#define QMC_ID        0x0D      /* 0xFF */
#define QMC_CTRL1_VAL 0x1D      /* continu, 200 Hz, ±8 G, OSR 512 */
#define QMC_LSB_PER_G 3000.0f

#define IST_WAI       0x00      /* 0x10 */
#define IST_STAT1     0x02      /* bit 0 DRDY */
#define IST_DATA      0x03
#define IST_CNTL1     0x0A      /* 0x01 : une mesure */
#define IST_AVGCNTL   0x41
#define IST_PDCNTL    0x42
#define IST_UT_PER_LSB 0.3f

#define HMC_CRA       0x00
#define HMC_CRB       0x01
#define HMC_MODE      0x02
#define HMC_DATA      0x03      /* X, Z, Y, poids fort d'abord */
#define HMC_STATUS    0x09      /* bit 0 RDY */
#define HMC_ID        0x0A      /* « H43 » */
#define HMC_LSB_PER_G 1090.0f   /* gain ±1,3 G */
#define HMC_OVERFLOW  (-4096)

#define LIS_WAI       0x0F      /* 0x3D */
#define LIS_CTRL1     0x20
#define LIS_CTRL2     0x21
#define LIS_CTRL3     0x22
#define LIS_CTRL4     0x23
#define LIS_CTRL5     0x24
#define LIS_STATUS    0x27      /* bit 3 ZYXDA */
#define LIS_DATA      0x28
#define LIS_AUTOINC   0x80      /* bit de poids fort de l'adresse : lecture en rafale */
#define LIS_LSB_PER_G 6842.0f   /* ±4 G */

/* ------------------------------------------------------------------ bus */

static bool rd(mag_t *m, uint8_t addr, uint8_t reg, uint8_t *buf, size_t n)
{
    return m->bus.read != NULL && m->bus.read(addr, reg, buf, n, m->bus.user);
}

static bool wr(mag_t *m, uint8_t addr, uint8_t reg, uint8_t val)
{
    return m->bus.write != NULL && m->bus.write(addr, reg, val, m->bus.user);
}

static int16_t le16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}

static int16_t be16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

/* ------------------------------------------------------------------ détection et réglage */

static mag_chip_t identify(mag_t *m, uint8_t addr)
{
    uint8_t b[3];
    if (addr == 0x0D && rd(m, addr, QMC_ID, b, 1) && b[0] == 0xFF) return MAG_CHIP_QMC5883L;
    if (addr >= 0x0C && addr <= 0x0F && rd(m, addr, IST_WAI, b, 1) && b[0] == 0x10) return MAG_CHIP_IST8310;
    if (addr == 0x1E && rd(m, addr, HMC_ID, b, 3) && b[0] == 'H' && b[1] == '4' && b[2] == '3') return MAG_CHIP_HMC5883L;
    if ((addr == 0x1C || addr == 0x1E) && rd(m, addr, LIS_WAI, b, 1) && b[0] == 0x3D) return MAG_CHIP_LIS3MDL;
    return MAG_CHIP_NONE;
}

static bool configure(mag_t *m)
{
    const uint8_t a = m->addr;
    switch (m->chip) {
    case MAG_CHIP_QMC5883L:
        return wr(m, a, QMC_SETRESET, 0x01) && wr(m, a, QMC_CTRL1, QMC_CTRL1_VAL);
    case MAG_CHIP_IST8310:          /* moyenne sur 16, impulsion normale, première mesure */
        return wr(m, a, IST_AVGCNTL, 0x24) && wr(m, a, IST_PDCNTL, 0xC0) && wr(m, a, IST_CNTL1, 0x01);
    case MAG_CHIP_HMC5883L:         /* moyenne sur 8, 15 Hz, ±1,3 G, continu */
        return wr(m, a, HMC_CRA, 0x70) && wr(m, a, HMC_CRB, 0x20) && wr(m, a, HMC_MODE, 0x00);
    case MAG_CHIP_LIS3MDL:          /* X, Y, Z ultra haute performance, 80 Hz, ±4 G, continu, BDU */
        return wr(m, a, LIS_CTRL1, 0x7C) && wr(m, a, LIS_CTRL2, 0x00) && wr(m, a, LIS_CTRL4, 0x0C) &&
               wr(m, a, LIS_CTRL5, 0x40) && wr(m, a, LIS_CTRL3, 0x00);
    default:
        return false;
    }
}

static bool probe(mag_t *m, uint64_t now_ms)
{
    m->stats.probes++;
    for (size_t i = 0; i < sizeof(ADDRS); i++) {
        const mag_chip_t c = identify(m, ADDRS[i]);
        if (c == MAG_CHIP_NONE) continue;
        m->chip = c;
        m->addr = ADDRS[i];
        if (!configure(m)) break;
        m->errors = 0;
        m->have_sample = false;
        m->t_found_ms = now_ms;
        return true;
    }
    m->chip = MAG_CHIP_NONE;
    m->addr = 0;
    return false;
}

static void lose(mag_t *m, uint64_t now_ms)
{
    m->stats.losses++;
    m->chip = MAG_CHIP_NONE;
    m->addr = 0;
    m->have_sample = false;
    m->t_next_ms = now_ms + m->probe_ms;
}

/* ------------------------------------------------------------------ lecture */

typedef enum { RD_ERROR, RD_NOT_READY, RD_OK } rd_result_t;

/* Mesure en µT dans le repère de la puce (Z vers le haut). */
static rd_result_t read_chip(mag_t *m, mag_vec_t *v)
{
    const uint8_t a = m->addr;
    uint8_t s, d[6];
    switch (m->chip) {
    case MAG_CHIP_QMC5883L:
        if (!rd(m, a, QMC_STATUS, &s, 1)) return RD_ERROR;
        if (!(s & 0x01)) return RD_NOT_READY;
        if (!rd(m, a, QMC_DATA, d, 6)) return RD_ERROR;
        if (s & 0x02) return RD_NOT_READY;                  /* saturé : mesure jetée */
        v->x = le16(d) * (100.0f / QMC_LSB_PER_G);
        v->y = le16(d + 2) * (100.0f / QMC_LSB_PER_G);
        v->z = le16(d + 4) * (100.0f / QMC_LSB_PER_G);
        return RD_OK;
    case MAG_CHIP_IST8310:
        if (!rd(m, a, IST_STAT1, &s, 1)) return RD_ERROR;
        if (!(s & 0x01)) return RD_NOT_READY;
        if (!rd(m, a, IST_DATA, d, 6)) return RD_ERROR;
        if (!wr(m, a, IST_CNTL1, 0x01)) return RD_ERROR;    /* mesure suivante */
        v->x = le16(d) * IST_UT_PER_LSB;
        v->y = le16(d + 2) * IST_UT_PER_LSB;
        v->z = le16(d + 4) * IST_UT_PER_LSB;
        return RD_OK;
    case MAG_CHIP_HMC5883L: {
        if (!rd(m, a, HMC_STATUS, &s, 1)) return RD_ERROR;
        if (!(s & 0x01)) return RD_NOT_READY;
        if (!rd(m, a, HMC_DATA, d, 6)) return RD_ERROR;
        const int16_t x = be16(d), z = be16(d + 2), y = be16(d + 4);
        if (x == HMC_OVERFLOW || y == HMC_OVERFLOW || z == HMC_OVERFLOW) return RD_NOT_READY;
        v->x = x * (100.0f / HMC_LSB_PER_G);
        v->y = y * (100.0f / HMC_LSB_PER_G);
        v->z = z * (100.0f / HMC_LSB_PER_G);
        return RD_OK;
    }
    case MAG_CHIP_LIS3MDL:
        if (!rd(m, a, LIS_STATUS, &s, 1)) return RD_ERROR;
        if (!(s & 0x08)) return RD_NOT_READY;
        if (!rd(m, a, LIS_DATA | LIS_AUTOINC, d, 6)) return RD_ERROR;
        v->x = le16(d) * (100.0f / LIS_LSB_PER_G);
        v->y = le16(d + 2) * (100.0f / LIS_LSB_PER_G);
        v->z = le16(d + 4) * (100.0f / LIS_LSB_PER_G);
        return RD_OK;
    default:
        return RD_ERROR;
    }
}

/* Repère de la puce (Z haut) → repère voiture (X avant, Y droite, Z bas). */
static mag_vec_t to_car(mag_orient_t o, mag_vec_t c)
{
    mag_vec_t v = {c.x, -c.y, -c.z};                    /* puce à plat, X vers la flèche */
    if (o.flipped) {                                    /* puce à l'envers : demi-tour autour de X */
        v.y = -v.y;
        v.z = -v.z;
    }
    for (uint8_t k = 0; k < (o.yaw_quarter & 3); k++) { /* +90° autour de Z (bas) : sens horaire */
        const float x = v.x;
        v.x = -v.y;
        v.y = x;
    }
    return v;
}

/* ------------------------------------------------------------------ API */

void mag_cal_identity(mag_cal_t *c)
{
    c->offset = (mag_vec_t){0, 0, 0};
    c->scale = (mag_vec_t){1, 1, 1};
}

void mag_init(mag_t *m, const mag_bus_t *bus, uint64_t now_ms)
{
    memset(m, 0, sizeof(*m));
    if (bus != NULL) m->bus = *bus;
    mag_cal_identity(&m->cal);
    m->period_ms = 50;
    m->probe_ms = 2000;
    m->stale_ms = 1000;
    m->t_next_ms = now_ms;
}

bool mag_tick(mag_t *m, uint64_t now_ms)
{
    if (now_ms < m->t_next_ms) return false;
    if (m->chip == MAG_CHIP_NONE) {
        m->t_next_ms = now_ms + (probe(m, now_ms) ? m->period_ms : m->probe_ms);
        return false;
    }
    m->t_next_ms = now_ms + m->period_ms;
    mag_vec_t c;
    switch (read_chip(m, &c)) {
    case RD_ERROR:
        m->stats.errors++;
        if (++m->errors >= 3) lose(m, now_ms);
        return false;
    case RD_NOT_READY: {
        const uint64_t since = m->have_sample ? m->t_sample_ms : m->t_found_ms;
        if (now_ms - since >= m->stale_ms) lose(m, now_ms);   /* puce figée ou réinitialisée */
        return false;
    }
    case RD_OK:
    default:
        break;
    }
    m->errors = 0;
    m->raw = to_car(m->orient, c);
    m->field.x = (m->raw.x - m->cal.offset.x) * m->cal.scale.x;
    m->field.y = (m->raw.y - m->cal.offset.y) * m->cal.scale.y;
    m->field.z = (m->raw.z - m->cal.offset.z) * m->cal.scale.z;
    m->have_sample = true;
    m->t_sample_ms = now_ms;
    m->stats.samples++;
    return true;
}

bool mag_present(const mag_t *m, uint64_t now_ms)
{
    return m->chip != MAG_CHIP_NONE && m->have_sample && now_ms - m->t_sample_ms < m->stale_ms;
}

mag_chip_t mag_chip(const mag_t *m)
{
    return m->chip;
}

uint8_t mag_addr(const mag_t *m)
{
    return m->addr;
}

const char *mag_chip_name(mag_chip_t c)
{
    switch (c) {
    case MAG_CHIP_QMC5883L: return "QMC5883L";
    case MAG_CHIP_IST8310:  return "IST8310";
    case MAG_CHIP_HMC5883L: return "HMC5883L";
    case MAG_CHIP_LIS3MDL:  return "LIS3MDL";
    default:                return "aucune";
    }
}

mag_vec_t mag_raw(const mag_t *m)
{
    return m->raw;
}

mag_vec_t mag_field(const mag_t *m)
{
    return m->field;
}

void mag_set_orient(mag_t *m, mag_orient_t o)
{
    m->orient = o;
}

void mag_set_cal(mag_t *m, const mag_cal_t *cal)
{
    m->cal = *cal;
}

const mag_stats_t *mag_stats(const mag_t *m)
{
    return &m->stats;
}

float mag_heading_deg(mag_vec_t v)
{
    float h = atan2f(-v.y, v.x) * (180.0f / PI_F);
    if (h < 0.0f) h += 360.0f;
    if (h >= 360.0f) h -= 360.0f;
    return h;
}

void mag_cal_reset(mag_cal_acc_t *a)
{
    a->min = (mag_vec_t){1e9f, 1e9f, 1e9f};
    a->max = (mag_vec_t){-1e9f, -1e9f, -1e9f};
    a->n = 0;
}

void mag_cal_feed(mag_cal_acc_t *a, mag_vec_t r)
{
    a->min.x = fminf(a->min.x, r.x);
    a->min.y = fminf(a->min.y, r.y);
    a->min.z = fminf(a->min.z, r.z);
    a->max.x = fmaxf(a->max.x, r.x);
    a->max.y = fmaxf(a->max.y, r.y);
    a->max.z = fmaxf(a->max.z, r.z);
    a->n++;
}

bool mag_cal_compute(const mag_cal_acc_t *a, float min_span_ut, mag_cal_t *out)
{
    if (a->n < 2) return false;
    const float sx = a->max.x - a->min.x, sy = a->max.y - a->min.y, sz = a->max.z - a->min.z;
    if (sx < min_span_ut || sy < min_span_ut) return false;
    out->offset.x = (a->max.x + a->min.x) * 0.5f;
    out->offset.y = (a->max.y + a->min.y) * 0.5f;
    out->offset.z = (a->max.z + a->min.z) * 0.5f;
    /* Fer doux : X et Y ramenés à leur étendue moyenne ; Z (peu parcouru à plat) laissé tel quel. */
    const float avg = (sx + sy) * 0.5f;
    out->scale.x = avg / sx;
    out->scale.y = avg / sy;
    out->scale.z = sz >= min_span_ut ? avg / sz : 1.0f;
    return true;
}
