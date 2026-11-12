// Simulateur web du Raidmaster 403 : le firmware (core/ + ui/, compilés en WebAssembly par tools/web.sh) tourne
// dans la maquette 3D (../raidmaster.glb, écran vivant, capuchons cliquables) et rejoue un trajet GPS suivi sur
// une carte OpenStreetMap. Fichiers construits (ne pas modifier à la main) : build.json, raidmaster-sim-*.js /
// .wasm / .data, scenarios/ (software/tools/web.sh, voir README.md).
//
// Une session = une instance neuve du module (comme un boîtier qu'on rallume) : changer de trajet, cocher les
// actions ou remettre la mémoire à zéro relance le module ; la vitesse et les essais agissent en direct.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";

const W = 480, H = 320;
const APPUI_LONG_MS = 600;            // seuil de l'appui long, à garder égal à celui de la carte boutons
const PAS_CAPUCHONS = 0.0185;         // m, entraxe des 5 capuchons (A, B, Menu, ▲, ▼ de gauche à droite)
const CENTRE_CAPUCHONS_Y = -0.0405;   // m, dans le repère du nœud « capuchons »
const ORDRE_CAPUCHONS = [0, 1, 4, 2, 3];  // de gauche à droite → identifiant du bouton (0 A, 1 B, 2 +, 3 −, 4 Menu)
const VITESSES = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
const VITESSE_DEFAUT = 20;
const MAX_POINTS = 20000;             // points de la trace dessinés sur la carte
const NVS = "raidmaster-403.nvs.";    // clés du stockage du module (hal_simweb/hal_storage_web.c)
const CHOIX = "raidmaster-sim.choix";
const I = { t: 0, pos: 1, lat: 2, lon: 3, vit: 4, route: 5, fix: 6, a: 7, b: 8, odo: 9, script: 10, echecs: 11,
            signal: 12, gps: 13, mag: 14, spin: 15, nuit: 16, gpsPresent: 17, ecran: 18, image: 19, cap: 20, n: 22 };

// --- langues

