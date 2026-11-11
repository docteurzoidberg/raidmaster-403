# `tests/tools/` — outils NMEA pour les scénarios de simulation

> **Statut : v0.** Scripts de base pour générer, bruiter, valider et enregistrer des traces NMEA.
> Les scénarios de bout en bout qui les utilisent sont décrits dans
> [`../fixtures/scenarios/README.md`](../fixtures/scenarios/README.md).

## Contenu

| Script | Rôle |
|---|---|
| `gen_route.py` | Itinéraire décrit en texte (`route.txt` : segments, bruit, pertes de fix, HDOP, fix 2D) → trace NMEA RMC + GGA + GSA, et distance vérité à chaque repère. Génère les traces des scénarios au build. Pure stdlib. |
| `gen_nmea.py` | Génère une trace NMEA synthétique (ligne droite ou cercle, vitesse constante ou profilée), à 1-10 Hz. Pure stdlib, pas de dépendance. |
| `add_noise.py` | Injecte du bruit (jitter position gaussien, HDOP spikes, fenêtres de perte de fix) dans une trace .nmea existante. Pure stdlib. |
| `validate_nmea.py` | Vérifie les checksums RMC/GGA et la continuité temporelle d'une trace. Pure stdlib. |
| `compute_reference.py` | Calcule la distance Haversine totale « vérité terrain » d'une trace propre. Pure stdlib. |
| `gpx_to_nmea.sh` | Wrapper `gpsbabel` pour convertir un .gpx (OpenStreetMap, Strava, Komoot) en .nmea. **Nécessite `gpsbabel` installé** (`sudo apt install gpsbabel`). |
| `record_live.py` | Lit un module u-blox branché en USB via FTDI et enregistre les trames NMEA brutes. Nécessite `pyserial` (voir `requirements.txt`). |
| `requirements.txt` | Dépendances optionnelles (`pyserial` pour `record_live.py`). |

## Installation

Les quatre scripts `.py` principaux (`gen_nmea`, `add_noise`, `validate_nmea`,
`compute_reference`) tournent en **pure stdlib Python 3.8+**, pas d'installation requise.

Pour les scripts qui ont des dépendances :

```bash
pip install -r tests/tools/requirements.txt
```

Pour `gpx_to_nmea.sh` :

```bash
sudo apt install gpsbabel     # Debian / Ubuntu / WSL
```

## Exemples d'usage (vérifiés)

Générer une trace linéaire 1 h à 90 km/h cap est, 10 Hz :

```bash
python3 tests/tools/gen_nmea.py \
    --mode linear --duration 3600 --rate 10 \
    --speed-kmh 90 --heading-deg 90 \
    --start-lat 48.8566 --start-lon 2.3522 \
    --hdop 1.0 \
    --out references/traces-brutes/stage_highway_1h.nmea
```

Générer une trace stationnaire 10 min, HDOP 1.5 :

```bash
python3 tests/tools/gen_nmea.py \
    --mode stationary --duration 600 --rate 10 \
    --start-lat 48.8566 --start-lon 2.3522 \
    --hdop 1.5 \
    --out references/traces-brutes/stage_stationary_10min.nmea
```

Ajouter du bruit à une trace :

```bash
python3 tests/tools/add_noise.py \
    --in references/traces-brutes/stage_highway_1h.nmea \
    --out references/traces-brutes/stage_highway_1h_noisy.nmea \
    --jitter-m 3.0 --hdop-max 3.0 \
    --fix-loss "00:12:00-00:13:30"
```

Valider une trace :

```bash
python3 tests/tools/validate_nmea.py references/traces-brutes/stage_highway_1h.nmea
```

Calculer la distance de référence (Haversine) :

```bash
python3 tests/tools/compute_reference.py references/traces-brutes/stage_highway_1h.nmea
```

## À faire

1. **Lecture critique de ces scripts** et refonte si nécessaire (interface, défaut, format
   de sortie des logs).
2. Décider si on dépend de `gpsbabel` + `pyserial` + `gpxpy` + `pynmea2` ou si on reste pure
   stdlib (le brief dit « au choix » — moi j'ai pris pure stdlib par simplicité).
3. ~~Scénarios du catalogue~~ : faits avec `gen_route.py` (2026-10-10), voir
   `tests/fixtures/scenarios/README.md`.
4. ~~Harnais~~ : mode script du simulateur + `ctest --preset sim`.
