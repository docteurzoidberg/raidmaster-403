/*
 * Raidmaster 403 — tests unitaires de core/gps_parser (Unity).
 *
 * Les phrases sont écrites sans checksum ; nmea() le calcule et l'ajoute, sauf test explicite.
 * Les traces longues de references/traces-brutes/ (non versionnées) sont rejouées si présentes,
 * sinon ces tests sont ignorés.
 *
 * Licence : GPL-3.0.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gps_parser.h"
#include "unity.h"

static gps_parser_t P;
static gps_fix_t F;
static char buf[128];

/* "GPRMC,..." → "$GPRMC,...*hh\r\n" */
static const char *nmea(const char *body)
{
    unsigned cs = 0;
    for (const char *c = body; *c; c++)
        cs ^= (unsigned char)*c;
    snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
    return buf;
}

static bool feed(const char *body)
{
    return gps_parser_feed(&P, nmea(body), &F);
}

void setUp(void)
{
    gps_parser_init(&P);
    memset(&F, 0, sizeof(F));
}

void tearDown(void) {}

/* ---------------------------------------------------------------- phrases isolées */

static void test_rmc_puis_gga_publie_un_fix_complet(void)
{
    TEST_ASSERT_FALSE(feed("GPRMC,060000.10,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A"));
    TEST_ASSERT_TRUE(feed("GPGGA,060000.10,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,"));
    TEST_ASSERT_TRUE(F.valid);
    TEST_ASSERT_TRUE(F.has_rmc);
    TEST_ASSERT_TRUE(F.has_gga);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 48.0 + 51.5047 / 60.0, F.lat_deg);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0 + 20.9680 / 60.0, F.lon_deg);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 27.0f * 1.852f, F.speed_kmh);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.40f, F.course_deg);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.2f, F.hdop);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 100.0f, F.alt_m);
    TEST_ASSERT_EQUAL_UINT8(10, F.sats);
    TEST_ASSERT_EQUAL_UINT8(1, F.fix_quality);
    TEST_ASSERT_EQUAL_UINT32(6 * 3600000U + 100U, F.tod_ms);
    /* 2026-10-09 06:00:00.100 UTC */
    TEST_ASSERT_EQUAL_INT64(1791525600100LL, F.t_utc_ms);
    TEST_ASSERT_EQUAL_UINT32(1, gps_parser_stats(&P)->fixes);
}

static void test_gga_puis_rmc_meme_resultat(void)
{
    TEST_ASSERT_FALSE(feed("GNGGA,120000.00,4851.5047,N,00220.9680,E,1,12,0.8,50.0,M,0.0,M,,"));
    TEST_ASSERT_TRUE(feed("GNRMC,120000.00,A,4851.5047,N,00220.9680,E,10.00,90.00,091026,,,A"));
    TEST_ASSERT_TRUE(F.valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 18.52f, F.speed_kmh);
    TEST_ASSERT_EQUAL_UINT8(12, F.sats);
}

/* u-blox en 5 décimales de minute : la précision du double doit être conservée (~2 cm). */
static void test_precision_double_cinq_decimales(void)
{
    feed("GNRMC,120000.00,A,4851.50391,N,00220.96803,E,0.0,,091026,,,A");
    feed("GNGGA,120000.00,4851.50391,N,00220.96803,E,1,12,0.8,50.0,M,0.0,M,,");
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 48.0 + 51.50391 / 60.0, F.lat_deg);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0 + 20.96803 / 60.0, F.lon_deg);
    TEST_ASSERT_TRUE(isnan(F.course_deg));          /* cap vide à l'arrêt */
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, F.speed_kmh);
}

static void test_hemispheres_sud_et_ouest(void)
{
    feed("GPRMC,000000.00,A,3351.6500,S,07040.2200,W,0.0,0.0,010127,,,A");
    feed("GPGGA,000000.00,3351.6500,S,07040.2200,W,1,8,1.0,0.0,M,0.0,M,,");
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -(33.0 + 51.65 / 60.0), F.lat_deg);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -(70.0 + 40.22 / 60.0), F.lon_deg);
}

