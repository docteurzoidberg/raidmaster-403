# Modèles 3D propres au projet

| Fichier | Utilisé par | Origine |
|---|---|---|
| `ublox_NEO.step` | U1 (NEO-M9N) | `POS/NEO.STEP` du dépôt officiel [u-blox/3D-Step-Models-Library](https://github.com/u-blox/3D-Step-Models-Library) (licence u-blox : utilisation, modification et redistribution libres) ; recentré, tourné de 180° (broche 1 en haut à gauche comme l'empreinte `RF_GPS:ublox_NEO`) et posé à z = 0 par FreeCAD. |
| `GY-271_module.step` | J3 (trous seuls, module soudé directement) | Généré par `gen3d.py` (FreeCADCmd 0.20) : carte 14,8 × 13,5 × 1,6 mm soudée directement par sa barrette mâle (entretoise 2,5 mm), sommet ≈ 5,1 mm. Aucun modèle trouvé en ligne. Sens de débord à vérifier sur le module reçu. |

Les boutons 12 mm utilisent le modèle KiCad `Button_Switch_THT.3dshapes/SW_PUSH-12mm_Wuerth-430476085716.step`
(mêmes pastilles que `SW_PUSH-12mm`, dont le modèle n'est pas livré avec KiCad 10).
