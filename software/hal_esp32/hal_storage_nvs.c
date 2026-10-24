/*
 * Raidmaster 403 — hal_storage sur l'ESP32 : blobs NVS (espace « raidmaster »), ouvert au
 * premier accès. Partition pleine ou d'une autre version de NVS : effacée (on repart des
 * réglages par défaut, comme un bloc illisible côté core).
 *
 * Licence : GPL-3.0.
 */

#include <stdbool.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "hal/hal_storage.h"

static const char *TAG = "hal_storage";

static nvs_handle_t s_nvs;
static bool         s_open;

static bool storage_open(void)
{
    if (s_open) return true;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "partition NVS effacée (%s)", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err == ESP_OK) err = nvs_open("raidmaster", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS indisponible : %s", esp_err_to_name(err));
        return false;
    }
    s_open = true;
    return true;
}

int hal_storage_get(const char *key, void *buf, size_t *len)
{
    if (!storage_open()) return -1;
    size_t n = *len;
    if (nvs_get_blob(s_nvs, key, buf, &n) != ESP_OK) return -1;
    *len = n;
    return 0;
}

int hal_storage_set(const char *key, const void *buf, size_t len)
{
    if (!storage_open()) return -1;
    esp_err_t err = nvs_set_blob(s_nvs, key, buf, len);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "écriture de « %s » : %s", key, esp_err_to_name(err));
        return -1;
    }
    return 0;
}
