// Raidmaster 403 — en-tête commun aux pages du site : langue (sélecteur à drapeau, choix gardé), textes de
// la navigation et du bouton GitHub. Chaque page garde ses propres textes et appelle applyLanguage() quand
// sa langue change.
//
// Balisage attendu : <select id="lang">, <span id="flag">, éléments [data-nav] (texte) et
// [data-nav-aria] (aria-label) dont la clé est dans NAV.
//
// Licence : GPL-3.0.

export const LANGUAGES = ["fr", "en"];
const STORAGE_KEY = "raidmaster-langue";

const NAV = {
  fr: {
    site: "Pages du site", home: "Accueil", mockup: "Maquette 3D", sim: "Simulateur", manual: "Manuel",
    lang: "Langue", github: "Code source sur GitHub",
  },
  en: {
    site: "Site pages", home: "Home", mockup: "3D model", sim: "Simulator", manual: "Manual",
    lang: "Language", github: "Source code on GitHub",
  },
};

const FLAGS = {
  fr: '<svg viewBox="0 0 3 2"><path fill="#002654" d="M0 0h1v2H0z"/><path fill="#fff" d="M1 0h1v2H1z"/><path fill="#ce1126" d="M2 0h1v2H2z"/></svg>',
  en: '<svg viewBox="0 0 60 30" preserveAspectRatio="none"><clipPath id="uj"><path d="M30 15h30v15zv15H0zH0V0zV0h30z"/></clipPath><path fill="#012169" d="M0 0v30h60V0z"/><path stroke="#fff" stroke-width="6" d="M0 0l60 30m0-30L0 30"/><path stroke="#c8102e" stroke-width="4" clip-path="url(#uj)" d="M0 0l60 30m0-30L0 30"/><path stroke="#fff" stroke-width="10" d="M30 0v30M0 15h60"/><path stroke="#c8102e" stroke-width="6" d="M30 0v30M0 15h60"/></svg>',
};

// ?lang=fr|en, sinon le dernier choix fait dans le sélecteur (toutes les pages du site), sinon le français
// si le navigateur le préfère, sinon l'anglais.
export function initialLanguage() {
  const asked = new URLSearchParams(location.search).get("lang");
  if (LANGUAGES.includes(asked)) return asked;
  try {
    const kept = localStorage.getItem(STORAGE_KEY);
    if (LANGUAGES.includes(kept)) return kept;
  } catch { /* stockage indisponible */ }
  const preferred = navigator.languages?.length ? navigator.languages : [navigator.language || ""];
  return preferred.some((l) => l.toLowerCase().startsWith("fr")) ? "fr" : "en";
}

// Met l'en-tête dans la langue donnée et garde le choix ; renvoie la langue retenue.
export function applyLanguage(lang) {
  const l = LANGUAGES.includes(lang) ? lang : "fr";
  document.documentElement.lang = l;
  for (const e of document.querySelectorAll("[data-nav]")) e.textContent = NAV[l][e.dataset.nav];
  for (const e of document.querySelectorAll("[data-nav-aria]")) e.setAttribute("aria-label", NAV[l][e.dataset.navAria]);
  const select = document.getElementById("lang");
  if (select) select.value = l;
  const flag = document.getElementById("flag");
  if (flag) flag.innerHTML = FLAGS[l];
  return l;
}

// Appelle callback(langue) quand le visiteur change de langue dans l'en-tête.
export function onLanguageChange(callback) {
  document.getElementById("lang")?.addEventListener("change", (e) => {
    try { localStorage.setItem(STORAGE_KEY, e.target.value); } catch { /* stockage indisponible */ }
    callback(e.target.value);
  });
}
