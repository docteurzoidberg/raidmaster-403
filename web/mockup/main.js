// Maquette 3D du Raidmaster 403 : modèle raidmaster.glb (voir README.md), assemblé ou éclaté.
// Les pièces, leur déplacement à l'éclaté, la démonstration et les textes viennent du modèle (réglages de la maquette) :
// rien à régler ici, sauf la traduction anglaise (TEXTS, MODEL_EN) quand un texte du modèle change.
// Les données du modèle (userData écrits par scene.py) gardent leurs noms français : maquette, eclate, groupe, vis,
// version, aretes, animation { ips, fin, pistes, etapes }, groupes [{ id, nom, detail }], visserie, note, date, gps.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";
import { applyLanguage, initialLanguage, onLanguageChange } from "../common/common.js";

// Axes de vue, dans le repère de la scène : X à droite, Y en haut, façade vers +Z. Les clés sont les valeurs de
// ?vue= (adresses publiées, gardées en français).
const VIEWS = { iso: [0.95, 0.5, 1], face: [0, 0.02, 1], cote: [1, 0.14, 0.04], dos: [-0.8, 0.42, -1] };

const HIGHLIGHT = new THREE.Color(0x3a2a0c);
const DELAY = 0.3;       // part de la course pendant laquelle les pièces proches attendent les plus lointaines
const FOV = 26;
const MARGIN = 0.9;      // part de l'image occupée par l'objet
const MM = 1000;         // modèle en mètres, scène en millimètres
const HARDWARE = "hardware";   // ligne de la visserie dans la liste (pièces marquées « vis » dans le modèle)

// --- langues : français (textes du modèle) et anglais

const TEXTS = {
  fr: {
    title: "Raidmaster éclaté",
    description: "Maquette 3D du Raidmaster 403 : la carte Waveshare, la carte fille et la carte boutons dans leur boîtier, assemblé ou éclaté.",
    lede: "Les trois cartes du tripmaster dans leur boîtier, assemblé ou éclaté, avec la trappe de la carte micro-SD et le câble en démonstration.",
    scene: "Maquette 3D : glisser pour tourner, molette ou pincement pour zoomer",
    loading: "Chargement du modèle…",
    model: "Maquette",
    help: "Glisser pour tourner, molette pour zoomer. Survoler une pièce pour la nommer.",
    helpTouch: "Glisser pour tourner, pincer pour zoomer. Toucher une pièce pour la nommer.",
    controls: "Commandes de la maquette",
    explode: "Ouverture",
    assembled: "Assemblé",
    exploded: "Éclaté",
    slider: "Ouverture de l'éclaté, en pour cent",
    demo: "Démonstration",
    replay: "Rejouer",
    view: "Vue",
    viewIso: "Trois quarts",
    viewFront: "Face",
    viewSide: "Côté",
    viewBack: "Dos",
    gps: "GPS",
    gpsPuck: "Puck externe",
    gpsInternal: "GPS interne",
    gpsPlug: "Bouchon",
    parts: "Pièces",
    webgl: "Cette page a besoin de WebGL, qui n'est pas disponible dans ce navigateur.",
    error: "Le modèle n'a pas pu être chargé.",
    mb: (n) => `${n.toFixed(1).replace(".", ",")} Mo`,
    date: (d) => `Modèle du ${d.split("-").reverse().join("/")}.`,
  },
  en: {
    title: "Raidmaster exploded view",
    description: "3D model of the Raidmaster 403: the Waveshare board, the daughter board and the button board in their enclosure, assembled or exploded.",
    lede: "The tripmaster's three boards in their enclosure, assembled or exploded, with the micro-SD card door and the cable shown in a demonstration.",
    scene: "3D model: drag to rotate, scroll or pinch to zoom",
    loading: "Loading the model…",
    model: "Model",
    help: "Drag to rotate, scroll to zoom. Hover over a part to name it.",
    helpTouch: "Drag to rotate, pinch to zoom. Tap a part to name it.",
    controls: "Model controls",
    explode: "Explode",
    assembled: "Assembled",
    exploded: "Exploded",
    slider: "Explosion amount, in percent",
    demo: "Demonstration",
    replay: "Replay",
    view: "View",
    viewIso: "Three-quarter",
    viewFront: "Front",
    viewSide: "Side",
    viewBack: "Back",
    gps: "GPS",
    gpsPuck: "External puck",
    gpsInternal: "Built-in GPS",
    gpsPlug: "Blanking plug",
    parts: "Parts",
    webgl: "This page needs WebGL, which is not available in this browser.",
    error: "The model could not be loaded.",
    mb: (n) => `${n.toFixed(1)} MB`,
    date: (d) => `Model dated ${d.replaceAll("-", "‑")}.`,   // tirets insécables
  },
};

