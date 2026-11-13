// Simulateur web du Raidmaster 403 : le firmware (core/ + ui/, compilés en WebAssembly par tools/web.sh) tourne
// dans la maquette 3D (../mockup/raidmaster.glb, écran vivant, capuchons cliquables) et rejoue un trajet GPS suivi sur
// une carte OpenStreetMap. Fichiers construits (ne pas modifier à la main) : build.json, raidmaster-sim-*.js /
// .wasm / .data, scenarios/ (software/tools/web.sh, voir README.md).
//
// Une session = une instance neuve du module (comme un boîtier qu'on rallume) : changer de trajet, cocher les
// actions ou remettre la mémoire à zéro relance le module ; la vitesse et les essais agissent en direct.
// Paramètres d'URL (adresses publiées, gardés en français) : ?scenario=, ?vitesse=, ?actions=1, ?vue=2d|3d,
// ?allumer=1, ?son=1, ?gps=, ?lang=. Données du modèle 3D (userData de scene.py) : noms français.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";
import { applyLanguage, initialLanguage, onLanguageChange } from "../common/common.js";

const W = 480, H = 320;
const LONG_PRESS_MS = 600;            // seuil de l'appui long, à garder égal à celui de la carte boutons
const CAP_PITCH = 0.0185;             // m, entraxe des 5 capuchons (A, B, Menu, ▲, ▼ de gauche à droite)
const CAP_CENTER_Y = -0.0405;         // m, dans le repère du nœud « capuchons »
const CAP_ORDER = [0, 1, 4, 2, 3];    // de gauche à droite → identifiant du bouton (0 A, 1 B, 2 +, 3 −, 4 Menu)
const SPEEDS = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
const DEFAULT_SPEED = 20;
const MAX_POINTS = 20000;             // points de la trace dessinés sur la carte
const IMPORTED = "imported";          // valeur du choix de trajet pour une trace importée
// Clés du stockage du navigateur (valeurs inchangées pour garder les choix déjà faits).
const NVS = "raidmaster-403.nvs.";    // stockage du module (hal_simweb/hal_storage_web.c)
const CHOICES = "raidmaster-sim.choix";
const VIEW_KEY = "raidmaster-sim.vue";
const SOUND_KEY = "raidmaster-sim.son";
// Tableau sim_info du module (simweb/main.c) : indice de chaque valeur.
const I = { t: 0, pos: 1, lat: 2, lon: 3, speed: 4, course: 5, fix: 6, a: 7, b: 8, odo: 9, script: 10, failures: 11,
            signal: 12, gps: 13, mag: 14, spin: 15, night: 16, gpsPresent: 17, screen: 18, frame: 19, heading: 20, n: 22 };

// --- langues

