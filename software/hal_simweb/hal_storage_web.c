/*
 * Raidmaster 403 — hal_storage du sim web : localStorage du navigateur (voir hal_simweb.h).
 *
 * Même modèle que hal_sim/hal_storage_json.c : les valeurs vivent en mémoire, chaque écriture est
 * recopiée (hexadécimal) dans le localStorage quand le stockage est persistant ; une clé absente
 * de la mémoire est cherchée dans le localStorage à sa première lecture. Navigateur sans stockage
 * (navigation privée stricte, Node) : en mémoire seulement, sans erreur.
 *
 * Licence : GPL-3.0.
 */

#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_storage.h"
#include "hal_simweb.h"

#define SLOTS   16
#define KEY_MAX 16
#define VAL_MAX 1024

static struct {
    char          key[KEY_MAX];
    size_t        len;
    unsigned char val[VAL_MAX];
} s_slots[SLOTS];
static int  s_used;
static bool s_persistent;

/* clang-format off */
EM_JS(int, js_storage_get, (const char *key, char *out, int cap), {
    try {
        const v = globalThis.localStorage?.getItem("raidmaster-403.nvs." + UTF8ToString(key));
        if (v === null || v === undefined || lengthBytesUTF8(v) >= cap) return -1;
        stringToUTF8(v, out, cap);
        return lengthBytesUTF8(v);
    } catch (e) {
        return -1;
    }
})

EM_JS(void, js_storage_set, (const char *key, const char *hex), {
    try {
        globalThis.localStorage?.setItem("raidmaster-403.nvs." + UTF8ToString(key), UTF8ToString(hex));
    } catch (e) {
        /* stockage plein ou refusé : la valeur reste en mémoire */
    }
})
/* clang-format on */

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void hal_storage_web_open(bool persistent)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_used = 0;
    s_persistent = persistent;
}

static int find(const char *key)
{
    for (int i = 0; i < s_used; i++)
        if (strcmp(s_slots[i].key, key) == 0) return i;
    return -1;
}

/* Recopie la valeur de `key` du localStorage dans un emplacement ; renvoie son indice ou -1. */
static int load(const char *key)
{
    static char hex[VAL_MAX * 2 + 1];
    if (!s_persistent || s_used == SLOTS || strlen(key) >= KEY_MAX) return -1;
    const int len = js_storage_get(key, hex, (int)sizeof(hex));
    if (len < 0 || len % 2 != 0) return -1;
    const int i = s_used;
    for (int k = 0; k < len / 2; k++) {
        const int hi = hexval(hex[2 * k]), lo = hexval(hex[2 * k + 1]);
        if (hi < 0 || lo < 0) return -1;
        s_slots[i].val[k] = (unsigned char)(hi << 4 | lo);
    }
    snprintf(s_slots[i].key, KEY_MAX, "%s", key);
    s_slots[i].len = (size_t)len / 2;
    s_used++;
    return i;
}

int hal_storage_get(const char *key, void *buf, size_t *len)
{
    int i = find(key);
    if (i < 0) i = load(key);
    if (i < 0 || s_slots[i].len > *len) return -1;
    memcpy(buf, s_slots[i].val, s_slots[i].len);
    *len = s_slots[i].len;
    return 0;
}

int hal_storage_set(const char *key, const void *buf, size_t len)
{
    if (strlen(key) >= KEY_MAX || len > VAL_MAX) return -1;
    int i = find(key);
    if (i < 0) {
        if (s_used == SLOTS) return -1;
        i = s_used++;
        snprintf(s_slots[i].key, KEY_MAX, "%s", key);
    }
    memcpy(s_slots[i].val, buf, len);
    s_slots[i].len = len;
    if (s_persistent) {
        static char hex[VAL_MAX * 2 + 1];
        for (size_t k = 0; k < len; k++) snprintf(hex + 2 * k, 3, "%02x", s_slots[i].val[k]);
        hex[2 * len] = '\0';
        js_storage_set(key, hex);
    }
    return 0;
}