// Textes du modèle en anglais : groupes par identifiant (ceux du modèle), étapes de la démonstration par nom
// français. Un texte absent ici s'affiche en français.
const MODEL_EN = {
  note: "Boards exported from KiCad, Waveshare board from the manufacturer's 3D model. 123 × 94 × 33 mm enclosure, 37 at the battery cover hump (127 × 97 × 39 with the bumpers): sketch of the model designed for printing, still to be tested.",
  hardware: ["Hardware", "4 M2 standoffs (5.1 mm), 4 M2 × 12 screws fitted from the back that hold the daughter board and the Waveshare to the back shell, 4 M2 × 6 screws for the button board, 4 recessed M3 × 10 screws that close the enclosure, 4 M2 × 6 screws for the battery cover, all stainless socket head cap screws; brass heat-set inserts: 6 M3 (4 in the front panel, 2 in the back for a mount) and 8 M2 (4 under the button board, 4 under the battery cover)."],
  groups: {
    capuchons: ["Button caps", "5 printed caps, Ø 14.5 mm, on 12 × 12 push buttons, held by a collar behind the front panel. Inlaid colour legends: A, B, menu, up, down."],
    facade: ["Front panel", "PETG, 123 × 94 mm, chamfered corners, two facets around the edge, sides cut down to screen height, grooves on top and bottom, translucent LED windows. Inlaid amber line around the screen and \"RAIDMASTER 403\" lettering (two-filament print). A locating lip fits into the back shell."],
    carte_boutons: ["Button board", "108 × 20 mm board snapped off the daughter board, bottom corners cut off: 5 push buttons at an 18.5 mm pitch and 2 LEDs on the front, 2 PCF8574 expanders on the back, linked to the daughter board by a 6-way flat flexible cable. KiCad export."],
    waveshare: ["Waveshare 3.5B", "ESP32-S3-Touch-LCD-3.5B: 3.5-inch touch screen, 92.4 × 61 mm glass, 4 M2 standoffs on the back. Manufacturer's model."],
    carte_fille: ["Daughter board", "4-layer 78 × 54 mm board plugged into the back of the Waveshare: GY-271 compass soldered flat, moved away from the left edge (trial); in the built-in GPS version, NEO-M9N GPS and U.FL antenna socket, moved to the Waveshare side near the left edge (trial). KiCad export."],
    batterie: ["Battery", "3000 mAh 103665 Li-ion cell (65 × 36 × 10 mm), lying at the bottom of a roomy pocket of the back shell and wedged with foam, behind a partition that separates it from the electronics."],
    fond: ["Back shell", "PETG, sides cut down to screen height, tapered back, grooves on top and bottom. A partition carries the boards and isolates the battery, which sits in a pocket closed by a screwed cover: a 4 mm hump sloping down to its edges, grooved like the enclosure. Two inserts in the back take a mount. The right side is slotted down to the joint to take the USB-C sleeve, the left side, on the flat of the waist, to take the GPS connector sleeve (puck or antenna)."],
    pare_chocs: ["Bumpers", "4 orange TPU corners, clamped between the front panel and the back shell (tab caught in the joint, tightened by the closing screws) and each hooked onto 3 ribs of the back shell, at both ends and in the corner: 1.8 mm proud of the enclosure and 2.5 mm in front of the front panel (ahead of the buttons), grip ribs on the lower sides."],
    acces: ["Top access", "In the top wall: sliding micro-SD card door, recessed 3 mm into the top (it slides backwards, clips in with a latch tab and comes off), PWR button plunger (Ø 9, inlaid red lightning bolt, held by an inner collar), two pinholes for RESET and BOOT."],
    puck: ["Puck connectors", "External puck version: drone NEO-M8N GPS and compass module sitting on the dashboard, plugged through a remade cable into a through-hole 6-pin JST-PH socket soldered on the left edge of the daughter board, on the Waveshare side (project pinout: VCC, TX, RX, SDA, SCL, GND). Separate sleeve in the left-side opening: almost straight 11.4 × 21.3 mm channel, as wide as possible for the fingers, chamfered only through the shell thickness, closed on the board side by a wall cut to the shape of the socket and extended by a collar around it, like the USB-C sleeve."],
    bouchon: ["Left-side blanking plug", "Solid sleeve that closes the left-side opening when no GPS connection leaves the enclosure (puck unplugged, or a board without GPS): same separate part as the other two sleeves, hollowed inside, without a hole."],
    antenne: ["Built-in GPS antenna", "Built-in GPS version: SMA bulkhead socket held by a separate sleeve in the left-side opening, U.FL pigtail from the SMA to the daughter board's U.FL socket, between the two boards. Active antenna under the windscreen."],
    manchon: ["USB-C sleeve", "Separate part slipped onto the board's USB-C socket before the back shell goes on: it surrounds the socket on all four sides, guides the plug through a sloped channel and closes the side, with no opening in the enclosure. The edges of the slot in the back shell hold it."],
    cable: ["USB-C cable", "Test plug the size of a standard plug (12.35 × 6.5 mm overmould, 22 mm long), plugged into the original socket through the sleeve to check access. Untick to remove it."],
  },
  steps: {
    "Trappe micro-SD": ["Micro-SD door", "It slides backwards, unclips and comes off."],
    "Carte micro-SD": ["Micro-SD card", "It comes out through the top of the enclosure."],
    "Câble USB-C": ["USB-C cable", "The plug goes through the sleeve into the board's socket."],
  },
};

