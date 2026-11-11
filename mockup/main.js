// Maquette 3D du Raidmaster 403 : modèle raidmaster.glb (voir README.md), assemblé ou éclaté.
// Les pièces, leur déplacement à l'éclaté, la démonstration et les textes viennent du modèle (maquette.json) :
// rien à régler ici, sauf la traduction anglaise (TEXTES, MODELE_EN) quand un texte du modèle change.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";

// Axes de vue, dans le repère de la scène : X à droite, Y en haut, façade vers +Z.
const VUES = { iso: [0.95, 0.5, 1], face: [0, 0.02, 1], cote: [1, 0.14, 0.04], dos: [-0.8, 0.42, -1] };

const SURBRILLANCE = new THREE.Color(0x3a2a0c);
const RETARD = 0.3;      // part de la course pendant laquelle les pièces proches attendent les plus lointaines
const FOV = 26;
const MARGE = 0.9;       // part de l'image occupée par l'objet
const MM = 1000;         // modèle en mètres, scène en millimètres

// --- langues : français (textes du modèle) et anglais

const TEXTES = {
  fr: {
    titre: "Raidmaster éclaté",
    description: "Maquette 3D du Raidmaster 403 : la carte Waveshare, la carte fille et la carte boutons dans leur boîtier, assemblé ou éclaté.",
    chapeau: "Les trois cartes du tripmaster dans leur boîtier, assemblé ou éclaté, avec la trappe de la carte micro-SD et le câble en démonstration.",
    langue: "Langue",
    github: "Code source sur GitHub",
    scene: "Maquette 3D : glisser pour tourner, molette ou pincement pour zoomer",
    chargement: "Chargement du modèle…",
    maquette: "Maquette",
    aide: "Glisser pour tourner, molette pour zoomer. Survoler une pièce pour la nommer.",
    aideTactile: "Glisser pour tourner, pincer pour zoomer. Toucher une pièce pour la nommer.",
    commandes: "Commandes de la maquette",
    ouverture: "Ouverture",
    assemble: "Assemblé",
    eclate: "Éclaté",
    curseur: "Ouverture de l'éclaté, en pour cent",
    demonstration: "Démonstration",
    rejouer: "Rejouer",
    vue: "Vue",
    vue_iso: "Trois quarts",
    vue_face: "Face",
    vue_cote: "Côté",
    vue_dos: "Dos",
    pieces: "Pièces",
    webgl: "Cette page a besoin de WebGL, qui n'est pas disponible dans ce navigateur.",
    erreur: "Le modèle n'a pas pu être chargé.",
    mo: (n) => `${n.toFixed(1).replace(".", ",")} Mo`,
    date: (d) => `Modèle du ${d.split("-").reverse().join("/")}.`,
  },
  en: {
    titre: "Raidmaster exploded view",
    description: "3D model of the Raidmaster 403: the Waveshare board, the daughter board and the button board in their enclosure, assembled or exploded.",
    chapeau: "The tripmaster's three boards in their enclosure, assembled or exploded, with the micro-SD card door and the cable shown in a demonstration.",
    langue: "Language",
    github: "Source code on GitHub",
    scene: "3D model: drag to rotate, scroll or pinch to zoom",
    chargement: "Loading the model…",
    maquette: "Model",
    aide: "Drag to rotate, scroll to zoom. Hover over a part to name it.",
    aideTactile: "Drag to rotate, pinch to zoom. Tap a part to name it.",
    commandes: "Model controls",
    ouverture: "Explode",
    assemble: "Assembled",
    eclate: "Exploded",
    curseur: "Explosion amount, in percent",
    demonstration: "Demonstration",
    rejouer: "Replay",
    vue: "View",
    vue_iso: "Three-quarter",
    vue_face: "Front",
    vue_cote: "Side",
    vue_dos: "Back",
    pieces: "Parts",
    webgl: "This page needs WebGL, which is not available in this browser.",
    erreur: "The model could not be loaded.",
    mo: (n) => `${n.toFixed(1)} MB`,
    date: (d) => `Model dated ${d.replaceAll("-", "\u2011")}.`,   // tirets insécables
  },
};

