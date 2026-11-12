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

Page en français et en anglais (menu du haut, avec le lien vers ce dépôt) : la langue vient de `?lang=`,
sinon du dernier choix (gardé dans le navigateur), sinon de la langue du navigateur. Les textes des pièces
sont en français dans le modèle ; leur traduction anglaise est dans `main.js` (`MODELE_EN`), le français
s'affiche pour un texte non traduit.
