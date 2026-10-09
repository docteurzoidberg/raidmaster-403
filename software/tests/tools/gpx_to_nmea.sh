#!/usr/bin/env bash
# Convertit un fichier .gpx (OpenStreetMap, Strava, Komoot) en .nmea via gpsbabel.
#
# Prérequis : gpsbabel installé (`sudo apt install gpsbabel` sur Debian/Ubuntu/WSL).
#
# Usage :
#   ./gpx_to_nmea.sh input.gpx output.nmea
#
# STAGING v0 — à revoir par l'agent simulation.
# Licence : GPL-3.0.

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 input.gpx output.nmea" >&2
    exit 2
fi

IN="$1"
OUT="$2"

if ! command -v gpsbabel >/dev/null 2>&1; then
    echo "ERREUR : gpsbabel introuvable. Installer avec : sudo apt install gpsbabel" >&2
    exit 1
fi

if [[ ! -f "$IN" ]]; then
    echo "ERREUR : fichier introuvable : $IN" >&2
    exit 2
fi

mkdir -p "$(dirname "$OUT")"
gpsbabel -i gpx -f "$IN" -o nmea -F "$OUT"
echo "OK : $IN -> $OUT ($(wc -l < "$OUT") lignes)"
