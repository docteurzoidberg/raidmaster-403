// Raidmaster 403 — accueil du site : textes français et anglais, en-tête commun (common/common.js).
//
// Licence : GPL-3.0.

import { applyLanguage, initialLanguage, onLanguageChange } from "./common/common.js";

const TEXTS = {
  fr: {
    title: "Raidmaster 403",
    description: "Raidmaster 403 : un tripmaster GPS de rallye-raid à construire soi-même. Projet DIY, open source et open hardware, non commercial.",
    eyebrow: "Projet DIY · open source · open hardware",
    altScreen: "L'écran du Raidmaster : démarrage animé de la 403 sur la grille, écran raid, balade, menu, calibration, mode nuit",
    tagline: "Un tripmaster GPS de rallye-raid à construire soi-même. Rien à vendre : le code, la carte électronique et le boîtier sont publiés sous licence libre.",
    note: "L'écran ci-dessus fait tourner le vrai firmware, compilé pour le navigateur.",
    try: "Essayer le simulateur", see3d: "Voir en 3D", source: "Sources sur GitHub",
    published: "Ce qui est publié", bEnclosure: "Boîtier STEP", bNonCommercial: "Non commercial",
    n1: "01 · L'écran", t1: "Les chiffres du roadbook, lisibles d'un coup d'œil",
    d1: "Distances partielles A et B, odomètre, vitesse, cap, moyenne et chrono. Mode raid pour l'épreuve, mode balade pour tous les jours, mode nuit, calibration automatique de la distance.",
    tagTouring: "Balade", tagNight: "Nuit", tagCalibration: "Calibration auto",
    altDemo: "Démo de l'interface : démarrage animé, écran raid, balade, menu, calibration, mode nuit",
    demoCaption: "Simulateur, échelle 1:1 de l'écran 480 × 320.",
    n2: "02 · Le boîtier", t2: "Autonome, sans fil vers la voiture",
    d2: "Imprimé en PETG ou ASA (l'habitacle monte à 60 °C en été), pare-chocs en TPU, cinq boutons (A, B, menu, haut, bas), batterie interne rechargée en USB-C. Les fichiers STEP de chaque pièce sont publiés.",
    see3dArrow: "Voir en 3D →",
    altExploded: "Rendu éclaté du boîtier : coque, façade, capuchons, trappes, pare-chocs et cartes",
    n3: "03 · L'électronique", t3: "Une Waveshare ESP32-S3 et une carte fille maison",
    altPcbTop: "Carte fille et carte boutons, face dessus", altPcbBottom: "Carte fille et carte boutons, face dessous",
    kBoard: "Carte", vBoard: "ESP32-S3, 8 Mo PSRAM, 16 Mo flash", kScreen: "Écran", vScreen: "3,5\" IPS 480 × 320, tactile",
    vGps: "NEO-M9N interne ou puck NEO-M8N", kHeading: "Cap", vHeading: "Magnétomètre QMC5883L", kPower: "Énergie",
    n4: "04 · L'histoire",
    d4: "Fait maison pour une Peugeot 403 Familiale de 1956, pensé pour le raid Crazy Dust 2027 et utilisable au quotidien en mode balade.",
    d4b: "Distance GPS uniquement : ni capteur de roue, ni intervention sur le câblage de la voiture.",
    n5: "05 · À construire soi-même", t5: "Tout est publié, rien n'est à vendre",
    d5: "Le Raidmaster est un projet personnel, partagé tel quel : pas de boutique, pas de kit, pas de support ni de garantie. Les fichiers sont là pour être lus, reconstruits, modifiés et améliorés.",
    cSoftware: "Logiciel", cSoftwareText: "Firmware ESP-IDF et LVGL, simulateur PC et web, tests.",
    cElectronics: "Électronique", cElectronicsText: "Projet KiCad de la carte fille et de la carte boutons, modèles 3D.",
    cEnclosure: "Boîtier", cEnclosureText: "Un fichier STEP par pièce, à imprimer en PETG, ASA et TPU.",
    cLicence: "Licence", cLicenceText: "GPL-3.0 : libre d'utiliser, d'étudier, de modifier et de redistribuer.",
    tileMockup: "Les cartes dans le boîtier, assemblé ou éclaté.", open: "Ouvrir →",
    tileSim: "Le vrai firmware dans le navigateur, sur un trajet GPS rejoué.", tryArrow: "Essayer →",
    tileManual: "Mise en route, boutons, menus, calibration.", inProgress: "En cours de rédaction",
    footer: "Raidmaster 403 · projet DIY non commercial · licence GPL-3.0",
    footerSource: "Code, projet KiCad et STEP sur", design: "Conception :", builtBy: "réalisation :",
  },
  en: {
    title: "Raidmaster 403",
    description: "Raidmaster 403: a GPS rally-raid tripmaster to build yourself. A DIY, open source and open hardware project, not a commercial product.",
    eyebrow: "DIY project · open source · open hardware",
    altScreen: "The Raidmaster screen: animated start-up of the 403 on the grid, raid screen, touring screen, menu, calibration, night mode",
    tagline: "A GPS rally-raid tripmaster to build yourself. Nothing for sale: the code, the circuit board and the enclosure are published under a free licence.",
    note: "The screen above runs the real firmware, compiled for the browser.",
    try: "Try the simulator", see3d: "See it in 3D", source: "Source on GitHub",
    published: "What is published", bEnclosure: "Enclosure STEP", bNonCommercial: "Non-commercial",
    n1: "01 · The screen", t1: "The roadbook figures, readable at a glance",
    d1: "Trip distances A and B, odometer, speed, heading, average and stopwatch. Raid mode for the stages, touring mode for every day, night mode, automatic distance calibration.",
    tagTouring: "Touring", tagNight: "Night", tagCalibration: "Auto calibration",
    altDemo: "Interface demo: animated start-up, raid screen, touring screen, menu, calibration, night mode",
    demoCaption: "Simulator, 1:1 scale of the 480 × 320 screen.",
    n2: "02 · The enclosure", t2: "Self-contained, no wire to the car",
    d2: "Printed in PETG or ASA (the cabin reaches 60 °C in summer), TPU bumpers, five buttons (A, B, menu, up, down), internal battery charged over USB-C. The STEP file of every part is published.",
    see3dArrow: "See it in 3D →",
    altExploded: "Exploded render of the enclosure: back shell, front panel, button caps, doors, bumpers and boards",
    n3: "03 · The electronics", t3: "A Waveshare ESP32-S3 and a home-made daughter board",
    altPcbTop: "Daughter board and button board, top side", altPcbBottom: "Daughter board and button board, bottom side",
    kBoard: "Board", vBoard: "ESP32-S3, 8 MB PSRAM, 16 MB flash", kScreen: "Screen", vScreen: "3.5\" IPS 480 × 320, touch",
    vGps: "Built-in NEO-M9N or NEO-M8N puck", kHeading: "Heading", vHeading: "QMC5883L magnetometer", kPower: "Power",
    n4: "04 · The story",
    d4: "Home-made for a 1956 Peugeot 403 Familiale, designed for the Crazy Dust 2027 raid and usable every day in touring mode.",
    d4b: "GPS distance only: no wheel sensor, no changes to the car's wiring.",
    n5: "05 · Build it yourself", t5: "Everything is published, nothing is for sale",
    d5: "The Raidmaster is a personal project, shared as is: no shop, no kit, no support and no warranty. The files are there to be read, rebuilt, changed and improved.",
    cSoftware: "Software", cSoftwareText: "ESP-IDF and LVGL firmware, PC and web simulators, tests.",
    cElectronics: "Electronics", cElectronicsText: "KiCad project of the daughter board and the button board, 3D models.",
    cEnclosure: "Enclosure", cEnclosureText: "One STEP file per part, to print in PETG, ASA and TPU.",
    cLicence: "Licence", cLicenceText: "GPL-3.0: free to use, study, change and redistribute.",
    tileMockup: "The boards in the enclosure, assembled or exploded.", open: "Open →",
    tileSim: "The real firmware in the browser, on a replayed GPS trip.", tryArrow: "Try it →",
    tileManual: "Getting started, buttons, menus, calibration.", inProgress: "Being written",
    footer: "Raidmaster 403 · non-commercial DIY project · GPL-3.0 licence",
    footerSource: "Code, KiCad project and STEP files on", design: "Design:", builtBy: "built by",
  },
};

