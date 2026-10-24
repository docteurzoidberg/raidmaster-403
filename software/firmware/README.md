# Firmware ESP32-S3 — Waveshare ESP32-S3-Touch-LCD-3.5B

Projet ESP-IDF du raidmaster 403. Il reprend **les mêmes dossiers** que le simulateur : `core/` (logique
métier) et `ui/` (écrans LVGL) y sont déclarés comme composants ESP-IDF, `hal_esp32/` implémente pour la
carte les interfaces HAL du core (`core/include/hal/`). Même boucle que `sim/main.c` : le core reçoit le
NMEA, l'UI affiche son état.

**Version actuelle : démo.** Aucun périphérique externe : le GPS est le trajet simulé du sim
(`core/gps_synth`, en temps réel), le tactile tient lieu des 5 boutons, réglages et distances sont
sauvegardés en NVS. L'animation de démarrage est lue dans sa partition flash. L'heure affichée est
celle de la RTC (PCF85063), réglée au premier fix d'un vrai GPS : sans pile de sauvegarde ni GPS, elle
reste inconnue (`--:--`), le GPS de démonstration ne la règle pas. La barre d'état montre l'alimentation
lue sur le PMIC (éclair sur USB, « USB » sans batterie, animation en charge).

## Prérequis

- **ESP-IDF v5.5.6** dans `~/esp/esp-idf-v5.5.6` (autre emplacement : variable `RAIDMASTER_IDF_PATH`) :

  ```bash
  git clone -b v5.5.6 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v5.5.6
  ~/esp/esp-idf-v5.5.6/install.sh esp32s3
  ```

  `tools/fw.sh` charge lui-même l'environnement (`export.sh`) : rien à sourcer à la main. Les
  composants (LVGL, pilote d'écran, expandeur) sont téléchargés au premier build, aux versions de
  `dependencies.lock`.
- **Python + Pillow + numpy** pour l'animation de démarrage, comme pour le sim (voir le README du dépôt). Sans eux,
  le firmware démarre sur la silhouette fixe.
- **Sous WSL2 : usbipd-win** pour passer la carte à Linux (voir plus bas).

## Commandes

Depuis `software/` :

```bash
tools/fw.sh build        # compile ; « FIRMWARE : ok, aucun avertissement — 803 Ko, … »
tools/fw.sh flash        # build + flash complet (programme + animation 1,1 Mo), puis journal 10 s
tools/fw.sh app-flash    # programme seul, plus rapide (l'animation est déjà en flash)
tools/fw.sh log 20       # redémarre la carte et affiche 20 s de journal
tools/fw.sh monitor      # moniteur série interactif (Ctrl+] pour quitter)
tools/fw.sh menuconfig   # réglages, menu « Raidmaster 403 »
tools/fw.sh clean        # supprime build-firmware/
tools/check.sh           # sim + tests + firmware en une passe
BOOT_ANIM_FORMAT=1 tools/fw.sh flash   # animation au format brut (5,5 Mo) pour comparer ; =2 pour revenir
```

L'écran de démarrage affiche `v<numéro> · ESP32 · <hash git>` (`software/VERSION`, `*` après le hash si le
firmware a été compilé avec des modifications non commitées). Animation : format 2 par défaut (codée par
différence, 1,1 Mo) ou 1 (brute, 5,5 Mo), mêmes pixels, à trancher (`PROMPT.md` § 8) ; le choix est gardé
dans `build-firmware/` et il faut un `flash` complet (pas `app-flash`) pour changer celle de la carte.

Build dans `software/build-firmware/` (`sdkconfig` compris) : **`sdkconfig.defaults` fait foi**. Un essai
dans menuconfig se reporte dans `sdkconfig.defaults` s'il est gardé.

## Carte sous WSL2 (usbipd)

L'USB-C de la carte est l'USB natif de l'ESP32-S3 (USB Serial/JTAG, `303a:1001`) : flash et journaux
passent par lui, sans bouton à presser.

1. **Une seule fois**, dans un PowerShell **administrateur** : `usbipd list` (repérer `303a:1001`), puis
   `usbipd bind --busid <BUSID>`.
2. Ensuite `tools/fw.sh flash` relie la carte tout seul (`usbipd attach --wsl --auto-attach` en tâche de
   fond : elle revient dans WSL après chaque reset) et prend le premier `/dev/ttyACM*`. À la main :
   `tools/fw.sh attach`.

