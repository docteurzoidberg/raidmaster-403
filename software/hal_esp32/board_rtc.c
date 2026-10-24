/*
 * Raidmaster 403 — horloge de la carte : RTC PCF85063 (I²C 0x51, en UTC), implémente l'horloge
 * de hal/hal_time.h (hal_time_utc_ms, hal_time_set_utc_ms).
 *
 * La RTC est alimentée par la sortie RTCLDO de l'AXP2101 : elle garde l'heure tant qu'il lui
 * reste une source (USB, batterie, ou pile de sauvegarde sur le connecteur SH1.0 VBACKUP). Sinon
 * son drapeau OS (oscillateur arrêté) est levé au retour du courant : l'heure est inconnue
 * jusqu'au premier fix GPS, qui la règle (core/app.c). Une heure antérieure à la compilation du
 * firmware est rejetée de même (le firmware d'usine Waveshare met la RTC au 2025-01-01 12:00).
 *
 * Au démarrage, une RTC valide règle l'heure système (time, gettimeofday) ; ensuite l'heure vient
 * de l'heure système, la RTC n'est relue qu'au démarrage suivant.
 *
 * Licence : GPL-3.0.
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"

#include "board.h"
#include "gps_parser.h"
#include "hal/hal_time.h"

static const char *TAG = "rtc";

#define PCF85063_ADDR  0x51
#define REG_SECONDS    0x04     /* puis minutes, heures, jour, jour de la semaine, mois, année (BCD) */
#define SECONDS_OS     0x80     /* oscillateur arrêté depuis le dernier réglage : heure perdue */
#define DAY_MS         86400000LL

static i2c_master_dev_handle_t s_dev;
static bool s_valid;            /* heure système réglée (par la RTC ou le GPS) */
static bool s_sync = true;      /* false : réglages ignorés (GPS de démonstration) */

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static uint8_t bin2bcd(int v)
{
    return (uint8_t)((v / 10) << 4 | v % 10);
}

static const char *fmt_utc(int64_t utc_s, char *buf, size_t len)
{
    const time_t t = (time_t)utc_s;
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S UTC", &tm);
    return buf;
}

/* « Oct 24 2026 » + « 14:03:27 » (esp_app_desc) → ms UTC. */
int64_t board_build_time_utc_ms(void)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const esp_app_desc_t *app = esp_app_get_description();
    char mon[4] = "";
    int d, y, hh, mm, ss;
    if (sscanf(app->date, "%3s %d %d", mon, &d, &y) != 3 || sscanf(app->time, "%d:%d:%d", &hh, &mm, &ss) != 3)
        return -1;
    const char *p = strstr(months, mon);
    if (p == NULL || (p - months) % 3 != 0) return -1;
    const int m = (int)(p - months) / 3 + 1;
    const int64_t s = (int64_t)gps_days_from_civil(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss - 3600;
    return s * 1000;
}

static void set_system_ms(int64_t utc_ms)
{
    const struct timeval tv = {.tv_sec = (time_t)(utc_ms / 1000), .tv_usec = (suseconds_t)(utc_ms % 1000 * 1000)};
    settimeofday(&tv, NULL);
    s_valid = true;
}

/* Heure de la RTC en s UTC ; -1 si elle est perdue ou incohérente, -2 si la RTC ne répond pas,
 * -3 si elle est antérieure à la compilation du firmware (à un jour près : fuseau du build). */
static int64_t rtc_read(void)
{
    const uint8_t reg = REG_SECONDS;
    uint8_t d[7];
    if (i2c_master_transmit_receive(s_dev, &reg, 1, d, sizeof(d), 50) != ESP_OK) return -2;
    if (d[0] & SECONDS_OS) return -1;
    const int year = 2000 + bcd2bin(d[6]);
    const int mon = bcd2bin(d[5] & 0x1F), day = bcd2bin(d[3] & 0x3F);
    if (year < 2024 || mon < 1 || mon > 12 || day < 1 || day > 31) return -1;
    const int64_t s = (int64_t)gps_days_from_civil(year, mon, day) * 86400 + bcd2bin(d[2] & 0x3F) * 3600 +
                      bcd2bin(d[1] & 0x7F) * 60 + bcd2bin(d[0] & 0x7F);
    if (s * 1000 < board_build_time_utc_ms() - DAY_MS) return -3;
    return s;
}

/* Écrire les secondes avec le bit OS à 0 efface le drapeau « heure perdue ». */
static bool rtc_write(int64_t utc_s)
{
    const time_t t = (time_t)utc_s;
    struct tm tm;
    gmtime_r(&t, &tm);
    const uint8_t w[8] = {REG_SECONDS, bin2bcd(tm.tm_sec), bin2bcd(tm.tm_min), bin2bcd(tm.tm_hour),
                          bin2bcd(tm.tm_mday), (uint8_t)tm.tm_wday, bin2bcd(tm.tm_mon + 1),
                          bin2bcd(tm.tm_year % 100)};
    return s_dev != NULL && i2c_master_transmit(s_dev, w, sizeof(w), 50) == ESP_OK;
}

esp_err_t board_rtc_init(void)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_i2c_bus(), &cfg, &s_dev), TAG, "PCF85063");
    const int64_t s = rtc_read();
    if (s == -3) {
        ESP_LOGI(TAG, "heure de la RTC antérieure au firmware (jamais réglée) : inconnue jusqu'au premier fix GPS");
        return ESP_OK;
    }
    if (s == -2) {
        ESP_LOGW(TAG, "RTC muette : heure inconnue jusqu'au premier fix GPS");
        return ESP_ERR_NOT_FOUND;
    }
    if (s < 0) {
        ESP_LOGI(TAG, "heure perdue (RTC restée sans alimentation) : inconnue jusqu'au premier fix GPS");
        return ESP_OK;
    }
    set_system_ms(s * 1000);
    char buf[32];
    ESP_LOGI(TAG, "heure de la RTC : %s", fmt_utc(s, buf, sizeof(buf)));
    return ESP_OK;
}

void board_clock_sync_enable(bool on)
{
    s_sync = on;
}

int64_t hal_time_utc_ms(void)
{
    if (!s_valid) return -1;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void hal_time_set_utc_ms(int64_t utc_ms)
{
    char buf[32];
    if (!s_sync) {
        ESP_LOGI(TAG, "GPS de démonstration : heure %s non reportée sur l'horloge",
                 fmt_utc(utc_ms / 1000, buf, sizeof(buf)));
        return;
    }
    set_system_ms(utc_ms);
    const bool ok = rtc_write(utc_ms / 1000);
    ESP_LOGI(TAG, "horloge réglée sur le GPS : %s%s", fmt_utc(utc_ms / 1000, buf, sizeof(buf)),
             ok ? "" : " (RTC non écrite)");
}
