#!/usr/bin/env bash
# Raidmaster 403 — firmware ESP32-S3 en une commande, sortie courte.
#
# Usage : tools/fw.sh <commande> [arguments]
#   build          compile (configure si besoin) : avertissements de notre code, taille du programme
#   flash          build + flash complet (programme + animation de démarrage), puis journal 10 s
#   app-flash      build + flash du programme seul (plus rapide, l'animation déjà en place)
#   log [s]        redémarre la carte et affiche son journal pendant s secondes (défaut 10)
#   monitor        moniteur série interactif (idf.py monitor, Ctrl+] pour quitter)
#   menuconfig     réglages ESP-IDF (menu « Raidmaster 403 ») ; reporter ce qu'on garde dans
#                  firmware/sdkconfig.defaults
#   attach         relie la carte (USB Windows) à WSL par usbipd
#   clean          supprime build-firmware/ (le prochain build repart de sdkconfig.defaults)
#
# Variables : RAIDMASTER_IDF_PATH (ESP-IDF à utiliser, défaut ~/esp/esp-idf-v5.5.6, quel que soit
# l'IDF_PATH du shell), PORT (défaut : premier /dev/ttyACM*), BOOT_ANIM_FORMAT (1 ou 2 : format de
# l'animation de démarrage, gardé ensuite dans build-firmware/ ; défaut 2).
# Journal complet du dernier build : build-firmware/fw-build.log.
# Code de sortie : 0 si tout s'est bien passé (build sans avertissement de notre code), 1 sinon.
#
# Licence : GPL-3.0.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-firmware"
LOG="$BUILD/fw-build.log"
USB_ID="303a:1001"                       # USB Serial/JTAG de l'ESP32-S3
USBIPD="/mnt/c/Program Files/usbipd-win/usbipd.exe"
RM_IDF="${RAIDMASTER_IDF_PATH:-$HOME/esp/esp-idf-v5.5.6}"