Si le flash n'arrive pas à se connecter : maintenir **BOOT**, appuyer sur **RESET**, relâcher BOOT
(mode téléchargement forcé), relancer `tools/fw.sh flash`, puis RESET.

## Réglages (menuconfig → « Raidmaster 403 »)

| Option | Défaut | Rôle |
|---|---|---|
| Sens de l'écran | 90° | UI paysage tournée vers le panneau portrait ; 270° si l'image est à l'envers |
| Horloge QSPI | 40 MHz | comme Waveshare ; plus haut à valider sur la carte |
| Rétroéclairage jour / nuit | 100 % / 30 % | suit le mode nuit du menu |
| Tactile en guise de boutons | oui | 5 bandes verticales invisibles, un cinquième de la largeur chacune : **A, B, −, +, Menu** (de gauche à droite) ; appui long ≥ 800 ms |

**Bouton PWR** (sur la tranche, relié au PMIC ; pas le bouton BOOT) : appui long (1,5 s) = extinction
propre (distances sauvegardées, « EXTINCTION » à l'écran, PMIC coupé) ; appui court : rien ; carte
éteinte : un appui la rallume. Le PMIC coupe aussi de lui-même après 4 s d'appui (réglage laissé par le
firmware d'usine), si le firmware ne répond plus.

Avec les boutons du tactile : Menu long = entrer / sortir du menu, −/+ pour naviguer, Menu court pour
valider ; A et B court = RAZ. Toucher un item du menu ne le sélectionne pas : seule compte la bande
touchée (sélection une ligne plus haut ou plus bas, ou validation de l'item en surbrillance).

## Ce qu'il y a dedans

| Fichier | Rôle |
|---|---|
| `CMakeLists.txt` | projet ESP-IDF ; composants `core`, `ui`, `hal_esp32` ; `LV_CONF_PATH` ; animation → partition `boot_anim` |
| `sdkconfig.defaults` | flash 16 Mo QIO, PSRAM octale 80 MHz, 240 MHz, console USB, FreeRTOS 1 kHz |
| `partitions.csv` | `nvs`, `factory` (4 Mo), `boot_anim` (6 Mo : de quoi garder le format 1) ; OTA plus tard |
| `main/main.c` | initialisation et boucle principale (une seule tâche : pas de verrou LVGL) |
| `main/touch_buttons.c` | tactile → boutons A/B/−/+/Menu (banc) |
| `main/lv_conf_esp32.h` | `ui/lv_conf.h` du sim, avec le malloc de la libc (PSRAM) au lieu du tas LVGL |
| `main/Kconfig.projbuild` | menu « Raidmaster 403 » |
| `dependencies.lock` | versions exactes des composants (versionné) |

Côté `hal_esp32/` : `board.c` (I²C, TCA9554, reset de l'écran), `board_display.c` (AXS15231B en QSPI,
rotation logicielle, envoi par bandes, rétroéclairage), `board_touch.c`, `board_rtc.c` (RTC PCF85063,
horloge de la carte), `board_pmu.c` (AXP2101 en lecture : USB, batterie, charge), `hal_time_esp.c`,
`hal_storage_nvs.c`, `hal_gps_demo.c`, `hal_asset_partition.c`. Brochage : `hal_esp32/board.h`.

## Journal de la carte

Au démarrage : `rtc: heure de la RTC : …` (ou `heure perdue` / `antérieure au firmware` : inconnue
jusqu'au premier fix GPS), `pmu: AXP2101 : REG00 …, VBUS … mV` (relevé brut) puis
`pmu: alimentation : USB, batterie absente` (une ligne à chaque changement ensuite), puis
`raidmaster: extinction (appui long sur PWR)` à l'extinction,
`board: carte prête (UI tournée de 90°, QSPI 40 MHz)`, `hal_gps: GPS de démonstration`,
`raidmaster: démarré, libre : interne … Ko, PSRAM … Ko`. Puis toutes les 10 s :
`raidmaster: <n> images/s, envoi <t> ms, libre : …` (images réellement envoyées à l'écran, durée de
rotation + envoi). Chaque contact du tactile : `touch_buttons: tactile (x, y) → bouton …`.