// Textes du modèle en anglais : groupes par identifiant, étapes de la démonstration par nom français.
// Un texte absent ici s'affiche en français.
const MODELE_EN = {
  note: "Boards exported from KiCad, Waveshare board from the manufacturer's 3D model. The enclosure is a 123 × 94 × 32 mm placeholder shell, not designed yet.",
  visserie: ["Hardware", "4 M2 standoffs (5.1 mm) and 4 M2 screws for the daughter board, 4 recessed M3 × 10 screws that close the enclosure from the back."],
  groupes: {
    capuchons: ["Button caps", "5 printed caps, Ø 14.5 mm, on 12 × 12 push buttons, held by a collar behind the front panel."],
    facade: ["Front panel", "PETG, 123 × 94 mm, chamfered corners, two facets around the edge, sides cut down to screen height, 1.5 mm lip in front of the glass, translucent LED windows. Placeholder part."],
    carte_boutons: ["Button board", "108 × 20 mm board snapped off the daughter board, bottom corners cut off: 5 push buttons at an 18.5 mm pitch and 2 LEDs on the front, 2 PCF8574 expanders on the back, linked to the daughter board by a 6-way flat flexible cable. KiCad export."],
    waveshare: ["Waveshare 3.5B", "ESP32-S3-Touch-LCD-3.5B: 3.5-inch touch screen, 92.4 × 61 mm glass, 4 M2 standoffs on the back. Manufacturer's model."],
    carte_fille: ["Daughter board", "4-layer 78 × 54 mm board plugged into the back of the Waveshare: NEO-M9N GPS, U.FL antenna socket, GY-271 compass soldered flat. KiCad export."],
    batterie: ["Battery", "5000 mAh 605090 Li-ion pouch cell, 90 × 50 × 6 mm, lying at the bottom."],
    fond: ["Back shell", "PETG, sides cut down to screen height, back tapered around the battery, solid rear face. The right side is slotted down to the joint to take the USB-C sleeve. Placeholder part."],
    acces: ["Top access", "In the top wall: sliding micro-SD card door (it slides backwards, clips in with a latch tab and comes off), PWR button plunger (Ø 9, held by an inner collar), two pinholes for RESET and BOOT. Placeholder parts."],
    manchon: ["USB-C sleeve", "Separate part slipped onto the board's USB-C socket before the back shell goes on: it surrounds the socket on all four sides, guides the plug through a sloped channel and closes the side, with no opening in the enclosure. The edges of the slot in the back shell hold it. Placeholder part."],
    cable: ["USB-C cable", "Test plug the size of a standard plug (12.35 × 6.5 mm overmould, 22 mm long), plugged into the original socket through the sleeve to check access. Untick to remove it."],
  },
  etapes: {
    "Trappe micro-SD": ["Micro-SD door", "It slides backwards, unclips and comes off."],
    "Carte micro-SD": ["Micro-SD card", "It comes out through the top of the enclosure."],
    "Câble USB-C": ["USB-C cable", "The plug goes through the sleeve into the board's socket."],
  },
};

const DRAPEAUX = {
  fr: '<svg viewBox="0 0 3 2"><path fill="#002654" d="M0 0h1v2H0z"/><path fill="#fff" d="M1 0h1v2H1z"/><path fill="#ce1126" d="M2 0h1v2H2z"/></svg>',
  en: '<svg viewBox="0 0 60 30" preserveAspectRatio="none"><clipPath id="uj"><path d="M30 15h30v15zv15H0zH0V0zV0h30z"/></clipPath><path fill="#012169" d="M0 0v30h60V0z"/><path stroke="#fff" stroke-width="6" d="M0 0l60 30m0-30L0 30"/><path stroke="#c8102e" stroke-width="4" clip-path="url(#uj)" d="M0 0l60 30m0-30L0 30"/><path stroke="#fff" stroke-width="10" d="M30 0v30M0 15h60"/><path stroke="#c8102e" stroke-width="6" d="M30 0v30M0 15h60"/></svg>',
};