let lang = initialLanguage();
const t = (key) => TEXTS[lang][key];

const $ = (id) => document.getElementById(id);
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;
const params = new URLSearchParams(location.search);
const zone = $("scene");
const canvas = $("view");
const waiting = $("waiting");
let waitingState = "loading";   // message affiché tant que le modèle n'est pas là : loading, error, webgl

const clamp01 = (v) => Math.min(Math.max(v, 0), 1);
const smooth = (t) => t * t * (3 - 2 * t);

let renderer;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "high-performance" });
} catch {
  waitingState = "webgl";
  waiting.textContent = t("webgl");
  throw new Error("WebGL indisponible");
}
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.toneMapping = THREE.NeutralToneMapping;
renderer.toneMappingExposure = 1.0;

const scene = new THREE.Scene();
scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
scene.environmentIntensity = 0.7;
scene.add(new THREE.HemisphereLight(0xffffff, 0x33373c, 0.9));
const keyLight = new THREE.DirectionalLight(0xffffff, 1.8);
keyLight.position.set(200, 260, 240);
const backLight = new THREE.DirectionalLight(0xffffff, 0.9);
backLight.position.set(-180, 90, -260);
scene.add(keyLight, backLight);

const camera = new THREE.PerspectiveCamera(FOV, 1, 5, 8000);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.09;
controls.minDistance = 40;
controls.maxDistance = 3000;

