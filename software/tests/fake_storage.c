/*
 * Raidmaster 403 — hal_storage en mémoire pour les tests du core.
 *
 * Licence : GPL-3.0.
 */

#include "fake_storage.h"

#include <string.h>

#include "hal/hal_storage.h"

#define SLOTS 8
#define VMAX  512

static struct {
    char   key[16];
    size_t len;
    unsigned char val[VMAX];
} s_slots[SLOTS];
static int s_used;
unsigned fake_storage_writes;

void fake_storage_clear(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_used = 0;
    fake_storage_writes = 0;
}

int hal_storage_get(const char *key, void *buf, size_t *len)
{
    for (int i = 0; i < s_used; i++) {
        if (strcmp(s_slots[i].key, key) == 0) {
            if (s_slots[i].len > *len)
                return -1;
            memcpy(buf, s_slots[i].val, s_slots[i].len);
            *len = s_slots[i].len;
            return 0;
        }
    }
    return -1;
}

int hal_storage_set(const char *key, const void *buf, size_t len)
{
    if (strlen(key) >= sizeof(s_slots[0].key) || len > VMAX)
        return -1;
    int i = 0;
    while (i < s_used && strcmp(s_slots[i].key, key) != 0)
        i++;
    if (i == s_used) {
        if (s_used == SLOTS)
            return -1;
        s_used++;
        strcpy(s_slots[i].key, key);
    }
    memcpy(s_slots[i].val, buf, len);
    s_slots[i].len = len;
    fake_storage_writes++;
    return 0;
}
