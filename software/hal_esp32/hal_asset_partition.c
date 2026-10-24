/*
 * Raidmaster 403 — hal_asset sur l'ESP32 : partition de données de même nom (partitions.csv),
 * mappée en mémoire en entier (lecture par le cache de la flash, aucune copie en RAM).
 *
 * Licence : GPL-3.0.
 */

#include "esp_log.h"
#include "esp_partition.h"

#include "hal/hal_asset.h"

static const char *TAG = "hal_asset";

#define MAX_MAPS 4

static struct {
    const void                 *data;
    esp_partition_mmap_handle_t handle;
} s_maps[MAX_MAPS];

const void *hal_asset_map(const char *name, size_t *size)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, name);
    if (p == NULL) {
        ESP_LOGW(TAG, "partition « %s » absente", name);
        return NULL;
    }
    for (int i = 0; i < MAX_MAPS; i++) {
        if (s_maps[i].data != NULL) continue;
        const esp_err_t err = esp_partition_mmap(p, 0, p->size, ESP_PARTITION_MMAP_DATA, &s_maps[i].data,
                                                 &s_maps[i].handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "mappage de « %s » : %s", name, esp_err_to_name(err));
            s_maps[i].data = NULL;
            return NULL;
        }
        *size = p->size;
        return s_maps[i].data;
    }
    ESP_LOGE(TAG, "trop d'assets mappés");
    return NULL;
}

void hal_asset_unmap(const void *data)
{
    for (int i = 0; i < MAX_MAPS; i++) {
        if (data != NULL && s_maps[i].data == data) {
            esp_partition_munmap(s_maps[i].handle);
            s_maps[i].data = NULL;
        }
    }
}
