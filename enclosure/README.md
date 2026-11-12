# Boîtier

Le boîtier du Raidmaster 403 est dessiné dans Fusion, en modèle paramétrique, pour une impression en PETG ou
ASA (pas de PLA : l'habitacle monte à 60 °C en été) avec des pare-chocs en TPU. Il mesure 123 × 93,6 ×
33,2 mm (126,6 × 34,7 × 97,2 mm hors tout avec les pare-chocs et les capuchons des boutons). Une
[maquette 3D](../mockup/) montre les cartes dans leur boîtier ([en ligne](https://docteurzoidberg.github.io/raidmaster-403/)).

Les cotes sont encore à éprouver (inserts, jeux, languettes du TPU) : ces fichiers ne sont pas une version
validée pour l'impression.

## Fichiers STEP du projet (`stepfiles/`)

Exports STEP des pièces depuis Fusion (2026-11-12), toutes dans le même repère (origine au centre de la vitre
de l'écran, sur sa face arrière ; X vers la droite vu de face, Y vers l'arrière, Z vers le haut ; mm) : elles
s'assemblent sans déplacement. Les pièces multi-filament contiennent un corps par couleur.

| Fichier | Contenu | Matière |
|---|---|---|
| `coque_arriere.step` | coque arrière : parois, cloison entre l'électronique et la batterie, poche de la batterie, colonnes des vis de fermeture, cannelures, logements du TPU et ses 12 nervures d'accroche, ouverture du flanc gauche pour la connectique GPS | PETG gris foncé |
| `facade.step` | façade : fenêtre de l'écran, bossages des inserts et de la carte boutons, lèvre de centrage qui entre dans la coque arrière, cannelures ; corps séparés pour le liseré et le lettrage « RAIDMASTER 403 » (ambre) et les deux fenêtres de LED (translucide) | PETG gris foncé, ambre, translucide |
| `capuchon_A.step`, `capuchon_B.step`, `capuchon_menu.step`, `capuchon_haut.step`, `capuchon_bas.step` | capuchons des 5 boutons, Ø 14,5 à collerette ; légende incrustée de 0,6 mm en corps séparé (A ambre, B cyan, menu rouge, flèches blanches) | PETG gris + couleur de la légende |
| `poussoir_pwr.step` | poussoir du bouton marche, avec son éclair rouge incrusté | PETG gris + rouge |
| `trappe_sd.step` | trappe coulissante de la carte micro-SD, à cran | PETG gris |
| `trappe_batterie.step` | trappe de la batterie, tenue par 4 vis M3 × 5 | PETG gris |
| `manchon_usb.step` | manchon de la prise USB-C, rapporté dans le flanc droit | PETG gris |
| `manchon_puck.step` | manchon de la connectique GPS, version puck externe (module GPS et boussole posé sur le tableau de bord) : canal de 11,4 × 21,3 mm devant les deux prises du puck soudées au bord de la carte fille ; rapporté dans le flanc gauche. Forme provisoire | PETG gris |
| `manchon_antenne.step` | manchon de la connectique GPS, version GPS interne : plaque qui tient une prise SMA de cloison (antenne active) ; rapporté dans le flanc gauche, à la place du précédent. Forme provisoire | PETG gris |
| `pare_chocs.step` | les 4 coins en TPU, coincés entre la façade et la coque arrière et accrochés chacun à 3 nervures de la coque (ils s'enfilent dessus par l'avant) | TPU orange |

Visserie (non fournie en STEP) : 4 vis CHC M3 × 10 et 4 inserts à chaud M3 (fermeture), 4 vis CHC M3 × 5 et
4 inserts à chaud M3 (trappe de la batterie), 4 vis CHC M2 × 12 (cartes), 4 vis CHC M2 × 6 (carte boutons),
toutes en inox ; inserts dans des perçages Ø 4,33 mm.

## Modèles 3D externes utilisés

Ces modèles ne sont pas dans le dépôt (droits de leurs auteurs) : les récupérer à leur source.

| Modèle | Description | Source |
|---|---|---|
| Waveshare ESP32-S3-Touch-LCD-3.5B, modèle 3D | assemblage STEP complet de la carte (vitre, LCD, PCB, composants), avec son plan coté ; sert à la conception du boîtier et à la maquette 3D | archive « 2D & 3D » du [wiki Waveshare](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B) : [ESP32-S3-Touch-LCD-3_5B.rar](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3_5B.rar) |
| Boîtier Waveshare d'origine, version 3.5B-C | plan coté et photos du boîtier vendu avec la carte : principe de la collerette et passage de la prise USB-C | [ESP32-S3-Touch-LCD-3.5B-C.rar](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3.5B-C.rar) |
| [Waveshare ESP32-S3 3.5" Case (Parametric)](https://makerworld.com/fr/models/2552125-waveshare-esp32-s3-3-5-case-parametric), par oirbsiu | coque arrière OpenSCAD ([BOSL2](https://github.com/BelfrySCAD/BOSL2)) imprimée et testée sur la même carte : source de cotes éprouvées (vitre, colonnes, USB-C, carte SD, boutons). Seules des cotes en sont reprises, le boîtier du projet est un dessin à part | MakerWorld, licence *Standard Digital File License* (pas de redistribution) |
| u-blox NEO (module GPS NEO-M9N) | modèle STEP du module, pour la carte fille | [u-blox/3D-Step-Models-Library](https://github.com/u-blox/3D-Step-Models-Library), fichier `POS/NEO.STEP` |
| Bouton poussoir 12 mm Würth 430476085716 | modèle des 5 boutons de la carte boutons | bibliothèque 3D livrée avec KiCad (`Button_Switch_THT.3dshapes`) |