let dirty = true;         // l'image est à refaire
let parts = [];
let targets = [];         // maillages que le pointeur peut désigner
let model = null;         // textes du modèle (en français) : note, date, groupes, visserie
let groups = {};          // identifiant → [nom, détail] dans la langue de la page, dans l'ordre de la liste
let hardware = ["Visserie", ""];
let explode = 0;          // ouverture de l'éclaté, de 0 (assemblé) à 1
const shown = {};
let hardwareShown = true;
let gpsVersion = null;    // version de la connectique GPS montrée (puck, interne, bouchon) ; pièces sans version : toujours
let demo = null;          // démonstration animée du modèle : { fps, end, steps }
let step = null;          // étape de la démonstration en cours : [nom, détail] en français
let demoTimer = 0;        // minuterie entre deux temps de la démonstration

// --- animations

const tweens = new Map();
function animate(name, duration, step, then) {
  if (reducedMotion) {
    step(1);
    dirty = true;
    then?.();
    return;
  }
  tweens.set(name, { start: performance.now(), duration, step, then });
}

// --- cadrage

// Part d'une pièce sur sa course, pour une ouverture k.
const progress = (p, k) => smooth(clamp01((k - p.delay) / (1 - p.delay)));

// Cible et distance pour que toutes les pièces tiennent dans l'image, pour un axe de vue et une ouverture.
function framing(axis, k) {
  const right = new THREE.Vector3().crossVectors(camera.up, axis);
  if (right.lengthSq() < 1e-6) right.set(1, 0, 0);
  right.normalize();
  const up = new THREE.Vector3().crossVectors(axis, right);
  const v = new THREE.Vector3();
  const points = parts.flatMap((p) => {
    const f = progress(p, k);
    return p.corners.map((c) => {
      v.copy(p.offset).multiplyScalar(f).add(p.demo).add(c);
      return [v.dot(right), v.dot(up), v.dot(axis)];
    });
  });
  const middle = (i) => {
    const values = points.map((q) => q[i]);
    return (Math.min(...values) + Math.max(...values)) / 2;
  };
  const [cx, cy, cz] = [middle(0), middle(1), middle(2)];
  const tv = Math.tan(THREE.MathUtils.degToRad(FOV) / 2) * MARGIN;
  const th = tv * camera.aspect;
  const back = Math.max(...points.map(([x, y, z]) => z + Math.max(Math.abs(x - cx) / th, Math.abs(y - cy) / tv)));
  const target = right.multiplyScalar(cx).addScaledVector(up, cy).addScaledVector(axis, cz);
  return { target, distance: back - cz };
}

// Suit un changement d'ouverture ou de format sans perdre le point de vue ni le zoom choisis.
function reframe(change) {
  const axis = camera.position.clone().sub(controls.target).normalize();
  const before = framing(axis, change.before);
  change.apply?.();
  const after = framing(axis, explode);
  const distance = camera.position.distanceTo(controls.target) * (after.distance / before.distance);
  controls.target.add(after.target.sub(before.target));
  camera.position.copy(controls.target).addScaledVector(axis, distance);
}

function setView(name, immediate) {
  const end = new THREE.Vector3(...VIEWS[name]).normalize();
  const axis = camera.position.clone().sub(controls.target);
  const d0 = axis.length();
  const start = axis.normalize();
  const t0 = controls.target.clone();
  const step = (u) => {
    const e = smooth(u);
    const { target, distance } = framing(end, explode);
    controls.target.lerpVectors(t0, target, e);
    const dir = start.clone().lerp(end, e).normalize();
    camera.position.copy(controls.target).addScaledVector(dir, THREE.MathUtils.lerp(d0, distance, e));
  };
  if (immediate) step(1);
  else animate("view", 700, step);
  for (const b of $("views").children) b.setAttribute("aria-pressed", String(b.dataset.view === name));
  dirty = true;
}

// --- ouverture

// Place chaque pièce : sa course d'éclaté, plus son décalage de démonstration.
function place() {
  for (const p of parts) p.object.position.copy(p.offset).multiplyScalar(progress(p, explode)).add(p.demo).divideScalar(MM);
}

