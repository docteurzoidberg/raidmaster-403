/*
 * Raidmaster 403 — décodage NMEA → GpsFix. Voir core/include/gps_parser.h.
 *
 * Licence : GPL-3.0.
 */

#include "gps_parser.h"

#include <math.h>
#include <string.h>

#include "minmea.h"

#define TOD_UNKNOWN UINT32_MAX
#define MS_PER_DAY  86400000LL
#define KNOT_KMH    1.852f

/* Algorithme de H. Hinnant (days_from_civil), valable sur toute la plage int. */
int32_t gps_days_from_civil(int year, int month, int day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)day - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return (int32_t)(era * 146097 + (int)doe - 719468);
}

/* Remplace timegm() pour minmea (voir core/minmea_port/minmea_compat.h). */
time_t rm_timegm(struct tm *tm)
{
    const int64_t days = gps_days_from_civil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return (time_t)(days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

/* Coordonnée NMEA (dddmm.mmmm, signe déjà appliqué par minmea) → degrés décimaux en double. */
static double coord_deg(const struct minmea_float *f)
{
    if (f->scale <= 0 || f->scale > INT_LEAST32_MAX / 100)
        return NAN;
    const int_least32_t unit = f->scale * 100;
    const int_least32_t deg = f->value / unit;
    const int_least32_t min = f->value % unit;       /* même signe que value */
    return (double)deg + (double)min / (60.0 * (double)f->scale);
}

static float to_float(const struct minmea_float *f)
{
    return f->scale > 0 ? (float)((double)f->value / (double)f->scale) : NAN;
}

static uint32_t tod_ms(const struct minmea_time *t)
{
    if (t->hours < 0 || t->hours > 23 || t->minutes < 0 || t->minutes > 59 ||
        t->seconds < 0 || t->seconds > 60 || t->microseconds < 0)
        return TOD_UNKNOWN;
    return (uint32_t)(((t->hours * 60 + t->minutes) * 60 + t->seconds) * 1000) +
           (uint32_t)(t->microseconds / 1000);
}

static int32_t date_days(const struct minmea_date *d)
{
    if (d->year < 0 || d->month < 1 || d->month > 12 || d->day < 1 || d->day > 31)
        return -1;
    const int year = d->year < 80 ? 2000 + d->year : (d->year < 100 ? 1900 + d->year : d->year);
    return gps_days_from_civil(year, d->month, d->day);
}

static void epoch_reset(gps_fix_t *f, uint32_t tod)
{
    memset(f, 0, sizeof(*f));
    f->lat_deg = NAN;
    f->lon_deg = NAN;
    f->speed_kmh = NAN;
    f->course_deg = NAN;
    f->hdop = NAN;
    f->alt_m = NAN;
    f->tod_ms = tod;
    f->t_utc_ms = -1;
}

void gps_parser_init(gps_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    epoch_reset(&p->cur, TOD_UNKNOWN);
    p->last_date_days = -1;
    p->last_pub_tod = TOD_UNKNOWN;
}

const gps_parser_stats_t *gps_parser_stats(const gps_parser_t *p)
{
    return &p->stats;
}

/* Finalise l'époque en cours dans *out et la ferme. */
static bool epoch_publish(gps_parser_t *p, gps_fix_t *out)
{
    if (!p->cur_open)
        return false;
    gps_fix_t *f = &p->cur;

    /* Date : celle du RMC de l'époque, sinon la dernière connue (+1 jour si minuit est passé). */
    if (f->t_utc_ms < 0 && f->tod_ms != TOD_UNKNOWN && p->last_date_days >= 0) {
        int64_t days = p->last_date_days;
        if (f->tod_ms < p->last_date_tod_ms)
            days += 1;
        f->t_utc_ms = days * MS_PER_DAY + f->tod_ms;
    }
    if (f->has_gga && f->fix_quality == 0)
        f->valid = false;
    if (isnan(f->lat_deg) || isnan(f->lon_deg))
        f->valid = false;

    *out = *f;
    p->cur_open = false;
    p->last_pub_tod = f->tod_ms;
    p->stats.fixes++;
    return true;
}

bool gps_parser_flush(gps_parser_t *p, gps_fix_t *out)
{
    return epoch_publish(p, out);
}

/* Rattache une phrase d'heure `tod` à l'époque en cours ; si l'heure change, publie la
 * précédente dans *out (renvoie true). *accept passe à false si la phrase appartient à une
 * époque déjà publiée (phrase en double ou en retard) : elle est alors ignorée. */
static bool epoch_enter(gps_parser_t *p, uint32_t tod, gps_fix_t *out, bool *accept)
{
    bool published = false;
    if (p->cur_open && p->cur.tod_ms != tod)
        published = epoch_publish(p, out);
    *accept = p->cur_open || tod == TOD_UNKNOWN || tod != p->last_pub_tod;
    if (*accept && !p->cur_open) {
        epoch_reset(&p->cur, tod);
        p->cur_open = true;
    }
    return published;
}

static void set_position(gps_fix_t *f, const struct minmea_float *lat, const struct minmea_float *lon)
{
    const double la = coord_deg(lat), lo = coord_deg(lon);
    if (isnan(la) || isnan(lo) || fabs(la) > 90.0 || fabs(lo) > 180.0)
        return;
    f->lat_deg = la;
    f->lon_deg = lo;
}

bool gps_parser_feed(gps_parser_t *p, const char *line, gps_fix_t *out)
{
    p->stats.lines++;
    if (!minmea_check(line, true)) {
        p->stats.bad_checksum++;
        return false;
    }

    /* Phrases propriétaires ($P..., ex. $PUBX u-blox) : hors du format standard de minmea. */
    if (line[1] == 'P') {
        p->stats.ignored++;
        return false;
    }

    gps_fix_t prev;
    bool published = false, accept = false;

    switch (minmea_sentence_id(line, true)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc s;
        if (!minmea_parse_rmc(&s, line)) {
            p->stats.parse_errors++;
            return false;
        }
        const uint32_t tod = tod_ms(&s.time);
        published = epoch_enter(p, tod, &prev, &accept);
        if (!accept)
            break;
        gps_fix_t *f = &p->cur;
        f->has_rmc = true;
        f->valid = s.valid;
        set_position(f, &s.latitude, &s.longitude);
        f->speed_kmh = to_float(&s.speed) * KNOT_KMH;
        f->course_deg = to_float(&s.course);
        const int32_t days = date_days(&s.date);
        if (days >= 0 && tod != TOD_UNKNOWN) {
            f->t_utc_ms = (int64_t)days * MS_PER_DAY + tod;
            p->last_date_days = days;
            p->last_date_tod_ms = tod;
        }
        break;
    }
    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga s;
        if (!minmea_parse_gga(&s, line)) {
            p->stats.parse_errors++;
            return false;
        }
        published = epoch_enter(p, tod_ms(&s.time), &prev, &accept);
        if (!accept)
            break;
        gps_fix_t *f = &p->cur;
        f->has_gga = true;
        f->fix_quality = (uint8_t)(s.fix_quality < 0 ? 0 : s.fix_quality);
        f->sats = (uint8_t)(s.satellites_tracked < 0 ? 0 : s.satellites_tracked);
        f->hdop = to_float(&s.hdop);
        f->alt_m = to_float(&s.altitude);
        if (!f->has_rmc) {          /* la position RMC prime, GGA la fournit si RMC est absent */
            set_position(f, &s.latitude, &s.longitude);
            f->valid = f->fix_quality > 0;
        }
        break;
    }
    case MINMEA_INVALID:
        p->stats.parse_errors++;
        return false;
    default:
        p->stats.ignored++;
        return false;
    }

    if (!accept)
        p->stats.ignored++;
    /* L'époque précédente a été fermée par cette phrase : on la rend, la courante attend. */
    if (published) {
        *out = prev;
        return true;
    }
    /* Époque complète : publication immédiate, sans attendre la suivante. */
    if (p->cur_open && p->cur.has_rmc && p->cur.has_gga)
        return epoch_publish(p, out);
    return false;
}