// ?lang=fr|en, sinon le dernier choix, sinon la langue du navigateur.
function langueInitiale() {
  const demandee = new URLSearchParams(location.search).get("lang");
  if (demandee in TEXTES) return demandee;
  try {
    const gardee = localStorage.getItem("raidmaster-langue");
    if (gardee in TEXTES) return gardee;
  } catch { /* stockage indisponible */ }
  return (navigator.language || "fr").toLowerCase().startsWith("fr") ? "fr" : "en";
}
let langue = langueInitiale();
const t = (cle) => TEXTES[langue][cle];

const $ = (id) => document.getElementById(id);
const calme = matchMedia("(prefers-reduced-motion: reduce)").matches;
const params = new URLSearchParams(location.search);
const zone = $("scene");
const canvas = $("vue");
const attente = $("attente");
let etatAttente = "chargement";   // message affiché tant que le modèle n'est pas là : chargement, erreur

const clamp01 = (v) => Math.min(Math.max(v, 0), 1);
const adoucit = (t) => t * t * (3 - 2 * t);

let renderer;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "high-performance" });
} catch {
  etatAttente = "webgl";
  attente.textContent = t("webgl");
  throw new Error("WebGL indisponible");
}
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.toneMapping = THREE.NeutralToneMapping;
renderer.toneMappingExposure = 1.0;

const scene = new THREE.Scene();
scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
scene.environmentIntensity = 0.7;
scene.add(new THREE.HemisphereLight(0xffffff, 0x33373c, 0.9));
const cle = new THREE.DirectionalLight(0xffffff, 1.8);
cle.position.set(200, 260, 240);
const contre = new THREE.DirectionalLight(0xffffff, 0.9);
contre.position.set(-180, 90, -260);
scene.add(cle, contre);

const camera = new THREE.PerspectiveCamera(FOV, 1, 5, 8000);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.09;
controls.minDistance = 40;
controls.maxDistance = 3000;

let sale = true;          // l'image est à refaire
let pieces = [];
let cibles = [];          // maillages que le pointeur peut désigner
let modele = null;        // textes du modèle (en français) : note, date, groupes, visserie
let groupes = {};         // identifiant → [nom, détail] dans la langue de la page, dans l'ordre de la liste
let visserie = ["Visserie", ""];
let ouverture = 0;
const visibles = {};
let visserieVisible = true;
let demo = null;          // démonstration animée du modèle : { ips, fin, pistes, etapes }
let etape = null;         // étape de la démonstration en cours : [nom, détail] en français
let attenteDemo = 0;      // minuterie entre deux temps de la démonstration

// --- animations

const tweens = new Map();
function anime(nom, duree, pas, apres) {
  if (calme) {
    pas(1);
    sale = true;
    apres?.();
    return;
  }
  tweens.set(nom, { debut: performance.now(), duree, pas, apres });
}

// --- cadrage

// Part d'une pièce sur sa course, pour une ouverture k.
const avance = (p, k) => adoucit(clamp01((k - p.retard) / (1 - p.retard)));

// Cible et distance pour que toutes les pièces tiennent dans l'image, pour un axe de vue et une ouverture.
function cadrage(axe, k) {
  const droite = new THREE.Vector3().crossVectors(camera.up, axe);
  if (droite.lengthSq() < 1e-6) droite.set(1, 0, 0);
  droite.normalize();
  const haut = new THREE.Vector3().crossVectors(axe, droite);
  const v = new THREE.Vector3();
  const points = pieces.flatMap((p) => {
    const f = avance(p, k);
    return p.coins.map((c) => {
      v.copy(p.offset).multiplyScalar(f).add(p.demo).add(c);
      return [v.dot(droite), v.dot(haut), v.dot(axe)];
    });
  });
  const milieu = (i) => {
    const valeurs = points.map((q) => q[i]);
    return (Math.min(...valeurs) + Math.max(...valeurs)) / 2;
  };
  const [cx, cy, cz] = [milieu(0), milieu(1), milieu(2)];
  const tv = Math.tan(THREE.MathUtils.degToRad(FOV) / 2) * MARGE;
  const th = tv * camera.aspect;
  const recul = Math.max(...points.map(([x, y, z]) => z + Math.max(Math.abs(x - cx) / th, Math.abs(y - cy) / tv)));
  const cible = droite.multiplyScalar(cx).addScaledVector(haut, cy).addScaledVector(axe, cz);
  return { cible, distance: recul - cz };
}