const TEXTS = {
  fr: {
    title: "Simulateur Raidmaster",
    description: "Simulateur du Raidmaster 403 dans le navigateur : le firmware tourne dans la maquette 3D et rejoue des trajets GPS sur une carte.",
    sub: "simulateur",
    lede: "Le firmware du tripmaster tourne dans la maquette 3D et rejoue un trajet GPS, suivi sur la carte. Cliquer les capuchons : appui court, ou long en gardant le doigt.",
    views: "Vues",
    tripmaster: "Tripmaster", map: "Carte", screenOnly: "Écran seul", view3d: "Vue 3D", recenter: "Recentrer",
    scene: "Tripmaster en 3D : cliquer les capuchons, glisser pour tourner",
    panel: "Panneau de développement",
    loading: "Chargement…", loadingModel: (p) => `Chargement du modèle… ${p}`,
    powerOn: "Allumer", follow: "Centrer",
    trip: "Trajet GPS", scenario: "Scénario", actions: "Jouer les actions et les vérifications",
    speed: "Vitesse", restart: "Relancer", import: "Importer un fichier NMEA (ou le glisser sur la page)",
    imported: (n) => `Fichier importé : ${n}`, phase: (n) => `Phase ${n}`,
    scenarioDetail: (s, mb) => `${s.desc} — rejeu des tests à ×${s.speed}, ${mb}.`,
    importedDetail: (mb) => `Trace importée, rejouée au rythme de ses horodatages (${mb}). Elle reste dans ce navigateur.`,
    mb: (n) => (n < 0.1 ? `${Math.max(1, Math.round(n * 1000))} ko` : `${n.toFixed(1).replace(".", ",")} Mo`),
    checks: "Vérifications", tests: "Essais",
    night: "Nuit", signal: "Signal GPS", puck: "Puck GPS", compass: "Boussole", spin: "Tour sur place", power: "Alimentation", nextScreen: "Écran suivant", sound: "Son",
    keyboard: "Clavier : a b + − m (Maj : appui long), n nuit, d signal, g puck, c boussole, r tour, p alimentation, v écran suivant.",
    memory: "Mémoire",
    memoryDetail: "Trips et réglages restent dans ce navigateur, comme dans la mémoire du boîtier (sauf avec les actions : mémoire de la session).",
    reset: "Remise à zéro", resetDone: "mémoire remise à zéro", log: "Journal",
    traceLoading: (mb) => `Chargement de la trace (${mb})…`,
    moduleError: "Le simulateur n'a pas pu être chargé.", traceError: "La trace n'a pas pu être chargée.",
    webgl: "WebGL indisponible : écran en 2D.",
    summaryRunning: (ok, n) => `${ok} / ${n}`, summaryEnd: (e) => (e ? `${e} échec(s)` : "toutes vertes"),
    expected: "attendu",
    version: (b) => `Module ${b.version}${b.git ? ` · ${b.git}` : ""}${b.boot_anim ? "" : " · sans animation de démarrage"}`,
    status: (i, v) => [clock(i[I.t]), `${fmt(i[I.speed], 0)} km/h`, `A ${fmt(i[I.a], 2)}`, `B ${fmt(i[I.b], 2)}`,
                       `ODO ${fmt(i[I.odo], 1)}`, `×${v}`],
  },
  en: {
    title: "Raidmaster simulator",
    description: "The Raidmaster 403 in the browser: the firmware runs inside the 3D model and replays GPS trips on a map.",
    sub: "simulator",
    lede: "The tripmaster firmware runs inside the 3D model and replays a GPS trip, followed on the map. Click the button caps: short press, or long press by holding.",
    views: "Views",
    tripmaster: "Tripmaster", map: "Map", screenOnly: "Screen only", view3d: "3D view", recenter: "Reset view",
    scene: "Tripmaster in 3D: click the button caps, drag to rotate",
    panel: "Developer panel",
    loading: "Loading…", loadingModel: (p) => `Loading the model… ${p}`,
    powerOn: "Power on", follow: "Center",
    trip: "GPS trip", scenario: "Scenario", actions: "Play the actions and checks",
    speed: "Speed", restart: "Restart", import: "Import an NMEA file (or drop it on the page)",
    imported: (n) => `Imported file: ${n}`, phase: (n) => `Phase ${n}`,
    scenarioDetail: (s, mb) => `${SCENARIOS_EN[s.id] ?? s.desc} — test replay at ×${s.speed}, ${mb}.`,
    importedDetail: (mb) => `Imported trace, replayed at the pace of its timestamps (${mb}). It stays in this browser.`,
    mb: (n) => (n < 0.1 ? `${Math.max(1, Math.round(n * 1000))} kB` : `${n.toFixed(1)} MB`),
    checks: "Checks", tests: "Tests",
    night: "Night", signal: "GPS signal", puck: "GPS puck", compass: "Compass", spin: "Spin on the spot", power: "Power", nextScreen: "Next screen", sound: "Sound",
    keyboard: "Keyboard: a b + − m (Shift: long press), n night, d signal, g puck, c compass, r spin, p power, v next screen.",
    memory: "Memory",
    memoryDetail: "Trips and settings stay in this browser, like in the device's memory (except with the actions: session memory).",
    reset: "Reset", resetDone: "memory reset", log: "Log",
    traceLoading: (mb) => `Loading the trace (${mb})…`,
    moduleError: "The simulator could not be loaded.", traceError: "The trace could not be loaded.",
    webgl: "WebGL unavailable: 2D screen.",
    summaryRunning: (ok, n) => `${ok} / ${n}`, summaryEnd: (e) => (e ? `${e} failed` : "all passed"),
    expected: "expected",
    version: (b) => `Module ${b.version}${b.git ? ` · ${b.git}` : ""}${b.boot_anim ? "" : " · no boot animation"}`,
    status: (i, v) => [clock(i[I.t]), `${fmt(i[I.speed], 0)} km/h`, `A ${fmt(i[I.a], 2)}`, `B ${fmt(i[I.b], 2)}`,
                       `ODO ${fmt(i[I.odo], 1)}`, `×${v}`],
  },
};

// Descriptions des scénarios en anglais (le français vient de tests/fixtures/scenarios/README.md).
const SCENARIOS_EN = {
  s10_stationary_noise: "10 min stationary, ±5 m and ±0.5 km/h: 0 m, chrono stopped",
  s02_highway_cruise: "1 h at 120 km/h, −1.5 % calibration in the menu: distance, average, max",
  s03_country_road: "1 h of winding road at 70-90 km/h, light noise",
  s01_city_commute: "30 min in town, 12 traffic lights, urban noise",
  s20_raz_burst: "5 trip A resets in 10 s: only the last counts, B and ODO untouched",
  s22_increment_sequence: "+/− in a row, long presses, floor at 0, trip B, no drift",
  s21_calib_mid_stage: "calibration 0 → +2 % halfway: the past is unchanged",
  s23_menu_during_drive: "menu opened while driving: trips keep counting",
  s24_ligne_droite_boutons: "distance then reset, A/B toggle, corrections",
  s25_calibration_auto: "full automatic calibration through the interface",
  s13_cold_start: "silent receiver then 30 s without fix: nothing counted, time unknown",
  s15_backtrack: "180° U-turn: the distance keeps increasing",
  s11_tunnel_loss: "2 min without fix: trips frozen; catch-up (provisional rule)",
  s14_fix_degraded_3d_to_2d: "2D fix for 10 min: distance unchanged",
  s18_gps_debranche: "puck unplugged while driving then plugged back at 9600 baud: \"GPS ×\", search, setup, catch-up",
  s30_full_raid_day: "12 h stage, noise, HDOP spikes: 717 km within ±0.2 %",
  s31_power_loss_mid: "power cut at 90 km/h: settings read back, ≤ 250 m lost",
  s04_mountain_pass: "mountain pass, bends at 25 °/s",
  s05_offroad_slow: "track at 20-40 km/h, bumps",
  s40_calibration_rotation: "compass calibration through the interface (spin on the spot), compass heading when stopped, GPS track when driving, compass unplugged",
  s12_canyon_hdop: "HDOP 8 for 2 min: fixes rejected, fix lost, ±2 %",
  s16_antimeridian: "crossing the 180° meridian",
  s17_midnight_utc: "UTC midnight while driving: local time and distance carry on",
};

