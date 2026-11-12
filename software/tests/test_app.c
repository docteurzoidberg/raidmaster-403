/*
 * Raidmaster 403 — tests unitaires de core/app (Unity) : actions boutons et menu, chrono,
 * heure locale, alarme, sauvegarde et relecture (hal_storage en mémoire, fake_storage.c), source du
 * cap et calibration de la boussole.
 *
 * Licence : GPL-3.0.
 */

#include <math.h>
#include <stdio.h>

#include "app.h"
#include "fake_clock.h"
#include "fake_storage.h"
#include "hal/hal_storage.h"
#include "unity.h"

static app_t A;
static app_state_t S;

extern bool fake_gps_present;

void setUp(void)
{
    fake_gps_present = true;
    fake_storage_clear();
    fake_clock_clear();
    app_init(&A, 0);
}

void tearDown(void) {}

/* Phrase avec checksum. */
static void sentence(char *out, size_t n, const char *body)
{
    unsigned cs = 0;
    for (const char *c = body; *c; c++)
        cs ^= (unsigned char)*c;
    snprintf(out, n, "$%s*%02X", body, cs);
}

/* Époque RMC + GGA : t en ms depuis 00:00 UTC le 09/10/2026, position plein nord depuis 48°N. */
static void epoch(uint32_t t_ms, double north_m, float kmh)
{
    const double lat = 48.0 + north_m / 111195.08;
    const int deg = (int)lat;
    const double min = (lat - deg) * 60.0;
    const unsigned hh = t_ms / 3600000U, mm = t_ms / 60000U % 60U, ss = t_ms / 1000U % 60U,
                   cc = t_ms / 10U % 100U;
    char body[128], line[160];
    snprintf(body, sizeof(body), "GPRMC,%02u%02u%02u.%02u,A,%02d%08.5f,N,00200.00000,E,%.2f,0.0,091026,,,A",
             hh, mm, ss, cc, deg, min, kmh / 1.852);
    sentence(line, sizeof(line), body);
    app_gps_line(&A, line, t_ms);
    snprintf(body, sizeof(body), "GPGGA,%02u%02u%02u.%02u,%02d%08.5f,N,00200.00000,E,1,10,0.9,100.0,M,0.0,M,,",
             hh, mm, ss, cc, deg, min);
    sentence(line, sizeof(line), body);
    app_gps_line(&A, line, t_ms);
}

/* Roule à v km/h pendant dur secondes (10 Hz) depuis t0 ; renvoie la position finale. */
static double drive(uint32_t t0, double n0, float kmh, unsigned dur_s)
{
    double n = n0;
    for (unsigned i = 0; i <= dur_s * 10U; i++) {
        n = n0 + kmh / 3.6 * i * 0.1;
        epoch(t0 + i * 100U, n, kmh);
    }
    return n;
}

static void state(uint32_t now)
{
    app_get_state(&A, now, &S);
}

static void test_valeurs_par_defaut(void)
{
    state(0);
    TEST_ASSERT_EQUAL_INT(APP_TRIP_A, S.active);
    TEST_ASSERT_EQUAL_INT(60, S.set.tz_min);
    TEST_ASSERT_EQUAL_INT(APP_VIEW_RAID, S.set.view);
    TEST_ASSERT_EQUAL_INT(0, S.set.night);
    TEST_ASSERT_EQUAL_INT(-1, S.local_hh);
    TEST_ASSERT_FALSE(S.trip.fix_ok);
}

static void test_distance_via_nmea(void)
{
    drive(36000000, 0.0, 36.0f, 100);           /* 10:00 UTC, 1000 m */
    state(36100000);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 1000.0, S.trip.trip_a_m);
    TEST_ASSERT_TRUE(S.trip.fix_ok);
    TEST_ASSERT_EQUAL_INT(11, S.local_hh);      /* UTC+1 */
    TEST_ASSERT_EQUAL_INT(0, S.local_mm);       /* horloge réglée au premier fix (10:00:00), figée ici */
}

static void test_correction_sur_trip_actif(void)
{
    drive(0, 0.0, 36.0f, 10);                   /* 100 m sur A et B */
    app_adjust_active(&A, +3);                  /* A : +30 m */
    app_toggle_active(&A);
    app_adjust_active(&A, -1);                  /* B : -10 m */
    state(10000);
    TEST_ASSERT_EQUAL_INT(APP_TRIP_B, S.active);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 130.0, S.trip.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 90.0, S.trip.trip_b_m);
}

