// Raidmaster 403 — test du sim web sous Node, sans navigateur (tools/web.sh test, tools/check.sh).
//
//   node simweb/tests/test_web.cjs <build-web/site> [build-sim/raidmaster-sim]
//
// 1. Chaque scénario du dépôt (site/scenarios/index.json) est joué par le module WebAssembly avec ses
//    actions et ses expect, à sa vitesse de rejeu de ctest, chacun dans une instance neuve du module
//    (mémoire de session) : tous les expect doivent passer, comme dans ctest.
// 2. Image de l'écran : s24 rejoué avec une capture (« shot ») ajoutée avant la fin ; l'image doit
//    montrer un écran (pas uni) et, si le sim PC est donné, être identique à la sienne pour le même
//    script (même code de l'UI, même rendu LVGL).
// 3. Boucle de la page : sim_frame (60 images à ×100) sur une trace sans actions, framebuffer redessiné.
// Sortie : une ligne par partie, code 1 au moindre échec.
//
// Licence : GPL-3.0.
"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const [siteArg, simArg] = process.argv.slice(2);
if (!siteArg) {
  console.error("usage : node test_web.cjs <build-web/site> [raidmaster-sim]");
  process.exit(2);
}
const site = path.resolve(siteArg);
const build = JSON.parse(fs.readFileSync(path.join(site, "build.json"), "utf8"));
const factory = require(path.join(site, `${build.module}.js`));
const wasmBinary = fs.readFileSync(path.join(site, `${build.module}.wasm`));
const index = JSON.parse(fs.readFileSync(path.join(site, "scenarios", "index.json"), "utf8")).scenarios;
const W = 480, H = 320;
let failed = false;

async function instance(hooks = {}) {
  const log = [];
  const M = await factory({
    wasmBinary,
    locateFile: (p) => path.join(site, p),
    print: (s) => log.push(s),
    printErr: (s) => log.push(s),
    ...hooks,
  });
  M.log = log;
  return M;
}

// Joue un scénario ; renvoie { ok, n, failures, log, M }.
async function play(sc, actionsText) {
  const expects = [];
  let end = null;
  const M = await instance({
    onExpect: (field, got, want, tol, ok) => expects.push({ field, got, want, tol, ok }),
    onScriptEnd: (n) => { end = n; },
  });
  M.FS.writeFile("/trace.nmea", fs.readFileSync(path.join(site, "scenarios", sc.nmea)));
  M.FS.writeFile("/actions.txt", actionsText ?? fs.readFileSync(path.join(site, "scenarios", sc.actions)));
  const rc = M.ccall("sim_start", "number", ["string", "number", "string", "number"],
                     ["/trace.nmea", sc.speed, "/actions.txt", 0]);
  if (rc !== 0) return { ok: false, n: 0, failures: -1, log: M.log, M };
  // Garde-fou : 13 h de temps simulé (s31 se joue à ×1 sur plusieurs heures).
  for (let t = 0; t < 13 * 3600e3 && end === null; t += 60e3) M._sim_run(60e3);
  const bad = expects.filter((e) => !e.ok);
  return { ok: end === 0 && bad.length === 0 && expects.length > 0, n: expects.length, failures: end, bad, log: M.log, M };
}

function readPpm(buf) {
  const head = buf.subarray(0, 20).toString("latin1");
  const m = /^P6\s+(\d+)\s+(\d+)\s+255\s/.exec(head);
  if (!m) throw new Error("PPM illisible");
  return { w: +m[1], h: +m[2], px: buf.subarray(m[0].length) };
}