const TEXTES = {
  fr: {
    titre: "Simulateur Raidmaster",
    description: "Simulateur du Raidmaster 403 dans le navigateur : le firmware tourne dans la maquette 3D et rejoue des trajets GPS sur une carte.",
    sous: "simulateur",
    chapeau: "Le firmware du tripmaster tourne dans la maquette 3D et rejoue un trajet GPS, suivi sur la carte. Cliquer les capuchons : appui court, ou long en gardant le doigt.",
    maquette: "Maquette 3D", langue: "Langue", github: "Code source sur GitHub", vues: "Vues",
    tripmaster: "Tripmaster", carte: "Carte", ecranSeul: "Écran seul", vue3d: "Vue 3D", recentrer: "Recentrer",
    scene: "Tripmaster en 3D : cliquer les capuchons, glisser pour tourner",
    panneau: "Panneau de développement",
    chargement: "Chargement…", chargementModele: (p) => `Chargement du modèle… ${p}`,
    allumer: "Allumer", suivre: "Centrer",
    trajet: "Trajet GPS", scenario: "Scénario", actions: "Jouer les actions et les vérifications",
    vitesse: "Vitesse", relancer: "Relancer", importer: "Importer un fichier NMEA (ou le glisser sur la page)",
    importe: (n) => `Fichier importé : ${n}`, phase: (n) => `Phase ${n}`,
    detailScenario: (s, mo) => `${s.desc} — rejeu des tests à ×${s.speed}, ${mo}.`,
    detailImporte: (mo) => `Trace importée, rejouée au rythme de ses horodatages (${mo}). Elle reste dans ce navigateur.`,
    mo: (n) => (n < 0.1 ? `${Math.max(1, Math.round(n * 1000))} ko` : `${n.toFixed(1).replace(".", ",")} Mo`),
    verifications: "Vérifications", essais: "Essais",
    nuit: "Nuit", signal: "Signal GPS", puck: "Puck GPS", boussole: "Boussole", rotation: "Tour sur place", alim: "Alimentation", ecran: "Écran suivant", son: "Son",
    clavier: "Clavier : a b + − m (Maj : appui long), n nuit, d signal, g puck, c boussole, r tour, p alimentation, v écran suivant.",
    memoire: "Mémoire",
    memoire_detail: "Trips et réglages restent dans ce navigateur, comme dans la mémoire du boîtier (sauf avec les actions : mémoire de la session).",
    raz: "Remise à zéro", razFaite: "mémoire remise à zéro", journal: "Journal",
    traceChargement: (mo) => `Chargement de la trace (${mo})…`,
    erreurModule: "Le simulateur n'a pas pu être chargé.", erreurTrace: "La trace n'a pas pu être chargée.",
    webgl: "WebGL indisponible : écran en 2D.",
    bilanEnCours: (ok, n) => `${ok} / ${n}`, bilanFin: (e) => (e ? `${e} échec(s)` : "toutes vertes"),
    attendu: "attendu",
    version: (b) => `Module ${b.version}${b.git ? ` · ${b.git}` : ""}${b.boot_anim ? "" : " · sans animation de démarrage"}`,
    etat: (i, v) => [horloge(i[I.t]), `${fmt(i[I.vit], 0)} km/h`, `A ${fmt(i[I.a], 2)}`, `B ${fmt(i[I.b], 2)}`,
                     `ODO ${fmt(i[I.odo], 1)}`, `×${v}`],
  },
  en: {
    titre: "Raidmaster simulator",
    description: "The Raidmaster 403 in the browser: the firmware runs inside the 3D model and replays GPS trips on a map.",
    sous: "simulator",
    chapeau: "The tripmaster firmware runs inside the 3D model and replays a GPS trip, followed on the map. Click the button caps: short press, or long press by holding.",
    maquette: "3D model", langue: "Language", github: "Source code on GitHub", vues: "Views",
    tripmaster: "Tripmaster", carte: "Map", ecranSeul: "Screen only", vue3d: "3D view", recentrer: "Reset view",
    scene: "Tripmaster in 3D: click the button caps, drag to rotate",
    panneau: "Developer panel",
    chargement: "Loading…", chargementModele: (p) => `Loading the model… ${p}`,
    allumer: "Power on", suivre: "Center",
    trajet: "GPS trip", scenario: "Scenario", actions: "Play the actions and checks",
    vitesse: "Speed", relancer: "Restart", importer: "Import an NMEA file (or drop it on the page)",
    importe: (n) => `Imported file: ${n}`, phase: (n) => `Phase ${n}`,
    detailScenario: (s, mo) => `${SCENARIOS_EN[s.id] ?? s.desc} — test replay at ×${s.speed}, ${mo}.`,
    detailImporte: (mo) => `Imported trace, replayed at the pace of its timestamps (${mo}). It stays in this browser.`,
    mo: (n) => (n < 0.1 ? `${Math.max(1, Math.round(n * 1000))} kB` : `${n.toFixed(1)} MB`),
    verifications: "Checks", essais: "Tests",
    nuit: "Night", signal: "GPS signal", puck: "GPS puck", boussole: "Compass", rotation: "Spin on the spot", alim: "Power", ecran: "Next screen", son: "Sound",
    clavier: "Keyboard: a b + − m (Shift: long press), n night, d signal, g puck, c compass, r spin, p power, v next screen.",
    memoire: "Memory",
    memoire_detail: "Trips and settings stay in this browser, like in the device's memory (except with the actions: session memory).",
    raz: "Reset", razFaite: "memory reset", journal: "Log",
    traceChargement: (mo) => `Loading the trace (${mo})…`,
    erreurModule: "The simulator could not be loaded.", erreurTrace: "The trace could not be loaded.",
    webgl: "WebGL unavailable: 2D screen.",
    bilanEnCours: (ok, n) => `${ok} / ${n}`, bilanFin: (e) => (e ? `${e} failed` : "all passed"),
    attendu: "expected",
    version: (b) => `Module ${b.version}${b.git ? ` · ${b.git}` : ""}${b.boot_anim ? "" : " · no boot animation"}`,
    etat: (i, v) => [horloge(i[I.t]), `${fmt(i[I.vit], 0)} km/h`, `A ${fmt(i[I.a], 2)}`, `B ${fmt(i[I.b], 2)}`,
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

const DRAPEAUX = {
  fr: '<svg viewBox="0 0 3 2"><path fill="#002654" d="M0 0h1v2H0z"/><path fill="#fff" d="M1 0h1v2H1z"/><path fill="#ce1126" d="M2 0h1v2H2z"/></svg>',
  en: '<svg viewBox="0 0 60 30" preserveAspectRatio="none"><clipPath id="uj"><path d="M30 15h30v15zv15H0zH0V0zV0h30z"/></clipPath><path fill="#012169" d="M0 0v30h60V0z"/><path stroke="#fff" stroke-width="6" d="M0 0l60 30m0-30L0 30"/><path stroke="#c8102e" stroke-width="4" clip-path="url(#uj)" d="M0 0l60 30m0-30L0 30"/><path stroke="#fff" stroke-width="10" d="M30 0v30M0 15h60"/><path stroke="#c8102e" stroke-width="6" d="M30 0v30M0 15h60"/></svg>',
};

const params = new URLSearchParams(location.search);
const $ = (id) => document.getElementById(id);
const lit = (cle, defaut) => { try { return JSON.parse(localStorage.getItem(cle)) ?? defaut; } catch { return defaut; } };
const ecrit = (cle, v) => { try { localStorage.setItem(cle, JSON.stringify(v)); } catch { /* stockage indisponible */ } };

function langueInitiale() {
  const demandee = params.get("lang");
  if (demandee in TEXTES) return demandee;
  try {
    const gardee = localStorage.getItem("raidmaster-langue");
    if (gardee in TEXTES) return gardee;
  } catch { /* stockage indisponible */ }
  return (navigator.language || "fr").toLowerCase().startsWith("fr") ? "fr" : "en";
}
let langue = langueInitiale();
const t = (cle) => TEXTES[langue][cle];
const fmt = (v, d) => (langue === "fr" ? v.toFixed(d).replace(".", ",") : v.toFixed(d));
const horloge = (ms) => {
  const s = Math.floor(ms / 1000);
  return `${String(Math.floor(s / 3600)).padStart(2, "0")}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
};

// --- état de la page

let build = null;            // build.json
let wasm = null;             // module WebAssembly compilé une fois, instancié à chaque session
let scenarios = [];
let importe = null;          // { nom, octets }
let audio = null;            // { ctx, sortie }
let allume = false;
let M = null;                // instance du module en cours
let session = null;          // { vitesseGps, actions, trace }
let info = new Float64Array(I.n);
let derniereImage = -1;
let choix = { scenario: params.get("scenario") ?? lit(CHOIX, {}).scenario ?? "s04_mountain_pass",
              vitesse: Number(params.get("vitesse")) || lit(CHOIX, {}).vitesse || VITESSE_DEFAUT,
              actions: params.has("actions") ? params.get("actions") === "1" : false };
const journal = [];
let journalSale = false;
const expects = [];

// --- journal

function note(ligne) {
  journal.push(ligne);
  if (journal.length > 400) journal.splice(0, journal.length - 400);
  journalSale = true;
}
function afficheJournal() {
  if (!journalSale) return;
  journalSale = false;
  const pre = $("journal");
  const enBas = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 24;
  pre.textContent = journal.join("\n");
  if (enBas) pre.scrollTop = pre.scrollHeight;
}

// --- module

function chargeScript(src) {
  return new Promise((ok, ko) => {
    const s = document.createElement("script");
    s.src = src;
    s.onload = ok;
    s.onerror = () => ko(new Error(src));
    document.head.append(s);
  });
}

async function prepareModule() {
  build = await (await fetch("build.json", { cache: "no-store" })).json();
  const [, w] = await Promise.all([
    chargeScript(`${build.module}.js`),
    fetch(`${build.module}.wasm`).then((r) => r.arrayBuffer()).then((b) => WebAssembly.compile(b)),
  ]);
  scenarios = (await (await fetch("scenarios/index.json", { cache: "no-cache" })).json()).scenarios;
  $("version").textContent = t("version")(build);
  wasm = w;      // en dernier : pret() attend le module et la liste des scénarios
}

async function octetsScenario(s) {
  $("attente").hidden = false;
  $("attente").textContent = t("traceChargement")(t("mo")(s.bytes / 1e6));
  const [nmea, actions] = await Promise.all([
    fetch(`scenarios/${s.nmea}`).then((r) => { if (!r.ok) throw new Error(s.nmea); return r.arrayBuffer(); }),
    fetch(`scenarios/${s.actions}`).then((r) => r.text()),
  ]);
  return { nmea: new Uint8Array(nmea), actions };
}

// Relance une session : trajet choisi, actions ou non, mémoire du navigateur ou de la session.
let lancement = 0;
async function lance() {
  if (!allume || !wasm) return;
  const numero = ++lancement;
  if (M) M._sim_save();
  M = null;
  let nmea, actionsTexte = null, vitesseGps = 1, persistant = true, nom;
  try {
    if (choix.scenario === "importe" && importe) {
      nmea = importe.octets;
      nom = importe.nom;
    } else {
      const s = scenarios.find((e) => e.id === choix.scenario) ?? scenarios[0];
      choix.scenario = s.id;
      const o = await octetsScenario(s);
      nmea = o.nmea;
      nom = s.id;
      if (choix.actions) {
        actionsTexte = o.actions;
        vitesseGps = s.speed;
        persistant = false;
      }
    }
  } catch (e) {
    console.error(e);
    $("attente").textContent = t("erreurTrace");
    return;
  }
  if (numero !== lancement) return;      // un autre lancement a suivi
  expects.length = 0;
  afficheExpects();
  $("bloc-expect").hidden = actionsTexte === null;
  note(`── ${nom}${actionsTexte ? ` · actions ×${vitesseGps}` : ""}`);
  const m = await createRaidmasterSim({
    instantiateWasm: (imports, recoit) => {
      WebAssembly.instantiate(wasm, imports).then((inst) => recoit(inst, wasm));
      return {};
    },
    print: note,
    printErr: note,
    audioCtx: audio?.ctx,
    audioOut: audio?.sortie,
    onExpect: (champ, obtenu, attendu, tol, ok, tMs) => { expects.push({ champ, obtenu, attendu, tol, ok, tMs }); afficheExpects(); },
    onScriptEnd: (echecs) => afficheExpects(echecs),
  });
  if (numero !== lancement) return;
  m.FS.writeFile("/trace.nmea", nmea);
  if (actionsTexte !== null) m.FS.writeFile("/actions.txt", actionsTexte);
  const rc = m.ccall("sim_start", "number", ["string", "number", "string", "number"],
                     ["/trace.nmea", vitesseGps, actionsTexte !== null ? "/actions.txt" : "", persistant ? 1 : 0]);
  if (rc !== 0) {
    $("attente").hidden = false;
    $("attente").textContent = t("erreurTrace");
    return;
  }
  session = { vitesseGps, actions: actionsTexte !== null, trace: litTrace(nmea) };
  M = m;
  derniereImage = -1;
  $("attente").hidden = true;
  remplitVitesses();
  appliqueVitesse();
  traceSurCarte();
}

function appliqueVitesse() {
  if (M) M._sim_set_rate(choix.vitesse / session.vitesseGps);
}

function lireInfo() {
  info = new Float64Array(M.HEAPF64.buffer, M._sim_info(), I.n).slice();
}

// --- panneau

function remplitScenarios() {
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
  if (importe) {
    const o = document.createElement("option");
    o.value = "importe";
    o.textContent = t("importe")(importe.nom);
    sel.prepend(o);
  }
  sel.value = choix.scenario === "importe" && !importe ? scenarios[0]?.id : choix.scenario;
  if (!sel.value && scenarios[0]) sel.value = scenarios[0].id;
  choix.scenario = sel.value;
  detailScenario();
}

function detailScenario() {
  const s = scenarios.find((e) => e.id === choix.scenario);
  $("scenario-detail").textContent = s ? t("detailScenario")(s, t("mo")(s.bytes / 1e6))
    : importe ? t("detailImporte")(t("mo")(importe.octets.length / 1e6)) : "";
  $("actions").disabled = choix.scenario === "importe";
}

function remplitVitesses() {
  const liste = [...VITESSES];
  if (session && !liste.includes(session.vitesseGps)) liste.push(session.vitesseGps);
  liste.sort((a, b) => a - b);
  const sel = $("vitesse");
  sel.textContent = "";
  for (const v of liste) {
    const o = document.createElement("option");
    o.value = v;
    o.textContent = `×${v}`;
    sel.append(o);
  }
  if (session?.actions && !params.has("vitesse") && !choix.vitesseFixee) choix.vitesse = session.vitesseGps;
  if (!liste.includes(choix.vitesse)) choix.vitesse = VITESSE_DEFAUT;
  sel.value = choix.vitesse;
}

function afficheExpects(fin) {
  const ol = $("expects");
  ol.textContent = "";
  for (const e of expects) {
    const li = document.createElement("li");
    li.className = e.ok ? "ok" : "ko";
    const a = document.createElement("span");
    a.textContent = `${fmt(e.tMs / 1000, 1)} s`;
    const b = document.createElement("span");
    b.textContent = `${e.champ} ${e.obtenu.toFixed(3)} (${t("attendu")} ${e.attendu}${e.tol ? ` ± ${e.tol}` : ""}) ${e.ok ? "✓" : "✗"}`;
    li.append(a, b);
    ol.append(li);
  }
  ol.scrollTop = ol.scrollHeight;
  const ko = expects.filter((e) => !e.ok).length;
  const bilan = $("bilan");
  if (fin !== undefined) {
    bilan.textContent = t("bilanFin")(fin);
    bilan.className = `bilan ${fin ? "ko" : "ok"}`;
  } else {
    bilan.textContent = expects.length ? t("bilanEnCours")(expects.length - ko, expects.length) : "";
    bilan.className = `bilan ${ko ? "ko" : "ok"}`;
  }
}

function etatEssais() {
  for (const b of $("essais").children) {
    const e = b.dataset.etat;
    if (!e) continue;
    const v = { night: info[I.nuit], signal: info[I.signal], gps: info[I.gps], mag: info[I.mag], spin: info[I.spin] }[e];
    b.setAttribute("aria-pressed", String(Boolean(v)));
    b.classList.toggle("hors", ["signal", "gps", "mag"].includes(e) && !v);
  }
}

$("scenario").addEventListener("change", (e) => {
  choix.scenario = e.target.value;
  ecrit(CHOIX, { scenario: choix.scenario, vitesse: choix.vitesse });
  detailScenario();
  lance();
});
$("actions").addEventListener("change", (e) => {
  choix.actions = e.target.checked;
  choix.vitesseFixee = false;
  lance();
});
$("vitesse").addEventListener("change", (e) => {
  choix.vitesse = Number(e.target.value);
  choix.vitesseFixee = true;
  ecrit(CHOIX, { scenario: choix.scenario, vitesse: choix.vitesse });
  appliqueVitesse();
});
$("relancer").addEventListener("click", () => lance());
for (const b of $("essais").querySelectorAll("[data-touche]")) b.addEventListener("click", () => M?._sim_key(b.dataset.touche.charCodeAt(0)));
$("raz").addEventListener("click", () => {
  M = null;      // sans sauvegarde : la session en cours ne réécrit pas la mémoire effacée
  try {
    for (let i = localStorage.length - 1; i >= 0; i--) {
      const k = localStorage.key(i);
      if (k?.startsWith(NVS)) localStorage.removeItem(k);
    }
  } catch { /* stockage indisponible */ }
  note(`[page] ${t("razFaite")}`);
  lance();
});

// Import d'une trace NMEA : champ fichier ou glisser-déposer sur la page.
async function importeFichier(f) {
  if (!f) return;
  importe = { nom: f.name, octets: new Uint8Array(await f.arrayBuffer()) };
  choix.scenario = "importe";
  choix.actions = false;
  $("actions").checked = false;
  remplitScenarios();
  lance();
}
$("fichier").addEventListener("change", (e) => importeFichier(e.target.files[0]));
document.addEventListener("dragover", (e) => { e.preventDefault(); document.body.classList.add("glisse"); });
document.addEventListener("dragleave", (e) => { if (!e.relatedTarget) document.body.classList.remove("glisse"); });
document.addEventListener("drop", (e) => {
  e.preventDefault();
  document.body.classList.remove("glisse");
  importeFichier(e.dataTransfer.files[0]);
});

// --- boutons : capuchons 3D, boutons 2D, clavier

let appui = null;     // { id, long, minuterie, fin }
function presse(id) {
  if (!M || appui) return;
  appui = { id, long: false };
  marqueAppui(id, "appui");
  appui.minuterie = setTimeout(() => {
    appui.long = true;
    M?._sim_button(id, 1);
    marqueAppui(id, "long");
  }, APPUI_LONG_MS);
}
function relache() {
  if (!appui) return;
  clearTimeout(appui.minuterie);
  if (!appui.long) M?._sim_button(appui.id, 0);
  appui = null;
  marqueAppui(null);
}

const TOUCHES = { a: 0, b: 1, "+": 2, "=": 2, "-": 3, m: 4 };
document.addEventListener("keydown", (e) => {
  if (e.repeat || e.ctrlKey || e.metaKey || e.altKey || !M) return;
  if (e.target.closest("input, select, textarea")) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (k in TOUCHES) {
    M._sim_button(TOUCHES[k], e.shiftKey ? 1 : 0);
  } else if ("ndgcrpv".includes(k) && k.length === 1) {
    M._sim_key(k.charCodeAt(0));
  } else {
    return;
  }
  e.preventDefault();
});

for (const b of $("boutons2d").children) {
  b.addEventListener("pointerdown", (e) => {
    try { b.setPointerCapture(e.pointerId); } catch { /* pointeur synthétique */ }
    presse(Number(b.dataset.bouton));
  });
  b.addEventListener("pointerup", relache);
  b.addEventListener("pointercancel", relache);
  b.addEventListener("contextmenu", (e) => e.preventDefault());
}

// --- vue 3D

const zone = $("scene");
const canvas = $("vue");
let renderer = null;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "high-performance" });
} catch {
  note(`[page] ${TEXTES[langue].webgl}`);
}
const ecran2d = $("ecran2d").getContext("2d");
const image2d = ecran2d.createImageData(W, H);

// Vue « écran seul » : la sortie vidéo sur toute la largeur du cadre, barre des boutons dessous. Bouton de
// bascule dans le cadre, choix gardé dans le navigateur ; ?vue=2d|3d l'impose ; forcée sans WebGL.
let ecranSeul = !renderer || (params.has("vue") ? params.get("vue") === "2d" : lit("raidmaster-sim.vue", "3d") === "2d");
function appliqueVue() {
  canvas.hidden = ecranSeul;
  $("ecran2d").hidden = !ecranSeul;
  $("boutons2d").hidden = !ecranSeul;
  zone.classList.toggle("ecran-seul", ecranSeul);
  const b = $("bascule-vue");
  b.hidden = !renderer;
  b.textContent = t(ecranSeul ? "vue3d" : "ecranSeul");
  b.setAttribute("aria-pressed", String(ecranSeul));
  $("recentrer").hidden = !renderer || ecranSeul;
  derniereImage = -1;     // recopie l'image courante dans la vue affichée
  sale = true;
}
// Recentrer : la vue 3D de départ (de face, boîtier entier), après une rotation ou un zoom.
$("recentrer").addEventListener("click", () => cadreFacade());
$("bascule-vue").addEventListener("click", () => {
  ecranSeul = !ecranSeul;
  ecrit("raidmaster-sim.vue", ecranSeul ? "2d" : "3d");
  appliqueVue();
  if (!ecranSeul) cadreFacade();
  pret();
});

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(24, 1, 5, 8000);
let controls = null;
let sale = true;
let capuchons = null;
let marques = [];         // anneau d'appui de chaque capuchon
let cadre = null;         // { centre, taille } du modèle
const texture = new THREE.DataTexture(new Uint8Array(W * H * 4), W, H, THREE.RGBAFormat);
texture.colorSpace = THREE.SRGBColorSpace;
texture.magFilter = THREE.LinearFilter;
texture.minFilter = THREE.LinearMipmapLinearFilter;
texture.generateMipmaps = true;
texture.flipY = false;    // UV du nœud « ecran » : v = 0 en haut, comme la ligne 0 du framebuffer

const raycaster = new THREE.Raycaster();
const pointeur = new THREE.Vector2();
function capuchonSous(e) {
  if (!capuchons) return null;
  const r = canvas.getBoundingClientRect();
  pointeur.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  raycaster.setFromCamera(pointeur, camera);
  // Maillage du capuchon seulement : ni ses arêtes (lignes) ni les anneaux d'appui.
  const hit = raycaster.intersectObject(capuchons, true).find((h) => h.object.isMesh && !marques.includes(h.object));
  if (!hit) return null;
  const p = capuchons.worldToLocal(hit.point.clone());
  const rang = Math.round(p.x / PAS_CAPUCHONS) + 2;
  return rang >= 0 && rang < 5 ? ORDRE_CAPUCHONS[rang] : null;
}

function marqueAppui(id, etat) {
  for (const b of $("boutons2d").children) {
    const moi = Number(b.dataset.bouton) === id;
    b.classList.toggle("appui", moi);
    b.classList.toggle("long", moi && etat === "long");
  }
  marques.forEach((m, rang) => {
    m.visible = ORDRE_CAPUCHONS[rang] === id;
    m.material.color.set(etat === "long" ? 0xff6b5e : 0xffb547);
  });
  sale = true;
}

if (renderer) {
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.toneMapping = THREE.NeutralToneMapping;
  scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
  scene.environmentIntensity = 0.7;
  scene.add(new THREE.HemisphereLight(0xffffff, 0x33373c, 0.9));
  const cle = new THREE.DirectionalLight(0xffffff, 1.8);
  cle.position.set(200, 260, 240);
  scene.add(cle);

  // Avant OrbitControls : un appui sur un capuchon ne fait pas tourner la vue.
  canvas.addEventListener("pointerdown", (e) => {
    const id = capuchonSous(e);
    if (id === null) return;
    e.stopImmediatePropagation();
    try { canvas.setPointerCapture(e.pointerId); } catch { /* pointeur synthétique */ }
    presse(id);
  });
  canvas.addEventListener("pointerup", relache);
  canvas.addEventListener("pointercancel", relache);
  canvas.addEventListener("pointermove", (e) => {
    if (e.pointerType === "mouse" && !e.buttons) canvas.classList.toggle("sur-bouton", capuchonSous(e) !== null);
  });
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());
  controls = new OrbitControls(camera, canvas);
  controls.enableDamping = true;
  controls.dampingFactor = 0.09;
  controls.minDistance = 60;
  controls.maxDistance = 1500;
  controls.addEventListener("change", () => { sale = true; });

  new GLTFLoader().load("../raidmaster.glb", prepareModele,
    (e) => {
      if (!allume) {
        $("attente").textContent = t("chargementModele")(e.lengthComputable
          ? `${Math.round((e.loaded / e.total) * 100)} %` : t("mo")(e.loaded / 1e6));
      }
    },
    (err) => { console.error(err); note(`[page] ${err.message ?? err}`); });
}

function prepareModele(gltf) {
  const racine = gltf.scene;
  racine.scale.setScalar(1000);       // modèle en mètres, scène en millimètres
  scene.add(racine);
  const infos = JSON.parse(racine.userData.maquette ?? "{}");
  // Connectique GPS : la version du modèle (puck par défaut) ; câble de démonstration retiré.
  const versions = new Set();
  racine.traverse((o) => { if (o.userData.version) o.userData.version.split(" ").forEach((v) => versions.add(v)); });
  const version = versions.has(params.get("gps")) ? params.get("gps") : versions.has(infos.gps) ? infos.gps : [...versions][0];
  const clair = new THREE.LineBasicMaterial({ color: 0xa8adb3, transparent: true, opacity: 0.34 });
  const fonce = new THREE.LineBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.4 });
  racine.traverse((o) => {
    if (!o.userData.eclate) return;
    const v = o.userData.version;
    o.visible = o.userData.groupe !== "cable" && (!v || v.split(" ").includes(version));
    if (o.userData.aretes) {
      o.traverse((m) => {
        if (!m.isMesh) return;
        const c = m.material.color;
        m.add(new THREE.LineSegments(new THREE.EdgesGeometry(m.geometry, 28), c.r + c.g + c.b < 0.2 ? clair : fonce));
      });
    }
  });
  // Écran : le framebuffer du firmware, sans éclairage ni tone mapping (couleurs de LVGL telles quelles).
  const ecran = racine.getObjectByName("ecran");
  ecran.traverse((m) => {
    if (m.isMesh) m.material = new THREE.MeshBasicMaterial({ map: texture, toneMapped: false });
  });
  capuchons = racine.getObjectByName("capuchons");
  marques = [-2, -1, 0, 1, 2].map((k) => {
    const m = new THREE.Mesh(new THREE.RingGeometry(0.0062, 0.0078, 40),
      new THREE.MeshBasicMaterial({ color: 0xffb547, transparent: true, opacity: 0.9, toneMapped: false, depthTest: false }));
    m.position.set(k * PAS_CAPUCHONS, CENTRE_CAPUCHONS_Y, 0.0046);
    m.renderOrder = 2;
    m.visible = false;
    capuchons.add(m);
    return m;
  });
  racine.updateMatrixWorld(true);
  const b = new THREE.Box3();
  racine.traverseVisible((o) => { if (o.isMesh) b.expandByObject(o); });
  cadre = { centre: b.getCenter(new THREE.Vector3()), taille: b.getSize(new THREE.Vector3()) };
  cadreFacade();
  sale = true;
  pret();
}

// « Allumer » quand le module et le modèle sont là.
function pret() {
  if (allume || !wasm || (renderer && !ecranSeul && !capuchons)) return;
  $("attente").textContent = "";
  $("allumer").hidden = false;
  if (params.get("allumer") === "1") allumer();
}

// Caméra de face, le boîtier entier dans l'image : sa face avant (la plus proche, donc la plus grande à
// l'écran) occupe au plus 88 % de la hauteur ou de la largeur de la vue.
function cadreFacade() {
  if (!cadre || !controls) return;
  const axe = new THREE.Vector3(0, 0, 1);
  const tv = Math.tan(THREE.MathUtils.degToRad(camera.fov) / 2) * 0.88;
  const d = Math.max(cadre.taille.y / 2 / tv, cadre.taille.x / 2 / (tv * camera.aspect)) + cadre.taille.z / 2;
  controls.target.copy(cadre.centre);
  camera.position.copy(cadre.centre).addScaledVector(axe, d);
  controls.update();
  sale = true;
}

new ResizeObserver(() => {
  const w = zone.clientWidth, h = zone.clientHeight;
  if (!w || !h || !renderer) return;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  cadreFacade();
}).observe(zone);

// --- carte

// « Centrer » (interrupteur, actif au départ) : le véhicule au centre de la carte en permanence, zoom autour de
// lui ; un glisser de la carte le coupe, un clic le remet.
let carte = null, traceGrise = null, traceFaite = null, marqueur = null, suivre = true, zoomEnCours = false;
const fleche = (couleur) => L.divIcon({
  className: "fleche", iconSize: [26, 26], iconAnchor: [13, 13],
  html: `<svg viewBox="-13 -13 26 26"><g><circle r="11" fill="#0f1011" fill-opacity=".75" stroke="${couleur}" stroke-width="2"/><path d="M0 -8 L5.5 6 L0 3 L-5.5 6 Z" fill="${couleur}"/></g></svg>`,
});
if (window.L) {
  carte = L.map("carte", { zoomControl: true, attributionControl: true }).setView([48.85, 2.36], 12);
  L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>',
  }).addTo(carte);
  traceGrise = L.polyline([], { color: "#2b3a4a", weight: 3, opacity: 0.85, dashArray: "7 6" }).addTo(carte);
  traceFaite = L.polyline([], { color: "#ffb547", weight: 4, opacity: 0.95 }).addTo(carte);
  marqueur = L.marker([48.85, 2.36], { icon: fleche("#ffb547"), interactive: false, keyboard: false });
  carte.on("dragstart", () => marqueSuivi(false));
  carte.on("zoomstart", () => { zoomEnCours = true; });
  carte.on("zoomend", () => { zoomEnCours = false; });
} else {
  $("attente-carte").hidden = false;
  $("attente-carte").textContent = "Leaflet ?";
}
function marqueSuivi(v) {
  suivre = v;
  $("suivre").setAttribute("aria-pressed", String(v));
  if (carte) {
    // Leaflet relit ces options à chaque geste : zoom centré sur le véhicule tant qu'on le suit.
    carte.options.scrollWheelZoom = carte.options.touchZoom = carte.options.doubleClickZoom = v ? "center" : true;
  }
}
marqueSuivi(suivre);
$("suivre").addEventListener("click", () => {
  marqueSuivi(!suivre);
  if (suivre && marqueur && carte?.hasLayer(marqueur)) carte.panTo(marqueur.getLatLng());
});

// Trace complète lue dans le NMEA : points RMC valides, temps depuis la première phrase horodatée (comme le
// rejeu de hal_gps_sim.c), longitude déroulée (méridien 180°).
function litTrace(octets) {
  const texte = new TextDecoder("latin1").decode(octets);
  const tps = [], lats = [], lons = [];
  let t0 = null, dernier = -1, ajout = 0, lonPrec = null;
  const deg = (v, h, n) => {
    const x = Number(v);
    const d = Math.floor(x / 100);
    return (d + (x - d * 100) / 60) * (h === "S" || h === "W" ? -1 : 1) || (n ? NaN : 0);
  };
  for (const ligne of texte.split("\n")) {
    const c = ligne.indexOf(",");
    if (c < 0) continue;
    const h = /^(\d\d)(\d\d)(\d\d(?:\.\d+)?)/.exec(ligne.slice(c + 1, c + 11));
    if (!h) continue;
    let tod = (+h[1] * 3600 + +h[2] * 60 + +h[3]);
    if (dernier >= 0 && tod + 12 * 3600 < dernier) ajout += 24 * 3600;
    dernier = tod;
    tod += ajout;
    t0 ??= tod;
    if (ligne.slice(3, 6) !== "RMC") continue;
    const f = ligne.split(",");
    if (f[2] !== "A") continue;
    const lat = deg(f[3], f[4], true), lon0 = deg(f[5], f[6], true);
    if (!Number.isFinite(lat) || !Number.isFinite(lon0)) continue;
    let lon = lon0;
    if (lonPrec !== null) lon += Math.round((lonPrec - lon) / 360) * 360;
    lonPrec = lon;
    tps.push(tod - t0);
    lats.push(lat);
    lons.push(lon);
  }
  const pas = Math.max(1, Math.ceil(tps.length / MAX_POINTS));
  const points = [], temps = [];
  for (let i = 0; i < tps.length; i += pas) {
    points.push([lats[i], lons[i]]);
    temps.push(tps[i]);
  }
  if (tps.length && (tps.length - 1) % pas) {
    points.push([lats.at(-1), lons.at(-1)]);
    temps.push(tps.at(-1));
  }
  return { points, temps, lonPrec: null };
}

function traceSurCarte() {
  if (!carte || !session) return;
  const { points } = session.trace;
  traceGrise.setLatLngs(points);
  traceFaite.setLatLngs([]);
  if (carte.hasLayer(marqueur)) marqueur.remove();
  if (points.length) carte.fitBounds(L.latLngBounds(points), { padding: [24, 24], maxZoom: 16 });
  marqueSuivi(true);
  session.indexCarte = -1;
}

// Partie parcourue (temps de la trace = temps simulé × vitesse de rejeu) et position reçue par le boîtier.
function majCarte() {
  if (!carte || !session) return;
  const { points, temps } = session.trace;
  const ecoule = (info[I.t] / 1000) * session.vitesseGps;
  let lo = 0, hi = temps.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (temps[mid] <= ecoule) lo = mid + 1; else hi = mid;
  }
  if (lo !== session.indexCarte) {
    session.indexCarte = lo;
    traceFaite.setLatLngs(points.slice(0, lo));
  }
  if (info[I.pos]) {
    let lon = info[I.lon];
    const ref = points[Math.max(0, Math.min(lo, points.length - 1))];
    if (ref) lon += Math.round((ref[1] - lon) / 360) * 360;
    const ll = L.latLng(info[I.lat], lon);
    if (!carte.hasLayer(marqueur)) marqueur.addTo(carte);
    marqueur.setLatLng(ll);
    const couleur = info[I.fix] ? "#ffb547" : "#949aa0";
    if (marqueur.couleur !== couleur) {
      marqueur.setIcon(fleche(couleur));
      marqueur.couleur = couleur;
    }
    const g = marqueur.getElement()?.querySelector("g");
    if (g && info[I.route] >= 0) g.setAttribute("transform", `rotate(${info[I.route]})`);
    if (suivre && !zoomEnCours) {
      const p = carte.latLngToContainerPoint(ll), c = carte.getSize().divideBy(2);
      if (p.distanceTo(c) > 0.5) carte.panTo(ll, { animate: false });
    }
  }
}

// --- onglets (téléphone)

for (const b of $("onglets").children) {
  b.addEventListener("click", () => {
    for (const c of $("onglets").children) c.setAttribute("aria-pressed", String(c === b));
    $("banc").dataset.onglet = b.dataset.onglet;
    if (b.dataset.onglet === "carte") carte?.invalidateSize();
    sale = true;
  });
}

// --- langue

function changeLangue(l) {
  langue = l in TEXTES ? l : "fr";
  document.documentElement.lang = langue;
  document.title = t("titre");
  document.querySelector('meta[name="description"]').content = t("description");
  for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t(e.dataset.i18n);
  for (const e of document.querySelectorAll("[data-i18n-aria]")) e.setAttribute("aria-label", t(e.dataset.i18nAria));
  $("langue").value = langue;
  $("drapeau").innerHTML = DRAPEAUX[langue];
  try { localStorage.setItem("raidmaster-langue", langue); } catch { /* stockage indisponible */ }
  if (scenarios.length) remplitScenarios();
  appliqueVue();
  if (build) $("version").textContent = t("version")(build);
  afficheExpects();
}
$("langue").addEventListener("change", (e) => changeLangue(e.target.value));
changeLangue(langue);

// --- son : coupé par défaut (décision du propriétaire, 2026-11-12), bouton « Son », choix gardé dans le navigateur.
// Le contexte audio naît au premier clic (les navigateurs refusent le son avant un geste de l'utilisateur) ; coupé,
// le jingle part quand même dans une sortie à volume nul.

const VOLUME = 0.5;
let son = params.has("son") ? params.get("son") === "1" : lit("raidmaster-sim.son", false) === true;
function marqueSon() {
  $("son").setAttribute("aria-pressed", String(son));
  if (audio) audio.sortie.gain.value = son ? VOLUME : 0;
}
function creeAudio() {
  if (audio) return;
  try {
    const ctx = new AudioContext();
    const sortie = ctx.createGain();
    sortie.connect(ctx.destination);
    audio = { ctx, sortie };
  } catch { /* pas de son */ }
  marqueSon();
}
$("son").addEventListener("click", () => {
  son = !son;
  ecrit("raidmaster-sim.son", son);
  creeAudio();
  audio?.ctx.resume();
  marqueSon();
});
marqueSon();

// --- allumage : premier clic

function allumer() {
  if (allume) return;
  allume = true;
  $("allumer").hidden = true;
  creeAudio();
  lance();
}
$("allumer").addEventListener("click", allumer);

// --- boucle

let avant = performance.now();
let prochaineCarte = 0, prochainEtat = 0;
function boucle(now) {
  requestAnimationFrame(boucle);
  const dt = now - avant;
  avant = now;
  if (M) {
    M._sim_frame(dt);
    lireInfo();
    if (info[I.image] !== derniereImage) {
      derniereImage = info[I.image];
      const fb = new Uint8Array(M.HEAPU8.buffer, M._sim_fb(), W * H * 4);
      if (!ecranSeul) {
        texture.image.data = fb;
        texture.needsUpdate = true;
        sale = true;
      } else {
        image2d.data.set(fb);
        ecran2d.putImageData(image2d, 0, 0);
      }
    }
    if (now >= prochainEtat) {
      prochainEtat = now + 250;
      $("etat").textContent = t("etat")(info, choix.vitesse).join("   ");
      etatEssais();
    }
    if (now >= prochaineCarte) {
      prochaineCarte = now + 150;
      majCarte();
    }
  }
  afficheJournal();
  if (controls?.update()) sale = true;
  if (renderer && !ecranSeul && sale) {
    sale = false;
    renderer.render(scene, camera);
  }
}
requestAnimationFrame(boucle);

addEventListener("pagehide", () => M?._sim_save());

// Accès de mise au point et d'essai (console, chrome-devtools) : module en cours, état, appuis.
window.raidmasterSim = {
  get module() { return M; },
  get info() { return Object.fromEntries(Object.entries(I).filter(([k]) => k !== "n").map(([k, i]) => [k, info[i]])); },
  get expects() { return expects.slice(); },
  bouton: (id, long = false) => M?._sim_button(id, long ? 1 : 0),
  vise: (x, y) => capuchonSous({ clientX: x, clientY: y }),
  // Centre de chaque capuchon dans la page (x, y en px), de gauche à droite, avec l'identifiant de son bouton.
  capuchons: () => {
    if (!capuchons) return [];
    const r = canvas.getBoundingClientRect();
    return [-2, -1, 0, 1, 2].map((k, rang) => {
      const p = capuchons.localToWorld(new THREE.Vector3(k * PAS_CAPUCHONS, CENTRE_CAPUCHONS_Y, 0.004)).project(camera);
      return { id: ORDRE_CAPUCHONS[rang], x: r.left + ((p.x + 1) / 2) * r.width, y: r.top + ((1 - p.y) / 2) * r.height };
    });
  },
};

$("actions").checked = choix.actions;
prepareModule().then(() => {
  remplitScenarios();
  remplitVitesses();
  pret();
}).catch((e) => {
  console.error(e);
  $("attente").textContent = t("erreurModule");
});
