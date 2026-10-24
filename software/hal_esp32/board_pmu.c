/*
 * Raidmaster 403 — alimentation : état du PMIC AXP2101 (I²C 0x34), implémente hal/hal_power.h ;
 * bouton PWR et extinction (board.h).
 *
 * Lecture : USB présent (REG00 bit 5, VBUS good), batterie branchée (REG00 bit 3), sens du
 * courant batterie (REG01 bits 6:5, 01 = charge), jauge en % (REGA4), tensions VBAT et VBUS
 * (REG34-35, REG38-39). Seule écriture : la mesure de VBUS (REG30 bit 2) ; détection de la
 * batterie, mesure de sa tension et jauge sont actives au démarrage du PMIC (REG68, REG30,
 * REG18). Les réglages de charge (courant, tension, NTC, DCDC1 en PWM…) viendront avec
 * l'intégration complète du PMIC (prompts/firmware.md § 6 quater).
 *
 * Sans batterie (relevé sur la carte, 2026-10-24), l'AXP2101 se trompe : la broche BAT remonte à
 * ~4,8 V par le chemin d'alimentation, il signale une batterie pleine qui se décharge et laisse
 * VBUS good à 0 alors que VBUS est à 4,9 V. D'où : USB = VBUS good ou VBUS mesuré ≥ 4 V ;
 * batterie = signalée et VBAT ≤ 4,4 V (une Li-ion ne dépasse pas 4,2 V).
 *
 * Bouton PWR (K3 sur la broche PWRON, recopié aussi sur EXIO6) : l'AXP2101 rallume seul la carte
 * éteinte et lève ses drapeaux d'appui court / long (REG49, appui long au bout de 1,5 s : REG27) ;
 * on ne garde que l'appui long. Extinction : bit « soft PWROFF » de REG10. Ces drapeaux et
 * l'effacement (écrire 1) sont les seules autres écritures, avec la coupure de la charge de la pile
 * de sauvegarde RTC (REG18 bit 2, laissée active à 3,3 V par le firmware d'usine) : une pile bouton
 * CR branchée sur VBACKUP n'est pas rechargeable (choix du propriétaire, 2026-10-24).
 *
 * Licence : GPL-3.0.
 */

#include <stdio.h>

#include "esp_check.h"
#include "esp_log.h"

#include "board.h"
#include "hal/hal_power.h"
#include "hal/hal_time.h"

static const char *TAG = "pmu";

#define AXP2101_ADDR     0x34
#define REG_STATUS1      0x00
#define REG_STATUS2      0x01
#define REG_COMMON_CFG   0x10     /* bit 0 : extinction logicielle */
#define REG_CHG_GAUGE    0x18     /* bit 2 : charge de la pile de sauvegarde RTC (VBACKUP) */
#define REG_PWROFF_EN    0x22     /* bit 1 : extinction forcée par un appui très long */
#define REG_PWRON_LEVEL  0x27     /* durées de l'appui long, de l'extinction forcée, de l'allumage */
#define REG_IRQ_STATUS1  0x49     /* bits 3 appui court, 2 appui long, 1-0 fronts ; 1 écrit = effacé */
#define REG_ADC_CTRL     0x30
#define REG_VBAT_H       0x34     /* 14 bits en mV, H puis L */
#define REG_VBUS_H       0x38
#define REG_BAT_PERCENT  0xA4
#define ADC_VBUS_EN      0x04
#define BACKUP_CHG_EN    0x04
#define VBUS_MIN_MV      4000U
#define VBAT_MAX_MV      4400U
#define READ_PERIOD_MS   1000U
#define PKEY_IRQ_MASK    0x0F
#define PKEY_LONG        0x04
#define PKEY_POLL_MS     50U

static i2c_master_dev_handle_t s_dev;
static hal_power_t s_last;
static bool        s_ok;
static uint64_t    s_next_ms;
static uint64_t    s_next_key_ms;

static bool rd(uint8_t reg, uint8_t *v)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, v, 1, 50) == ESP_OK;
}

static bool wr(uint8_t reg, uint8_t v)
{
    const uint8_t w[2] = {reg, v};
    return i2c_master_transmit(s_dev, w, sizeof(w), 50) == ESP_OK;
}

static bool rd_mv(uint8_t reg_h, unsigned *mv)
{
    uint8_t h, l;
    if (!rd(reg_h, &h) || !rd((uint8_t)(reg_h + 1), &l)) return false;
    *mv = (unsigned)(h & 0x3F) << 8 | l;
    return true;
}