function setExplode(k) {
  const before = explode;
  explode = k;
  place();
  reframe({ before });
  $("explode").value = Math.round(k * 100);
  $("assembled").setAttribute("aria-pressed", String(k === 0));
  $("exploded").setAttribute("aria-pressed", String(k === 1));
  dirty = true;
}

function explodeTo(k, duration = 1400) {
  const from = explode;
  animate("explode", duration * Math.max(Math.abs(k - from), 0.25), (u) => setExplode(THREE.MathUtils.lerp(from, k, u)));
}

// --- démonstration : l'animation préparée dans la scène Blender (réglages de la maquette, « animation »)

// Décalage d'une pièce à l'image f, entre ses clés [image, vecteur], rangé dans v.
function demoOffset(p, f, v) {
  const k = p.keys;
  let i = 0;
  while (i < k.length - 1 && f >= k[i + 1][0]) i++;
  if (i === k.length - 1 || f <= k[i][0]) return v.copy(k[i][1]);
  return v.lerpVectors(k[i][1], k[i + 1][1], smooth((f - k[i][0]) / (k[i + 1][0] - k[i][0])));
}

// Règle le décalage de chaque pièce animée ; la caméra suit, comme pour l'éclaté.
function poseDemo(set) {
  const animated = parts.filter((p) => p.keys);
  reframe({ before: explode, apply: () => animated.forEach(set) });
  place();
  dirty = true;
}

const demoOut = () => parts.some((p) => p.keys && p.demo.lengthSq() > 0);

function stopDemo() {
  tweens.delete("demo");
  clearTimeout(demoTimer);
  step = null;
  $("demo").setAttribute("aria-pressed", "false");
  if (demoOut()) poseDemo((p) => p.demo.set(0, 0, 0));
  showCard();
}

function playDemo() {
  if (!demo) return;
  if (reducedMotion) {   // sans mouvement : le bouton montre la fin de la démonstration, puis la pose assemblée
    if (demoOut()) stopDemo();
    else poseDemo((p) => demoOffset(p, demo.end, p.demo));
    return;
  }
  stopDemo();
  const run = () => {
    $("demo").setAttribute("aria-pressed", "true");
    // 1. de la pose assemblée à la première image : la fiche se débranche
    animate("demo", 700, (u) => poseDemo((p) => demoOffset(p, 1, p.demo).multiplyScalar(smooth(u))), () => {
      // 2. l'animation, à sa cadence
      animate("demo", ((demo.end - 1) / demo.fps) * 1000, (u) => {
        const f = 1 + u * (demo.end - 1);
        poseDemo((p) => demoOffset(p, f, p.demo));
        const s = demo.steps.find((q) => f >= q.from && f <= q.to);
        if (s?.name !== step?.[0]) {
          step = s ? [s.name, s.detail] : null;
          showCard();
        }
      }, () => {
        // 3. un temps d'arrêt, puis retour à la pose assemblée
        demoTimer = setTimeout(() => {
          const last = new Map(parts.filter((p) => p.keys).map((p) => [p, p.demo.clone()]));
          animate("demo", 1200, (u) => poseDemo((p) => p.demo.copy(last.get(p)).multiplyScalar(1 - smooth(u))),
            stopDemo);
        }, 2500);
      });
    });
  };
  if (explode > 0) {
    const from = explode;
    animate("explode", 1400 * Math.max(from, 0.25), (u) => setExplode(THREE.MathUtils.lerp(from, 0, u)), run);
  } else run();
}

// --- pièces

// Pièce de la version GPS montrée ; une pièce peut être de plusieurs versions (« puck bouchon »), sans version : toujours.
const inVersion = (p) => !p.version || p.version.split(" ").includes(gpsVersion);

