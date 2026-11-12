/*
 * Tests de core/gps_link avec un récepteur u-blox émulé : débit trouvé, MON-VER, configuration
 * M8 ou M9, débit cible, récepteur inconnu (NMEA seul), absent, débranché puis rebranché.
 *
 * Le récepteur émulé n'entend la liaison et n'est entendu d'elle que si les deux UART sont au même
 * débit ; sinon la liaison reçoit du bruit.
 *
 * Licence : GPL-3.0.
 */

#include <stdio.h>
#include <string.h>

#include "gps_link.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ récepteur émulé */

typedef struct {
    bool             plugged;
    bool             answers_ubx;   /* faux : clone NMEA seul, sourd à l'UBX */
    ubx_generation_t gen;
    uint32_t         baud;
    uint16_t         period_ms;     /* 1000 au démarrage, meas_ms une fois configuré */
    bool             gsv;           /* GSV en sortie (coupé par la configuration) */
    uint64_t         next_ms;
    uint32_t         seq;
    ubx_stream_t     in;            /* ce que la liaison lui envoie */
    uint8_t          out[1024];     /* réponses en attente */
    size_t           n_out;
    int              mon_ver_polls, configs, baud_changes;
    uint32_t         new_baud;      /* débit à prendre après la réponse en cours */
} fake_rx_t;

static fake_rx_t g_rx;
static uint32_t  g_link_baud;
static gps_link_t g_link;
static int        g_nmea;
static uint64_t   g_now;

static void rx_queue(const uint8_t *d, size_t n)
{
    if (g_rx.n_out + n <= sizeof(g_rx.out)) {
        memcpy(g_rx.out + g_rx.n_out, d, n);
        g_rx.n_out += n;
    }
}

static void rx_ack(uint8_t cls, uint8_t id)
{
    uint8_t f[16];
    const uint8_t pl[2] = {cls, id};
    rx_queue(f, ubx_frame(UBX_CLS_ACK, UBX_ID_ACK_ACK, pl, 2, f, sizeof(f)));
}

static void rx_frame(uint8_t cls, uint8_t id, const uint8_t *p, uint16_t len, void *user)
{
    (void)user;
    if (!g_rx.answers_ubx) return;
    if (cls == UBX_CLS_MON && id == UBX_ID_MON_VER && len == 0) {
        g_rx.mon_ver_polls++;
        uint8_t pl[40 + 3 * 30] = {0};
        const bool m9 = g_rx.gen == UBX_GEN_9;
        strcpy((char *)pl, m9 ? "ROM SPG 4.04 (7f89f7)" : "ROM CORE 3.01 (107888)");
        strcpy((char *)pl + 30, m9 ? "00190000" : "00080000");
        strcpy((char *)pl + 40, m9 ? "FWVER=SPG 4.04" : "FWVER=SPG 3.01");
        strcpy((char *)pl + 70, m9 ? "PROTVER=32.01" : "PROTVER=18.00");
        strcpy((char *)pl + 100, m9 ? "MOD=NEO-M9N" : "MOD=NEO-M8N");
        uint8_t f[160];
        rx_queue(f, ubx_frame(UBX_CLS_MON, UBX_ID_MON_VER, pl, sizeof(pl), f, sizeof(f)));
    } else if (cls == UBX_CLS_CFG && id == UBX_ID_CFG_VALSET && g_rx.gen == UBX_GEN_9) {
        /* lecture des paires clé / valeur qui nous intéressent */
        for (uint16_t i = 4; i + 4 < len;) {
            const uint32_t key = (uint32_t)p[i] | (uint32_t)p[i + 1] << 8 | (uint32_t)p[i + 2] << 16 | (uint32_t)p[i + 3] << 24;
            const unsigned sz = ((key >> 28) & 7) == 3 ? 2 : ((key >> 28) & 7) == 4 ? 4 : 1;
            uint32_t v = 0;
            for (unsigned k = 0; k < sz; k++) v |= (uint32_t)p[i + 4 + k] << (8 * k);
            if (key == 0x30210001u) { g_rx.period_ms = (uint16_t)v; g_rx.configs++; }
            if (key == 0x209100c5u) g_rx.gsv = v != 0;
            if (key == 0x40520001u) { g_rx.new_baud = v; g_rx.baud_changes++; }
            i = (uint16_t)(i + 4 + sz);
        }
        rx_ack(cls, id);
    } else if (cls == UBX_CLS_CFG && g_rx.gen == UBX_GEN_8) {
        if (id == UBX_ID_CFG_RATE) { g_rx.period_ms = (uint16_t)(p[0] | p[1] << 8); g_rx.configs++; }
        if (id == UBX_ID_CFG_MSG && p[0] == 0xF0 && p[1] == 0x03) g_rx.gsv = p[2] != 0;
        if (id == UBX_ID_CFG_PRT && p[0] == 1) {
            g_rx.new_baud = (uint32_t)p[8] | (uint32_t)p[9] << 8 | (uint32_t)p[10] << 16 | (uint32_t)p[11] << 24;
            g_rx.baud_changes++;
        }
        rx_ack(cls, id);
    }
}