static void test_antemeridien(void)
{
    feed("GPRMC,000000.00,A,1000.0000,N,17959.9999,E,5.0,90.0,010127,,,A");
    feed("GPGGA,000000.00,1000.0000,N,17959.9999,E,1,8,1.0,0.0,M,0.0,M,,");
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 179.0 + 59.9999 / 60.0, F.lon_deg);
    feed("GPRMC,000000.10,A,1000.0000,N,17959.9999,W,5.0,90.0,010127,,,A");
    feed("GPGGA,000000.10,1000.0000,N,17959.9999,W,1,8,1.0,0.0,M,0.0,M,,");
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -(179.0 + 59.9999 / 60.0), F.lon_deg);
}

/* ---------------------------------------------------------------- validité */

static void test_rmc_statut_v_invalide(void)
{
    feed("GPRMC,060000.00,V,4851.5047,N,00220.9680,E,0.0,0.0,091026,,,N");
    TEST_ASSERT_TRUE(feed("GPGGA,060000.00,4851.5047,N,00220.9680,E,1,4,9.9,0.0,M,0.0,M,,"));
    TEST_ASSERT_FALSE(F.valid);
}

static void test_gga_qualite_zero_invalide(void)
{
    feed("GPRMC,060000.00,A,4851.5047,N,00220.9680,E,0.0,0.0,091026,,,A");
    TEST_ASSERT_TRUE(feed("GPGGA,060000.00,4851.5047,N,00220.9680,E,0,4,9.9,0.0,M,0.0,M,,"));
    TEST_ASSERT_FALSE(F.valid);
}

/* Avant la première acquisition, u-blox envoie des champs vides (heure comprise). */
static void test_sans_fix_champs_vides(void)
{
    TEST_ASSERT_FALSE(feed("GPRMC,,V,,,,,,,,,,N"));
    TEST_ASSERT_TRUE(feed("GPGGA,,,,,,0,00,99.99,,,,,,"));
    TEST_ASSERT_FALSE(F.valid);
    TEST_ASSERT_TRUE(isnan(F.lat_deg));
    TEST_ASSERT_TRUE(isnan(F.speed_kmh));
    TEST_ASSERT_EQUAL_INT64(-1, F.t_utc_ms);
    /* l'époque suivante, elle aussi sans heure, est bien publiée à son tour */
    TEST_ASSERT_FALSE(feed("GPRMC,,V,,,,,,,,,,N"));
    TEST_ASSERT_TRUE(feed("GPGGA,,,,,,0,00,99.99,,,,,,"));
    TEST_ASSERT_EQUAL_UINT32(2, gps_parser_stats(&P)->fixes);
}

/* ---------------------------------------------------------------- lignes rejetées */

static void test_checksum_faux_rejete(void)
{
    TEST_ASSERT_FALSE(gps_parser_feed(&P,
        "$GPRMC,060000.10,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A*00\r\n", &F));
    TEST_ASSERT_EQUAL_UINT32(1, gps_parser_stats(&P)->bad_checksum);
}

static void test_checksum_absent_rejete(void)
{
    TEST_ASSERT_FALSE(gps_parser_feed(&P,
        "$GPRMC,060000.10,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A\r\n", &F));
    TEST_ASSERT_EQUAL_UINT32(1, gps_parser_stats(&P)->bad_checksum);
}

static void test_ligne_tronquee_rejetee(void)
{
    /* coupure UART au milieu de la phrase : plus de checksum */
    TEST_ASSERT_FALSE(gps_parser_feed(&P, "$GPRMC,060000.10,A,4851.50", &F));
    TEST_ASSERT_FALSE(gps_parser_feed(&P, "", &F));
    TEST_ASSERT_FALSE(gps_parser_feed(&P, "GPRMC,060000.10", &F));
    TEST_ASSERT_EQUAL_UINT32(3, gps_parser_stats(&P)->bad_checksum);
    TEST_ASSERT_EQUAL_UINT32(0, gps_parser_stats(&P)->fixes);
}

static void test_champs_illisibles_rejetes(void)
{
    /* checksum juste mais heure non numérique */
    TEST_ASSERT_FALSE(feed("GPRMC,06h000.10,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A"));
    TEST_ASSERT_EQUAL_UINT32(1, gps_parser_stats(&P)->parse_errors);
}

