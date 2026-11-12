#!/usr/bin/env bash
# Raidmaster 403 — simulateur web (WebAssembly) : build, test, publication dans mockup/sim/.
#
# Active emsdk (${EMSDK_DIR:-~/sdk/emsdk}, hors PATH), configure le preset web au besoin (sources LVGL
# et Python repris de build-sim/ s'ils y sont ; animation de démarrage : la planche si elle est là,
# sinon build-sim/boot_anim.bin), construit dans build-web/site/ (module raidmaster-sim-<VERSION>,
# build.json, scénarios), puis selon la commande :
#
# Usage : tools/web.sh [build | test | copy | all | serve [port]]
#   build  build seul (journal complet : build-web/web-build.log)
#   test   build, puis simweb/tests/test_web.cjs sous Node : chaque scénario du dépôt avec ses
#          expect, et une capture de l'écran comparée à celle du sim PC (build-sim/raidmaster-sim)
#   copy   build, puis recopie build-web/site/ dans mockup/sim/ (anciens modules retirés)
#   all    build, test et copie (défaut)
#   serve  sert mockup/ en local (python3 -m http.server, port 8000) : http://localhost:8000/sim/
# Code de sortie : 0 si tout est vert (build sans avertissement, test réussi), 1 sinon, 3 si emsdk
# manque.
#
# Licence : GPL-3.0.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CMD="${1:-all}"
MOCKUP="$ROOT/../mockup"

if [ "$CMD" = serve ]; then
    port="${2:-8000}"
    echo "http://localhost:$port/sim/  (Ctrl+C pour arrêter)"
    exec python3 -m http.server "$port" --directory "$MOCKUP"
fi
case "$CMD" in build|test|copy|all) ;; *) sed -n '2,19p' "$0"; exit 2 ;; esac

EMSDK_DIR="${EMSDK_DIR:-$HOME/sdk/emsdk}"
if [ ! -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    echo "WEB : non vérifié (emsdk absent : $EMSDK_DIR)"
    exit 3
fi
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
command -v emcc >/dev/null || { echo "WEB : emcc introuvable après activation d'emsdk"; exit 3; }

mkdir -p build-web
LOG=build-web/web-build.log
t0=$SECONDS
if [ ! -f build-web/build.ninja ]; then
    args=()
    lvgl="$ROOT/build-sim/_deps/lvgl-src"
    [ -d "$lvgl" ] && args+=("-DFETCHCONTENT_SOURCE_DIR_LVGL=$lvgl")
    py="$(sed -n 's/^RAIDMASTER_PYTHON:FILEPATH=//p' build-sim/CMakeCache.txt 2>/dev/null)"
    [ -n "$py" ] && args+=("-DRAIDMASTER_PYTHON=$py")
    [ -f build-sim/boot_anim.bin ] && args+=("-DRAIDMASTER_WEB_BOOT_ANIM=$ROOT/build-sim/boot_anim.bin")
    if ! cmake --preset web "${args[@]}" >build-web/web-configure.log 2>&1; then
        echo "WEB CONFIGURE : échec"; tail -20 build-web/web-configure.log; exit 1
    fi
fi
status=0
if cmake --build build-web >"$LOG" 2>&1; then build_ok=1; else build_ok=0; status=1; fi
diag="$(grep -E '(warning|error):' "$LOG" | grep -v '^em\(cc\|++\): warning: .*\[-Wemcc\]' | sort -u)"
nw=$(grep -c 'warning:' <<<"$diag" || true)
ne=$(grep -c 'error:' <<<"$diag" || true)
site=build-web/site
mod="$(sed -n 's/.*"module": "\(.*\)".*/\1/p' "$site/build.json" 2>/dev/null)"
if [ "$build_ok" = 1 ] && [ -z "$diag" ]; then
    size="$(du -ch "$site/$mod".{wasm,js,data} 2>/dev/null | tail -1 | cut -f1)"
    echo "WEB BUILD : ok, aucun avertissement ($((SECONDS - t0)) s, $mod : $size)"
else
    [ "$build_ok" = 1 ] && echo "WEB BUILD : ok, $nw avertissement(s)" || echo "WEB BUILD : ÉCHEC, $ne erreur(s), $nw avertissement(s)"
    [ -n "$diag" ] && head -20 <<<"$diag" | sed "s|$ROOT/||"
    [ "$build_ok" = 0 ] && [ -z "$diag" ] && tail -15 "$LOG"
    status=1
fi
[ "$build_ok" = 1 ] || exit 1

if [ "$CMD" = test ] || [ "$CMD" = all ]; then
    node="${EMSDK_NODE:-node}"
    if [ ! -x build-sim/raidmaster-sim ]; then
        echo "WEB TEST : sim PC absent (build-sim/raidmaster-sim), capture non comparée"
    fi
    "$node" simweb/tests/test_web.cjs "$site" build-sim/raidmaster-sim || status=1
fi

if [ "$CMD" = copy ] || [ "$CMD" = all ]; then
    dst="$MOCKUP/sim"
    mkdir -p "$dst/scenarios"
    n=0
    for f in "$site/$mod".js "$site/$mod".wasm "$site/$mod".data "$site/build.json" "$site"/scenarios/*; do
        [ -f "$f" ] || continue
        rel="${f#"$site"/}"
        if ! cmp -s "$f" "$dst/$rel"; then cp "$f" "$dst/$rel"; n=$((n + 1)); fi
    done
    # anciens modules et scénarios retirés du dépôt
    for f in "$dst"/raidmaster-sim-*.{js,wasm,data} "$dst"/scenarios/*; do
        [ -f "$f" ] || continue
        rel="${f#"$dst"/}"
        [ -f "$site/$rel" ] || { rm -f "$f"; n=$((n + 1)); }
    done
    echo "WEB COPIE : mockup/sim/ $([ "$n" = 0 ] && echo "déjà à jour" || echo "$n fichier(s) mis à jour")"
fi
exit "$status"