(async () => {
  // 1. scénarios
  const t0 = Date.now();
  const failedIds = [];
  let total = 0;
  for (const sc of index) {
    const r = await play(sc);
    total += r.n;
    if (!r.ok) {
      failedIds.push(sc.id);
      console.log(`  ${sc.id} : ÉCHEC (${r.failures} échec(s), ${r.n} expect)`);
      for (const e of r.bad ?? []) console.log(`    expect ${e.field} ${e.got} (attendu ${e.want} ± ${e.tol})`);
      for (const l of r.log.filter((l) => /ÉCHEC|inconnu|mal formé|illisible/.test(l)).slice(0, 5)) console.log(`    ${l}`);
    }
  }
  const s = ((Date.now() - t0) / 1000).toFixed(1);
  if (failedIds.length) {
    failed = true;
    console.log(`WEB TEST : scénarios en ÉCHEC : ${failedIds.join(", ")} (${index.length} scénarios, ${s} s)`);
  } else {
    console.log(`WEB TEST : ${index.length} scénarios verts, ${total} expect (${s} s)`);
  }

  // 2. image de l'écran comparée au sim PC
  const sc = index.find((e) => e.id === "s24_ligne_droite_boutons");
  const base = fs.readFileSync(path.join(site, "scenarios", sc.actions), "utf8");
  const script = base.replace(/^(\d+)\s+quit\b.*$/m, (all, t) => `${+t - 50} shot web-test\n${all}`);
  const r = await play(sc, script);
  const img = readPpm(Buffer.from(r.M.FS.readFile("/out/web-test.ppm")));
  const colours = new Set();
  for (let i = 0; i < img.px.length; i += 3) colours.add((img.px[i] << 16) | (img.px[i + 1] << 8) | img.px[i + 2]);
  let line = `WEB IMAGE : ${img.w}×${img.h}, ${colours.size} couleurs`;
  let ok = r.ok && img.w === W && img.h === H && colours.size > 8;
  if (simArg && fs.existsSync(simArg)) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "raidmaster-web-"));
    try {
      fs.writeFileSync(path.join(dir, "actions.txt"), script);
      fs.writeFileSync(path.join(dir, "trace.nmea"), fs.readFileSync(path.join(site, "scenarios", sc.nmea)));
      execFileSync(path.resolve(simArg), ["--no-storage", "--speed", String(sc.speed), "--replay",
        path.join(dir, "trace.nmea"), "--script", path.join(dir, "actions.txt"), "--out", dir],
        { env: { ...process.env, SDL_VIDEODRIVER: "dummy", SDL_AUDIODRIVER: "dummy" }, stdio: "pipe" });
      const ref = readPpm(fs.readFileSync(path.join(dir, "web-test.ppm")));
      let diff = 0;
      for (let i = 0; i < W * H; i++) {
        const k = 3 * i;
        if (img.px[k] !== ref.px[k] || img.px[k + 1] !== ref.px[k + 1] || img.px[k + 2] !== ref.px[k + 2]) diff++;
      }
      line += diff === 0 ? ", identique au sim PC" : `, ${diff} pixel(s) différent(s) du sim PC`;
      ok = ok && diff === 0;
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  } else {
    line += " (sim PC absent : pas de comparaison)";
  }
  console.log(ok ? line : `${line} — ÉCHEC`);
  failed ||= !ok;

  // 3. boucle de la page
  const M = await instance();
  M.FS.writeFile("/trace.nmea", fs.readFileSync(path.join(site, "scenarios", sc.nmea)));
  M.ccall("sim_start", "number", ["string", "number", "string", "number"], ["/trace.nmea", 1, "", 0]);
  M._sim_set_rate(100);
  const info = () => new Float64Array(M.HEAPF64.buffer, M._sim_info(), 22);
  const serial0 = info()[19];
  const t1 = Date.now();
  for (let i = 0; i < 400; i++) M._sim_frame(16.7);
  const ms = (Date.now() - t1) / 400;
  const st = info();
  const loopOk = st[19] > serial0 && st[1] === 1 && st[9] > 0.1;
  console.log(`WEB BOUCLE : ${(st[0] / 1000).toFixed(0)} s simulées, odo ${st[9].toFixed(3)} km, ` +
              `${ms.toFixed(2)} ms par image à ×100${loopOk ? "" : " — ÉCHEC"}`);
  failed ||= !loopOk;
  process.exit(failed ? 1 : 0);
})().catch((e) => {
  console.log(`WEB TEST : ÉCHEC (${e.message})`);
  process.exit(1);
});
