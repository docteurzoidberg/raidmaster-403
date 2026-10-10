/*
 * Raidmaster 403 — tests unitaires de core/trip_engine (Unity).
 *
 * Fixes synthétiques construits directement (sans NMEA), puis rejeu des traces de
 * references/traces-brutes/ à travers gps_parser (ignoré si absentes). Distances de
 * référence : tests/tools/compute_reference.py (haversine sur les traces propres).
 *
 * Licence : GPL-3.0.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gps_parser.h"
#include "trip_engine.h"
#include "unity.h"

#define LAT0 48.8566
#define LON0 2.3522
#define M_PER_DEG_LAT (6371008.8 * 3.14159265358979323846 / 180.0)

static trip_engine_t E;
static trip_state_t S;

void setUp(void)
{
    trip_engine_init(&E, NULL, NULL);
}

void tearDown(void) {}

static gps_fix_t mkfix(int64_t t_ms, double north_m, float v_kmh)
{
    gps_fix_t f;
    memset(&f, 0, sizeof(f));
    f.valid = true;
    f.has_rmc = f.has_gga = true;
    f.lat_deg = LAT0 + north_m / M_PER_DEG_LAT;
    f.lon_deg = LON0;
    f.speed_kmh = v_kmh;
    f.course_deg = 0.0f;
    f.hdop = 1.0f;
    f.alt_m = 100.0f;
    f.sats = 10;
    f.fix_quality = 1;
    f.t_utc_ms = 1791525600000LL + t_ms;
    f.tod_ms = (uint32_t)(f.t_utc_ms % 86400000LL);
    return f;
}

/* Roule plein nord à vitesse constante, 10 Hz, de t0 à t0 + dur (s). Renvoie la position finale. */
static double drive(int64_t t0_ms, double north0_m, float v_kmh, double dur_s)
{
    const int n = (int)lround(dur_s * 10.0);
    double north = north0_m;
    for (int i = 0; i <= n; i++) {
        north = north0_m + v_kmh / 3.6 * i * 0.1;
        gps_fix_t f = mkfix(t0_ms + i * 100, north, v_kmh);
        trip_engine_feed(&E, &f, (uint64_t)(t0_ms + i * 100));
    }
    return north;
}

static void state_at(int64_t mono_ms)
{
    trip_engine_get_state(&E, (uint64_t)mono_ms, &S);
}

/* ---------------------------------------------------------------- distance */

static void test_haversine_un_degre(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 111195.08, trip_haversine_m(45.0, 0.0, 46.0, 0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, trip_haversine_m(LAT0, LON0, LAT0, LON0));
}

static void test_ligne_droite_vitesse_constante(void)
{
    drive(0, 0.0, 36.0f, 60.0);                 /* 10 m/s pendant 60 s */
    state_at(60000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 600.0, S.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 600.0, S.trip_b_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 600.0, S.odo_m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.0f, S.speed_kmh);
    TEST_ASSERT_TRUE(S.fix_ok);
}

static void test_acceleration_trapezes(void)
{
    /* 0 → 36 km/h en 10 s, vitesse linéaire : 50 m exactement par les trapèzes */
    for (int i = 0; i <= 100; i++) {
        gps_fix_t f = mkfix(i * 100, 0.0, 36.0f * (float)i / 100.0f);
        trip_engine_feed(&E, &f, (uint64_t)i * 100);
    }
    state_at(10000);
    /* sous 2 km/h (i ≤ 5) la vitesse compte pour 0 : il manque ~0,15 m */
    TEST_ASSERT_DOUBLE_WITHIN(0.2, 50.0, S.trip_a_m);
}

/* Gigue de position de ±5 m à l'arrêt, vitesse Doppler nulle : aucune distance. */
static void test_arret_gigue_position(void)
{
    for (int i = 0; i < 6000; i++) {
        gps_fix_t f = mkfix(i * 100, ((i * 7919) % 11 - 5) * 1.0, 0.0f);
        f.lon_deg += ((i * 104729) % 11 - 5) * 1e-5;
        trip_engine_feed(&E, &f, (uint64_t)i * 100);
    }
    state_at(600000);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.odo_m);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, S.speed_kmh);
    TEST_ASSERT_TRUE(isnan(S.course_deg));
}