function applyVisibility() {
  for (const p of parts) {
    p.object.visible = shown[p.group] !== false && (!p.hardware || hardwareShown) && inVersion(p);
  }
  // Une ligne de la liste par groupe qui a des pièces dans la version montrée.
  for (const l of $("parts").children) {
    const g = l.dataset.group;
    if (g !== HARDWARE) l.hidden = !parts.some((p) => p.group === g && inVersion(p));
  }
  targets = parts.filter((p) => p.object.visible).flatMap((p) => p.meshes);
  dirty = true;
}

let hovered = null;       // groupe sous le pointeur ou dans la liste
let pinned = null;        // groupe choisi au toucher ou au clic

function showCard() {
  const g = hovered ?? pinned;
  const [name, detail] = g === HARDWARE ? hardware
    : (groups[g] ?? (step && translateStep(step)) ?? [t("model"), defaultCard()]);
  $("card-name").textContent = name;
  $("card-detail").textContent = detail;
  for (const l of $("parts").children) l.classList.toggle("marked", l.dataset.group === g);
  for (const p of parts) {
    const lit = g === HARDWARE ? p.hardware : p.group === g && !p.hardware;
    for (const m of p.materials) {
      m.emissive.copy(m.userData.emissive);
      if (lit) m.emissive.add(HIGHLIGHT);
    }
  }
  dirty = true;
}
const touch = matchMedia("(hover: none)").matches;
const defaultCard = () => t(touch ? "helpTouch" : "help");
const translateStep = ([name, detail]) => (lang === "en" && MODEL_EN.steps[name]) || [name, detail];

function highlight(group) {
  if (group === hovered) return;
  hovered = group;
  showCard();
}

function row(group, name, onToggle) {
  const l = document.createElement("label");
  l.className = "part";
  l.dataset.group = group;
  const c = document.createElement("input");
  c.type = "checkbox";
  c.id = `part-${group}`;
  c.checked = true;
  c.addEventListener("change", () => onToggle(c.checked));
  const n = document.createElement("span");
  n.textContent = name;
  l.append(c, n);
  l.addEventListener("pointerenter", () => highlight(group));
  l.addEventListener("pointerleave", () => highlight(null));
  c.addEventListener("focus", () => highlight(group));
  c.addEventListener("blur", () => highlight(null));
  return l;
}

function buildList() {
  const list = $("parts");
  for (const [g, [name]] of Object.entries(groups)) {
    list.append(row(g, name, (v) => {
      shown[g] = v;
      applyVisibility();
    }));
  }
  const l = row(HARDWARE, hardware[0], (v) => {
    hardwareShown = v;
    applyVisibility();
  });
  l.classList.add("hardware");
  list.append(l);
}

function markVersion() {
  for (const b of $("versions").children) b.setAttribute("aria-pressed", String(b.dataset.version === gpsVersion));
}

function chooseVersion(v) {
  gpsVersion = v;
  markVersion();
  if (pinned && !parts.some((p) => p.group === pinned && p.object.visible)) pinned = null;
  applyVisibility();
  showCard();
}

// --- pointeur

const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
let pickPending = false;
let pressAt = null;

function setPointer(e) {
  const r = canvas.getBoundingClientRect();
  pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
}

function groupUnderPointer() {
  raycaster.setFromCamera(pointer, camera);
  const hit = raycaster.intersectObjects(targets, false)[0];
  return hit ? hit.object.userData.pickGroup : null;
}

canvas.addEventListener("pointermove", (e) => {
  if (e.pointerType === "mouse" && !e.buttons) {
    setPointer(e);
    pickPending = true;
  }
});
canvas.addEventListener("pointerleave", () => {
  pickPending = false;
  highlight(null);
});
canvas.addEventListener("pointerdown", (e) => {
  pressAt = [e.clientX, e.clientY];
});
canvas.addEventListener("pointerup", (e) => {
  if (!pressAt || Math.hypot(e.clientX - pressAt[0], e.clientY - pressAt[1]) > 6) return;
  setPointer(e);
  const g = groupUnderPointer();
  pinned = g === pinned ? null : g;
  showCard();
});

// --- chargement

