/*
 * Raidmaster 403 — hal_mag sur l'ESP32 : boussole du puck sur l'I2C1 (IO39 SDA, IO40 SCL,
 * 100 kHz), puce reconnue et lue par core/mag, journal du cap brut. La calibration (tour complet
 * depuis le menu), le lissage et le choix de la source du cap sont dans core/app.
 *
 * Le bus I2C1 est aussi celui prévu pour la carte boutons (PCF8574 0x20 / 0x21) : pas de conflit
 * d'adresse avec les boussoles (0x0C à 0x0F, 0x1C, 0x1E).
 *
 * Boussole coupée dans menuconfig : jamais de mesure, « boussole absente ».
 *
 * Licence : GPL-3.0.
 */

#include "sdkconfig.h"

#include "hal/hal_mag.h"

#if CONFIG_RAIDMASTER_COMPASS

#include <math.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "hal/hal_time.h"

static const char *TAG = "boussole";

#define I2C1_HZ       100000
#define XFER_MS       20
#define MAX_DEVS      4
#if CONFIG_RAIDMASTER_COMPASS_FLIPPED
#define COMPASS_FLIPPED true
#else
#define COMPASS_FLIPPED false
#endif

typedef struct {
    uint8_t                 addr;
    i2c_master_dev_handle_t dev;
    bool                    ok;   /* dernière transaction réussie (sinon : sonder avant d'écrire) */
} dev_slot_t;

static i2c_master_bus_handle_t s_bus;
static dev_slot_t             s_devs[MAX_DEVS];
static mag_t                  s_mag;
static bool                   s_present;
static uint64_t               s_next_log_ms;

/* Périphérique de l'adresse, créé au premier accusé de réception. Une adresse muette est sondée
 * (i2c_master_probe, silencieux) au lieu d'une transaction qui remplirait le journal de NACK. */
static i2c_master_dev_handle_t dev_for(uint8_t addr)
{
    dev_slot_t *slot = NULL;
    for (int i = 0; i < MAX_DEVS; i++) {
        if (s_devs[i].dev != NULL && s_devs[i].addr == addr) {
            slot = &s_devs[i];
            break;
        }
    }
    if (slot != NULL && slot->ok) return slot->dev;
    if (i2c_master_probe(s_bus, addr, XFER_MS) != ESP_OK) return NULL;
    if (slot == NULL) {
        for (int i = 0; i < MAX_DEVS && slot == NULL; i++)
            if (s_devs[i].dev == NULL) slot = &s_devs[i];
        if (slot == NULL) return NULL;
        const i2c_device_config_t dc = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = I2C1_HZ,
        };
        if (i2c_master_bus_add_device(s_bus, &dc, &slot->dev) != ESP_OK) return NULL;
        slot->addr = addr;
    }
    slot->ok = true;
    return slot->dev;
}

static void mark(uint8_t addr, bool ok)
{
    for (int i = 0; i < MAX_DEVS; i++)
        if (s_devs[i].dev != NULL && s_devs[i].addr == addr) s_devs[i].ok = ok;
}

static bool bus_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n, void *user)
{
    (void)user;
    i2c_master_dev_handle_t d = dev_for(addr);
    if (d == NULL) return false;
    const bool ok = i2c_master_transmit_receive(d, &reg, 1, buf, n, XFER_MS) == ESP_OK;
    mark(addr, ok);
    return ok;
}

static bool bus_write(uint8_t addr, uint8_t reg, uint8_t val, void *user)
{
    (void)user;
    i2c_master_dev_handle_t d = dev_for(addr);
    if (d == NULL) return false;
    const uint8_t b[2] = {reg, val};
    const bool ok = i2c_master_transmit(d, b, 2, XFER_MS) == ESP_OK;
    mark(addr, ok);
    return ok;
}

void hal_mag_init(void)
{
    const i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = CONFIG_RAIDMASTER_COMPASS_SDA_GPIO,
        .scl_io_num = CONFIG_RAIDMASTER_COMPASS_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,      /* le puck a les siens ; ceux-ci servent à vide */
    };
    if (i2c_new_master_bus(&bc, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "bus I2C1 indisponible");
        return;
    }
    const uint64_t now = hal_time_ms();
    const mag_bus_t bus = {.read = bus_read, .write = bus_write};
    mag_init(&s_mag, &bus, now);
    mag_set_orient(&s_mag, (mag_orient_t){.yaw_quarter = CONFIG_RAIDMASTER_COMPASS_YAW / 90,
                                          .flipped = COMPASS_FLIPPED});
    s_next_log_ms = now;
    ESP_LOGI(TAG, "I2C1 (SDA %d, SCL %d, 100 kHz), rotation %d°%s", CONFIG_RAIDMASTER_COMPASS_SDA_GPIO,
             CONFIG_RAIDMASTER_COMPASS_SCL_GPIO, CONFIG_RAIDMASTER_COMPASS_YAW,
             COMPASS_FLIPPED ? ", à l'envers" : "");
}

bool hal_mag_poll(mag_vec_t *raw)
{
    if (s_bus == NULL) return false;
    const uint64_t now = hal_time_ms();
    const bool fresh = mag_tick(&s_mag, now);
    const bool present = mag_present(&s_mag, now);
    if (present != s_present) {
        s_present = present;
        if (present) ESP_LOGI(TAG, "%s à 0x%02X", mag_chip_name(mag_chip(&s_mag)), mag_addr(&s_mag));
        else         ESP_LOGW(TAG, "boussole absente");
    }
#if CONFIG_RAIDMASTER_COMPASS_LOG_S > 0
    if (present && now >= s_next_log_ms) {
        s_next_log_ms = now + CONFIG_RAIDMASTER_COMPASS_LOG_S * 1000ULL;
        const mag_vec_t r = mag_raw(&s_mag);
        ESP_LOGI(TAG, "(%.1f, %.1f, %.1f) µT, |B| %.0f µT, cap brut %.0f° (non calibré)", r.x, r.y, r.z,
                 sqrtf(r.x * r.x + r.y * r.y + r.z * r.z), mag_heading_deg(r));
    }
#endif
    if (fresh) *raw = mag_raw(&s_mag);
    return fresh;
}

bool hal_mag_present(void)
{
    return s_bus != NULL && mag_present(&s_mag, hal_time_ms());
}

#else /* boussole coupée dans menuconfig */

void hal_mag_init(void) {}

bool hal_mag_poll(mag_vec_t *raw)
{
    (void)raw;
    return false;
}

bool hal_mag_present(void)
{
    return false;
}

#endif /* CONFIG_RAIDMASTER_COMPASS */