let lang = initialLanguage();

function setLanguage(l) {
  lang = applyLanguage(l);
  const t = TEXTS[lang];
  document.title = t.title;
  document.querySelector('meta[name="description"]').content = t.description;
  for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t[e.dataset.i18n];
  for (const e of document.querySelectorAll("[data-i18n-alt]")) e.alt = t[e.dataset.i18nAlt];
  for (const e of document.querySelectorAll("[data-i18n-aria]")) e.setAttribute("aria-label", t[e.dataset.i18nAria]);
}

onLanguageChange(setLanguage);
setLanguage(lang);

// Parallaxe du sol en grille (fond fixe) : l'horizon remonte d'au plus RISE de la hauteur d'écran sur toute la
// page, le sol s'atténue un peu après le haut de page.
const RISE = 0.3;
const root = document.documentElement;
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
const hero = document.querySelector(".hero");
let scheduled = false;
function updateScroll() {
  scheduled = false;
  const still = reducedMotion.matches;
  const progress = Math.min(1, scrollY / Math.max(1, root.scrollHeight - innerHeight));
  root.style.setProperty("--rise", still ? "0" : (progress * RISE * innerHeight).toFixed(1));
  root.style.setProperty("--fade", (1 - 0.15 * Math.min(1, scrollY / hero.offsetHeight)).toFixed(3));
}
addEventListener("scroll", () => { if (!scheduled) { scheduled = true; requestAnimationFrame(updateScroll); } }, { passive: true });
addEventListener("resize", updateScroll);
updateScroll();
