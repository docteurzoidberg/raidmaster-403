/*
 * Tests de core/mag avec des boussoles émulées (registres) : détection des quatre puces, réglage,
 * décodage (ordre des axes, boutisme, échelle), orientation, cap, calibration, perte et retour.
 *
 * Licence : GPL-3.0.
 */

#include <math.h>
#include <string.h>

#include "mag.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ puce émulée */

typedef struct {
    bool       present;
    uint8_t    addr;
    mag_chip_t chip;
    uint8_t    regs[256];
    int        reads, writes;
} fake_chip_t;

static fake_chip_t g_chip;

static void put_le(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static void put_be(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)((uint16_t)v >> 8);
    p[1] = (uint8_t)v;
}

/* Champ dans le repère de la puce, en µT : écrit dans ses registres, mesure prête. */
static void chip_field(float x, float y, float z)
{
    uint8_t *r = g_chip.regs;
    switch (g_chip.chip) {
    case MAG_CHIP_QMC5883L:
        put_le(r + 0, (int16_t)lroundf(x * 30.0f));
        put_le(r + 2, (int16_t)lroundf(y * 30.0f));
        put_le(r + 4, (int16_t)lroundf(z * 30.0f));
        r[0x06] = 0x01;
        break;
    case MAG_CHIP_IST8310:
        put_le(r + 3, (int16_t)lroundf(x / 0.3f));
        put_le(r + 5, (int16_t)lroundf(y / 0.3f));
        put_le(r + 7, (int16_t)lroundf(z / 0.3f));
        break;                                          /* prête seulement après déclenchement */
    case MAG_CHIP_HMC5883L:
        put_be(r + 3, (int16_t)lroundf(x * 10.9f));
        put_be(r + 5, (int16_t)lroundf(z * 10.9f));     /* ordre X, Z, Y */
        put_be(r + 7, (int16_t)lroundf(y * 10.9f));
        r[0x09] = 0x01;
        break;
    case MAG_CHIP_LIS3MDL:
        put_le(r + 0x28, (int16_t)lroundf(x * 68.42f));
        put_le(r + 0x2A, (int16_t)lroundf(y * 68.42f));
        put_le(r + 0x2C, (int16_t)lroundf(z * 68.42f));
        r[0x27] = 0x08;
        break;
    default:
        break;
    }
}

static void chip_reset(mag_chip_t chip, uint8_t addr)
{
    memset(&g_chip, 0, sizeof(g_chip));
    g_chip.present = true;
    g_chip.chip = chip;
    g_chip.addr = addr;
    uint8_t *r = g_chip.regs;
    switch (chip) {
    case MAG_CHIP_QMC5883L: r[0x0D] = 0xFF; break;
    case MAG_CHIP_IST8310:  r[0x00] = 0x10; break;
    case MAG_CHIP_HMC5883L: r[0x0A] = 'H'; r[0x0B] = '4'; r[0x0C] = '3'; break;
    case MAG_CHIP_LIS3MDL:  r[0x0F] = 0x3D; break;
    default: break;
    }
    chip_field(20.0f, 0.0f, 40.0f);
}

static bool bus_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n, void *user)
{
    (void)user;
    if (!g_chip.present || addr != g_chip.addr) return false;
    g_chip.reads++;
    if (g_chip.chip == MAG_CHIP_LIS3MDL) {
        const bool inc = (reg & 0x80) != 0;             /* sans le bit de rafale : même registre */
        reg &= 0x7F;
        for (size_t i = 0; i < n; i++) buf[i] = g_chip.regs[(uint8_t)(reg + (inc ? i : 0))];
        return true;
    }
    for (size_t i = 0; i < n; i++) buf[i] = g_chip.regs[(uint8_t)(reg + i)];
    if (g_chip.chip == MAG_CHIP_IST8310 && reg == 0x03) g_chip.regs[0x02] = 0;   /* DRDY lu */
    return true;
}

static bool bus_write(uint8_t addr, uint8_t reg, uint8_t val, void *user)
{
    (void)user;
    if (!g_chip.present || addr != g_chip.addr) return false;
    g_chip.writes++;
    g_chip.regs[reg] = val;
    if (g_chip.chip == MAG_CHIP_IST8310 && reg == 0x0A && val == 0x01) {
        g_chip.regs[0x0A] = 0;                          /* mesure faite, puce en veille */
        g_chip.regs[0x02] = 0x01;
    }
    return true;
}

