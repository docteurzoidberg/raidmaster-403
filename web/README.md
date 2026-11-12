# Site du Raidmaster 403

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/>** (GitHub Pages : le workflow
[`../.github/workflows/pages.yml`](../.github/workflows/pages.yml) publie ce dossier à la racine du site).

| Adresse | Dossier | Contenu |
|---|---|---|
| `/` | `index.html`, `home.css`, `home.js`, `img/` | accueil : présentation du projet, écran du tripmaster qui joue la démo, liens |
| `/mockup/` | `mockup/` | maquette 3D des cartes dans leur boîtier |
| `/sim/` | `sim/` | simulateur : le firmware dans la maquette 3D, trajet GPS sur une carte |
| `/manual/` | `manual/` | manuel d'utilisation (en cours de rédaction) |

`common/` : en-tête commun aux quatre pages (`common.css` : couleurs, police, en-tête ; `common.js` :
navigation, sélecteur de langue à drapeau, bouton GitHub). Chaque page est en français et en anglais : la langue
vient de `?lang=fr` ou `?lang=en`, sinon du dernier choix fait dans le sélecteur (gardé dans le navigateur, le
même pour tout le site), sinon du français si le navigateur le préfère, sinon de l'anglais.

`img/` : copies des illustrations du README (`docs/demo/`) utilisées par l'accueil ; le site ne lit rien hors de
ce dossier.

En local, servir le dossier (un fichier ouvert directement ne charge ni les modules ni le modèle) :

```bash
cd software
tools/web.sh serve       # http://localhost:8000/ (accueil), /mockup/, /sim/, /manual/
```

## Maquette 3D (`mockup/`)

Les trois cartes du Raidmaster 403 (carte Waveshare ESP32-S3-Touch-LCD-3.5B, carte fille GPS et
magnétomètre, carte boutons) dans leur boîtier, assemblé ou éclaté, avec la trappe de la carte micro-SD et
le câble en démonstration. Le boîtier montré (cannelures, pare-chocs en TPU, liseré et lettrage de façade)
est une esquisse du modèle dessiné pour l'impression, encore à éprouver ; ses pièces en STEP sont dans
[`../enclosure/stepfiles/`](../enclosure/stepfiles/).

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/mockup/>**.

| Fichier | Rôle |
|---|---|
| `mockup/index.html`, `mockup/main.js`, `mockup/style.css` | page [three.js](https://threejs.org) (chargé depuis jsDelivr, à la version de l'importmap de `index.html`) |
| `mockup/raidmaster.glb` | modèle : pièces, positions éclatées et textes ; le maillage de la carte Waveshare vient du modèle 3D publié par Waveshare (voir [`../enclosure/README.md`](../enclosure/README.md)) |

Paramètres d'adresse : `?ouverture=0` (assemblé) à `100` (éclaté), `?vue=face`, `?image=105` (fige la
démonstration sur une image), `?gps=puck`, `?gps=interne` ou `?gps=bouchon`, `?lang=fr` ou `?lang=en`.

Connectique GPS en deux versions, choisies au soudage de la carte fille : **puck externe** (module GPS et
boussole posé sur le tableau de bord, branché au flanc gauche) ou **GPS interne** (antenne en SMA au flanc
gauche), plus un **bouchon** qui ferme le flanc gauche quand rien n'y est branché. Les trois sont dans le modèle ;
le bloc « GPS » de la page en montre une.

Les textes des pièces sont en français dans le modèle ; leur traduction anglaise est dans `main.js`
(`MODELE_EN`), le français s'affiche pour un texte non traduit.

## Simulateur (`sim/`)

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/sim/>** (en local : `http://localhost:8000/sim/`).
Le firmware compilé en WebAssembly tourne dans ce même modèle (`../mockup/raidmaster.glb`) : écran vivant, capuchons
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
la position des capuchons à l'écran et appuie sur les boutons. L'interface du boîtier reste en français.

## Manuel (`manual/`)

**En ligne : <https://docteurzoidberg.github.io/raidmaster-403/manual/>**. Page d'attente pour l'instant : le
manuel d'utilisation (mise en route, boutons, écrans, menus, calibration, mode nuit, recharge) sera écrit avec le
premier boîtier utilisable, et tenu à jour à chaque changement du firmware qui touche à l'utilisation.
