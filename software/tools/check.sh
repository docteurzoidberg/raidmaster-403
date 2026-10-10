#!/usr/bin/env bash
# Raidmaster 403 — vérification complète en une commande, sortie courte.
#
# Build du preset sim (configure si besoin), avertissements et erreurs de compilation,
# tests ctest (avec le décompte Unity : réussis / échoués / ignorés), état du GIF de démo.
# Les journaux complets restent dans build-sim/check-build.log et build-sim/Testing/.
#
# Usage : tools/check.sh [-c]
#   -c  reconstruit tout (--clean-first) : un build incrémental ne réaffiche pas les
#       avertissements des fichiers déjà compilés.
# Code de sortie : 0 si build propre (aucun avertissement) et tests verts, 1 sinon.
#
# Licence : GPL-3.0.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CLEAN=()
while getopts "ch" opt; do
    case "$opt" in
        c) CLEAN=(--clean-first) ;;
        *) sed -n '2,12p' "$0"; exit 2 ;;
    esac
done

status=0
mkdir -p build-sim
LOG=build-sim/check-build.log

if [ ! -f build-sim/build.ninja ]; then
    if ! cmake --preset sim >"$LOG" 2>&1; then
        echo "CONFIGURE : échec"; tail -20 "$LOG"; exit 1
    fi
fi

if cmake --build build-sim "${CLEAN[@]}" >"$LOG" 2>&1; then
    build_ok=1
else
    build_ok=0; status=1
fi
diag="$(grep -E '(warning|error):' "$LOG" | sort -u)"
nw=$(grep -c 'warning:' <<<"$diag" || true)
ne=$(grep -c 'error:' <<<"$diag" || true)
if [ "$build_ok" = 1 ] && [ -z "$diag" ]; then
    echo "BUILD : ok, aucun avertissement"
else
    [ "$build_ok" = 1 ] && echo "BUILD : ok, $nw avertissement(s)" || echo "BUILD : ÉCHEC, $ne erreur(s), $nw avertissement(s)"
    [ -n "$diag" ] && head -20 <<<"$diag" | sed "s|$ROOT/||"
    [ "$build_ok" = 0 ] && [ -z "$diag" ] && tail -15 "$LOG"
    status=1
fi

if [ "$build_ok" = 1 ]; then
    TLOG=build-sim/check-tests.log
    if ctest --preset sim >"$TLOG" 2>&1; then t_ok=1; else t_ok=0; status=1; fi
    summary="$(grep -E 'tests passed' "$TLOG")"
    unity="$(grep -hE '^[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' build-sim/Testing/Temporary/LastTest.log 2>/dev/null \
             | awk '{t+=$1; f+=$3; i+=$5} END {if (NR) printf "Unity : %d tests, %d échec(s), %d ignoré(s)", t, f, i}')"
    echo "TESTS : ${summary:-aucun test trouvé}${unity:+ — $unity}"
    if [ "$t_ok" = 0 ]; then
        grep -hE ':FAIL' build-sim/Testing/Temporary/LastTest.log 2>/dev/null | sed "s|$ROOT/||" | head -20
        grep -E 'Not Run|\*\*\*' "$TLOG" | head -10
    fi
fi

gif="docs/demo/raidmaster-demo.gif"
case "$(git status --porcelain -- "$gif" 2>/dev/null)" in
    "") echo "GIF : inchangé" ;;
    *)  echo "GIF : modifié ($(du -h "$gif" | cut -f1)) — à commiter avec la modification d'UI, sinon git checkout -- $gif" ;;
esac

exit "$status"