static mag_t    g_mag;
static uint64_t g_now;

static void mag_start(void)
{
    g_now = 1000;
    const mag_bus_t bus = {.read = bus_read, .write = bus_write};
    mag_init(&g_mag, &bus, g_now);
}

static int run_for(uint32_t ms)
{
    int fresh = 0;
    for (uint32_t t = 0; t < ms; t += 10) {
        g_now += 10;
        if (mag_tick(&g_mag, g_now)) fresh++;
    }
    return fresh;
}

#define NEAR(want, got) TEST_ASSERT_FLOAT_WITHIN(0.2f, (want), (got))

/* ------------------------------------------------------------------ tests */

static void test_absent(void)
{
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    g_chip.present = false;
    mag_start();
    run_for(5000);
    TEST_ASSERT_FALSE(mag_present(&g_mag, g_now));
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_NONE, mag_chip(&g_mag));
    TEST_ASSERT_EQUAL_UINT32(3, mag_stats(&g_mag)->probes);   /* à 0, 2 et 4 s */
}

static void test_qmc5883l(void)
{
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    TEST_ASSERT_TRUE(run_for(500) > 5);
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_QMC5883L, mag_chip(&g_mag));
    TEST_ASSERT_EQUAL_HEX8(0x1D, g_chip.regs[0x09]);            /* continu, 200 Hz, ±8 G */
    TEST_ASSERT_EQUAL_HEX8(0x01, g_chip.regs[0x0B]);
    /* puce à plat : Y et Z changent de signe vers le repère voiture (Z bas) */
    chip_field(10.0f, 15.0f, -40.0f);
    run_for(100);
    const mag_vec_t f = mag_field(&g_mag);
    NEAR(10.0f, f.x);
    NEAR(-15.0f, f.y);
    NEAR(40.0f, f.z);
    TEST_ASSERT_EQUAL_STRING("QMC5883L", mag_chip_name(mag_chip(&g_mag)));
}

static void test_qmc_overflow_ignored(void)
{
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    run_for(200);
    const uint32_t n = mag_stats(&g_mag)->samples;
    g_chip.regs[0x06] = 0x03;                                  /* saturé */
    run_for(200);
    TEST_ASSERT_EQUAL_UINT32(n, mag_stats(&g_mag)->samples);
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));             /* moins de stale_ms */
}

static void test_hmc5883l_axis_order(void)
{
    chip_reset(MAG_CHIP_HMC5883L, 0x1E);
    mag_start();
    chip_field(5.0f, 30.0f, -20.0f);
    run_for(300);
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_HMC5883L, mag_chip(&g_mag));
    TEST_ASSERT_EQUAL_HEX8(0x00, g_chip.regs[0x02]);            /* continu */
    const mag_vec_t f = mag_field(&g_mag);
    NEAR(5.0f, f.x);
    NEAR(-30.0f, f.y);
    NEAR(20.0f, f.z);
}

static void test_lis3mdl_burst(void)
{
    chip_reset(MAG_CHIP_LIS3MDL, 0x1C);
    mag_start();
    chip_field(-12.0f, 7.0f, 33.0f);
    run_for(300);
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_LIS3MDL, mag_chip(&g_mag));
    TEST_ASSERT_EQUAL_HEX8(0x00, g_chip.regs[0x22]);            /* continu */
    const mag_vec_t f = mag_field(&g_mag);
    NEAR(-12.0f, f.x);                                          /* juste seulement en rafale */
    NEAR(-7.0f, f.y);
    NEAR(-33.0f, f.z);
}

static void test_ist8310_single_shots(void)
{
    chip_reset(MAG_CHIP_IST8310, 0x0E);
    mag_start();
    chip_field(25.0f, -25.0f, 10.0f);
    const int fresh = run_for(1000);
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_IST8310, mag_chip(&g_mag));
    TEST_ASSERT_TRUE(fresh >= 15);                              /* 20 Hz : chaque lecture relance */
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));
    NEAR(25.0f, mag_field(&g_mag).x);
    NEAR(25.0f, mag_field(&g_mag).y);
}