static void test_vitesse_sous_seuil_ignoree(void)
{
    drive(0, 0.0, 1.5f, 120.0);
    state_at(120000);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.odo_m);
    TEST_ASSERT_EQUAL_UINT32(0, S.moving_s);
}

/* Fixes refusés (invalide, HDOP haut) : l'intervalle suivant les englobe, rien n'est perdu. */
static void test_fixes_refuses_englobes(void)
{
    for (int i = 0; i <= 99; i++) {           /* le dernier (99) est accepté */
        gps_fix_t f = mkfix(i * 100, i * 1.0, 36.0f);
        if (i % 3 == 1) f.valid = false;
        if (i % 3 == 2) f.hdop = 4.5f;
        trip_engine_feed(&E, &f, (uint64_t)i * 100);
    }
    state_at(9900);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 99.0, S.trip_a_m);
}

/* Perte de fix 60 s (tunnel) : comblée à vol d'oiseau. */
static void test_trou_comble(void)
{
    double n = drive(0, 0.0, 72.0f, 10.0);      /* 200 m */
    drive(70000, n + 1200.0, 72.0f, 10.0);      /* 1200 m pendant le trou, puis 200 m */
    state_at(80000);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 1600.0, S.trip_a_m);
    TEST_ASSERT_EQUAL_UINT32(80, S.moving_s);
}

/* Saut invraisemblable (100 km en 10 s) : pas compté. */
static void test_teleportation_ignoree(void)
{
    double n = drive(0, 0.0, 72.0f, 10.0);
    gps_fix_t f = mkfix(20000, n + 100000.0, 72.0f);
    trip_engine_feed(&E, &f, 20000);
    state_at(20000);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 200.0, S.trip_a_m);
}

/* GGA seule (pas de vitesse) : écart de position. */
static void test_sans_vitesse_ecart_position(void)
{
    for (int i = 0; i <= 100; i++) {
        gps_fix_t f = mkfix(i * 100, i * 1.0, NAN);
        f.has_rmc = false;
        trip_engine_feed(&E, &f, (uint64_t)i * 100);
    }
    state_at(10000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 100.0, S.trip_a_m);
}

/* Heure GPS qui recule (rejeu en boucle) : on repart du nouveau point, sans distance. */
static void test_retour_arriere_heure(void)
{
    drive(0, 0.0, 36.0f, 10.0);                 /* 100 m */
    double n = drive(-60000, 5000.0, 36.0f, 10.0);
    (void)n;
    state_at(10000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 200.0, S.trip_a_m);
}

static void test_fix_double_ignore(void)
{
    gps_fix_t f = mkfix(0, 0.0, 36.0f);
    trip_engine_feed(&E, &f, 0);
    trip_engine_feed(&E, &f, 0);
    f = mkfix(100, 1.0, 36.0f);
    trip_engine_feed(&E, &f, 100);
    trip_engine_feed(&E, &f, 100);
    state_at(100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, S.trip_a_m);
}

/* ---------------------------------------------------------------- vitesse, moyenne, max */

static void test_vitesse_lissee_500ms(void)
{
    double n = drive(0, 0.0, 50.0f, 10.0);
    drive(10100, n, 60.0f, 0.2);                /* 3 échantillons à 60 */
    state_at(10300);
    /* fenêtre [9,8 s ; 10,3 s] : 50, 50, 50 (9,8 / 9,9 / 10,0) puis 60 × 3 → 55 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, S.speed_kmh);
}

static void test_moyenne_hors_arrets_et_max(void)
{
    double n = drive(0, 0.0, 60.0f, 600.0);     /* 10 km en 10 min */
    n = drive(600100, n, 0.0f, 600.0);          /* 10 min arrêté */
    drive(1200200, n, 90.0f, 1.0);
    state_at(1201200);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 60.0f, S.avg_kmh);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, S.max_kmh);
    TEST_ASSERT_UINT32_WITHIN(1, 601, S.moving_s);
}

static void test_fix_perdu_apres_delai(void)
{
    drive(0, 0.0, 50.0f, 1.0);
    state_at(2000);
    TEST_ASSERT_TRUE(S.fix_ok);
    state_at(3500);
    TEST_ASSERT_FALSE(S.fix_ok);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, S.speed_kmh);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 13.89, S.trip_a_m);   /* distances figées, pas perdues */
}

