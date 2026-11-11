# Maquette 3D

Les trois cartes du Raidmaster 403 (carte Waveshare ESP32-S3-Touch-LCD-3.5B, carte fille GPS et
magnétomètre, carte boutons) dans un boîtier de principe, assemblé ou éclaté, avec la trappe de la carte
micro-SD et le câble en démonstration. Boîtier **de principe** : il sert à vérifier l'encombrement, ce
n'est pas le boîtier définitif.

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
démonstration sur une image), `?lang=fr` ou `?lang=en`.

Page en français et en anglais (menu du haut, avec le lien vers ce dépôt) : la langue vient de `?lang=`,
sinon du dernier choix (gardé dans le navigateur), sinon de la langue du navigateur. Les textes des pièces
sont en français dans le modèle ; leur traduction anglaise est dans `main.js` (`MODELE_EN`), le français
s'affiche pour un texte non traduit.