static void test_heading(void)
{
    NEAR(0.0f, mag_heading_deg((mag_vec_t){20, 0, 40}));
    NEAR(90.0f, mag_heading_deg((mag_vec_t){0, -20, 40}));     /* nord à gauche : cap est */
    NEAR(180.0f, mag_heading_deg((mag_vec_t){-20, 0, 40}));
    NEAR(270.0f, mag_heading_deg((mag_vec_t){0, 20, 40}));
    NEAR(45.0f, mag_heading_deg((mag_vec_t){10, -10, 0}));
}

static void test_orientation(void)
{
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    chip_field(20.0f, 0.0f, -40.0f);                           /* nord le long de X de la puce */
    run_for(100);
    NEAR(0.0f, mag_heading_deg(mag_field(&g_mag)));
    mag_set_orient(&g_mag, (mag_orient_t){.yaw_quarter = 1});
    run_for(100);
    NEAR(270.0f, mag_heading_deg(mag_field(&g_mag)));
    mag_set_orient(&g_mag, (mag_orient_t){.yaw_quarter = 2});
    run_for(100);
    NEAR(180.0f, mag_heading_deg(mag_field(&g_mag)));
    /* puce à l'envers : Z de la puce vers le bas, le champ vertical change de signe */
    mag_set_orient(&g_mag, (mag_orient_t){.flipped = true});
    run_for(100);
    NEAR(-40.0f, mag_field(&g_mag).z);
    NEAR(0.0f, mag_heading_deg(mag_field(&g_mag)));
}

static void test_calibration(void)
{
    /* tour complet à plat avec un fer dur (5, −8) µT et une ellipse 1,2 / 1,0 */
    mag_cal_acc_t a;
    mag_cal_reset(&a);
    for (int d = 0; d < 360; d += 5) {
        const float r = (float)d * 3.14159265f / 180.0f;
        mag_cal_feed(&a, (mag_vec_t){5.0f + 24.0f * cosf(r), -8.0f + 20.0f * sinf(r), 40.0f});
    }
    mag_cal_t c;
    TEST_ASSERT_FALSE(mag_cal_compute(&a, 60.0f, &c));          /* étendue insuffisante */
    TEST_ASSERT_TRUE(mag_cal_compute(&a, 20.0f, &c));
    NEAR(5.0f, c.offset.x);
    NEAR(-8.0f, c.offset.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 44.0f / 48.0f, c.scale.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 44.0f / 40.0f, c.scale.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.scale.z);

    /* appliquée à la lecture : cap juste malgré le fer dur */
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    mag_set_cal(&g_mag, &c);
    chip_field(5.0f, 8.0f - 20.0f, -40.0f);                    /* voiture : (5, −8 + 20) → plein ouest */
    run_for(100);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 270.0f, mag_heading_deg(mag_field(&g_mag)));
}

static void test_unplug_and_replug(void)
{
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    run_for(300);
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));
    g_chip.present = false;
    run_for(300);
    TEST_ASSERT_FALSE(mag_present(&g_mag, g_now));
    TEST_ASSERT_EQUAL_UINT32(1, mag_stats(&g_mag)->losses);
    chip_reset(MAG_CHIP_IST8310, 0x0E);                         /* autre puck */
    run_for(2500);
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));
    TEST_ASSERT_EQUAL_INT(MAG_CHIP_IST8310, mag_chip(&g_mag));
}

static void test_frozen_chip_lost(void)
{
    /* la puce répond mais ne mesure plus (réinitialisée par une coupure brève) */
    chip_reset(MAG_CHIP_QMC5883L, 0x0D);
    mag_start();
    run_for(200);
    g_chip.regs[0x06] = 0x00;
    run_for(1200);
    TEST_ASSERT_EQUAL_UINT32(1, mag_stats(&g_mag)->losses);
    g_chip.regs[0x06] = 0x01;                                   /* elle mesure de nouveau */
    run_for(2500);
    TEST_ASSERT_TRUE(mag_present(&g_mag, g_now));
    TEST_ASSERT_EQUAL_UINT32(2, mag_stats(&g_mag)->probes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_absent);
    RUN_TEST(test_qmc5883l);
    RUN_TEST(test_qmc_overflow_ignored);
    RUN_TEST(test_hmc5883l_axis_order);
    RUN_TEST(test_lis3mdl_burst);
    RUN_TEST(test_ist8310_single_shots);
    RUN_TEST(test_heading);
    RUN_TEST(test_orientation);
    RUN_TEST(test_calibration);
    RUN_TEST(test_unplug_and_replug);
    RUN_TEST(test_frozen_chip_lost);
    return UNITY_END();
}
