#!/usr/bin/env bash
# Raidmaster 403 — génère les polices bitmap LVGL (Share Tech Mono) compilées dans le binaire.
#
# Mêmes fichiers pour le simulateur et l'ESP32 : aucune police chargée à l'exécution.
# Seules les tailles utilisées par les écrans sont générées (tokens UI_FONT_* de ui_tokens.h),
# et les grandes tailles ne contiennent que les glyphes réellement affichés (chiffres…) pour
# limiter la flash. Toute nouvelle taille / nouveau caractère : l'ajouter ici, relancer, et
# déclarer la taille dans ../fonts.c.
#
# Prérequis : Node.js (npx). Usage : bash ui/fonts/gen_fonts.sh
# Licence : GPL-3.0 (police : SIL OFL 1.1, voir ../assets/LICENSE-ShareTechMono.txt).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"    # chemins relatifs dans les en-têtes générés
TTF="../assets/ShareTechMono-Regular.ttf"
CONV=(npx -y lv_font_conv@1.5.3)

# Texte courant : ASCII + Latin-1 (accents, °, ·) + − (U+2212) + › (U+203A) + … (U+2026).
TEXT_RANGE="0x20-0x7E,0xA0-0xFF,0x2212,0x203A,0x2026"

gen() {   # gen <px> <range|""> <symbols|"">
    local px="$1" range="$2" symbols="$3"
    local args=(--font "$TTF")
    [[ -n "$range"   ]] && args+=(-r "$range")
    [[ -n "$symbols" ]] && args+=(--symbols "$symbols")
    "${CONV[@]}" --size "$px" --bpp 4 --format lvgl --no-compress \
        --lv-include lvgl.h --lv-font-name "stm_${px}" "${args[@]}" -o "stm_${px}.c"
    echo "stm_${px}.c"
}

# Petites tailles : jeu de texte complet (barre d'état, libellés, menus, unités).
for px in 12 13 14 15 16 17; do
    gen "$px" "$TEXT_RANGE" ""
done

# Grandes tailles : glyphes utiles seulement.
gen 25  "" " 0123456789°,.-NSEOkmCXTI"     # valeurs bas d'écran balade, « EXTINCTION » (firmware)
gen 29  "" " 0123456789,.%+-−"             # facteur calibration
gen 34  "" " 0123456789,.-km"              # lectures calibration
gen 38  "" "0123456789°-"                  # V / CAP mode raid
gen 44  "" "0123456789.-"                  # Trip B
gen 72  "" "0123456789.-"                  # Trip A
gen 134 "" "0123456789"                    # logo « 403 »
gen 190 "" "0123456789-"                   # speedo balade
