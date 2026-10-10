# Scénarios de bout en bout

Chaque dossier contient `actions.txt` (appuis de boutons et vérifications horodatés) et soit
`route.txt` (itinéraire synthétique, trace générée au build par `tests/tools/gen_route.py` dans
`build-sim/tests/scenarios/<id>.nmea`), soit `trace.nmea` (trace versionnée, ex. enregistrement réel).
Le simulateur sans fenêtre rejoue la trace, joue les actions et échoue si une vérification `expect`
échoue. Lancés par `ctest --preset sim` (tests `scenario_*`, liste et vitesse de rejeu dans
`tests/CMakeLists.txt`). Format : `prompts/simulation.md` § 2.1.

```bash
python3 tests/tools/gen_route.py tests/fixtures/scenarios/s02_highway_cruise/route.txt --out /tmp/s02.nmea
SDL_VIDEODRIVER=dummy ./build-sim/raidmaster-sim --no-storage --speed 200 \
    --replay /tmp/s02.nmea --script tests/fixtures/scenarios/s02_highway_cruise/actions.txt
```

`gen_route.py` imprime la distance vérité à chaque repère `mark` : c'est la référence des `expect`.

| Phase | Scénario | Rejeu | Ce qui est vérifié |
|---|---|---|---|
| 1 | `s10_stationary_noise` | ×100 | 10 min à l'arrêt, ±5 m et ±0,5 km/h : 0 m, chrono arrêté |
| 1 | `s02_highway_cruise` | ×200 | 1 h à 120 km/h, calibration −1,5 % au menu : distance, moyenne, max |
| 2 | `s03_country_road` | ×200 | 1 h de route sinueuse 70-90 km/h, bruit léger |
| 2 | `s01_city_commute` | ×100 | 30 min en ville, 12 feux, bruit urbain |
| 3 | `s20_raz_burst` | ×10 | 5 RAZ A en 10 s : seule la dernière compte, B et ODO intacts |
| 3 | `s22_increment_sequence` | ×10 | +/− en série, appuis longs, plancher à 0, Trip B, aucune dérive |
| 3 | `s21_calib_mid_stage` | ×100 | calibration 0 → +2 % à mi-parcours : passé inchangé |
| 3 | `s23_menu_during_drive` | ×20 | menu ouvert en roulant : les trips continuent |
| 3 | `s24_ligne_droite_boutons` | ×100 | distance puis RAZ, bascule A/B, corrections |
| 3 | `s25_calibration_auto` | ×10 | calibration auto complète par l'interface |
| 4 | `s13_cold_start` | ×10 | module muet puis 30 s sans fix : rien compté, heure inconnue |
| 4 | `s15_backtrack` | ×10 | demi-tour à 180° : la distance continue de monter |
| 4 | `s11_tunnel_loss` | ×10 | 2 min sans fix : trips figés ; rattrapage (règle provisoire) |
| 4 | `s14_fix_degraded_3d_to_2d` | ×50 | fix 2D pendant 10 min : distance inchangée |
| 5 | `s30_full_raid_day` | ×1000 | étape de 12 h, bruit, pics de HDOP : 717 km à ±0,2 % |
| 5 | `s31_power_loss_mid` | ×1 | coupure d'alimentation à 90 km/h : réglages relus, ≤ 250 m perdus |
| 6 | `s04_mountain_pass` | ×100 | col, virages à 25 °/s |
| 6 | `s05_offroad_slow` | ×100 | piste 20-40 km/h, cahots |
| 7 | `s12_canyon_hdop` | ×50 | HDOP 8 pendant 2 min : fixs refusés, fix perdu, ±2 % |
| 7 | `s16_antimeridian` | ×10 | traversée du méridien 180° |
| 7 | `s17_midnight_utc` | ×10 | minuit UTC en roulant : heure locale et distance continuent |

À venir : boussole et inclinomètre (s40, s41, s42), dégradations matérielles (s32 SD pleine, s33
batterie critique).