// Suit un changement d'ouverture ou de format sans perdre le point de vue ni le zoom choisis.
function recadre(faitChanger) {
  const axe = camera.position.clone().sub(controls.target).normalize();
  const avant = cadrage(axe, faitChanger.avant);
  faitChanger.applique?.();
  const apres = cadrage(axe, ouverture);
  const distance = camera.position.distanceTo(controls.target) * (apres.distance / avant.distance);
  controls.target.add(apres.cible.sub(avant.cible));
  camera.position.copy(controls.target).addScaledVector(axe, distance);
}

function placeVue(nom, immediat) {
  const fin = new THREE.Vector3(...VUES[nom]).normalize();
  const axe = camera.position.clone().sub(controls.target);
  const d0 = axe.length();
  const debut = axe.normalize();
  const t0 = controls.target.clone();
  const pas = (u) => {
    const e = adoucit(u);
    const { cible, distance } = cadrage(fin, ouverture);
    controls.target.lerpVectors(t0, cible, e);
    const dir = debut.clone().lerp(fin, e).normalize();
    camera.position.copy(controls.target).addScaledVector(dir, THREE.MathUtils.lerp(d0, distance, e));
  };
  if (immediat) pas(1);
  else anime("vue", 700, pas);
  for (const b of $("vues").children) b.setAttribute("aria-pressed", String(b.dataset.vue === nom));
  sale = true;
}

// --- ouverture

// Place chaque pièce : sa course d'éclaté, plus son décalage de démonstration.
function pose() {
  for (const p of pieces) p.objet.position.copy(p.offset).multiplyScalar(avance(p, ouverture)).add(p.demo).divideScalar(MM);
}

function ouvre(k) {
  const avant = ouverture;
  ouverture = k;
  pose();
  recadre({ avant });
  $("ouverture").value = Math.round(k * 100);
  $("assemble").setAttribute("aria-pressed", String(k === 0));
  $("eclate").setAttribute("aria-pressed", String(k === 1));
  sale = true;
}

function ouvreVers(k, duree = 1400) {
  const depart = ouverture;
  anime("ouverture", duree * Math.max(Math.abs(k - depart), 0.25), (u) => ouvre(THREE.MathUtils.lerp(depart, k, u)));
}

// --- démonstration : l'animation préparée dans la scène Blender (maquette.json, « animation »)

// Décalage d'une pièce à l'image f, entre ses clés [image, vecteur], rangé dans v.
function decalage(p, f, v) {
  const c = p.cles;
  let i = 0;
  while (i < c.length - 1 && f >= c[i + 1][0]) i++;
  if (i === c.length - 1 || f <= c[i][0]) return v.copy(c[i][1]);
  return v.lerpVectors(c[i][1], c[i + 1][1], adoucit((f - c[i][0]) / (c[i + 1][0] - c[i][0])));
}

// Règle le décalage de chaque pièce animée ; la caméra suit, comme pour l'éclaté.
function poseDemo(regle) {
  const animees = pieces.filter((p) => p.cles);
  recadre({ avant: ouverture, applique: () => animees.forEach(regle) });
  pose();
  sale = true;
}

const demoEnPlace = () => pieces.some((p) => p.cles && p.demo.lengthSq() > 0);

function arreteDemo() {
  tweens.delete("demo");
  clearTimeout(attenteDemo);
  etape = null;
  $("demo").setAttribute("aria-pressed", "false");
  if (demoEnPlace()) poseDemo((p) => p.demo.set(0, 0, 0));
  montreFiche();
}

