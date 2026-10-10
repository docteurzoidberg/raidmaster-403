# Raidmaster 403

Tripmaster GPS de rallye-raid fait maison pour une **Peugeot 403 Familiale de 1956**, pensé pour le raid
**Crazy Dust 2027** et utilisable au quotidien en mode balade. Boîtier autonome aimanté sur la tôle du
tableau de bord : distances partielles A et B, odomètre, vitesse, cap, moyenne, chrono.

<p align="center">
  <img src="docs/demo/raidmaster-demo.gif" alt="Démo de l'interface dans le simulateur : démarrage animé, écran raid, balade, menu, calibration, mode nuit" width="480">
</p>

<p align="center"><sub>Simulateur PC, échelle 1:1 de l'écran 480 × 320. GIF régénéré automatiquement à chaque build qui change l'interface.</sub></p>

## Matériel

| Élément | Choix |
|---|---|
| Carte | [Waveshare ESP32-S3-Touch-LCD-3.5](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5) : ESP32-S3, 8 Mo PSRAM, 16 Mo flash |
| Écran | 3,5" IPS 480 × 320, ST7796 en SPI, tactile capacitif |
| GPS | u-blox NEO-M9N externe (UART, 10 Hz) + antenne active déportée |
| Cap | magnétomètre QMC5883L (I²C) |
| Alimentation | batterie Li-ion interne gérée par l'AXP2101, recharge USB-C, aucun fil 12 V |
| Commandes | 5 boutons (A, B, +, −, menu) + tactile |
| Boîtier | PETG / ASA imprimé, aimants au dos (pas de PLA : habitacle à 60 °C en été) |

Distance **GPS uniquement**, sans capteur de roue ni intervention sur le câblage de la voiture.

## État

| Étape | Contenu | État |
|---|---|---|
| 1 | Squelette CMake + simulateur SDL 480 × 320 | fait |
| 2 | Core portable + tests : `gps_parser` (NMEA, sur minmea), `trip_engine` | parseur fait, `trip_engine` à faire |
| 3 | Interface LVGL : raid, balade, menu, calibration, démarrage animé, mode nuit | écrans faits, câblage au core à faire |
| 4 | Simulateur : rejeu de traces NMEA, scénarios | à faire |
| 5-6 | HAL ESP32 et firmware sur la carte | à faire |

Pour l'instant, l'interface tourne sur des données de démonstration. Le plan détaillé et le journal des
décisions sont dans [`PROMPT.md`](PROMPT.md).

## Démarrage rapide (simulateur PC)

Prérequis : CMake ≥ 3.20, Ninja, un compilateur C11, SDL2 (`sudo apt install libsdl2-dev` sous
Debian / Ubuntu / WSL). LVGL, Unity et les autres dépendances sont téléchargées par CMake ou déjà dans le
dépôt.

```bash
cmake --preset sim
cmake --build build-sim
./build-sim/raidmaster-sim
```

| Touche | Action |
|---|---|
| `a` / `b` | bouton A / B (`Maj` + touche : appui long) |
| `+` / `−` | correction de distance |
| `m` | menu (`Maj` + `m` : appui long) |
| `n` ou `Tab` | bascule jour / nuit |
| `v` | écran suivant |
| `d` | démo automatique marche / arrêt |
| `Échap` | quitter |

### Tests

```bash
ctest --preset sim
```