static void test_phrases_non_utilisees_ignorees(void)
{
    TEST_ASSERT_FALSE(feed("GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00"));
    TEST_ASSERT_FALSE(feed("GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A"));
    TEST_ASSERT_FALSE(feed("PUBX,00,060000.00,4851.5047,N,00220.9680,E,100.0,G3,2.1,2.0,0.007,77.52,0.007,,0.92,1.19,0.77,9,0,0"));
    TEST_ASSERT_EQUAL_UINT32(3, gps_parser_stats(&P)->ignored);
    TEST_ASSERT_EQUAL_UINT32(0, gps_parser_stats(&P)->fixes);
}

/* ---------------------------------------------------------------- époques */

/* RMC seul (GGA perdue) : publié à l'arrivée de l'époque suivante, sans HDOP. */
static void test_epoque_incomplete_publiee_a_la_suivante(void)
{
    TEST_ASSERT_FALSE(feed("GPRMC,060000.00,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A"));
    TEST_ASSERT_TRUE(feed("GPRMC,060000.10,A,4851.5054,N,00220.9680,E,27.00,0.40,091026,,,A"));
    TEST_ASSERT_EQUAL_UINT32(6 * 3600000U, F.tod_ms);
    TEST_ASSERT_TRUE(F.valid);
    TEST_ASSERT_FALSE(F.has_gga);
    TEST_ASSERT_TRUE(isnan(F.hdop));
    /* fin de flux : la dernière époque sort par flush */
    TEST_ASSERT_TRUE(gps_parser_flush(&P, &F));
    TEST_ASSERT_EQUAL_UINT32(6 * 3600000U + 100U, F.tod_ms);
    TEST_ASSERT_FALSE(gps_parser_flush(&P, &F));
}

/* GGA seul : la position vient de GGA, la date de la dernière RMC. */
static void test_gga_seul_position_et_date(void)
{
    feed("GPRMC,060000.00,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A");
    feed("GPGGA,060000.00,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,");
    feed("GPGGA,060000.10,4851.5054,N,00220.9690,E,1,10,1.2,100.0,M,0.0,M,,");
    TEST_ASSERT_TRUE(gps_parser_flush(&P, &F));
    TEST_ASSERT_TRUE(F.valid);
    TEST_ASSERT_FALSE(F.has_rmc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 48.0 + 51.5054 / 60.0, F.lat_deg);
    TEST_ASSERT_TRUE(isnan(F.speed_kmh));
    TEST_ASSERT_EQUAL_INT64(1791525600100LL, F.t_utc_ms);
}

/* Une phrase en double d'une époque déjà publiée ne republie rien. */
static void test_doublon_ignore(void)
{
    feed("GPRMC,060000.00,A,4851.5047,N,00220.9680,E,27.00,0.40,091026,,,A");
    TEST_ASSERT_TRUE(feed("GPGGA,060000.00,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,"));
    TEST_ASSERT_FALSE(feed("GPGGA,060000.00,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,"));
    TEST_ASSERT_FALSE(gps_parser_flush(&P, &F));
    TEST_ASSERT_EQUAL_UINT32(1, gps_parser_stats(&P)->fixes);
}

/* ---------------------------------------------------------------- dates */

static void test_jours_depuis_1970(void)
{
    TEST_ASSERT_EQUAL_INT32(0, gps_days_from_civil(1970, 1, 1));
    TEST_ASSERT_EQUAL_INT32(11016, gps_days_from_civil(2000, 2, 29));
    TEST_ASSERT_EQUAL_INT32(20735, gps_days_from_civil(2026, 10, 9));
    TEST_ASSERT_EQUAL_INT32(-1, gps_days_from_civil(1969, 12, 31));
}

