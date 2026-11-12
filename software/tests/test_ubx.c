/*
 * Tests de core/ubx : trames, découpage du flux mêlé NMEA / UBX, MON-VER, trames de configuration.
 *
 * Licence : GPL-3.0.
 */

#include <string.h>

#include "ubx.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ trames */

static void test_mon_ver_poll_frame(void)
{
    /* trame classique de la littérature u-blox */
    const uint8_t want[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34};
    uint8_t f[16];
    TEST_ASSERT_EQUAL_size_t(8, ubx_build_mon_ver_poll(f, sizeof(f)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, f, 8);
}

static void test_cfg_rate_frame_m8(void)
{
    /* CFG-RATE 100 ms, navRate 1, timeRef GPS : B5 62 06 08 06 00 64 00 01 00 01 00 7A 12 */
    const uint8_t want[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12};
    const ubx_cfg_t c = {.meas_ms = 100, .baud = 115200, .dyn_model = UBX_DYN_AUTOMOTIVE};
    uint8_t buf[256];
    const size_t n = ubx_build_m8_config(&c, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > sizeof(want));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, buf, sizeof(want));
    /* CFG-RATE + CFG-NAV5 + 6 × CFG-MSG */
    TEST_ASSERT_EQUAL_size_t((6 + 8) + (36 + 8) + 6 * (3 + 8), n);
}

static void test_frame_too_small(void)
{
    uint8_t f[7];
    TEST_ASSERT_EQUAL_size_t(0, ubx_build_mon_ver_poll(f, sizeof(f)));
}

/* ------------------------------------------------------------------ découpage */

static char     g_lines[8][UBX_NMEA_MAX + 1];
static int      g_nlines;
static uint8_t  g_cls[8], g_id[8];
static uint16_t g_len[8];
static int      g_nframes;

static void cb_nmea(const char *line, size_t len, void *user)
{
    (void)user;
    TEST_ASSERT_EQUAL_size_t(strlen(line), len);
    if (g_nlines < 8) strcpy(g_lines[g_nlines++], line);
}

static void cb_frame(uint8_t cls, uint8_t id, const uint8_t *p, uint16_t len, void *user)
{
    (void)user;
    (void)p;
    if (g_nframes < 8) {
        g_cls[g_nframes] = cls;
        g_id[g_nframes] = id;
        g_len[g_nframes] = len;
        g_nframes++;
    }
}

static void reset_cb(void)
{
    g_nlines = g_nframes = 0;
}

static void test_stream_mixed(void)
{
    ubx_stream_t s;
    ubx_stream_init(&s, cb_nmea, cb_frame, NULL);
    reset_cb();
    uint8_t buf[256];
    size_t n = 0;
    const char *l1 = "\x07\x99junk$GNRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,230394,,,A*6C\r\n";
    memcpy(buf + n, l1, strlen(l1));
    n += strlen(l1);
    n += ubx_build_mon_ver_poll(buf + n, sizeof(buf) - n);
    const char *l2 = "$GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n";
    memcpy(buf + n, l2, strlen(l2));
    n += strlen(l2);
    /* livré octet par octet : le découpage ne dépend pas des paquets */
    for (size_t i = 0; i < n; i++) ubx_stream_feed(&s, buf + i, 1);
    TEST_ASSERT_EQUAL_INT(2, g_nlines);
    TEST_ASSERT_EQUAL_STRING("$GNRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,230394,,,A*6C", g_lines[0]);
    TEST_ASSERT_EQUAL_CHAR('$', g_lines[1][0]);
    TEST_ASSERT_EQUAL_INT(1, g_nframes);
    TEST_ASSERT_EQUAL_HEX8(UBX_CLS_MON, g_cls[0]);
    TEST_ASSERT_EQUAL_HEX8(UBX_ID_MON_VER, g_id[0]);
    TEST_ASSERT_EQUAL_UINT16(0, g_len[0]);
    TEST_ASSERT_EQUAL_UINT32(1, s.stats.ubx_frames);
    TEST_ASSERT_TRUE(s.stats.junk >= 6);
}

static void test_stream_bad_checksum_and_truncated(void)
{
    ubx_stream_t s;
    ubx_stream_init(&s, cb_nmea, cb_frame, NULL);
    reset_cb();
    uint8_t f[16];
    const size_t n = ubx_build_mon_ver_poll(f, sizeof(f));
    f[n - 1] ^= 0xFF;
    ubx_stream_feed(&s, f, n);
    TEST_ASSERT_EQUAL_INT(0, g_nframes);
    TEST_ASSERT_EQUAL_UINT32(1, s.stats.ubx_bad);
    /* phrase coupée par une autre (débit faux) : la première est jetée */
    const char *t = "$GNRMC,1235$GNGGA,1*00\r\n";
    ubx_stream_feed(&s, (const uint8_t *)t, strlen(t));
    TEST_ASSERT_EQUAL_INT(1, g_nlines);
    TEST_ASSERT_EQUAL_STRING("$GNGGA,1*00", g_lines[0]);
    /* longueur annoncée hors limite */
    const uint8_t big[] = {0xB5, 0x62, 0x0A, 0x04, 0xFF, 0x7F};
    ubx_stream_feed(&s, big, sizeof(big));
    TEST_ASSERT_EQUAL_UINT32(2, s.stats.ubx_bad);
}

/* ------------------------------------------------------------------ MON-VER */