static void rx_reset(ubx_generation_t gen, uint32_t baud, bool answers)
{
    memset(&g_rx, 0, sizeof(g_rx));
    g_rx.plugged = true;
    g_rx.answers_ubx = answers;
    g_rx.gen = gen;
    g_rx.baud = baud;
    g_rx.period_ms = 1000;
    g_rx.gsv = true;
    ubx_stream_init(&g_rx.in, NULL, rx_frame, NULL);
}

/* La liaison écrit : le récepteur l'entend s'il est au même débit. */
static void io_write(const uint8_t *d, size_t n, void *user)
{
    (void)user;
    if (!g_rx.plugged || g_rx.baud != g_link_baud) return;
    ubx_stream_feed(&g_rx.in, d, n);
    if (g_rx.new_baud != 0) {           /* changement de débit après la réponse (comme les u-blox) */
        g_rx.baud = g_rx.new_baud;
        g_rx.new_baud = 0;
    }
}

static void io_set_baud(uint32_t baud, void *user)
{
    (void)user;
    g_link_baud = baud;
}

static void io_nmea(const char *line, size_t len, void *user)
{
    (void)user;
    (void)len;
    TEST_ASSERT_EQUAL_CHAR('$', line[0]);
    g_nmea++;
}

static void nmea_line(char *out, size_t cap, const char *body)
{
    uint8_t x = 0;
    for (const char *c = body; *c; c++) x ^= (uint8_t)*c;
    snprintf(out, cap, "$%s*%02X\r\n", body, x);
}

/* Le récepteur émet : la liaison reçoit ses octets au bon débit, du bruit sinon. */
static void deliver(const uint8_t *d, size_t n)
{
    if (g_rx.baud == g_link_baud) {
        gps_link_feed(&g_link, d, n, g_now);
    } else {
        uint8_t junk[64];
        for (size_t i = 0; i < sizeof(junk); i++) junk[i] = (uint8_t)(0x80 | (i * 37 + g_now));
        gps_link_feed(&g_link, junk, n < sizeof(junk) ? n : sizeof(junk), g_now);
    }
}

static void rx_step(void)
{
    if (!g_rx.plugged) return;
    if (g_rx.n_out > 0) {
        const size_t n = g_rx.n_out;
        g_rx.n_out = 0;
        deliver(g_rx.out, n);
    }
    if (g_now >= g_rx.next_ms) {
        g_rx.next_ms = g_now + g_rx.period_ms;
        char l[128];
        char body[100];
        const unsigned s = g_rx.seq++;
        snprintf(body, sizeof(body), "GNRMC,%02u%02u%02u.00,A,4807.038,N,01131.000,E,0.0,,230394,,,A",
                 (s / 3600) % 24, (s / 60) % 60, s % 60);
        nmea_line(l, sizeof(l), body);
        deliver((const uint8_t *)l, strlen(l));
        snprintf(body, sizeof(body), "GNGGA,%02u%02u%02u.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",
                 (s / 3600) % 24, (s / 60) % 60, s % 60);
        nmea_line(l, sizeof(l), body);
        deliver((const uint8_t *)l, strlen(l));
        if (g_rx.gsv) {
            nmea_line(l, sizeof(l), "GPGSV,1,1,00");
            deliver((const uint8_t *)l, strlen(l));
        }
    }
}

static void link_start(void)
{
    g_now = 1000;
    g_nmea = 0;
    g_link_baud = 0;
    const gps_link_io_t io = {.write = io_write, .set_baud = io_set_baud, .nmea = io_nmea};
    gps_link_init(&g_link, NULL, &io, g_now);
}

static void run_for(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        g_now += 10;
        rx_step();
        gps_link_tick(&g_link, g_now);
    }
}

/* ------------------------------------------------------------------ tests */

static void test_nmea_checksum(void)
{
    const char *ok = "$GNGGA,1*00";
    TEST_ASSERT_FALSE(gps_nmea_checksum_ok(ok, strlen(ok)));
    char l[64];
    nmea_line(l, sizeof(l), "GNGGA,1");
    l[strlen(l) - 2] = '\0';
    TEST_ASSERT_TRUE(gps_nmea_checksum_ok(l, strlen(l)));
    TEST_ASSERT_FALSE(gps_nmea_checksum_ok("$GNGGA,1*4", 10));
    TEST_ASSERT_FALSE(gps_nmea_checksum_ok("GNGGA,1*41", 10));
}

