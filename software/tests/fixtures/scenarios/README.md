# Scénarios de bout en bout

Chaque dossier rejoue `trace.nmea` dans le simulateur sans fenêtre, joue `actions.txt` (appuis de
boutons et vérifications horodatés) et échoue si une vérification `expect` échoue. Lancés par
`ctest --preset sim` (tests `scenario_*`, voir `tests/CMakeLists.txt`).

```bash
SDL_VIDEODRIVER=dummy ./build-sim/raidmaster-sim --no-storage --speed 100 \
    --replay tests/fixtures/scenarios/s01_ligne_droite_boutons/trace.nmea \
    --script tests/fixtures/scenarios/s01_ligne_droite_boutons/actions.txt
```

Format de `actions.txt` : voir l'en-tête de `sim/main.c` (une action par ligne,
`<ms> <action>`, temps réel depuis le lancement ; l'écran de démarrage dure 3 s et ignore les
boutons). Vitesse de rejeu : premier commentaire du fichier (`×N`), reprise dans
`tests/CMakeLists.txt`.

Traces générées par `tests/tools/` (1 Hz pour rester petites) :

| Scénario | Génération |
|---|---|
| `s01_ligne_droite_boutons` | `gen_nmea.py --mode linear --duration 120 --rate 1 --speed-kmh 90 --heading-deg 90 --start-lat 48.8566 --start-lon 2.3522 --start-utc 2026-10-09T06:00:00+00:00` |
| `s02_arret_bruit` | `gen_nmea.py --mode stationary --duration 120 --rate 1 --hdop 1.5` (mêmes position et heure), puis `add_noise.py --jitter-m 4.0 --hdop-max 1.9 --seed 403` |
| `s03_calibration_auto` | `gen_nmea.py --mode stationary --duration 60 --rate 1` (06:00:00) suivi de `gen_nmea.py --mode linear --duration 60 --rate 1 --speed-kmh 72 --heading-deg 0` (06:01:00), concaténés |
