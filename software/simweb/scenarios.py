#!/usr/bin/env python3
"""Raidmaster 403 — scénarios du dépôt pour le sim web.

Lit la liste des scénarios et leur vitesse de rejeu dans tests/CMakeLists.txt (« id|vitesse »), leur
phase et leur description dans tests/fixtures/scenarios/README.md, puis écrit dans --out :
  <id>.nmea         trace générée par tests/tools/gen_route.py (déterministe) ou trace.nmea versionnée
  <id>.actions.txt  actions et vérifications du scénario
  index.json        liste dans l'ordre de tests/CMakeLists.txt : id, phase, vitesse, description,
                    fichiers et taille de la trace
Une trace ou des actions ne sont réécrites que si leur contenu change. La page récupère chaque trace à la demande.

Licence : GPL-3.0.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile


def write_if_changed(path: pathlib.Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.write_bytes(data)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", required=True, help="racine du dépôt")
    ap.add_argument("--out", required=True, help="dossier de sortie")
    args = ap.parse_args()

    soft = pathlib.Path(args.repo) / "software"
    fixtures = soft / "tests" / "fixtures" / "scenarios"
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    cmake = (soft / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
    entries = re.findall(r'"(s\d+_[a-z0-9_]+)\|(\d+)"', cmake)
    readme = (fixtures / "README.md").read_text(encoding="utf-8")
    rows = {}
    for m in re.finditer(r"^\|\s*(\d+)\s*\|\s*`(s\d+_[a-z0-9_]+)`\s*\|[^|]*\|\s*(.+?)\s*\|\s*$", readme, re.M):
        rows[m.group(2)] = (int(m.group(1)), m.group(3))

    index = []
    for sid, speed in entries:
        d = fixtures / sid
        nmea = out / f"{sid}.nmea"
        if (d / "route.txt").exists():
            with tempfile.TemporaryDirectory() as tmp:
                t = pathlib.Path(tmp) / "trace.nmea"
                subprocess.run([sys.executable, str(soft / "tests" / "tools" / "gen_route.py"),
                                str(d / "route.txt"), "--out", str(t), "--quiet"], check=True)
                write_if_changed(nmea, t.read_bytes())
        else:
            write_if_changed(nmea, (d / "trace.nmea").read_bytes())
        write_if_changed(out / f"{sid}.actions.txt", (d / "actions.txt").read_bytes())
        phase, desc = rows.get(sid, (0, ""))
        index.append({
            "id": sid,
            "phase": phase,
            "speed": int(speed),
            "desc": re.sub(r"`([^`]*)`", r"\1", desc),
            "nmea": nmea.name,
            "actions": f"{sid}.actions.txt",
            "bytes": nmea.stat().st_size,
        })
    text = json.dumps({"scenarios": index}, ensure_ascii=False, indent=1) + "\n"
    (out / "index.json").write_text(text, encoding="utf-8")     # toujours : sortie de la règle CMake
    print(f"{len(index)} scénarios, {sum(e['bytes'] for e in index) / 1e6:.1f} Mo de traces → {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