static void test_raz_a_et_chrono(void)
{
    drive(0, 0.0, 36.0f, 10);                   /* le chrono part au premier mouvement */
    state(10000);
    TEST_ASSERT_TRUE(S.trip.chrono_running);
    TEST_ASSERT_UINT32_WITHIN(1, 10000, S.trip.chrono_ms);

    app_reset_trip(&A, APP_TRIP_A);
    state(20000);
    TEST_ASSERT_FALSE(S.trip.chrono_running);   /* attend le prochain mouvement */
    TEST_ASSERT_EQUAL_UINT32(0, S.trip.chrono_ms);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.trip.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 100.0, S.trip.trip_b_m);

    drive(30000, 100.0, 0.0f, 5);               /* à l'arrêt : toujours pas */
    state(35000);
    TEST_ASSERT_FALSE(S.trip.chrono_running);
    drive(35100, 100.0, 36.0f, 5);
    state(40100);
    TEST_ASSERT_TRUE(S.trip.chrono_running);
    TEST_ASSERT_UINT32_WITHIN(1, 5000, S.trip.chrono_ms);
}

static void test_heure_locale_minuit_et_fuseaux(void)
{
    epoch(23 * 3600000U + 45 * 60000U, 0.0, 0.0f);     /* 23:45 UTC */
    state(0);
    TEST_ASSERT_EQUAL_INT(0, S.local_hh);               /* UTC+1 → 00:45 */
    TEST_ASSERT_EQUAL_INT(45, S.local_mm);
    app_set_tz(&A, -330);                               /* UTC−5:30 */
    state(0);
    TEST_ASSERT_EQUAL_INT(18, S.local_hh);
    TEST_ASSERT_EQUAL_INT(15, S.local_mm);
    app_set_tz(&A, 1000);                               /* borné à +14 h */
    state(0);
    TEST_ASSERT_EQUAL_INT(14 * 60, S.set.tz_min);
    app_set_tz(&A, 75);                                 /* pas de 30 min */
    state(0);
    TEST_ASSERT_EQUAL_INT(60, S.set.tz_min);
}

/* L'heure affichée est celle de l'horloge de la carte, réglée au premier fix de chaque
 * démarrage seulement (la fausse horloge n'avance pas : un second réglage se verrait). */
static void test_horloge_reglee_au_premier_fix(void)
{
    char line[160];
    state(0);
    TEST_ASSERT_EQUAL_INT(-1, S.local_hh);              /* horloge pas réglée, pas de GPS */
    sentence(line, sizeof(line), "GPRMC,100000.00,V,,,,,,,091026,,,N");
    app_gps_line(&A, line, 0);                          /* heure GPS sans fix : ignorée */
    state(0);
    TEST_ASSERT_EQUAL_INT(-1, S.local_hh);
    TEST_ASSERT_EQUAL_UINT(0, fake_clock_sets);

    epoch(10 * 3600000U, 0.0, 0.0f);                    /* premier fix, 10:00 UTC */
    state(0);
    TEST_ASSERT_EQUAL_UINT(1, fake_clock_sets);
    TEST_ASSERT_EQUAL_INT(11, S.local_hh);              /* UTC+1 */
    TEST_ASSERT_EQUAL_INT(0, S.local_mm);
    drive(10 * 3600000U + 100U, 0.0, 30.0f, 10);        /* fixes suivants : pas de réglage */
    TEST_ASSERT_EQUAL_UINT(1, fake_clock_sets);

    app_init(&A, 0);                                    /* redémarrage : nouveau réglage */
    state(0);
    TEST_ASSERT_EQUAL_INT(11, S.local_hh);              /* l'horloge a gardé l'heure */
    epoch(10 * 3600000U + 5U * 60000U, 0.0, 0.0f);
    state(0);
    TEST_ASSERT_EQUAL_UINT(2, fake_clock_sets);
    TEST_ASSERT_EQUAL_INT(5, S.local_mm);
}

static void test_alarme_vitesse(void)
{
    TEST_ASSERT_EQUAL_UINT16(30, app_speed_alarm_step(0, +1));
    TEST_ASSERT_EQUAL_UINT16(0, app_speed_alarm_step(30, -1));
    TEST_ASSERT_EQUAL_UINT16(0, app_speed_alarm_step(0, -1));
    TEST_ASSERT_EQUAL_UINT16(130, app_speed_alarm_step(130, +1));
    TEST_ASSERT_EQUAL_UINT16(90, app_speed_alarm_step(80, +1));
    TEST_ASSERT_EQUAL_UINT16(60, app_speed_alarm_step(50, +1));     /* Maroc : 60 */
    TEST_ASSERT_EQUAL_UINT16(120, app_speed_alarm_step(130, -1));

    app_set_speed_alarm(&A, 50);
    drive(0, 0.0, 45.0f, 2);
    state(2000);
    TEST_ASSERT_FALSE(S.speed_alarm);
    drive(2100, 25.0, 60.0f, 2);
    state(4100);
    TEST_ASSERT_TRUE(S.speed_alarm);
    app_set_speed_alarm(&A, 55);                /* pas un multiple de 10 : coupée */
    state(4100);
    TEST_ASSERT_EQUAL_UINT16(0, S.set.speed_alarm_kmh);
    TEST_ASSERT_FALSE(S.speed_alarm);
}