function joueDemo() {
  if (!demo) return;
  if (calme) {        // sans mouvement : le bouton montre la fin de la démonstration, puis la pose assemblée
    if (demoEnPlace()) arreteDemo();
    else poseDemo((p) => decalage(p, demo.fin, p.demo));
    return;
  }
  arreteDemo();
  const lance = () => {
    $("demo").setAttribute("aria-pressed", "true");
    // 1. de la pose assemblée à la première image : la fiche se débranche
    anime("demo", 700, (u) => poseDemo((p) => decalage(p, 1, p.demo).multiplyScalar(adoucit(u))), () => {
      // 2. l'animation, à sa cadence
      anime("demo", ((demo.fin - 1) / demo.ips) * 1000, (u) => {
        const f = 1 + u * (demo.fin - 1);
        poseDemo((p) => decalage(p, f, p.demo));
        const e = demo.etapes.find((q) => f >= q.de && f <= q.a);
        if (e?.nom !== etape?.[0]) {
          etape = e ? [e.nom, e.detail] : null;
          montreFiche();
        }
      }, () => {
        // 3. un temps d'arrêt, puis retour à la pose assemblée
        attenteDemo = setTimeout(() => {
          const fin = new Map(pieces.filter((p) => p.cles).map((p) => [p, p.demo.clone()]));
          anime("demo", 1200, (u) => poseDemo((p) => p.demo.copy(fin.get(p)).multiplyScalar(1 - adoucit(u))),
            arreteDemo);
        }, 2500);
      });
    });
  };
  if (ouverture > 0) {
    const depart = ouverture;
    anime("ouverture", 1400 * Math.max(depart, 0.25), (u) => ouvre(THREE.MathUtils.lerp(depart, 0, u)), lance);
  } else lance();
}

// --- pièces

function appliqueVisibilite() {
  for (const p of pieces) p.objet.visible = visibles[p.groupe] !== false && (!p.vis || visserieVisible);
  cibles = pieces.filter((p) => p.objet.visible).flatMap((p) => p.maillages);
  sale = true;
}

let survole = null;       // groupe sous le pointeur ou dans la liste
let epingle = null;       // groupe choisi au toucher ou au clic

function montreFiche() {
  const g = survole ?? epingle;
  const [nom, detail] = g === "visserie" ? visserie
    : (groupes[g] ?? (etape && traduitEtape(etape)) ?? [t("maquette"), ficheParDefaut()]);
  $("fiche-nom").textContent = nom;
  $("fiche-detail").textContent = detail;
  for (const l of $("liste").children) l.classList.toggle("marque", l.dataset.groupe === g);
  for (const p of pieces) {
    const allume = g === "visserie" ? p.vis : p.groupe === g && !p.vis;
    for (const m of p.materiaux) {
      m.emissive.copy(m.userData.emissive);
      if (allume) m.emissive.add(SURBRILLANCE);
    }
  }
  sale = true;
}
const tactile = matchMedia("(hover: none)").matches;
const ficheParDefaut = () => t(tactile ? "aideTactile" : "aide");
const traduitEtape = ([nom, detail]) => (langue === "en" && MODELE_EN.etapes[nom]) || [nom, detail];

function designe(groupe) {
  if (groupe === survole) return;
  survole = groupe;
  montreFiche();
}

function ligne(groupe, nom, coche) {
  const l = document.createElement("label");
  l.className = "piece";
  l.dataset.groupe = groupe;
  const c = document.createElement("input");
  c.type = "checkbox";
  c.id = `piece-${groupe}`;
  c.checked = true;
  c.addEventListener("change", () => coche(c.checked));
  const n = document.createElement("span");
  n.textContent = nom;
  l.append(c, n);
  l.addEventListener("pointerenter", () => designe(groupe));
  l.addEventListener("pointerleave", () => designe(null));
  c.addEventListener("focus", () => designe(groupe));
  c.addEventListener("blur", () => designe(null));
  return l;
}

function construitListe() {
  const liste = $("liste");
  for (const [g, [nom]] of Object.entries(groupes)) {
    liste.append(ligne(g, nom, (v) => {
      visibles[g] = v;
      appliqueVisibilite();
    }));
  }
  const l = ligne("visserie", visserie[0], (v) => {
    visserieVisible = v;
    appliqueVisibilite();
  });
  l.classList.add("visserie");
  liste.append(l);
}

// --- pointeur

const rayon = new THREE.Raycaster();
const pointeur = new THREE.Vector2();
let aTester = false;
let appui = null;

function placePointeur(e) {
  const r = canvas.getBoundingClientRect();
  pointeur.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
}

