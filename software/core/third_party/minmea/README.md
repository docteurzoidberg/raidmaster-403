# minmea (copie)

Bibliothèque C de décodage NMEA 0183, copiée sans modification depuis
https://github.com/kosma/minmea, commit `c43c9e7c5ed788122a9d8a5445679e1594f8a030` (2026-09-20).

- Fichiers repris : `minmea.c`, `minmea.h`, `COPYING` (WTFPL v2), `LICENSE.MIT`, `LICENSE.grants`.
- Licence : WTFPL v2, ou au choix MIT ou LGPL-3.0+ (`LICENSE.grants`). Utilisée ici sous **MIT**,
  compatible avec la GPL-3.0 du projet.
- Adaptation au projet hors de ce dossier : `core/minmea_port/minmea_compat.h` (remplace `timegm`).
- Mise à jour : recopier les fichiers depuis un nouveau commit, noter le commit ci-dessus, relancer
  `ctest --preset sim`.