/* ---------------------------------------------------------------- boutons */

static void test_raz_et_corrections(void)
{
    drive(0, 0.0, 36.0f, 100.0);                /* 1000 m */
    trip_engine_reset_b(&E);
    state_at(100000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.trip_a_m);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.trip_b_m);

    trip_engine_adjust_a(&E, TRIP_ADJUST_STEP_M);
    trip_engine_adjust_a(&E, TRIP_ADJUST_STEP_M);
    trip_engine_adjust_a(&E, -TRIP_ADJUST_STEP_M);
    state_at(100000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1010.0, S.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.odo_m);   /* l'odomètre n'est pas corrigé */

    trip_engine_reset_a(&E);
    trip_engine_adjust_a(&E, -TRIP_ADJUST_STEP_M);
    state_at(100000);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.trip_a_m);          /* borné à 0 */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, S.avg_kmh);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, S.max_kmh);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.odo_m);

    trip_engine_reset_odo(&E);
    state_at(100000);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.odo_m);
}

/* ---------------------------------------------------------------- calibration */

static void test_calibration_non_retroactive(void)
{
    double n = drive(0, 0.0, 36.0f, 100.0);     /* 1000 m bruts à 0 % */
    trip_engine_set_calib(&E, 2.0f);
    drive(100100, n, 36.0f, 100.0);             /* 1000 m bruts à +2 % */
    state_at(200100);
    /* + l'intervalle de 0,1 s entre les deux trajets (1 m à +2 %) */
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 1000.0 + 1.02 + 1020.0, S.trip_a_m);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, S.calib_pct);
}

static void test_calibration_bornes_et_pas(void)
{
    trip_engine_set_calib(&E, 7.3f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 5.0f, trip_engine_calib(&E));
    trip_engine_set_calib(&E, -9.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -5.0f, trip_engine_calib(&E));
    trip_engine_set_calib(&E, 1.26f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.3f, trip_engine_calib(&E));
}

static void test_autocalibration(void)
{
    trip_engine_set_calib(&E, 1.0f);            /* l'auto-calibration part du brut */
    trip_engine_autocal_start(&E);
    drive(0, 0.0, 36.0f, 100.0);                /* 1000 m bruts */
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, trip_engine_autocal_raw_m(&E));
    trip_engine_autocal_stop(&E);
    drive(100100, 1000.0, 36.0f, 10.0);         /* roule encore : la mesure est figée */
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, trip_engine_autocal_raw_m(&E));
    TEST_ASSERT_FALSE(trip_engine_autocal_running(&E));
    float pct = 0.0f;
    TEST_ASSERT_TRUE(trip_engine_autocal_finish(&E, 1020.0, &pct));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, pct);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, trip_engine_calib(&E));
}

static void test_autocalibration_refusee(void)
{
    trip_engine_autocal_start(&E);
    drive(0, 0.0, 36.0f, 5.0);                  /* 50 m : trop court */
    float pct = 0.0f;
    TEST_ASSERT_FALSE(trip_engine_autocal_finish(&E, 52.0, &pct));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, trip_engine_calib(&E));

    trip_engine_autocal_start(&E);
    drive(5100, 51.0, 36.0f, 100.0);            /* 1000 m, réel 1100 : +10 % hors bornes */
    TEST_ASSERT_FALSE(trip_engine_autocal_finish(&E, 1100.0, &pct));
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 10.0f, pct);   /* 1001 m bruts (0,1 s de raccord) */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, trip_engine_calib(&E));
}

/* ---------------------------------------------------------------- chrono, persistance */

static void test_chrono(void)
{
    trip_engine_chrono_start(&E, 1000);
    state_at(4000);
    TEST_ASSERT_EQUAL_UINT32(3000, S.chrono_ms);
    TEST_ASSERT_TRUE(S.chrono_running);
    trip_engine_chrono_stop(&E, 5000);
    trip_engine_chrono_start(&E, 9000);
    trip_engine_chrono_start(&E, 9500);         /* déjà lancé : sans effet */
    state_at(10000);
    TEST_ASSERT_EQUAL_UINT32(5000, S.chrono_ms);
    trip_engine_chrono_reset(&E);
    state_at(20000);
    TEST_ASSERT_EQUAL_UINT32(0, S.chrono_ms);
    TEST_ASSERT_FALSE(S.chrono_running);
}