static void log_state(const hal_power_t *p)
{
    char batt[40] = "batterie absente";
    if (p->battery && p->pct >= 0) snprintf(batt, sizeof(batt), "batterie %d %%%s", p->pct, p->charging ? " en charge" : "");
    else if (p->battery)           snprintf(batt, sizeof(batt), "batterie%s", p->charging ? " en charge" : "");
    ESP_LOGI(TAG, "alimentation : %s, %s", p->external ? "USB" : "sans USB", batt);
}

/* Relit le PMIC ; journal à chaque changement de source ou de charge (pas du pourcentage). */
static void poll(void)
{
    uint8_t st1, st2, pct;
    unsigned vbat_mv, vbus_mv;
    if (s_dev == NULL || !rd(REG_STATUS1, &st1) || !rd(REG_STATUS2, &st2) || !rd_mv(REG_VBAT_H, &vbat_mv) ||
        !rd_mv(REG_VBUS_H, &vbus_mv)) {
        if (s_ok) ESP_LOGW(TAG, "AXP2101 muet");
        s_ok = false;
        return;
    }
    hal_power_t p = {
        .external = (st1 & 0x20) != 0 || vbus_mv >= VBUS_MIN_MV,
        .battery = (st1 & 0x08) != 0 && vbat_mv <= VBAT_MAX_MV,
        .pct = -1,
    };
    p.charging = p.battery && ((st2 >> 5) & 0x03) == 0x01;
    if (p.battery && rd(REG_BAT_PERCENT, &pct)) p.pct = (int8_t)(pct > 100 ? 100 : pct);
    if (!s_ok || p.external != s_last.external || p.battery != s_last.battery || p.charging != s_last.charging)
        log_state(&p);
    s_last = p;
    s_ok = true;
}

esp_err_t board_pmu_init(void)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_i2c_bus(), &cfg, &s_dev), TAG, "AXP2101");
    uint8_t adc;
    if (rd(REG_ADC_CTRL, &adc) && !(adc & ADC_VBUS_EN)) wr(REG_ADC_CTRL, (uint8_t)(adc | ADC_VBUS_EN));
    wr(REG_IRQ_STATUS1, PKEY_IRQ_MASK);     /* appuis d'avant le démarrage oubliés */
    uint8_t chg = 0;
    if (rd(REG_CHG_GAUGE, &chg) && (chg & BACKUP_CHG_EN)) {
        wr(REG_CHG_GAUGE, (uint8_t)(chg & ~BACKUP_CHG_EN));
        ESP_LOGI(TAG, "charge de la pile de sauvegarde RTC coupée (REG18 %02x → %02x)", chg, chg & ~BACKUP_CHG_EN);
        chg &= (uint8_t)~BACKUP_CHG_EN;
    }
    /* Relevé brut pour la mise au point (avec et sans batterie). */
    uint8_t st1 = 0, st2 = 0, off_en = 0, levels = 0;
    unsigned vbat_mv = 0, vbus_mv = 0;
    rd(REG_STATUS1, &st1);
    rd(REG_STATUS2, &st2);
    rd(REG_PWROFF_EN, &off_en);
    rd(REG_PWRON_LEVEL, &levels);
    rd_mv(REG_VBAT_H, &vbat_mv);
    rd_mv(REG_VBUS_H, &vbus_mv);
    ESP_LOGI(TAG, "AXP2101 : REG00 %02x, REG01 %02x, REG18 %02x, REG22 %02x, REG27 %02x, VBAT %u mV, VBUS %u mV",
             st1, st2, chg, off_en, levels, vbat_mv, vbus_mv);
    poll();
    s_next_ms = hal_time_ms() + READ_PERIOD_MS;
    return s_ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool hal_power_read(hal_power_t *out)
{
    const uint64_t now = hal_time_ms();
    if (now >= s_next_ms) {
        s_next_ms = now + READ_PERIOD_MS;
        poll();
    }
    *out = s_last;
    return s_ok;
}

bool board_pwr_key_long_press(void)
{
    const uint64_t now = hal_time_ms();
    if (s_dev == NULL || now < s_next_key_ms) return false;
    s_next_key_ms = now + PKEY_POLL_MS;
    uint8_t irq;
    if (!rd(REG_IRQ_STATUS1, &irq) || !(irq & PKEY_IRQ_MASK)) return false;
    wr(REG_IRQ_STATUS1, irq & PKEY_IRQ_MASK);      /* effacés : appui court et fronts ignorés */
    return (irq & PKEY_LONG) != 0;
}

void board_power_off(void)
{
    uint8_t v;
    if (s_dev != NULL && rd(REG_COMMON_CFG, &v)) wr(REG_COMMON_CFG, (uint8_t)(v | 0x01));
}
