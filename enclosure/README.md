# Boîtier

Conception du boîtier aimanté (brief `prompts/boitier.md`). Document Fusion prévu :
`tableau-raidmaster-403` ; plus tard ici : exports (STEP, STL / 3MF), plans, photos d'essais. Les
éprouvettes vont dans `../../console-epreuves/`, jamais ici.

## Modèle 3D et plan coté de la carte Waveshare ESP32-S3-Touch-LCD-3.5B (`cad/`)

| Fichier | Contenu | Source |
|---|---|---|
| `cad/esp32-s3-touch-lcd-3.5B.stp` | assemblage 3D complet (vitre, LCD, PCB, composants ; Creo, 17 Mo) | archive [2D & 3D](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3_5B.rar) du wiki |
| `cad/esp32-s3-touch-lcd-3.5B-dimensions.pdf` | plan coté (nom d'origine « … 外形尺寸.pdf », « dimensions extérieures ») | idem |

Usage : conception du boîtier ; cotes principales reprises dans `prompts/boitier.md` § 3.1.

Les fichiers de `cad/` restent hors git (droits Waveshare) : les récupérer depuis le lien ci-dessus.

## Boîtier de référence MakerWorld (`reference/makerworld-2552125/`)

[Waveshare ESP32-S3 3.5" Case (Parametric)](https://makerworld.com/fr/models/2552125-waveshare-esp32-s3-3-5-case-parametric#profileId-2810851),
par oirbsiu : coque arrière OpenSCAD (BOSL2) imprimée et testée par son auteur sur la même carte, qui
sert donc de **source de cotes déjà éprouvées** (vitre, colonnes, USB-C, carte SD, boutons et poussoirs).

| Fichier | Contenu |
|---|---|
| `reference/makerworld-2552125/waveshare-esp32-s3-case.scad` | source paramétrique (versionné, dépôt privé) |
| `cad/makerworld-2552125/waveshare-esp32-s3-case.3mf` | projet Bambu Studio du modèle (maillage seul, hors git) |

Licence MakerWorld *Standard Digital File License* : usage personnel, ni partage ni redistribution du
fichier ou d'une œuvre dérivée. Le `.scad` reste versionné tant que le dépôt est privé, et doit en être
retiré (historique compris) avant de rendre le dépôt public. Notre boîtier est un dessin à part dans
Fusion : on n'en reprend que des cotes, relevées dans `prompts/boitier.md` § 3.2.

Rendu : OpenSCAD 2021.01 (WSL et Windows) avec la bibliothèque
[BOSL2](https://github.com/BelfrySCAD/BOSL2), installée dans `~/.local/share/OpenSCAD/libraries/BOSL2`
(clone git : `git pull` pour la mettre à jour) et `D:\<sync>\Documents\OpenSCAD\libraries\BOSL2`
(copie). Aperçu F5 rapide ; en 2021.01 le rendu F6 / export STL dépasse 20 min (la grille du
haut-parleur surtout : `enable_speaker_grill = false`), quelques secondes avec une version récente et le
moteur Manifold. Deux avertissements sans effet viennent du fichier lui-même (variable
`west_lip_relief_round` jamais définie, pour un rayon inutilisé).