static void test_persistance(void)
{
    drive(0, 0.0, 36.0f, 100.0);
    trip_engine_set_calib(&E, -1.5f);
    trip_persist_t p;
    trip_engine_get_persist(&E, &p);
    trip_engine_init(&E, NULL, &p);             /* redémarrage */
    state_at(0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.odo_m);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.5f, S.calib_pct);
    TEST_ASSERT_FALSE(S.fix_ok);
    /* pas de distance entre le dernier point d'avant et le premier d'après */
    gps_fix_t f = mkfix(500000, 5000.0, 36.0f);
    trip_engine_feed(&E, &f, 500000);
    state_at(500000);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1000.0, S.trip_a_m);
}

/* ---------------------------------------------------------------- traces de référence */

#ifndef RAIDMASTER_TRACES_DIR
#define RAIDMASTER_TRACES_DIR "references/traces-brutes"
#endif

static bool replay(const char *name, double *trip_m)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", RAIDMASTER_TRACES_DIR, name);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    gps_parser_t P;
    gps_parser_init(&P);
    gps_fix_t f;
    char line[256];
    uint64_t mono = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (gps_parser_feed(&P, line, &f)) {
            mono += 100;
            trip_engine_feed(&E, &f, mono);
        }
    }
    if (gps_parser_flush(&P, &f))
        trip_engine_feed(&E, &f, mono + 100);
    fclose(fp);
    state_at((int64_t)mono);
    *trip_m = S.trip_a_m;
    return true;
}

static void check_trace(const char *name, double ref_m, double tol_m)
{
    double d;
    if (!replay(name, &d))
        TEST_IGNORE_MESSAGE("trace absente (references/traces-brutes non versionné)");
    printf("%s : %.3f km (référence %.3f km, écart %+.3f %%)\n", name, d / 1000.0, ref_m / 1000.0,
           ref_m > 0 ? (d / ref_m - 1.0) * 100.0 : 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(tol_m, ref_m, d);
}

/* Références : compute_reference.py sur les traces propres. */
static void test_trace_autoroute_1h(void)      { check_trace("stage_highway_1h.nmea", 89997.464, 45.0); }      /* 0,05 % */
static void test_trace_autoroute_bruitee(void) { check_trace("stage_highway_1h_noisy.nmea", 89997.464, 450.0); } /* 0,5 % */
static void test_trace_circuit_10min(void)     { check_trace("stage_circular_10min.nmea", 8340.816, 42.0); }    /* 0,5 % */
static void test_trace_arret_10min(void)       { check_trace("stage_stationary_10min.nmea", 0.0, 1.0); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_haversine_un_degre);
    RUN_TEST(test_ligne_droite_vitesse_constante);
    RUN_TEST(test_acceleration_trapezes);
    RUN_TEST(test_arret_gigue_position);
    RUN_TEST(test_vitesse_sous_seuil_ignoree);
    RUN_TEST(test_fixes_refuses_englobes);
    RUN_TEST(test_trou_comble);
    RUN_TEST(test_teleportation_ignoree);
    RUN_TEST(test_sans_vitesse_ecart_position);
    RUN_TEST(test_retour_arriere_heure);
    RUN_TEST(test_fix_double_ignore);
    RUN_TEST(test_vitesse_lissee_500ms);
    RUN_TEST(test_moyenne_hors_arrets_et_max);
    RUN_TEST(test_fix_perdu_apres_delai);
    RUN_TEST(test_raz_et_corrections);
    RUN_TEST(test_calibration_non_retroactive);
    RUN_TEST(test_calibration_bornes_et_pas);
    RUN_TEST(test_autocalibration);
    RUN_TEST(test_autocalibration_refusee);
    RUN_TEST(test_chrono);
    RUN_TEST(test_persistance);
    RUN_TEST(test_trace_autoroute_1h);
    RUN_TEST(test_trace_autoroute_bruitee);
    RUN_TEST(test_trace_circuit_10min);
    RUN_TEST(test_trace_arret_10min);
    return UNITY_END();
}