static uint16_t mon_ver_payload(uint8_t *p, const char *sw, const char *hw, const char *const *ext, int n_ext)
{
    memset(p, 0, 40 + 30 * (size_t)n_ext);
    strncpy((char *)p, sw, 30);
    strncpy((char *)p + 30, hw, 10);
    for (int i = 0; i < n_ext; i++) strncpy((char *)p + 40 + 30 * i, ext[i], 30);
    return (uint16_t)(40 + 30 * n_ext);
}

static void test_mon_ver_m8(void)
{
    uint8_t p[UBX_MAX_PAYLOAD];
    const char *ext[] = {"FWVER=SPG 3.01", "PROTVER=18.00", "MOD=NEO-M8N", "GPS;GLO;GAL;BDS", "SBAS;IMES;QZSS"};
    const uint16_t len = mon_ver_payload(p, "ROM CORE 3.01 (107888)", "00080000", ext, 5);
    ubx_version_t v;
    TEST_ASSERT_TRUE(ubx_mon_ver_decode(p, len, &v));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_8, v.gen);
    TEST_ASSERT_EQUAL_STRING("NEO-M8N", v.module);
    TEST_ASSERT_EQUAL_STRING("18.00", v.protver);
    TEST_ASSERT_EQUAL_STRING("00080000", v.hw);
    TEST_ASSERT_EQUAL_STRING("u-blox M8", ubx_generation_name(v.gen));
}

static void test_mon_ver_m9_and_module_fallback(void)
{
    uint8_t p[UBX_MAX_PAYLOAD];
    const char *ext[] = {"FWVER=SPG 4.04", "PROTVER=32.01", "MOD=NEO-M9N"};
    ubx_version_t v;
    TEST_ASSERT_TRUE(ubx_mon_ver_decode(p, mon_ver_payload(p, "ROM SPG 4.04 (7f89f7)", "00190000", ext, 3), &v));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_9, v.gen);
    /* hwVersion inconnue (clone) : la génération vient du module */
    TEST_ASSERT_TRUE(ubx_mon_ver_decode(p, mon_ver_payload(p, "x", "12345678", ext + 2, 1), &v));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_9, v.gen);
    TEST_ASSERT_FALSE(ubx_mon_ver_decode(p, 39, &v));
}

/* ------------------------------------------------------------------ M9 */

static bool contains(const uint8_t *buf, size_t n, const uint8_t *pat, size_t m)
{
    for (size_t i = 0; i + m <= n; i++)
        if (memcmp(buf + i, pat, m) == 0) return true;
    return false;
}

static void test_m9_valset(void)
{
    const ubx_cfg_t c = {.meas_ms = 100, .baud = 115200, .dyn_model = UBX_DYN_AUTOMOTIVE};
    uint8_t f[256];
    const size_t n = ubx_build_m9_config(&c, f, sizeof(f));
    TEST_ASSERT_TRUE(n > 12);
    TEST_ASSERT_EQUAL_HEX8(UBX_CLS_CFG, f[2]);
    TEST_ASSERT_EQUAL_HEX8(UBX_ID_CFG_VALSET, f[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, f[6]);          /* version */
    TEST_ASSERT_EQUAL_HEX8(0x01, f[7]);          /* couche RAM */
    const uint8_t rate[] = {0x01, 0x00, 0x21, 0x30, 0x64, 0x00};          /* CFG-RATE-MEAS = 100 */
    const uint8_t dyn[] = {0x21, 0x00, 0x11, 0x20, 0x04};                 /* CFG-NAVSPG-DYNMODEL = 4 */
    const uint8_t gsv[] = {0xc5, 0x00, 0x91, 0x20, 0x00};                 /* GSV coupé sur l'UART1 */
    TEST_ASSERT_TRUE(contains(f, n, rate, sizeof(rate)));
    TEST_ASSERT_TRUE(contains(f, n, dyn, sizeof(dyn)));
    TEST_ASSERT_TRUE(contains(f, n, gsv, sizeof(gsv)));
    /* la trame se relit : checksum juste */
    ubx_stream_t s;
    ubx_stream_init(&s, NULL, NULL, NULL);
    ubx_stream_feed(&s, f, n);
    TEST_ASSERT_EQUAL_UINT32(1, s.stats.ubx_frames);

    const size_t nb = ubx_build_m9_baud(115200, f, sizeof(f));
    const uint8_t baud[] = {0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00};   /* CFG-UART1-BAUDRATE */
    TEST_ASSERT_EQUAL_size_t(8 + 4 + 8, nb);
    TEST_ASSERT_TRUE(contains(f, nb, baud, sizeof(baud)));
}

static void test_m8_baud(void)
{
    uint8_t f[64];
    const size_t n = ubx_build_m8_baud(38400, f, sizeof(f));
    TEST_ASSERT_EQUAL_size_t(28, n);
    TEST_ASSERT_EQUAL_HEX8(UBX_ID_CFG_PRT, f[3]);
    TEST_ASSERT_EQUAL_HEX8(1, f[6]);                         /* UART1 */
    TEST_ASSERT_EQUAL_HEX8(0x00, f[6 + 8]);                  /* 38400 = 0x9600 */
    TEST_ASSERT_EQUAL_HEX8(0x96, f[6 + 9]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mon_ver_poll_frame);
    RUN_TEST(test_cfg_rate_frame_m8);
    RUN_TEST(test_frame_too_small);
    RUN_TEST(test_stream_mixed);
    RUN_TEST(test_stream_bad_checksum_and_truncated);
    RUN_TEST(test_mon_ver_m8);
    RUN_TEST(test_mon_ver_m9_and_module_fallback);
    RUN_TEST(test_m9_valset);
    RUN_TEST(test_m8_baud);
    return UNITY_END();
}
