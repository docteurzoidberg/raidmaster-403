# Tripmaster GPS 403

Tripmaster rallye-raid custom pour Peugeot 403 Familiale (raid Crazy Dust 2027). Boîtier
autonome aimanté sur le tableau de bord : écran 3,5" IPS 480×320 (Waveshare ESP32-S3-Touch-LCD-3.5),
GPS u-blox NEO-M9N externe, magnéto QMC5883L, batterie Li-ion rechargée en USB-C. Stack ESP-IDF 5.x
+ C11 + LVGL v9, approche **sim first** : logique et UI validées sur simulateur SDL PC avant tout
flash firmware.

## État : étape 1 (squelette sim)

Fenêtre blanche 480×320 landscape, aucune logique métier. Les étapes suivantes (core, UI LVGL,
HAL, firmware ESP32) sont décrites dans [`PROMPT.md`](PROMPT.md) § 6.

## Build — cible `sim` (PC Linux)

Dépendance système : SDL2 (`sudo apt install libsdl2-dev` sur Debian/Ubuntu/WSL).

```bash
cmake --preset sim
cmake --build build-sim
./build-sim/tripmaster-sim
```

Fermer avec `Échap` ou la croix de la fenêtre.

## Build — cible `firmware` (ESP32-S3)

À finaliser en **étape 5** du plan (voir `PROMPT.md` § 6). Le preset `firmware` est un stub.

## Documentation

- [`CLAUDE.md`](CLAUDE.md) — règles de session (multi-agents).
- [`PROMPT.md`](PROMPT.md) — contexte global, étapes, journal des décisions.
- [`AGENTS.md`](AGENTS.md) — matrice de responsabilité qui-touche-quoi.
- [`prompts/`](prompts/) — briefs par rôle (features, hardware, boîtier, firmware, UI, simulation).

## Licence

[GPL-3.0](LICENSE) — compatible Open Rally Computer et composants LVGL / ESP-IDF.
