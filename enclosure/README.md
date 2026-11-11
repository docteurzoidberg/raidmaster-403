# Boîtier

Le boîtier du Raidmaster 403 sera dessiné dans Fusion et imprimé en PETG ou ASA (pas de PLA : l'habitacle
monte à 60 °C en été). Il n'est pas encore dessiné ; une [maquette 3D de principe](../mockup/) vérifie déjà
l'encombrement des trois cartes ([en ligne](https://docteurzoidberg.github.io/raidmaster-403/mockup/)).

## Fichiers STEP du projet (`stepfiles/`)

Exports STEP du boîtier depuis Fusion, à venir.

| Fichier | Contenu |
|---|---|
| — | aucun pour l'instant |

## Modèles 3D externes utilisés

Ces modèles ne sont pas dans le dépôt (droits de leurs auteurs) : les récupérer à leur source.

| Modèle | Description | Source |
|---|---|---|
| Waveshare ESP32-S3-Touch-LCD-3.5B, modèle 3D | assemblage STEP complet de la carte (vitre, LCD, PCB, composants), avec son plan coté ; sert à la conception du boîtier et à la maquette 3D | archive « 2D & 3D » du [wiki Waveshare](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B) : [ESP32-S3-Touch-LCD-3_5B.rar](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3_5B.rar) |
| Boîtier Waveshare d'origine, version 3.5B-C | plan coté et photos du boîtier vendu avec la carte : principe de la collerette et passage de la prise USB-C | [ESP32-S3-Touch-LCD-3.5B-C.rar](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3.5B-C.rar) |
| [Waveshare ESP32-S3 3.5" Case (Parametric)](https://makerworld.com/fr/models/2552125-waveshare-esp32-s3-3-5-case-parametric), par oirbsiu | coque arrière OpenSCAD ([BOSL2](https://github.com/BelfrySCAD/BOSL2)) imprimée et testée sur la même carte : source de cotes éprouvées (vitre, colonnes, USB-C, carte SD, boutons). Seules des cotes en sont reprises, le boîtier du projet est un dessin à part | MakerWorld, licence *Standard Digital File License* (pas de redistribution) |
| u-blox NEO (module GPS NEO-M9N) | modèle STEP du module, pour la carte fille | [u-blox/3D-Step-Models-Library](https://github.com/u-blox/3D-Step-Models-Library), fichier `POS/NEO.STEP` |
| Bouton poussoir 12 mm Würth 430476085716 | modèle des 5 boutons de la carte boutons | bibliothèque 3D livrée avec KiCad (`Button_Switch_THT.3dshapes`) |
