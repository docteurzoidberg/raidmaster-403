/*
 * Raidmaster 403 — hal_asset sur PC : <RAIDMASTER_ASSET_DIR>/<nom>.bin lu en entier en mémoire
 * (fichiers générés au build, ex. boot_anim.bin).
 *
 * Licence : GPL-3.0.
 */

#include <stdio.h>
#include <stdlib.h>

#include "hal/hal_asset.h"

#ifndef RAIDMASTER_ASSET_DIR
#define RAIDMASTER_ASSET_DIR "."
#endif

const void *hal_asset_map(const char *name, size_t *size)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.bin", RAIDMASTER_ASSET_DIR, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "hal_asset : %s introuvable\n", path);
        return NULL;
    }
    void *data = NULL;
    long n = -1;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) {
        data = malloc((size_t)n);
        if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) {
            free(data);
            data = NULL;
        }
    }
    fclose(f);
    if (data != NULL) *size = (size_t)n;
    return data;
}

void hal_asset_unmap(const void *data)
{
    free((void *)data);
}