function prepare(gltf) {
  const root = gltf.scene;
  root.scale.setScalar(MM);
  scene.add(root);

  const info = JSON.parse(root.userData.maquette ?? "{}");
  model = info;
  translateModel();

  root.traverse((o) => {
    const e = o.userData.eclate;
    if (!e) return;
    // Blender : Y vers l'arrière, Z en haut ; scène : Y en haut, façade vers +Z
    const p = {
      object: o, name: o.name, group: o.userData.groupe, hardware: Boolean(o.userData.vis),
      version: o.userData.version ?? null,
      offset: new THREE.Vector3(e[0], e[2], -e[1]), demo: new THREE.Vector3(), meshes: [], materials: [],
    };
    o.traverse((m) => {
      if (!m.isMesh) return;
      m.material = m.material.clone();
      m.material.userData.emissive = m.material.emissive.clone();
      m.userData.pickGroup = p.hardware ? HARDWARE : p.group;
      p.materials.push(m.material);
      p.meshes.push(m);
    });
    parts.push(p);
  });

  // Démonstration : clés [image, [dx, dy, dz]] en mm, dans le repère de Blender ; l'image 0 est la pose assemblée.
  const a = info.animation;
  if (a) {
    demo = {
      fps: a.ips, end: a.fin,
      steps: (a.etapes ?? []).map((s) => ({ from: s.de, to: s.a, name: s.nom, detail: s.detail })),
    };
    for (const track of a.pistes) {
      const p = parts.find((q) => q.name === track.piece);
      if (p) p.keys = [[0, new THREE.Vector3()], ...track.cles.map(([f, [x, y, z]]) => [f, new THREE.Vector3(x, z, -y)])];
    }
    $("demo-block").hidden = false;
  }

  const farthest = Math.max(...parts.map((p) => p.offset.length()));
  for (const p of parts) p.delay = DELAY * (1 - p.offset.length() / farthest);

  // Coins des pièces pour le cadrage, dans le repère de la scène.
  root.updateMatrixWorld(true);
  for (const p of parts) {
    const b = new THREE.Box3().setFromObject(p.object);
    p.corners = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => new THREE.Vector3(
      (i & 1 ? b.max : b.min).x, (i & 2 ? b.max : b.min).y, (i & 4 ? b.max : b.min).z,
    ));
  }
  camera.position.set(0, 0, 1);

  // Arêtes des pièces imprimées, pour lire les formes sombres.
  const light = new THREE.LineBasicMaterial({ color: 0xa8adb3, transparent: true, opacity: 0.34 });
  const dark = new THREE.LineBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.4 });
  for (const p of parts) {
    if (!p.object.userData.aretes) continue;
    for (const m of p.meshes) {
      const c = m.material.color;
      m.add(new THREE.LineSegments(new THREE.EdgesGeometry(m.geometry, 28), c.r + c.g + c.b < 0.2 ? light : dark));
    }
  }

  // Connectique GPS en trois versions (puck externe, GPS interne, bouchon) : ?gps=puck|interne|bouchon, sinon celle
  // du modèle.
  const versions = [...new Set(parts.flatMap((p) => (p.version ? p.version.split(" ") : [])))];
  if (versions.length) {
    const asked = params.get("gps");
    gpsVersion = versions.includes(asked) ? asked : versions.includes(info.gps) ? info.gps : versions[0];
    $("gps-block").hidden = false;
    markVersion();
  }

  buildList();
  applyVisibility();
  setView(params.has("vue") && VIEWS[params.get("vue")] ? params.get("vue") : "iso", true);
  waiting.hidden = true;

  // ?ouverture=40 ouvre directement à 40 %, ?image=120 fige la démonstration à cette image ; sinon la
  // démonstration se joue une fois, peu après le chargement (sans démonstration : l'objet s'ouvre tout seul).
  const openAt = Number(params.get("ouverture"));
  if (params.has("ouverture")) setExplode(clamp01(openAt / 100));
  else if (demo && params.has("image")) {
    setExplode(0);
    poseDemo((p) => demoOffset(p, Number(params.get("image")), p.demo));
  } else if (demo) {
    setExplode(0);
    if (!reducedMotion) setTimeout(playDemo, 1200);
  } else if (reducedMotion) setExplode(1);
  else {
    setExplode(0);
    setTimeout(() => explodeTo(1, 2200), 600);
  }
}