/* Passage de minuit UTC : la date RMC change ; une GGA seule juste après minuit prend J+1. */
static void test_minuit_utc(void)
{
    feed("GPRMC,235959.90,A,4851.5047,N,00220.9680,E,27.00,0.40,311226,,,A");
    feed("GPGGA,235959.90,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,");
    const int64_t avant = F.t_utc_ms;
    feed("GPRMC,000000.00,A,4851.5054,N,00220.9680,E,27.00,0.40,010127,,,A");
    feed("GPGGA,000000.00,4851.5054,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,");
    TEST_ASSERT_EQUAL_INT64(100, F.t_utc_ms - avant);

    gps_parser_init(&P);
    feed("GPRMC,235959.90,A,4851.5047,N,00220.9680,E,27.00,0.40,311226,,,A");
    feed("GPGGA,235959.90,4851.5047,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,");
    feed("GPGGA,000000.00,4851.5054,N,00220.9680,E,1,10,1.2,100.0,M,0.0,M,,");
    TEST_ASSERT_TRUE(gps_parser_flush(&P, &F));
    TEST_ASSERT_EQUAL_INT64(100, F.t_utc_ms - avant);
}

/* ---------------------------------------------------------------- traces de référence */

#ifndef RAIDMASTER_TRACES_DIR
#define RAIDMASTER_TRACES_DIR "references/traces-brutes"
#endif

typedef struct {
    uint32_t fixes, invalid, gaps;
} trace_result_t;

/* Rejoue un fichier ; renvoie false s'il est absent. Compte les fixes non espacés de 100 ms. */
static bool replay(const char *name, trace_result_t *r)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", RAIDMASTER_TRACES_DIR, name);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    memset(r, 0, sizeof(*r));
    gps_parser_init(&P);
    char line[256];
    int64_t last = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (gps_parser_feed(&P, line, &F)) {
            r->fixes++;
            r->invalid += !F.valid;
            if (last >= 0 && F.t_utc_ms - last != 100)
                r->gaps++;
            last = F.t_utc_ms;
        }
    }
    if (gps_parser_flush(&P, &F))
        r->fixes++;
    fclose(fp);
    TEST_ASSERT_EQUAL_UINT32(0, gps_parser_stats(&P)->bad_checksum);
    TEST_ASSERT_EQUAL_UINT32(0, gps_parser_stats(&P)->parse_errors);
    return true;
}

static void check_trace(const char *name, uint32_t fixes, uint32_t invalid)
{
    trace_result_t r;
    if (!replay(name, &r))
        TEST_IGNORE_MESSAGE("trace absente (references/traces-brutes non versionné)");
    TEST_ASSERT_EQUAL_UINT32(fixes, r.fixes);
    TEST_ASSERT_EQUAL_UINT32(invalid, r.invalid);
    TEST_ASSERT_EQUAL_UINT32(0, r.gaps);
}

static void test_trace_autoroute_1h(void)    { check_trace("stage_highway_1h.nmea", 36000, 0); }
static void test_trace_autoroute_bruitee(void) { check_trace("stage_highway_1h_noisy.nmea", 36000, 3001); }
static void test_trace_circuit_10min(void)   { check_trace("stage_circular_10min.nmea", 6000, 0); }
static void test_trace_arret_10min(void)     { check_trace("stage_stationary_10min.nmea", 6000, 0); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rmc_puis_gga_publie_un_fix_complet);
    RUN_TEST(test_gga_puis_rmc_meme_resultat);
    RUN_TEST(test_precision_double_cinq_decimales);
    RUN_TEST(test_hemispheres_sud_et_ouest);
    RUN_TEST(test_antemeridien);
    RUN_TEST(test_rmc_statut_v_invalide);
    RUN_TEST(test_gga_qualite_zero_invalide);
    RUN_TEST(test_sans_fix_champs_vides);
    RUN_TEST(test_checksum_faux_rejete);
    RUN_TEST(test_checksum_absent_rejete);
    RUN_TEST(test_ligne_tronquee_rejetee);
    RUN_TEST(test_champs_illisibles_rejetes);
    RUN_TEST(test_phrases_non_utilisees_ignorees);
    RUN_TEST(test_epoque_incomplete_publiee_a_la_suivante);
    RUN_TEST(test_gga_seul_position_et_date);
    RUN_TEST(test_doublon_ignore);
    RUN_TEST(test_jours_depuis_1970);
    RUN_TEST(test_minuit_utc);
    RUN_TEST(test_trace_autoroute_1h);
    RUN_TEST(test_trace_autoroute_bruitee);
    RUN_TEST(test_trace_circuit_10min);
    RUN_TEST(test_trace_arret_10min);
    return UNITY_END();
}