function groupeSousPointeur() {
  rayon.setFromCamera(pointeur, camera);
  const hit = rayon.intersectObjects(cibles, false)[0];
  return hit ? hit.object.userData.groupe : null;
}

canvas.addEventListener("pointermove", (e) => {
  if (e.pointerType === "mouse" && !e.buttons) {
    placePointeur(e);
    aTester = true;
  }
});
canvas.addEventListener("pointerleave", () => {
  aTester = false;
  designe(null);
});
canvas.addEventListener("pointerdown", (e) => {
  appui = [e.clientX, e.clientY];
});
canvas.addEventListener("pointerup", (e) => {
  if (!appui || Math.hypot(e.clientX - appui[0], e.clientY - appui[1]) > 6) return;
  placePointeur(e);
  const g = groupeSousPointeur();
  epingle = g === epingle ? null : g;
  montreFiche();
});

// --- chargement

function prepare(gltf) {
  const racine = gltf.scene;
  racine.scale.setScalar(MM);
  scene.add(racine);

  const infos = JSON.parse(racine.userData.maquette ?? "{}");
  modele = infos;
  traduitModele();

  racine.traverse((o) => {
    const e = o.userData.eclate;
    if (!e) return;
    // Blender : Y vers l'arrière, Z en haut ; scène : Y en haut, façade vers +Z
    const p = {
      objet: o, nom: o.name, groupe: o.userData.groupe, vis: Boolean(o.userData.vis),
      offset: new THREE.Vector3(e[0], e[2], -e[1]), demo: new THREE.Vector3(), maillages: [], materiaux: [],
    };
    o.traverse((m) => {
      if (!m.isMesh) return;
      m.material = m.material.clone();
      m.material.userData.emissive = m.material.emissive.clone();
      m.userData.groupe = p.vis ? "visserie" : p.groupe;
      p.materiaux.push(m.material);
      p.maillages.push(m);
    });
    pieces.push(p);
  });

  // Démonstration : clés [image, [dx, dy, dz]] en mm, dans le repère de Blender ; l'image 0 est la pose assemblée.
  demo = infos.animation ?? null;
  if (demo) {
    for (const piste of demo.pistes) {
      const p = pieces.find((q) => q.nom === piste.piece);
      if (p) p.cles = [[0, new THREE.Vector3()], ...piste.cles.map(([f, [x, y, z]]) => [f, new THREE.Vector3(x, z, -y)])];
    }
    demo.etapes ??= [];
    $("bloc-demo").hidden = false;
  }

  const loin = Math.max(...pieces.map((p) => p.offset.length()));
  for (const p of pieces) p.retard = RETARD * (1 - p.offset.length() / loin);

  // Coins des pièces pour le cadrage, dans le repère de la scène.
  racine.updateMatrixWorld(true);
  for (const p of pieces) {
    const b = new THREE.Box3().setFromObject(p.objet);
    p.coins = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => new THREE.Vector3(
      (i & 1 ? b.max : b.min).x, (i & 2 ? b.max : b.min).y, (i & 4 ? b.max : b.min).z,
    ));
  }
  camera.position.set(0, 0, 1);

  // Arêtes des pièces imprimées, pour lire les formes sombres.
  const clair = new THREE.LineBasicMaterial({ color: 0xa8adb3, transparent: true, opacity: 0.34 });
  const fonce = new THREE.LineBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.4 });
  for (const p of pieces) {
    if (!p.objet.userData.aretes) continue;
    for (const m of p.maillages) {
      const c = m.material.color;
      m.add(new THREE.LineSegments(new THREE.EdgesGeometry(m.geometry, 28), c.r + c.g + c.b < 0.2 ? clair : fonce));
    }
  }

  construitListe();
  appliqueVisibilite();
  placeVue(params.has("vue") && VUES[params.get("vue")] ? params.get("vue") : "iso", true);
  attente.hidden = true;

  // ?ouverture=40 ouvre directement à 40 %, ?image=120 fige la démonstration à cette image ; sinon la
  // démonstration se joue une fois, peu après le chargement (sans démonstration : l'objet s'ouvre tout seul).
  const demande = Number(params.get("ouverture"));
  if (params.has("ouverture")) ouvre(clamp01(demande / 100));
  else if (demo && params.has("image")) {
    ouvre(0);
    poseDemo((p) => decalage(p, Number(params.get("image")), p.demo));
  } else if (demo) {
    ouvre(0);
    if (!calme) setTimeout(joueDemo, 1200);
  } else if (calme) ouvre(1);
  else {
    ouvre(0);
    setTimeout(() => ouvreVers(1, 2200), 600);
  }
}

