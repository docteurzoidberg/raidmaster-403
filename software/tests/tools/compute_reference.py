#!/usr/bin/env python3
"""
Calcule la distance totale « vérité terrain » d'une trace NMEA.

Haversine entre fixs successifs valides (RMC status=A). Pas de filtre : on prend tout.
Sert de borne haute pour les tests Unity : la distance intégrée par le firmware doit
rester dans l'intervalle [ref × 0.98, ref × 1.02] sauf pour les scénarios de dégradation
testant précisément un cas limite.

Pure stdlib Python 3.8+.

STAGING v0 — à revoir par l'agent simulation.
"""

import argparse
import math
import sys
from pathlib import Path

EARTH_R_M = 6371008.8


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlam / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return EARTH_R_M * c


def nmea_lat_to_deg(s: str, hemi: str):
    if not s or len(s) < 4:
        return None
    d = int(s[0:2])
    m = float(s[2:])
    v = d + m / 60.0
    return -v if hemi == "S" else v


def nmea_lon_to_deg(s: str, hemi: str):
    if not s or len(s) < 5:
        return None
    d = int(s[0:3])
    m = float(s[3:])
    v = d + m / 60.0
    return -v if hemi == "W" else v


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("trace", type=str, help="chemin .nmea")
    p.add_argument("--only-valid", action="store_true",
                   help="n'intègre que les RMC avec status=A (défaut : oui, flag conservé pour compat)")
    args = p.parse_args(argv)

    path = Path(args.trace)
    if not path.exists():
        print(f"Fichier introuvable : {path}", file=sys.stderr)
        return 2

    prev_lat = None
    prev_lon = None
    total_m = 0.0
    nb_fixes = 0
    nb_skipped = 0

    with path.open(encoding="ascii") as fh:
        for raw in fh:
            raw = raw.strip()
            if not raw.startswith("$GPRMC") and not raw.startswith("$GNRMC"):
                continue
            if "*" not in raw:
                continue
            body = raw[1:].split("*")[0]
            fields = body.split(",")
            if len(fields) < 7 or fields[2] != "A":
                nb_skipped += 1
                continue
            lat = nmea_lat_to_deg(fields[3], fields[4])
            lon = nmea_lon_to_deg(fields[5], fields[6])
            if lat is None or lon is None:
                nb_skipped += 1
                continue

            nb_fixes += 1
            if prev_lat is not None:
                total_m += haversine_m(prev_lat, prev_lon, lat, lon)
            prev_lat, prev_lon = lat, lon

    total_km = total_m / 1000.0
    print(f"Fichier       : {path}")
    print(f"Fixs valides  : {nb_fixes}")
    print(f"Fixs ignorés  : {nb_skipped}")
    print(f"Distance (m)  : {total_m:.3f}")
    print(f"Distance (km) : {total_km:.6f}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