static void test_sauvegarde_et_relecture(void)
{
    drive(0, 0.0, 36.0f, 100);
    app_toggle_active(&A);
    app_set_calib(&A, 1.5f);
    app_set_tz(&A, 120);
    app_set_night(&A, true);
    app_set_view(&A, APP_VIEW_BALADE);
    app_set_speed_alarm(&A, 90);
    app_set_gpx_log(&A, true);
    app_save(&A);

    app_init(&A, 500000);                       /* redémarrage */
    state(500000);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 1000.0, S.trip.trip_a_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 1000.0, S.trip.odo_m);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, S.trip.calib_pct);
    TEST_ASSERT_EQUAL_INT(APP_TRIP_B, S.active);
    TEST_ASSERT_EQUAL_INT(120, S.set.tz_min);
    TEST_ASSERT_EQUAL_INT(1, S.set.night);
    TEST_ASSERT_EQUAL_INT(APP_VIEW_BALADE, S.set.view);
    TEST_ASSERT_EQUAL_UINT16(90, S.set.speed_alarm_kmh);
    TEST_ASSERT_EQUAL_INT(1, S.set.gpx_log);
}

static void test_sauvegarde_differee(void)
{
    drive(0, 0.0, 36.0f, 5);
    const unsigned w0 = fake_storage_writes;
    app_tick(&A, 5000);
    TEST_ASSERT_EQUAL_UINT(w0, fake_storage_writes);    /* moins de 10 s */
    app_tick(&A, 10000);
    TEST_ASSERT_EQUAL_UINT(w0 + 1, fake_storage_writes);
    app_tick(&A, 30000);                                /* rien n'a bougé depuis */
    TEST_ASSERT_EQUAL_UINT(w0 + 1, fake_storage_writes);
}

static void test_sauvegarde_illisible(void)
{
    const unsigned char junk[12] = {1, 2, 3};
    hal_storage_set("state", junk, sizeof(junk));
    app_init(&A, 0);
    state(0);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, S.trip.odo_m);
    TEST_ASSERT_EQUAL_INT(60, S.set.tz_min);
}

static void test_autocalibration(void)
{
    app_autocal_start(&A);
    drive(0, 0.0, 36.0f, 100);                  /* 1000 m bruts */
    app_autocal_stop(&A);
    state(10000);
    TEST_ASSERT_FALSE(S.autocal_running);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 1000.0, S.autocal_raw_m);
    float pct;
    TEST_ASSERT_TRUE(app_autocal_apply(&A, 980.0, &pct));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -2.0f, pct);
    state(10000);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -2.0f, S.trip.calib_pct);
}

/* ------------------------------------------------------------------ boussole et cap */

#define MAG_OFF_X 12.0f     /* fer dur de la voiture (µT) */
#define MAG_OFF_Y -7.0f
#define MAG_H     21.0f     /* composante horizontale du champ terrestre */

/* Mesure brute de la boussole pour un cap vrai (°), avec le fer dur. */
static mag_vec_t mag_for(float head_deg)
{
    const float r = head_deg * 3.14159265f / 180.0f;
    return (mag_vec_t){MAG_OFF_X + MAG_H * cosf(r), MAG_OFF_Y - MAG_H * sinf(r), 42.0f};
}

/* Roule plein nord à v km/h pendant dur_ms (10 Hz GPS, 20 Hz boussole tournée vers head). */
static double roll(uint32_t *t, double n, float kmh, uint32_t dur_ms, float head, bool with_mag)
{
    for (uint32_t k = 0; k < dur_ms; k += 50) {
        if (k % 100 == 0) {
            n += kmh / 3.6 * 0.1;
            epoch(*t, n, kmh);
        }
        if (with_mag) app_mag_sample(&A, mag_for(head), *t);
        app_tick(&A, *t);
        *t += 50;
    }
    return n;
}

/* Tour de calibration sur place : 360° en turn_s secondes, 20 Hz. */
static void spin(uint32_t *t, float from_deg, float sweep_deg, float turn_s)
{
    const int n = (int)(turn_s * 20.0f);
    for (int i = 0; i <= n; i++) {
        app_mag_sample(&A, mag_for(from_deg + sweep_deg * (float)i / (float)n), *t);
        *t += 50;
    }
}

