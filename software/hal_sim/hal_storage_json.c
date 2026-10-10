/*
 * Raidmaster 403 — hal_storage sur PC : fichier JSON plat (sim-storage.json par défaut).
 *
 * Une entrée par clé, valeur binaire en hexadécimal :
 *   {
 *     "state": "01004d52..."
 *   }
 * Le fichier est relu au démarrage et réécrit entièrement (fichier temporaire puis rename)
 * à chaque écriture. Remplace la NVS de l'ESP32.
 *
 * Licence : GPL-3.0.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/hal_storage.h"
#include "hal_sim.h"

#define SLOTS   16
#define KEY_MAX 16
#define VAL_MAX 1024

static struct {
    char          key[KEY_MAX];
    size_t        len;
    unsigned char val[VAL_MAX];
} s_slots[SLOTS];
static int  s_used;
static char s_path[512];

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Lit une chaîne JSON simple (sans échappements) ; renvoie le pointeur après le guillemet fermant. */
static const char *read_string(const char *p, char *out, size_t cap)
{
    if (*p != '"') return NULL;
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        if (n + 1 < cap) out[n++] = *p;
        p++;
    }
    if (*p != '"') return NULL;
    out[n] = '\0';
    return p + 1;
}

static void load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return;
    static char text[SLOTS * (VAL_MAX * 2 + KEY_MAX + 16) + 64];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = '\0';

    const char *p = strchr(text, '{');
    static char hex[VAL_MAX * 2 + 1];
    while (p != NULL && s_used < SLOTS) {
        p = strchr(p, '"');
        if (p == NULL) break;
        char key[KEY_MAX];
        p = read_string(p, key, sizeof(key));
        if (p == NULL) break;
        while (isspace((unsigned char)*p) || *p == ':') p++;
        p = read_string(p, hex, sizeof(hex));
        if (p == NULL) break;
        size_t len = strlen(hex);
        if (len % 2 != 0) continue;
        bool ok = true;
        for (size_t i = 0; i < len / 2; i++) {
            int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
            if (hi < 0 || lo < 0) { ok = false; break; }
            s_slots[s_used].val[i] = (unsigned char)(hi << 4 | lo);
        }
        if (!ok) continue;
        snprintf(s_slots[s_used].key, KEY_MAX, "%s", key);
        s_slots[s_used].len = len / 2;
        s_used++;
    }
}

static int save(void)
{
    if (s_path[0] == '\0') return 0;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", s_path);
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return -1;
    fputs("{\n", f);
    for (int i = 0; i < s_used; i++) {
        fprintf(f, "  \"%s\": \"", s_slots[i].key);
        for (size_t k = 0; k < s_slots[i].len; k++) fprintf(f, "%02x", s_slots[i].val[k]);
        fprintf(f, "\"%s\n", i + 1 < s_used ? "," : "");
    }
    fputs("}\n", f);
    if (fclose(f) != 0) return -1;
    return rename(tmp, s_path) == 0 ? 0 : -1;
}

void hal_storage_sim_open(const char *path)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_used = 0;
    s_path[0] = '\0';
    if (path == NULL) return;
    snprintf(s_path, sizeof(s_path), "%s", path);
    load(path);
}

int hal_storage_get(const char *key, void *buf, size_t *len)
{
    for (int i = 0; i < s_used; i++) {
        if (strcmp(s_slots[i].key, key) == 0) {
            if (s_slots[i].len > *len) return -1;
            memcpy(buf, s_slots[i].val, s_slots[i].len);
            *len = s_slots[i].len;
            return 0;
        }
    }
    return -1;
}

int hal_storage_set(const char *key, const void *buf, size_t len)
{
    if (strlen(key) >= KEY_MAX || len > VAL_MAX) return -1;
    int i = 0;
    while (i < s_used && strcmp(s_slots[i].key, key) != 0) i++;
    if (i == s_used) {
        if (s_used == SLOTS) return -1;
        snprintf(s_slots[i].key, KEY_MAX, "%s", key);
        s_used++;
    }
    memcpy(s_slots[i].val, buf, len);
    s_slots[i].len = len;
    return save();
}
