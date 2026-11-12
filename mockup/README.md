# Maquette 3D

Les trois cartes du Raidmaster 403 (carte Waveshare ESP32-S3-Touch-LCD-3.5B, carte fille GPS et
magnétomètre, carte boutons) dans leur boîtier, assemblé ou éclaté, avec la trappe de la carte micro-SD et
le câble en démonstration. Le boîtier montré (cannelures, pare-chocs en TPU, liseré et lettrage de façade)
est une esquisse du modèle dessiné pour l'impression, encore à éprouver ; ses pièces en STEP sont dans
[`../enclosure/stepfiles/`](../enclosure/stepfiles/).

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/>** (GitHub Pages : le workflow
[`../.github/workflows/pages.yml`](../.github/workflows/pages.yml) publie ce dossier à la racine du site).

| Fichier | Rôle |
|---|---|
| `index.html`, `main.js`, `style.css` | page [three.js](https://threejs.org) (chargé depuis jsDelivr, à la version de l'importmap de `index.html`) |
| `raidmaster.glb` | modèle : pièces, positions éclatées et textes ; le maillage de la carte Waveshare vient du modèle 3D publié par Waveshare (voir [`../enclosure/README.md`](../enclosure/README.md)) |

En local, servir le dossier (un fichier ouvert directement ne charge pas le modèle) :

```bash
cd mockup
python3 -m http.server 8000      # puis http://localhost:8000
```

Paramètres d'adresse : `?ouverture=0` (assemblé) à `100` (éclaté), `?vue=face`, `?image=105` (fige la
démonstration sur une image), `?gps=puck`, `?gps=interne` ou `?gps=bouchon`, `?lang=fr` ou `?lang=en`.

Connectique GPS en deux versions, choisies au soudage de la carte fille : **puck externe** (module GPS et
boussole posé sur le tableau de bord, branché au flanc gauche) ou **GPS interne** (antenne en SMA au flanc
gauche), plus un **bouchon** qui ferme le flanc gauche quand rien n'y est branché. Les trois sont dans le modèle ;
le bloc « GPS » de la page en montre une.

Page en français et en anglais (menu du haut, avec le lien vers ce dépôt et vers le simulateur) : la langue
vient de `?lang=`, sinon du dernier choix (gardé dans le navigateur), sinon de la langue du navigateur. Les
textes des pièces sont en français dans le modèle ; leur traduction anglaise est dans `main.js`
(`MODELE_EN`), le français s'affiche pour un texte non traduit.

## Simulateur (`sim/`)

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/sim/>** (en local : `http://localhost:8000/sim/`).
Le firmware compilé en WebAssembly tourne dans ce même modèle (`../raidmaster.glb`) : écran vivant, capuchons
cliquables (appui long : 0,6 s), carte [Leaflet](https://leafletjs.com) / OpenStreetMap du trajet rejoué,
panneau de développement (scénarios, vitesse, actions et vérifications, import NMEA, essais, mémoire, journal).
Le boîtier s'affiche de face (« Recentrer » après une rotation) ; « Écran seul » montre l'écran sur toute la
largeur du cadre avec une barre des cinq boutons ; sur la carte, « Centrer » garde le véhicule au centre (un
glisser le coupe).

| Fichier | Rôle |
|---|---|
| `sim/index.html`, `sim/sim.js`, `sim/style.css` | page (écrite à la main) |
| `sim/raidmaster-sim-<version>.js` / `.wasm` / `.data` | module construit par `software/tools/web.sh` (numéro de version dans le nom : le cache de Pages ne sert pas un ancien module) ; `.data` : animation de démarrage |
| `sim/build.json` | nom du module, version, empreinte git du build |
| `sim/scenarios/` | traces et actions des scénarios des tests, `index.json` ; récupérés à la demande |

Paramètres d'adresse : `?scenario=s24_ligne_droite_boutons`, `?vitesse=100`, `?actions=1` (actions et
vérifications), `?vue=2d` ou `?vue=3d` (vue de départ ; sinon le dernier choix du bouton « Écran seul / Vue 3D », gardé dans le
navigateur ; écran seul forcé sans WebGL), `?son=1`, `?allumer=1` (sans clic, donc sans son),
`?lang=fr` ou `?lang=en`. Pour les essais automatiques, `window.raidmasterSim` donne l'état, les vérifications,
la position des capuchons à l'écran et appuie sur les boutons. Langue choisie comme pour la maquette (même
réglage gardé) ; l'interface du boîtier reste en français.