Tests unitaires du core avec [Unity](https://github.com/ThrowTheSwitch/Unity). Les traces NMEA longues de
`references/traces-brutes/` ne sont pas versionnées : présentes, elles sont rejouées ; absentes, ces tests
sont ignorés.

### GIF de démo

`docs/demo/raidmaster-demo.gif` est régénéré par la cible CMake `demo_gif` à chaque build où le binaire du
simulateur change. Le simulateur tourne alors sans fenêtre, en temps virtuel, et enchaîne démarrage, raid,
balade, menu, calibration et raid de nuit. Le GIF se commite avec la modification d'interface qui l'a
produit.

- Désactiver : `cmake --preset sim -DRAIDMASTER_DEMO_GIF=OFF`.
- À la demande, agrandi ou vers un autre fichier : `tools/demo_gif.sh [-s 1..4] [-o sortie.gif]`.

### Animation de démarrage

La 403 tourne sur une grille cyan lumineuse. Elle est rendue dans Blender
(`ui/assets/boot_anim/boot_anim.blend`), puis convertie au build en `boot_anim.bin` (palette 256 couleurs,
environ 5,5 Mo) par `ui/assets/boot_anim/build_boot_anim.py`. Cette conversion demande Python avec Pillow,
désigné par la variable CMake `RAIDMASTER_PYTHON`. Sans eux, l'écran de démarrage affiche une silhouette
fixe.

### Polices

Tout l'affichage est en **Share Tech Mono**, compilée en polices bitmap LVGL, uniquement pour les tailles
et les glyphes utilisés. Les mêmes fichiers servent au simulateur et à l'ESP32. Régénération :
`ui/fonts/gen_fonts.sh` (demande Node.js, appelle `lv_font_conv`).

## Participer au projet : partir de zéro

Le développement se fait sur PC, sans la carte : simulateur et tests suffisent jusqu'à l'étape 5.
Testé sous Ubuntu 24.04 (natif ou WSL2). Le dépôt est privé : demander un accès au propriétaire.

1. **Outils système**

   ```bash
   sudo apt install build-essential cmake ninja-build git pkg-config libsdl2-dev python3-venv
   ```

   CMake ≥ 3.20. Sous WSL2, la fenêtre SDL s'affiche via WSLg (Windows 11).

2. **Cloner**

   ```bash
   git clone git@github.com:docteurzoidberg/raidmaster-403.git
   cd raidmaster-403
   ```

3. **Python + Pillow** (optionnel, pour l'animation de démarrage)

   ```bash
   python3 -m venv .venv
   .venv/bin/pip install pillow
   ```

   CMake prend `.venv/` à la racine du dépôt s'il existe, sinon le `python3` du système. Pour en
   imposer un autre : `-DRAIDMASTER_PYTHON=/chemin/vers/python`. Sans Pillow, le build passe quand même :
   le démarrage affiche une silhouette fixe.

4. **Configurer, compiler, tester, lancer**

   ```bash
   cmake --preset sim          # télécharge LVGL et Unity au premier passage (connexion requise)
   cmake --build build-sim
   ctest --preset sim
   ./build-sim/raidmaster-sim
   ```

   Le build régénère aussi `docs/demo/raidmaster-demo.gif`. Si tu ne touches pas à l'interface et que
   `git status` le montre modifié, annule-le (`git checkout docs/demo/raidmaster-demo.gif`) ou configure
   avec `-DRAIDMASTER_DEMO_GIF=OFF`.

5. **Traces NMEA de test** (optionnel). `references/` n'est pas versionné. Les traces longues se
   génèrent avec les scripts Python de `tests/tools/` (sans dépendance, voir `tests/tools/README.md`),
   par exemple :

   ```bash
   mkdir -p references/traces-brutes
   python3 tests/tools/gen_nmea.py --mode linear --duration 3600 --rate 10 --speed-kmh 90 \
       --heading-deg 90 --start-lat 48.8566 --start-lon 2.3522 --start-utc 2026-10-09T06:00:00+00:00 \
       --out references/traces-brutes/stage_highway_1h.nmea
   ```

   Sans elles, les tests qui les rejouent sont ignorés. Si les paramètres diffèrent de ceux de
   référence, ces tests peuvent échouer sur les comptes attendus.

6. **Avant de proposer une modification**
   - Lire [`CLAUDE.md`](CLAUDE.md), [`PROMPT.md`](PROMPT.md) (état, plan, décisions) et le brief du rôle
     concerné dans [`prompts/`](prompts/). [`AGENTS.md`](AGENTS.md) dit quel rôle touche quels dossiers.
   - C11 strict, compilation sans avertissement (`-Wall -Wextra -Wpedantic`), `core/` sans aucun
     accès matériel ni LVGL.
   - Une branche par sujet, `ctest --preset sim` vert, GIF de démo commité avec toute modification de
     l'interface, ce README mis à jour s'il devient faux.
   - Optionnel : `compile_commands.json` est exporté dans `build-sim/` pour clangd ou ton IDE (lien
     symbolique à la racine si besoin).
   - Code sous GPL-3.0.

## Firmware ESP32

Pas encore commencé : étapes 5 et 6 du plan. Le preset CMake `firmware` n'est qu'un squelette.

## Organisation du dépôt

```
core/        logique métier en C11 portable (aucun accès matériel), core/third_party/minmea
ui/          interface LVGL v9 : écrans, thème jour / nuit, polices, images, animation de démarrage
sim/         simulateur PC SDL2 : fenêtre, clavier, captures, enregistrement du GIF
tests/       tests unitaires Unity ; tests/tools/ : outils Python de traces NMEA
hal_esp32/   pilotes ESP32 (à venir)
hal_sim/     HAL du simulateur (à venir)
firmware/    projet ESP-IDF (à venir)
tools/       scripts (demo_gif.sh)
docs/        GIF de démo, copies locales des maquettes (docs/artefacts/)
prompts/     cahier des charges découpé par rôle (features, hardware, boîtier, firmware, ui, simulation)
references/  non versionné : Open Rally Computer (inspiration), traces NMEA de test
```

## Documentation

- [`PROMPT.md`](PROMPT.md) : contexte, étapes, état et journal des décisions.
- [`prompts/`](prompts/) : un brief par rôle.
- [`AGENTS.md`](AGENTS.md) et [`CLAUDE.md`](CLAUDE.md) : organisation du travail entre agents.
- [`docs/artefacts/`](docs/artefacts/) : maquettes des écrans, styles de flèche de cap, propositions
  d'animation de démarrage.

## Licence

[GPL-3.0](LICENSE).

Composants tiers :

| Composant | Licence | Usage |
|---|---|---|
| [LVGL](https://lvgl.io) v9.2.2 | MIT | interface, téléchargé au build |
| [minmea](https://github.com/kosma/minmea) | MIT (au choix WTFPL / MIT / LGPL-3) | décodage NMEA, dans `core/third_party/minmea/` |
| [Unity](https://github.com/ThrowTheSwitch/Unity) v2.6.1 | MIT | tests, téléchargé au build |
| [Share Tech Mono](https://fonts.google.com/specimen/Share+Tech+Mono) | SIL OFL 1.1 | police, dans `ui/assets/` |
