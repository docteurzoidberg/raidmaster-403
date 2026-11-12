/*
 * Raidmaster 403 — hal_gps réel sur l'ESP32 : récepteur u-blox sur un UART (NEO-M9N soudé sur la
 * carte fille, ou puck de drone NEO-M8N branché), piloté par core/gps_link (débit trouvé, MON-VER,
 * configuration 10 Hz automobile, débit cible). Choisi par menuconfig (« Raidmaster 403 » →
 * « Source GPS ») ; le trajet simulé (hal_gps_demo.c) reste le choix par défaut.
 *
 * Broches par défaut : UART1, TX IO17 → RXD du récepteur, RX IO18 ← TXD, GPS_EN IO45 (régulateur
 * GNSS de la carte fille ; broche de strapping, mise à 1 seulement après le démarrage). Sur la
 * Waveshare seule (puck sur le J8, alimenté par VBAT), GPS_EN ne commande rien : sans effet.
 *
 * L'heure du récepteur règle l'horloge de la carte au premier fix (board_clock_sync_enable(true)).
 *
 * Licence : GPL-3.0.
 */

#include "sdkconfig.h"

#if CONFIG_RAIDMASTER_GPS_UART

#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "board.h"
#include "gps_link.h"
#include "hal/hal_gps.h"
#include "hal/hal_time.h"

static const char *TAG = "hal_gps";

#define GPS_UART     ((uart_port_t)CONFIG_RAIDMASTER_GPS_UART_NUM)
#define RX_BUF_SIZE  4096          /* > 0,3 s de flux à 115 200 bauds : la boucle tourne au moins à 30 Hz */

static hal_gps_line_cb  s_cb;
static void            *s_user;
static gps_link_t       s_link;
static gps_link_state_t s_state;
static bool             s_present;

static void io_write(const uint8_t *data, size_t n, void *user)
{
    (void)user;
    uart_write_bytes(GPS_UART, data, n);
}

/* La commande de changement de débit doit partir entière à l'ancien débit. */
static void io_set_baud(uint32_t baud, void *user)
{
    (void)user;
    uart_wait_tx_done(GPS_UART, pdMS_TO_TICKS(200));
    uart_set_baudrate(GPS_UART, baud);
    uart_flush_input(GPS_UART);
}

static void io_nmea(const char *line, size_t len, void *user)
{
    (void)user;
    if (s_cb != NULL) s_cb(line, len, s_user);
}

void hal_gps_init(hal_gps_line_cb cb, void *user)
{
    s_cb = cb;
    s_user = user;

#if CONFIG_RAIDMASTER_GPS_EN_GPIO >= 0
    const gpio_config_t en = {
        .pin_bit_mask = 1ULL << CONFIG_RAIDMASTER_GPS_EN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&en));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_RAIDMASTER_GPS_EN_GPIO, 1));
#endif

    const uart_config_t uc = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART, RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART, &uc));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART, CONFIG_RAIDMASTER_GPS_TX_GPIO, CONFIG_RAIDMASTER_GPS_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    gps_link_cfg_t cfg;
    gps_link_default_cfg(&cfg);
    cfg.target_baud = CONFIG_RAIDMASTER_GPS_BAUD;
    cfg.meas_ms = (uint16_t)(1000 / CONFIG_RAIDMASTER_GPS_RATE_HZ);
    const gps_link_io_t io = {.write = io_write, .set_baud = io_set_baud, .nmea = io_nmea};
    gps_link_init(&s_link, &cfg, &io, hal_time_ms());
    s_state = gps_link_state(&s_link);

    board_clock_sync_enable(true);
    ESP_LOGI(TAG, "récepteur sur UART%d (TX %d, RX %d, EN %d), cible %d bauds, %d Hz", CONFIG_RAIDMASTER_GPS_UART_NUM,
             CONFIG_RAIDMASTER_GPS_TX_GPIO, CONFIG_RAIDMASTER_GPS_RX_GPIO, CONFIG_RAIDMASTER_GPS_EN_GPIO,
             CONFIG_RAIDMASTER_GPS_BAUD, CONFIG_RAIDMASTER_GPS_RATE_HZ);
}

static void log_changes(uint64_t now)
{
    const gps_link_state_t st = gps_link_state(&s_link);
    if (st != s_state) {
        s_state = st;
        const ubx_version_t *v = gps_link_version(&s_link);
        if (st == GPS_LINK_RUN && v->gen != UBX_GEN_UNKNOWN) {
            ESP_LOGI(TAG, "%s %s (PROTVER %s, %s), %" PRIu32 " bauds", ubx_generation_name(v->gen), v->module,
                     v->protver, v->sw, gps_link_baud(&s_link));
        } else if (st == GPS_LINK_RUN) {
            ESP_LOGW(TAG, "récepteur sans réponse UBX : NMEA tel quel à %" PRIu32 " bauds", gps_link_baud(&s_link));
        } else {
            ESP_LOGI(TAG, "%s (%" PRIu32 " bauds)", gps_link_state_name(st), gps_link_baud(&s_link));
        }
    }
    const bool present = gps_link_present(&s_link, now);
    if (present != s_present) {
        s_present = present;
        if (!present) {
            const gps_link_stats_t *s = gps_link_stats(&s_link);
            ESP_LOGW(TAG, "module GPS absent (pertes %" PRIu32 ", NAK %" PRIu32 ")", s->losses, s->naks);
        }
    }
}

bool hal_gps_present(void)
{
    return gps_link_present(&s_link, hal_time_ms());
}

void hal_gps_poll(void)
{
    uint8_t buf[256];
    size_t avail = 0;
    uart_get_buffered_data_len(GPS_UART, &avail);
    while (avail > 0) {
        const int n = uart_read_bytes(GPS_UART, buf, avail < sizeof(buf) ? avail : sizeof(buf), 0);
        if (n <= 0) break;
        gps_link_feed(&s_link, buf, (size_t)n, hal_time_ms());
        avail = avail > (size_t)n ? avail - (size_t)n : 0;
    }
    const uint64_t now = hal_time_ms();
    gps_link_tick(&s_link, now);
    log_changes(now);
}

#endif /* CONFIG_RAIDMASTER_GPS_UART */