// --- langue de la page

// Textes du modèle dans la langue de la page (le français du modèle sert de repli).
function traduitModele() {
  if (!modele) return;
  const en = langue === "en";
  groupes = Object.fromEntries((modele.groupes ?? []).map((g) =>
    [g.id, (en && MODELE_EN.groupes[g.id]) || [g.nom, g.detail]]));
  visserie = (en && MODELE_EN.visserie) || modele.visserie || visserie;
  const note = (en && MODELE_EN.note) || modele.note;
  $("note").textContent = [note, modele.date && t("date")(modele.date)].filter(Boolean).join(" ");
  for (const l of $("liste").children) {
    const g = l.dataset.groupe;
    l.querySelector("span").textContent = g === "visserie" ? visserie[0] : groupes[g]?.[0] ?? g;
  }
}

function changeLangue(l) {
  langue = l in TEXTES ? l : "fr";
  document.documentElement.lang = langue;
  document.title = t("titre");
  const description = document.querySelector('meta[name="description"]');   // absente de la page en un seul fichier
  if (description) description.content = t("description");
  for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t(e.dataset.i18n);
  if (!attente.hidden) attente.textContent = t(etatAttente);
  for (const e of document.querySelectorAll("[data-i18n-aria]")) e.setAttribute("aria-label", t(e.dataset.i18nAria));
  $("langue").value = langue;
  $("drapeau").innerHTML = DRAPEAUX[langue];
  try { localStorage.setItem("raidmaster-langue", langue); } catch { /* stockage indisponible */ }
  traduitModele();
  if (pieces.length) montreFiche();
  else $("fiche-detail").textContent = ficheParDefaut();
}

$("langue").addEventListener("change", (e) => changeLangue(e.target.value));
changeLangue(langue);

// --- boucle : rien n'est dessiné tant que rien ne change

function boucle(now) {
  requestAnimationFrame(boucle);
  if (!pieces.length) return;
  for (const [nom, t] of tweens) {
    const u = clamp01((now - t.debut) / t.duree);
    t.pas(u);
    if (u === 1 && tweens.get(nom) === t) {
      tweens.delete(nom);
      t.apres?.();
    }
    sale = true;
  }
  if (controls.update()) sale = true;
  if (aTester) {
    aTester = false;
    designe(groupeSousPointeur());
  }
  if (sale) {
    sale = false;
    renderer.render(scene, camera);
  }
}

new ResizeObserver(() => {
  const w = zone.clientWidth;
  const h = zone.clientHeight;
  if (!w || !h) return;
  renderer.setSize(w, h, false);
  const applique = () => {
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  };
  if (pieces.length) recadre({ avant: ouverture, applique });
  else applique();
  sale = true;
}).observe(zone);

$("assemble").addEventListener("click", () => {
  arreteDemo();
  ouvreVers(0);
});
$("eclate").addEventListener("click", () => {
  arreteDemo();
  ouvreVers(1);
});
$("ouverture").addEventListener("input", (e) => {
  arreteDemo();
  tweens.delete("ouverture");
  ouvre(e.target.valueAsNumber / 100);
});
$("demo").addEventListener("click", joueDemo);
for (const b of $("vues").children) b.addEventListener("click", () => placeVue(b.dataset.vue));

requestAnimationFrame(boucle);

new GLTFLoader().load(
  "raidmaster.glb",
  prepare,
  (e) => {
    attente.textContent = e.lengthComputable
      ? `${t("chargement")} ${Math.round((e.loaded / e.total) * 100)} %`
      : `${t("chargement")} ${t("mo")(e.loaded / 1e6)}`;
  },
  (erreur) => {
    console.error(erreur);
    etatAttente = "erreur";
    attente.textContent = t("erreur");
  },
);
