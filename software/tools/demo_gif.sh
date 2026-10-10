#!/usr/bin/env bash
# Raidmaster 403 — génère à la demande le GIF de démo de l'UI depuis le simulateur.
#
# Build le sim si besoin (preset `sim`), le lance sans fenêtre en mode enregistrement
# (RAIDMASTER_RECORD, temps virtuel : démarrage, écran raid qui vit, balade, menu, calibration,
# raid de nuit) et écrit docs/demo/raidmaster-demo.gif. Aucune dépendance hors SDL2 + CMake :
# l'encodeur GIF est dans le sim (sim/gif_writer.c).
#
# Usage : tools/demo_gif.sh [-s ÉCHELLE] [-o SORTIE.gif]
#   -s ÉCHELLE  agrandissement entier 1..4 (défaut 1 → 480×320)
#   -o SORTIE   fichier produit (défaut docs/demo/raidmaster-demo.gif)
#
# Licence : GPL-3.0.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/../docs/demo/raidmaster-demo.gif"
SCALE=1

while getopts "s:o:h" opt; do
    case "$opt" in
        s) SCALE="$OPTARG" ;;
        o) OUT="$OPTARG" ;;
        h) sed -n '2,13p' "$0"; exit 0 ;;
        *) sed -n '2,13p' "$0" >&2; exit 2 ;;
    esac
done
case "$SCALE" in
    1|2|3|4) ;;
    *) echo "demo_gif : échelle invalide « $SCALE » (1..4)" >&2; exit 2 ;;
esac
OUT="$(realpath -m "$OUT")"     # -o relatif : relatif au dossier d'appel

cd "$ROOT"
if [ ! -f build-sim/build.ninja ] && [ ! -f build-sim/Makefile ]; then
    cmake --preset sim
fi
cmake --build build-sim --target raidmaster-sim

TMP="$(mktemp -d "${TMPDIR:-/tmp}/raidmaster-demo.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

# TMPDIR → le fichier tampon des images (tmpfile()) atterrit aussi dans le dossier temporaire.
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy TMPDIR="$TMP" \
    RAIDMASTER_RECORD="$TMP/demo.gif" RAIDMASTER_RECORD_SCALE="$SCALE" \
    ./build-sim/raidmaster-sim

mkdir -p "$(dirname "$OUT")"
mv -f "$TMP/demo.gif" "$OUT"
echo "demo_gif : $OUT ($(du -h "$OUT" | cut -f1))"