// --- langue de la page

// Textes du modèle dans la langue de la page (le français du modèle sert de repli).
function translateModel() {
  if (!model) return;
  const en = lang === "en";
  groups = Object.fromEntries((model.groupes ?? []).map((g) =>
    [g.id, (en && MODEL_EN.groups[g.id]) || [g.nom, g.detail]]));
  hardware = (en && MODEL_EN.hardware) || model.visserie || hardware;
  const note = (en && MODEL_EN.note) || model.note;
  $("note").textContent = [note, model.date && t("date")(model.date)].filter(Boolean).join(" ");
  for (const l of $("parts").children) {
    const g = l.dataset.group;
    l.querySelector("span").textContent = g === HARDWARE ? hardware[0] : groups[g]?.[0] ?? g;
  }
}

function setLanguage(l) {
  lang = l in TEXTS ? l : "fr";
  document.title = t("title");
  const description = document.querySelector('meta[name="description"]');   // absente de la page en un seul fichier
  if (description) description.content = t("description");
  for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t(e.dataset.i18n);
  if (!waiting.hidden) waiting.textContent = t(waitingState);
  for (const e of document.querySelectorAll("[data-i18n-aria]")) e.setAttribute("aria-label", t(e.dataset.i18nAria));
  applyLanguage(lang);   // en-tête commun : navigation, sélecteur, drapeau
  translateModel();
  if (parts.length) showCard();
  else $("card-detail").textContent = defaultCard();
}

onLanguageChange(setLanguage);
setLanguage(lang);

// --- boucle : rien n'est dessiné tant que rien ne change

function loop(now) {
  requestAnimationFrame(loop);
  if (!parts.length) return;
  for (const [name, tw] of tweens) {
    const u = clamp01((now - tw.start) / tw.duration);
    tw.step(u);
    if (u === 1 && tweens.get(name) === tw) {
      tweens.delete(name);
      tw.then?.();
    }
    dirty = true;
  }
  if (controls.update()) dirty = true;
  if (pickPending) {
    pickPending = false;
    highlight(groupUnderPointer());
  }
  if (dirty) {
    dirty = false;
    renderer.render(scene, camera);
  }
}

new ResizeObserver(() => {
  const w = zone.clientWidth;
  const h = zone.clientHeight;
  if (!w || !h) return;
  renderer.setSize(w, h, false);
  const apply = () => {
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  };
  if (parts.length) reframe({ before: explode, apply });
  else apply();
  dirty = true;
}).observe(zone);

$("assembled").addEventListener("click", () => {
  stopDemo();
  explodeTo(0);
});
$("exploded").addEventListener("click", () => {
  stopDemo();
  explodeTo(1);
});
$("explode").addEventListener("input", (e) => {
  stopDemo();
  tweens.delete("explode");
  setExplode(e.target.valueAsNumber / 100);
});
$("demo").addEventListener("click", playDemo);
for (const b of $("views").children) b.addEventListener("click", () => setView(b.dataset.view));
for (const b of $("versions").children) b.addEventListener("click", () => chooseVersion(b.dataset.version));

requestAnimationFrame(loop);

new GLTFLoader().load(
  "raidmaster.glb",
  prepare,
  (e) => {
    waiting.textContent = e.lengthComputable
      ? `${t("loading")} ${Math.round((e.loaded / e.total) * 100)} %`
      : `${t("loading")} ${t("mb")(e.loaded / 1e6)}`;
  },
  (error) => {
    console.error(error);
    waitingState = "error";
    waiting.textContent = t("error");
  },
);