static void test_cap_sans_boussole(void)
{
    uint32_t t = 0;
    roll(&t, 0.0, 0.0f, 2000, 0.0f, false);
    state(t);
    TEST_ASSERT_TRUE(S.gps_present);
    TEST_ASSERT_FALSE(S.mag_present);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_NONE, S.heading_src);        /* à l'arrêt : pas de route */
    TEST_ASSERT_TRUE(isnan(S.heading_deg));
    roll(&t, 0.0, 50.0f, 2000, 0.0f, false);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_GPS, S.heading_src);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, S.heading_deg);
    fake_gps_present = false;
    state(t);
    TEST_ASSERT_FALSE(S.gps_present);
}

static void test_boussole_non_calibree_ignoree(void)
{
    uint32_t t = 0;
    roll(&t, 0.0, 0.0f, 2000, 90.0f, true);
    state(t);
    TEST_ASSERT_TRUE(S.mag_present);
    TEST_ASSERT_FALSE(S.mag_calibrated);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_NONE, S.heading_src);
}

static void test_calibration_boussole(void)
{
    uint32_t t = 0;
    app_magcal_start(&A);
    spin(&t, 0.0f, 180.0f, 10.0f);                              /* demi-tour : insuffisant */
    state(t);
    TEST_ASSERT_TRUE(S.magcal_running);
    TEST_ASSERT_FALSE(S.magcal_ready);
    TEST_ASSERT_FALSE(app_magcal_apply(&A));
    spin(&t, 180.0f, 200.0f, 10.0f);                            /* le tour complet */
    state(t);
    TEST_ASSERT_TRUE(S.magcal_sectors >= APP_MAGCAL_MIN_SECTORS);
    TEST_ASSERT_TRUE(S.magcal_ready);
    TEST_ASSERT_TRUE(app_magcal_apply(&A));
    state(t);
    TEST_ASSERT_FALSE(S.magcal_running);
    TEST_ASSERT_TRUE(S.mag_calibrated);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, MAG_OFF_X, A.mag.cal.offset.x);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, MAG_OFF_Y, A.mag.cal.offset.y);
    /* enregistrée : relue au démarrage suivant */
    app_init(&A, t);
    state(t);
    TEST_ASSERT_TRUE(S.mag_calibrated);
}

static void test_source_du_cap(void)
{
    uint32_t t = 0;
    app_magcal_start(&A);
    spin(&t, 0.0f, 370.0f, 20.0f);
    TEST_ASSERT_TRUE(app_magcal_apply(&A));

    /* à l'arrêt, voiture tournée vers l'est : boussole */
    double n = roll(&t, 0.0, 0.0f, 2000, 90.0f, true);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_MAG, S.heading_src);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, S.heading_deg);

    /* en route plein nord à 50 km/h : route GPS (la boussole perturbée dirait autre chose) */
    n = roll(&t, n, 50.0f, 2000, 30.0f, true);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_GPS, S.heading_src);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, S.heading_deg);

    /* 15 km/h : entre les seuils, on garde la route GPS */
    n = roll(&t, n, 15.0f, 2000, 30.0f, true);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_GPS, S.heading_src);

    /* 10 km/h : boussole */
    n = roll(&t, n, 10.0f, 2000, 30.0f, true);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_MAG, S.heading_src);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.0f, S.heading_deg);

    /* boussole débranchée : route GPS en repli, puis rien à l'arrêt */
    n = roll(&t, n, 10.0f, 2000, 0.0f, false);
    state(t);
    TEST_ASSERT_FALSE(S.mag_present);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_GPS, S.heading_src);
    roll(&t, n, 0.0f, 2000, 0.0f, false);
    state(t);
    TEST_ASSERT_EQUAL_INT(APP_HEAD_NONE, S.heading_src);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valeurs_par_defaut);
    RUN_TEST(test_distance_via_nmea);
    RUN_TEST(test_correction_sur_trip_actif);
    RUN_TEST(test_raz_a_et_chrono);
    RUN_TEST(test_heure_locale_minuit_et_fuseaux);
    RUN_TEST(test_horloge_reglee_au_premier_fix);
    RUN_TEST(test_alarme_vitesse);
    RUN_TEST(test_sauvegarde_et_relecture);
    RUN_TEST(test_sauvegarde_differee);
    RUN_TEST(test_sauvegarde_illisible);
    RUN_TEST(test_autocalibration);
    RUN_TEST(test_cap_sans_boussole);
    RUN_TEST(test_boussole_non_calibree_ignoree);
    RUN_TEST(test_calibration_boussole);
    RUN_TEST(test_source_du_cap);
    return UNITY_END();
}