const params = new URLSearchParams(location.search);
const $ = (id) => document.getElementById(id);
const load = (key, fallback) => { try { return JSON.parse(localStorage.getItem(key)) ?? fallback; } catch { return fallback; } };
const save = (key, v) => { try { localStorage.setItem(key, JSON.stringify(v)); } catch { /* stockage indisponible */ } };

let lang = initialLanguage();
const t = (key) => TEXTS[lang][key];
const fmt = (v, d) => (lang === "fr" ? v.toFixed(d).replace(".", ",") : v.toFixed(d));
const clock = (ms) => {
  const s = Math.floor(ms / 1000);
  return `${String(Math.floor(s / 3600)).padStart(2, "0")}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
};

// --- état de la page

let build = null;            // build.json
let wasm = null;             // module WebAssembly compilé une fois, instancié à chaque session
let scenarios = [];
let imported = null;         // { name, bytes }
let audio = null;            // { ctx, out }
let poweredOn = false;
let M = null;                // instance du module en cours
let session = null;          // { gpsSpeed, actions, trace, mapIndex }
let info = new Float64Array(I.n);
let lastFrame = -1;
const stored = load(CHOICES, {});
let choice = { scenario: params.get("scenario") ?? stored.scenario ?? "s04_mountain_pass",
               speed: Number(params.get("vitesse")) || stored.speed || DEFAULT_SPEED,
               actions: params.has("actions") ? params.get("actions") === "1" : false };
const saveChoice = () => save(CHOICES, { scenario: choice.scenario, speed: choice.speed });
const logLines = [];
let logDirty = false;
const expects = [];

// --- journal

function log(line) {
  logLines.push(line);
  if (logLines.length > 400) logLines.splice(0, logLines.length - 400);
  logDirty = true;
}
function showLog() {
  if (!logDirty) return;
  logDirty = false;
  const pre = $("log");
  const atBottom = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 24;
  pre.textContent = logLines.join("\n");
  if (atBottom) pre.scrollTop = pre.scrollHeight;
}

// --- module

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const s = document.createElement("script");
    s.src = src;
    s.onload = resolve;
    s.onerror = () => reject(new Error(src));
    document.head.append(s);
  });
}

async function prepareModule() {
  build = await (await fetch("build.json", { cache: "no-store" })).json();
  const [, w] = await Promise.all([
    loadScript(`${build.module}.js`),
    fetch(`${build.module}.wasm`).then((r) => r.arrayBuffer()).then((b) => WebAssembly.compile(b)),
  ]);
  scenarios = (await (await fetch("scenarios/index.json", { cache: "no-cache" })).json()).scenarios;
  $("version").textContent = t("version")(build);
  wasm = w;      // en dernier : ready() attend le module et la liste des scénarios
}

async function scenarioBytes(s) {
  $("waiting").hidden = false;
  $("waiting").textContent = t("traceLoading")(t("mb")(s.bytes / 1e6));
  const [nmea, actions] = await Promise.all([
    fetch(`scenarios/${s.nmea}`).then((r) => { if (!r.ok) throw new Error(s.nmea); return r.arrayBuffer(); }),
    fetch(`scenarios/${s.actions}`).then((r) => r.text()),
  ]);
  return { nmea: new Uint8Array(nmea), actions };
}

// Relance une session : trajet choisi, actions ou non, mémoire du navigateur ou de la session.
let runCount = 0;
async function start() {
  if (!poweredOn || !wasm) return;
  const run = ++runCount;
  if (M) M._sim_save();
  M = null;
  let nmea, actionsText = null, gpsSpeed = 1, persistent = true, name;
  try {
    if (choice.scenario === IMPORTED && imported) {
      nmea = imported.bytes;
      name = imported.name;
    } else {
      const s = scenarios.find((e) => e.id === choice.scenario) ?? scenarios[0];
      choice.scenario = s.id;
      const o = await scenarioBytes(s);
      nmea = o.nmea;
      name = s.id;
      if (choice.actions) {
        actionsText = o.actions;
        gpsSpeed = s.speed;
        persistent = false;
      }
    }
  } catch (e) {
    console.error(e);
    $("waiting").textContent = t("traceError");
    return;
  }
  if (run !== runCount) return;      // un autre lancement a suivi
  expects.length = 0;
  showExpects();
  $("expect-block").hidden = actionsText === null;
  log(`── ${name}${actionsText ? ` · actions ×${gpsSpeed}` : ""}`);
  const m = await createRaidmasterSim({
    instantiateWasm: (imports, receive) => {
      WebAssembly.instantiate(wasm, imports).then((inst) => receive(inst, wasm));
      return {};
    },
    print: log,
    printErr: log,
    audioCtx: audio?.ctx,
    audioOut: audio?.out,
    onExpect: (field, got, want, tol, ok, tMs) => { expects.push({ field, got, want, tol, ok, tMs }); showExpects(); },
    onScriptEnd: (failures) => showExpects(failures),
  });
  if (run !== runCount) return;
  m.FS.writeFile("/trace.nmea", nmea);
  if (actionsText !== null) m.FS.writeFile("/actions.txt", actionsText);
  const rc = m.ccall("sim_start", "number", ["string", "number", "string", "number"],
                     ["/trace.nmea", gpsSpeed, actionsText !== null ? "/actions.txt" : "", persistent ? 1 : 0]);
  if (rc !== 0) {
    $("waiting").hidden = false;
    $("waiting").textContent = t("traceError");
    return;
  }
  session = { gpsSpeed, actions: actionsText !== null, trace: readTrace(nmea) };
  M = m;
  lastFrame = -1;
  $("waiting").hidden = true;
  fillSpeeds();
  applySpeed();
  traceOnMap();
}

function applySpeed() {
  if (M) M._sim_set_rate(choice.speed / session.gpsSpeed);
}

function readInfo() {
  info = new Float64Array(M.HEAPF64.buffer, M._sim_info(), I.n).slice();
}

// --- panneau

function fillScenarios() {
  const sel = $("scenario");
  sel.textContent = "";
  const phases = new Map();
  for (const s of scenarios) {
    if (!phases.has(s.phase)) {
      const g = document.createElement("optgroup");
      g.label = t("phase")(s.phase);
      phases.set(s.phase, g);
      sel.append(g);
    }
    const o = document.createElement("option");
    o.value = s.id;
    o.textContent = s.id.replace(/^(s\d+)_/, "$1 · ").replaceAll("_", " ");
    phases.get(s.phase).append(o);
  }
  if (imported) {
    const o = document.createElement("option");
    o.value = IMPORTED;
    o.textContent = t("imported")(imported.name);
    sel.prepend(o);
  }
  sel.value = choice.scenario === IMPORTED && !imported ? scenarios[0]?.id : choice.scenario;
  if (!sel.value && scenarios[0]) sel.value = scenarios[0].id;
  choice.scenario = sel.value;
  showScenarioDetail();
}

function showScenarioDetail() {
  const s = scenarios.find((e) => e.id === choice.scenario);
  $("scenario-detail").textContent = s ? t("scenarioDetail")(s, t("mb")(s.bytes / 1e6))
    : imported ? t("importedDetail")(t("mb")(imported.bytes.length / 1e6)) : "";
  $("actions").disabled = choice.scenario === IMPORTED;
}

function fillSpeeds() {
  const list = [...SPEEDS];
  if (session && !list.includes(session.gpsSpeed)) list.push(session.gpsSpeed);
  list.sort((a, b) => a - b);
  const sel = $("speed");
  sel.textContent = "";
  for (const v of list) {
    const o = document.createElement("option");
    o.value = v;
    o.textContent = `×${v}`;
    sel.append(o);
  }
  if (session?.actions && !params.has("vitesse") && !choice.speedPinned) choice.speed = session.gpsSpeed;
  if (!list.includes(choice.speed)) choice.speed = DEFAULT_SPEED;
  sel.value = choice.speed;
}

function showExpects(end) {
  const ol = $("expects");
  ol.textContent = "";
  for (const e of expects) {
    const li = document.createElement("li");
    li.className = e.ok ? "ok" : "ko";
    const a = document.createElement("span");
    a.textContent = `${fmt(e.tMs / 1000, 1)} s`;
    const b = document.createElement("span");
    b.textContent = `${e.field} ${e.got.toFixed(3)} (${t("expected")} ${e.want}${e.tol ? ` ± ${e.tol}` : ""}) ${e.ok ? "✓" : "✗"}`;
    li.append(a, b);
    ol.append(li);
  }
  ol.scrollTop = ol.scrollHeight;
  const ko = expects.filter((e) => !e.ok).length;
  const summary = $("summary");
  if (end !== undefined) {
    summary.textContent = t("summaryEnd")(end);
    summary.className = `summary ${end ? "ko" : "ok"}`;
  } else {
    summary.textContent = expects.length ? t("summaryRunning")(expects.length - ko, expects.length) : "";
    summary.className = `summary ${ko ? "ko" : "ok"}`;
  }
}

function showTestStates() {
  for (const b of $("tests").children) {
    const s = b.dataset.state;
    if (!s) continue;
    const v = { night: info[I.night], signal: info[I.signal], gps: info[I.gps], mag: info[I.mag], spin: info[I.spin] }[s];
    b.setAttribute("aria-pressed", String(Boolean(v)));
    b.classList.toggle("off", ["signal", "gps", "mag"].includes(s) && !v);
  }
}

$("scenario").addEventListener("change", (e) => {
  choice.scenario = e.target.value;
  saveChoice();
  showScenarioDetail();
  start();
});
$("actions").addEventListener("change", (e) => {
  choice.actions = e.target.checked;
  choice.speedPinned = false;
  start();
});
$("speed").addEventListener("change", (e) => {
  choice.speed = Number(e.target.value);
  choice.speedPinned = true;
  saveChoice();
  applySpeed();
});
$("restart").addEventListener("click", () => start());
for (const b of $("tests").querySelectorAll("[data-key]")) b.addEventListener("click", () => M?._sim_key(b.dataset.key.charCodeAt(0)));
$("reset").addEventListener("click", () => {
  M = null;      // sans sauvegarde : la session en cours ne réécrit pas la mémoire effacée
  try {
    for (let i = localStorage.length - 1; i >= 0; i--) {
      const k = localStorage.key(i);
      if (k?.startsWith(NVS)) localStorage.removeItem(k);
    }
  } catch { /* stockage indisponible */ }
  log(`[page] ${t("resetDone")}`);
  start();
});

// Import d'une trace NMEA : champ fichier ou glisser-déposer sur la page.
async function importFile(f) {
  if (!f) return;
  imported = { name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) };
  choice.scenario = IMPORTED;
  choice.actions = false;
  $("actions").checked = false;
  fillScenarios();
  start();
}
$("file").addEventListener("change", (e) => importFile(e.target.files[0]));
document.addEventListener("dragover", (e) => { e.preventDefault(); document.body.classList.add("dragging"); });
document.addEventListener("dragleave", (e) => { if (!e.relatedTarget) document.body.classList.remove("dragging"); });
document.addEventListener("drop", (e) => {
  e.preventDefault();
  document.body.classList.remove("dragging");
  importFile(e.dataTransfer.files[0]);
});

// --- boutons : capuchons 3D, boutons 2D, clavier

let press = null;     // { id, long, timer }
function pressButton(id) {
  if (!M || press) return;
  press = { id, long: false };
  markPress(id, "pressed");
  press.timer = setTimeout(() => {
    press.long = true;
    M?._sim_button(id, 1);
    markPress(id, "long");
  }, LONG_PRESS_MS);
}
function releaseButton() {
  if (!press) return;
  clearTimeout(press.timer);
  if (!press.long) M?._sim_button(press.id, 0);
  press = null;
  markPress(null);
}

const KEYS = { a: 0, b: 1, "+": 2, "=": 2, "-": 3, m: 4 };
document.addEventListener("keydown", (e) => {
  if (e.repeat || e.ctrlKey || e.metaKey || e.altKey || !M) return;
  if (e.target.closest("input, select, textarea")) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (k in KEYS) {
    M._sim_button(KEYS[k], e.shiftKey ? 1 : 0);
  } else if ("ndgcrpv".includes(k) && k.length === 1) {
    M._sim_key(k.charCodeAt(0));
  } else {
    return;
  }
  e.preventDefault();
});

for (const b of $("buttons2d").children) {
  b.addEventListener("pointerdown", (e) => {
    try { b.setPointerCapture(e.pointerId); } catch { /* pointeur synthétique */ }
    pressButton(Number(b.dataset.button));
  });
  b.addEventListener("pointerup", releaseButton);
  b.addEventListener("pointercancel", releaseButton);
  b.addEventListener("contextmenu", (e) => e.preventDefault());
}

// --- vue 3D

const zone = $("scene");
const canvas = $("view");
let renderer = null;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "high-performance" });
} catch {
  log(`[page] ${TEXTS[lang].webgl}`);
}
const screen2d = $("screen2d").getContext("2d");
const image2d = screen2d.createImageData(W, H);

// Vue « écran seul » : la sortie vidéo sur toute la largeur du cadre, barre des boutons dessous. Bouton de
// bascule dans le cadre, choix gardé dans le navigateur ; ?vue=2d|3d l'impose ; forcée sans WebGL.
let screenOnly = !renderer || (params.has("vue") ? params.get("vue") === "2d" : load(VIEW_KEY, "3d") === "2d");
function applyView() {
  canvas.hidden = screenOnly;
  $("screen2d").hidden = !screenOnly;
  $("buttons2d").hidden = !screenOnly;
  zone.classList.toggle("screen-only", screenOnly);
  const b = $("view-toggle");
  b.hidden = !renderer;
  b.textContent = t(screenOnly ? "view3d" : "screenOnly");
  b.setAttribute("aria-pressed", String(screenOnly));
  $("recenter").hidden = !renderer || screenOnly;
  lastFrame = -1;     // recopie l'image courante dans la vue affichée
  dirty = true;
}
// Recentrer : la vue 3D de départ (de face, boîtier entier), après une rotation ou un zoom.
$("recenter").addEventListener("click", () => frameFront());
$("view-toggle").addEventListener("click", () => {
  screenOnly = !screenOnly;
  save(VIEW_KEY, screenOnly ? "2d" : "3d");
  applyView();
  if (!screenOnly) frameFront();
  ready();
});

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(24, 1, 5, 8000);
let controls = null;
let dirty = true;
let caps = null;
let rings = [];           // anneau d'appui de chaque capuchon
let bounds = null;        // { center, size } du modèle
const texture = new THREE.DataTexture(new Uint8Array(W * H * 4), W, H, THREE.RGBAFormat);
texture.colorSpace = THREE.SRGBColorSpace;
texture.magFilter = THREE.LinearFilter;
texture.minFilter = THREE.LinearMipmapLinearFilter;
texture.generateMipmaps = true;
texture.flipY = false;    // UV du nœud « ecran » : v = 0 en haut, comme la ligne 0 du framebuffer

const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
function capUnder(e) {
  if (!caps) return null;
  const r = canvas.getBoundingClientRect();
  pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  raycaster.setFromCamera(pointer, camera);
  // Maillage du capuchon seulement : ni ses arêtes (lignes) ni les anneaux d'appui.
  const hit = raycaster.intersectObject(caps, true).find((h) => h.object.isMesh && !rings.includes(h.object));
  if (!hit) return null;
  const p = caps.worldToLocal(hit.point.clone());
  const rank = Math.round(p.x / CAP_PITCH) + 2;
  return rank >= 0 && rank < 5 ? CAP_ORDER[rank] : null;
}

function markPress(id, state) {
  for (const b of $("buttons2d").children) {
    const me = Number(b.dataset.button) === id;
    b.classList.toggle("pressed", me);
    b.classList.toggle("long", me && state === "long");
  }
  rings.forEach((m, rank) => {
    m.visible = CAP_ORDER[rank] === id;
    m.material.color.set(state === "long" ? 0xff6b5e : 0xffb547);
  });
  dirty = true;
}

if (renderer) {
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.toneMapping = THREE.NeutralToneMapping;
  scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
  scene.environmentIntensity = 0.7;
  scene.add(new THREE.HemisphereLight(0xffffff, 0x33373c, 0.9));
  const keyLight = new THREE.DirectionalLight(0xffffff, 1.8);
  keyLight.position.set(200, 260, 240);
  scene.add(keyLight);

  // Avant OrbitControls : un appui sur un capuchon ne fait pas tourner la vue.
  canvas.addEventListener("pointerdown", (e) => {
    const id = capUnder(e);
    if (id === null) return;
    e.stopImmediatePropagation();
    try { canvas.setPointerCapture(e.pointerId); } catch { /* pointeur synthétique */ }
    pressButton(id);
  });
  canvas.addEventListener("pointerup", releaseButton);
  canvas.addEventListener("pointercancel", releaseButton);
  canvas.addEventListener("pointermove", (e) => {
    if (e.pointerType === "mouse" && !e.buttons) canvas.classList.toggle("on-button", capUnder(e) !== null);
  });
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());
  controls = new OrbitControls(camera, canvas);
  controls.enableDamping = true;
  controls.dampingFactor = 0.09;
  controls.minDistance = 60;
  controls.maxDistance = 1500;
  controls.addEventListener("change", () => { dirty = true; });

  new GLTFLoader().load("../mockup/raidmaster.glb", prepareModel,
    (e) => {
      if (!poweredOn) {
        $("waiting").textContent = t("loadingModel")(e.lengthComputable
          ? `${Math.round((e.loaded / e.total) * 100)} %` : t("mb")(e.loaded / 1e6));
      }
    },
    (err) => { console.error(err); log(`[page] ${err.message ?? err}`); });
}

function prepareModel(gltf) {
  const root = gltf.scene;
  root.scale.setScalar(1000);       // modèle en mètres, scène en millimètres
  scene.add(root);
  const modelInfo = JSON.parse(root.userData.maquette ?? "{}");
  // Connectique GPS : la version du modèle (puck par défaut) ; câble de démonstration retiré.
  const versions = new Set();
  root.traverse((o) => { if (o.userData.version) o.userData.version.split(" ").forEach((v) => versions.add(v)); });
  const version = versions.has(params.get("gps")) ? params.get("gps") : versions.has(modelInfo.gps) ? modelInfo.gps : [...versions][0];
  const light = new THREE.LineBasicMaterial({ color: 0xa8adb3, transparent: true, opacity: 0.34 });
  const dark = new THREE.LineBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.4 });
  root.traverse((o) => {
    if (!o.userData.eclate) return;
    const v = o.userData.version;
    o.visible = o.userData.groupe !== "cable" && (!v || v.split(" ").includes(version));
    if (o.userData.aretes) {
      o.traverse((m) => {
        if (!m.isMesh) return;
        const c = m.material.color;
        m.add(new THREE.LineSegments(new THREE.EdgesGeometry(m.geometry, 28), c.r + c.g + c.b < 0.2 ? light : dark));
      });
    }
  });
  // Écran : le framebuffer du firmware, sans éclairage ni tone mapping (couleurs de LVGL telles quelles).
  const screen = root.getObjectByName("ecran");
  screen.traverse((m) => {
    if (m.isMesh) m.material = new THREE.MeshBasicMaterial({ map: texture, toneMapped: false });
  });
  caps = root.getObjectByName("capuchons");
  rings = [-2, -1, 0, 1, 2].map((k) => {
    const m = new THREE.Mesh(new THREE.RingGeometry(0.0062, 0.0078, 40),
      new THREE.MeshBasicMaterial({ color: 0xffb547, transparent: true, opacity: 0.9, toneMapped: false, depthTest: false }));
    m.position.set(k * CAP_PITCH, CAP_CENTER_Y, 0.0046);
    m.renderOrder = 2;
    m.visible = false;
    caps.add(m);
    return m;
  });
  root.updateMatrixWorld(true);
  const b = new THREE.Box3();
  root.traverseVisible((o) => { if (o.isMesh) b.expandByObject(o); });
  bounds = { center: b.getCenter(new THREE.Vector3()), size: b.getSize(new THREE.Vector3()) };
  frameFront();
  dirty = true;
  ready();
}

// « Allumer » quand le module et le modèle sont là.
function ready() {
  if (poweredOn || !wasm || (renderer && !screenOnly && !caps)) return;
  $("waiting").textContent = "";
  $("power").hidden = false;
  if (params.get("allumer") === "1") powerOn();
}

// Caméra de face, le boîtier entier dans l'image : sa face avant (la plus proche, donc la plus grande à
// l'écran) occupe au plus 88 % de la hauteur ou de la largeur de la vue.
function frameFront() {
  if (!bounds || !controls) return;
  const axis = new THREE.Vector3(0, 0, 1);
  const tv = Math.tan(THREE.MathUtils.degToRad(camera.fov) / 2) * 0.88;
  const d = Math.max(bounds.size.y / 2 / tv, bounds.size.x / 2 / (tv * camera.aspect)) + bounds.size.z / 2;
  controls.target.copy(bounds.center);
  camera.position.copy(bounds.center).addScaledVector(axis, d);
  controls.update();
  dirty = true;
}

new ResizeObserver(() => {
  const w = zone.clientWidth, h = zone.clientHeight;
  if (!w || !h || !renderer) return;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  frameFront();
}).observe(zone);

// --- carte

// « Centrer » (interrupteur, actif au départ) : le véhicule au centre de la carte en permanence, zoom autour de
// lui ; un glisser de la carte le coupe, un clic le remet.
let map = null, plannedLine = null, drivenLine = null, marker = null, follow = true, zooming = false;
const arrow = (colour) => L.divIcon({
  className: "arrow", iconSize: [26, 26], iconAnchor: [13, 13],
  html: `<svg viewBox="-13 -13 26 26"><g><circle r="11" fill="#0f1011" fill-opacity=".75" stroke="${colour}" stroke-width="2"/><path d="M0 -8 L5.5 6 L0 3 L-5.5 6 Z" fill="${colour}"/></g></svg>`,
});
if (window.L) {
  map = L.map("map", { zoomControl: true, attributionControl: true }).setView([48.85, 2.36], 12);
  L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>',
  }).addTo(map);
  plannedLine = L.polyline([], { color: "#2b3a4a", weight: 3, opacity: 0.85, dashArray: "7 6" }).addTo(map);
  drivenLine = L.polyline([], { color: "#ffb547", weight: 4, opacity: 0.95 }).addTo(map);
  marker = L.marker([48.85, 2.36], { icon: arrow("#ffb547"), interactive: false, keyboard: false });
  map.on("dragstart", () => setFollow(false));
  map.on("zoomstart", () => { zooming = true; });
  map.on("zoomend", () => { zooming = false; });
} else {
  $("map-waiting").hidden = false;
  $("map-waiting").textContent = "Leaflet ?";
}
function setFollow(v) {
  follow = v;
  $("follow").setAttribute("aria-pressed", String(v));
  if (map) {
    // Leaflet relit ces options à chaque geste : zoom centré sur le véhicule tant qu'on le suit.
    map.options.scrollWheelZoom = map.options.touchZoom = map.options.doubleClickZoom = v ? "center" : true;
  }
}
setFollow(follow);
$("follow").addEventListener("click", () => {
  setFollow(!follow);
  if (follow && marker && map?.hasLayer(marker)) map.panTo(marker.getLatLng());
});

// Trace complète lue dans le NMEA : points RMC valides, temps depuis la première phrase horodatée (comme le
// rejeu de hal_gps_sim.c), longitude déroulée (méridien 180°).
function readTrace(bytes) {
  const text = new TextDecoder("latin1").decode(bytes);
  const times = [], lats = [], lons = [];
  let t0 = null, previous = -1, dayOffset = 0, prevLon = null;
  const deg = (v, h, n) => {
    const x = Number(v);
    const d = Math.floor(x / 100);
    return (d + (x - d * 100) / 60) * (h === "S" || h === "W" ? -1 : 1) || (n ? NaN : 0);
  };
  for (const line of text.split("\n")) {
    const c = line.indexOf(",");
    if (c < 0) continue;
    const h = /^(\d\d)(\d\d)(\d\d(?:\.\d+)?)/.exec(line.slice(c + 1, c + 11));
    if (!h) continue;
    let tod = (+h[1] * 3600 + +h[2] * 60 + +h[3]);
    if (previous >= 0 && tod + 12 * 3600 < previous) dayOffset += 24 * 3600;
    previous = tod;
    tod += dayOffset;
    t0 ??= tod;
    if (line.slice(3, 6) !== "RMC") continue;
    const f = line.split(",");
    if (f[2] !== "A") continue;
    const lat = deg(f[3], f[4], true), lon0 = deg(f[5], f[6], true);
    if (!Number.isFinite(lat) || !Number.isFinite(lon0)) continue;
    let lon = lon0;
    if (prevLon !== null) lon += Math.round((prevLon - lon) / 360) * 360;
    prevLon = lon;
    times.push(tod - t0);
    lats.push(lat);
    lons.push(lon);
  }
  const stride = Math.max(1, Math.ceil(times.length / MAX_POINTS));
  const points = [], pointTimes = [];
  for (let i = 0; i < times.length; i += stride) {
    points.push([lats[i], lons[i]]);
    pointTimes.push(times[i]);
  }
  if (times.length && (times.length - 1) % stride) {
    points.push([lats.at(-1), lons.at(-1)]);
    pointTimes.push(times.at(-1));
  }
  return { points, times: pointTimes };
}

function traceOnMap() {
  if (!map || !session) return;
  const { points } = session.trace;
  plannedLine.setLatLngs(points);
  drivenLine.setLatLngs([]);
  if (map.hasLayer(marker)) marker.remove();
  if (points.length) map.fitBounds(L.latLngBounds(points), { padding: [24, 24], maxZoom: 16 });
  setFollow(true);
  session.mapIndex = -1;
}

// Partie parcourue (temps de la trace = temps simulé × vitesse de rejeu) et position reçue par le boîtier.
function updateMap() {
  if (!map || !session) return;
  const { points, times } = session.trace;
  const elapsed = (info[I.t] / 1000) * session.gpsSpeed;
  let lo = 0, hi = times.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (times[mid] <= elapsed) lo = mid + 1; else hi = mid;
  }
  if (lo !== session.mapIndex) {
    session.mapIndex = lo;
    drivenLine.setLatLngs(points.slice(0, lo));
  }
  if (info[I.pos]) {
    let lon = info[I.lon];
    const ref = points[Math.max(0, Math.min(lo, points.length - 1))];
    if (ref) lon += Math.round((ref[1] - lon) / 360) * 360;
    const ll = L.latLng(info[I.lat], lon);
    if (!map.hasLayer(marker)) marker.addTo(map);
    marker.setLatLng(ll);
    const colour = info[I.fix] ? "#ffb547" : "#949aa0";
    if (marker.colour !== colour) {
      marker.setIcon(arrow(colour));
      marker.colour = colour;
    }
    const g = marker.getElement()?.querySelector("g");
    if (g && info[I.course] >= 0) g.setAttribute("transform", `rotate(${info[I.course]})`);
    if (follow && !zooming) {
      const p = map.latLngToContainerPoint(ll), c = map.getSize().divideBy(2);
      if (p.distanceTo(c) > 0.5) map.panTo(ll, { animate: false });
    }
  }
}

// --- onglets (téléphone)

for (const b of $("tabs").children) {
  b.addEventListener("click", () => {
    for (const c of $("tabs").children) c.setAttribute("aria-pressed", String(c === b));
    $("workspace").dataset.tab = b.dataset.tab;
    if (b.dataset.tab === "map") map?.invalidateSize();
    dirty = true;
  });
}

// --- langue

function setLanguage(l) {
  lang = l in TEXTS ? l : "fr";
  document.title = t("title");
  document.querySelector('meta[name="description"]').content = t("description");
  for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t(e.dataset.i18n);
  for (const e of document.querySelectorAll("[data-i18n-aria]")) e.setAttribute("aria-label", t(e.dataset.i18nAria));
  applyLanguage(lang);   // en-tête commun : navigation, sélecteur, drapeau
  if (scenarios.length) fillScenarios();
  applyView();
  if (build) $("version").textContent = t("version")(build);
  showExpects();
}
onLanguageChange(setLanguage);
setLanguage(lang);

// --- son : coupé par défaut (décision du propriétaire, 2026-11-12), bouton « Son », choix gardé dans le navigateur.
// Le contexte audio naît au premier clic (les navigateurs refusent le son avant un geste de l'utilisateur) ; coupé,
// le jingle part quand même dans une sortie à volume nul.

const VOLUME = 0.5;
let sound = params.has("son") ? params.get("son") === "1" : load(SOUND_KEY, false) === true;
function markSound() {
  $("sound").setAttribute("aria-pressed", String(sound));
  if (audio) audio.out.gain.value = sound ? VOLUME : 0;
}
function createAudio() {
  if (audio) return;
  try {
    const ctx = new AudioContext();
    const out = ctx.createGain();
    out.connect(ctx.destination);
    audio = { ctx, out };
  } catch { /* pas de son */ }
  markSound();
}
$("sound").addEventListener("click", () => {
  sound = !sound;
  save(SOUND_KEY, sound);
  createAudio();
  audio?.ctx.resume();
  markSound();
});
markSound();

// --- allumage : premier clic

function powerOn() {
  if (poweredOn) return;
  poweredOn = true;
  $("power").hidden = true;
  createAudio();
  start();
}
$("power").addEventListener("click", powerOn);

// --- boucle

let before = performance.now();
let nextMap = 0, nextStatus = 0;
function loop(now) {
  requestAnimationFrame(loop);
  const dt = now - before;
  before = now;
  if (M) {
    M._sim_frame(dt);
    readInfo();
    if (info[I.frame] !== lastFrame) {
      lastFrame = info[I.frame];
      const fb = new Uint8Array(M.HEAPU8.buffer, M._sim_fb(), W * H * 4);
      if (!screenOnly) {
        texture.image.data = fb;
        texture.needsUpdate = true;
        dirty = true;
      } else {
        image2d.data.set(fb);
        screen2d.putImageData(image2d, 0, 0);
      }
    }
    if (now >= nextStatus) {
      nextStatus = now + 250;
      $("status").textContent = t("status")(info, choice.speed).join("   ");
      showTestStates();
    }
    if (now >= nextMap) {
      nextMap = now + 150;
      updateMap();
    }
  }
  showLog();
  if (controls?.update()) dirty = true;
  if (renderer && !screenOnly && dirty) {
    dirty = false;
    renderer.render(scene, camera);
  }
}
requestAnimationFrame(loop);

addEventListener("pagehide", () => M?._sim_save());

// Accès de mise au point et d'essai (console, chrome-devtools) : module en cours, état, appuis.
window.raidmasterSim = {
  get module() { return M; },
  get info() { return Object.fromEntries(Object.entries(I).filter(([k]) => k !== "n").map(([k, i]) => [k, info[i]])); },
  get expects() { return expects.slice(); },
  button: (id, long = false) => M?._sim_button(id, long ? 1 : 0),
  pick: (x, y) => capUnder({ clientX: x, clientY: y }),
  // Centre de chaque capuchon dans la page (x, y en px), de gauche à droite, avec l'identifiant de son bouton.
  caps: () => {
    if (!caps) return [];
    const r = canvas.getBoundingClientRect();
    return [-2, -1, 0, 1, 2].map((k, rank) => {
      const p = caps.localToWorld(new THREE.Vector3(k * CAP_PITCH, CAP_CENTER_Y, 0.004)).project(camera);
      return { id: CAP_ORDER[rank], x: r.left + ((p.x + 1) / 2) * r.width, y: r.top + ((1 - p.y) / 2) * r.height };
    });
  },
};

$("actions").checked = choice.actions;
prepareModule().then(() => {
  fillScenarios();
  fillSpeeds();
  ready();
}).catch((e) => {
  console.error(e);
  $("waiting").textContent = t("moduleError");
});
