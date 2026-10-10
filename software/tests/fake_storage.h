/* Raidmaster 403 — hal_storage en mémoire pour les tests. Licence : GPL-3.0. */
#pragma once

extern unsigned fake_storage_writes;    /* nombre d'écritures depuis le dernier clear */
void fake_storage_clear(void);