static void test_m8_puck_at_9600(void)
{
    rx_reset(UBX_GEN_8, 9600, true);
    link_start();
    TEST_ASSERT_FALSE(gps_link_present(&g_link, g_now));
    run_for(10000);
    TEST_ASSERT_EQUAL_INT(GPS_LINK_RUN, gps_link_state(&g_link));
    TEST_ASSERT_TRUE(gps_link_present(&g_link, g_now));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_8, gps_link_version(&g_link)->gen);
    TEST_ASSERT_EQUAL_STRING("NEO-M8N", gps_link_version(&g_link)->module);
    TEST_ASSERT_EQUAL_UINT32(115200, gps_link_baud(&g_link));
    TEST_ASSERT_EQUAL_UINT32(115200, g_rx.baud);
    TEST_ASSERT_EQUAL_UINT16(100, g_rx.period_ms);
    TEST_ASSERT_FALSE(g_rx.gsv);
    TEST_ASSERT_EQUAL_INT(1, g_rx.baud_changes);
    TEST_ASSERT_TRUE(gps_link_stats(&g_link)->acks > 0);
    /* 10 Hz une fois configuré : RMC + GGA par époque, transmises au core */
    const int before = g_nmea;
    run_for(1000);
    TEST_ASSERT_INT_WITHIN(2, 20, g_nmea - before);
}

static void test_m9_at_38400(void)
{
    rx_reset(UBX_GEN_9, 38400, true);
    link_start();
    run_for(8000);
    TEST_ASSERT_EQUAL_INT(GPS_LINK_RUN, gps_link_state(&g_link));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_9, gps_link_version(&g_link)->gen);
    TEST_ASSERT_EQUAL_STRING("32.01", gps_link_version(&g_link)->protver);
    TEST_ASSERT_EQUAL_UINT32(115200, g_rx.baud);
    TEST_ASSERT_EQUAL_UINT16(100, g_rx.period_ms);
    TEST_ASSERT_EQUAL_INT(1, g_rx.configs);
}

static void test_already_at_target(void)
{
    /* récepteur déjà réglé (redémarrage de l'ESP32 seul) : pas de changement de débit */
    rx_reset(UBX_GEN_9, 115200, true);
    link_start();
    run_for(3000);
    TEST_ASSERT_EQUAL_INT(GPS_LINK_RUN, gps_link_state(&g_link));
    TEST_ASSERT_EQUAL_INT(0, g_rx.baud_changes);
    TEST_ASSERT_EQUAL_INT(1, g_rx.configs);
}

static void test_nmea_only_clone(void)
{
    rx_reset(UBX_GEN_8, 38400, false);
    link_start();
    run_for(10000);
    TEST_ASSERT_EQUAL_INT(GPS_LINK_RUN, gps_link_state(&g_link));
    TEST_ASSERT_TRUE(gps_link_present(&g_link, g_now));
    TEST_ASSERT_EQUAL_INT(UBX_GEN_UNKNOWN, gps_link_version(&g_link)->gen);
    TEST_ASSERT_EQUAL_UINT32(38400, gps_link_baud(&g_link));
    TEST_ASSERT_TRUE(g_nmea > 10);
}

static void test_absent(void)
{
    rx_reset(UBX_GEN_8, 9600, true);
    g_rx.plugged = false;
    link_start();
    run_for(10000);
    TEST_ASSERT_FALSE(gps_link_present(&g_link, g_now));
    TEST_ASSERT_EQUAL_INT(GPS_LINK_SEARCH, gps_link_state(&g_link));
    TEST_ASSERT_TRUE(gps_link_stats(&g_link)->baud_tries >= 6);
    TEST_ASSERT_EQUAL_INT(0, g_nmea);
}

static void test_unplug_and_replug(void)
{
    rx_reset(UBX_GEN_8, 38400, true);
    link_start();
    run_for(8000);
    TEST_ASSERT_TRUE(gps_link_present(&g_link, g_now));
    g_rx.plugged = false;                          /* débranché */
    run_for(4000);
    TEST_ASSERT_FALSE(gps_link_present(&g_link, g_now));
    TEST_ASSERT_EQUAL_UINT32(1, gps_link_stats(&g_link)->losses);
    rx_reset(UBX_GEN_8, 38400, true);              /* rebranché : redémarre à son débit d'usine */
    run_for(10000);
    TEST_ASSERT_EQUAL_INT(GPS_LINK_RUN, gps_link_state(&g_link));
    TEST_ASSERT_TRUE(gps_link_present(&g_link, g_now));
    TEST_ASSERT_EQUAL_UINT32(115200, g_rx.baud);
    TEST_ASSERT_EQUAL_UINT16(100, g_rx.period_ms);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nmea_checksum);
    RUN_TEST(test_m8_puck_at_9600);
    RUN_TEST(test_m9_at_38400);
    RUN_TEST(test_already_at_target);
    RUN_TEST(test_nmea_only_clone);
    RUN_TEST(test_absent);
    RUN_TEST(test_unplug_and_replug);
    return UNITY_END();
}