usage() { sed -n '2,19p' "$0"; exit 2; }
[ $# -ge 1 ] || usage
cmd="$1"; shift

idf_env() {
    if [ ! -f "$RM_IDF/export.sh" ]; then
        echo "ESP-IDF introuvable dans $RM_IDF (variable RAIDMASTER_IDF_PATH, voir firmware/README.md)"; exit 1
    fi
    # shellcheck disable=SC1091
    . "$RM_IDF/export.sh" >/dev/null 2>&1 || { echo "ESP-IDF : export.sh a échoué"; exit 1; }
}

idf() {
    (cd "$ROOT/firmware" && idf.py -B "$BUILD" -DSDKCONFIG="$BUILD/sdkconfig" \
        ${BOOT_ANIM_FORMAT:+-DRAIDMASTER_BOOT_ANIM_FORMAT="$BOOT_ANIM_FORMAT"} "$@")
}

do_build() {
    mkdir -p "$BUILD"
    if ! idf build >"$LOG" 2>&1; then
        echo "FIRMWARE : ÉCHEC du build"
        grep -E '(error|warning):|undefined reference|FAILED:' "$LOG" | sed "s|$ROOT/||" | sort -u | head -20
        grep -qE '(error|warning):|undefined reference' "$LOG" || tail -15 "$LOG"
        return 1
    fi
    # Avertissements de notre code (core, ui, hal_esp32, firmware), pas des composants tiers.
    local diag
    diag="$(grep -E '(warning|error):' "$LOG" | grep -vE 'managed_components|esp-idf' | sed "s|$ROOT/||" | sort -u)"
    local size
    size="$(grep -E 'raidmaster-403.bin binary size' "$LOG" | sed -E 's/.*size (0x[0-9a-f]+) bytes.* (0x[0-9a-f]+) bytes \(([0-9]+)%\) free.*/\1 \3/')"
    local bytes pct
    read -r bytes pct <<<"$size"
    local kib=""
    [ -n "${bytes:-}" ] && kib="$(( bytes / 1024 )) Ko, ${pct} % de la partition libre"
    if [ -n "$diag" ]; then
        echo "FIRMWARE : ok, $(grep -c 'warning:' <<<"$diag") avertissement(s) de notre code${kib:+ — $kib}"
        head -20 <<<"$diag"
        return 1
    fi
    echo "FIRMWARE : ok, aucun avertissement${kib:+ — $kib}"
    if ! grep -q 'boot_anim.bin' "$BUILD/flash_args" 2>/dev/null; then
        echo "  (animation de démarrage non générée : Python + Pillow introuvables, voir le journal)"
    fi
}

# Port série ; à défaut, tente de relier la carte à WSL par usbipd.
find_port() {
    if [ -n "${PORT:-}" ]; then echo "$PORT"; return 0; fi
    local p
    p="$(ls /dev/ttyACM* 2>/dev/null | head -1)"
    if [ -n "$p" ]; then echo "$p"; return 0; fi
    do_attach >&2 || return 1
    for _ in $(seq 1 20); do
        p="$(ls /dev/ttyACM* 2>/dev/null | head -1)"
        if [ -n "$p" ]; then echo "$p"; return 0; fi
        sleep 0.5
    done
    echo "carte reliée mais aucun /dev/ttyACM* (voir « dmesg | tail »)" >&2
    return 1
}

do_attach() {
    if [ ! -x "$USBIPD" ]; then
        echo "usbipd-win introuvable ($USBIPD) : brancher la carte à WSL à la main"; return 1
    fi
    local line busid state
    line="$("$USBIPD" list 2>/dev/null | tr -d '\r' | grep -i "$USB_ID" | head -1)"
    if [ -z "$line" ]; then
        echo "carte non vue par Windows (USB $USB_ID) : câble USB-C branché ? carte alimentée ?"; return 1
    fi
    busid="$(awk '{print $1}' <<<"$line")"
    state="$(grep -oE '(Not shared|Shared|Attached)$' <<<"$line")"
    case "$state" in
        "Not shared")
            echo "carte en $busid, pas encore partagée : une seule fois, dans un PowerShell administrateur :"
            echo "    usbipd bind --busid $busid"
            echo "puis relancer la commande."
            return 1 ;;
        "Attached")
            return 0 ;;
    esac
    # --auto-attach : la carte revient dans WSL après chaque reset (flash, redémarrage).
    echo "carte en $busid : liaison à WSL (usbipd attach --auto-attach, en tâche de fond)"
    setsid nohup "$USBIPD" attach --wsl --busid "$busid" --auto-attach >/dev/null 2>&1 &
    return 0
}

do_log() {
    local secs="${1:-10}" port
    port="$(find_port)" || return 1
    python "$ROOT/tools/fw_log.py" "$port" "$secs"
}

case "$cmd" in
    build)
        idf_env; do_build ;;
    flash|app-flash)
        idf_env
        do_build || exit 1
        port="$(find_port)" || exit 1
        if ! idf -p "$port" "$cmd" >"$BUILD/fw-flash.log" 2>&1; then
            echo "FLASH : échec ($port)"; grep -E 'error|Error|fatal|A fatal' "$BUILD/fw-flash.log" | head -10
            tail -5 "$BUILD/fw-flash.log"; exit 1
        fi
        echo "FLASH : ok ($cmd sur $port, $(grep -cE '^Wrote ' "$BUILD/fw-flash.log") zone(s) écrite(s))"
        python "$ROOT/tools/fw_log.py" "$port" "${1:-10}" ;;
    log)
        idf_env; do_log "${1:-10}" ;;
    monitor)
        idf_env
        port="$(find_port)" || exit 1
        idf -p "$port" monitor ;;
    menuconfig)
        idf_env; idf menuconfig ;;
    attach)
        do_attach ;;
    clean)
        rm -rf "$BUILD"; echo "build-firmware/ supprimé" ;;
    *)
        usage ;;
esac
