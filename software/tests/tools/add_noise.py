#!/usr/bin/env python3
"""
Injecte du bruit dans une trace NMEA « propre » déjà générée.

Trois types de bruit, cumulables :

* `--jitter-m SIGMA`      : jitter gaussien de la position (sigma mètres, défaut 0).
* `--hdop-max HDOP`       : HDOP uniforme aléatoire entre 1.0 et HDOP sur toute la trace.
* `--fix-loss HH:MM:SS-HH:MM:SS` (répétable) : fenêtres de perte de fix. Pendant la
  fenêtre, GGA fix_quality=0 et RMC status=V, position inchangée.

Les timestamps relatifs des `--fix-loss` démarrent au premier timestamp de la trace.

Pure stdlib Python 3.8+.

STAGING v0 — à revoir par l'agent simulation.
"""

import argparse
import math
import random
import re
import sys
from pathlib import Path

EARTH_R_M = 6371008.8


# -------------------------------------------------------------------------- #
# NMEA helpers (dupliqués de gen_nmea.py pour l'autonomie du script)
# -------------------------------------------------------------------------- #


def nmea_checksum(sentence_no_dollar_no_star: str) -> str:
    cs = 0
    for ch in sentence_no_dollar_no_star:
        cs ^= ord(ch)
    return f"{cs:02X}"


def to_nmea_latitude(lat_deg: float) -> tuple[str, str]:
    hemi = "N" if lat_deg >= 0 else "S"
    lat_abs = abs(lat_deg)
    deg = int(lat_abs)
    minutes = (lat_abs - deg) * 60.0
    return f"{deg:02d}{minutes:07.4f}", hemi


def to_nmea_longitude(lon_deg: float) -> tuple[str, str]:
    hemi = "E" if lon_deg >= 0 else "W"
    lon_abs = abs(lon_deg)
    deg = int(lon_abs)
    minutes = (lon_abs - deg) * 60.0
    return f"{deg:03d}{minutes:07.4f}", hemi


def nmea_lat_to_deg(s: str, hemi: str) -> float:
    d = int(s[0:2])
    m = float(s[2:])
    v = d + m / 60.0
    return -v if hemi == "S" else v


def nmea_lon_to_deg(s: str, hemi: str) -> float:
    d = int(s[0:3])
    m = float(s[3:])
    v = d + m / 60.0
    return -v if hemi == "W" else v


def jitter_latlon(lat: float, lon: float, sigma_m: float, rng: random.Random) -> tuple[float, float]:
    """Déplace de ~N(0, sigma_m) en direction aléatoire."""
    if sigma_m <= 0:
        return lat, lon
    dist = abs(rng.gauss(0, sigma_m))
    brng = math.radians(rng.uniform(0, 360))
    lat1 = math.radians(lat)
    lon1 = math.radians(lon)
    d_over_r = dist / EARTH_R_M
    lat2 = math.asin(
        math.sin(lat1) * math.cos(d_over_r)
        + math.cos(lat1) * math.sin(d_over_r) * math.cos(brng)
    )
    lon2 = lon1 + math.atan2(
        math.sin(brng) * math.sin(d_over_r) * math.cos(lat1),
        math.cos(d_over_r) - math.sin(lat1) * math.sin(lat2),
    )
    return math.degrees(lat2), (math.degrees(lon2) + 540) % 360 - 180


# -------------------------------------------------------------------------- #
# Parse temps NMEA HHMMSS.cc en secondes depuis minuit
# -------------------------------------------------------------------------- #

_HMS_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})$")


def hms_to_sec(s: str) -> float:
    m = _HMS_RE.match(s.strip())
    if not m:
        raise ValueError(f"attendu HH:MM:SS, reçu : {s!r}")
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3))


def nmea_time_to_sec(hms: str) -> float:
    h = int(hms[0:2])
    m = int(hms[2:4])
    s = float(hms[4:])
    return h * 3600 + m * 60 + s


# -------------------------------------------------------------------------- #
# Traitement
# -------------------------------------------------------------------------- #


def process(in_path: Path, out_path: Path, jitter_m: float, hdop_max: float,
            fix_loss_windows: list[tuple[float, float]], seed: int) -> dict:
    rng = random.Random(seed)
    first_t: float | None = None
    stats = {"rmc": 0, "gga": 0, "lost": 0, "jittered": 0}

    with in_path.open(encoding="ascii") as fin, out_path.open("w", encoding="ascii", newline="") as fout:
        for raw in fin:
            raw = raw.strip()
            if not raw.startswith("$"):
                fout.write(raw + "\r\n")
                continue
            if "*" not in raw:
                fout.write(raw + "\r\n")
                continue
            body = raw[1:].split("*")[0]
            fields = body.split(",")
            type_ = fields[0]

            # Récupère le timestamp pour vérifier perte de fix.
            is_in_loss = False
            if type_ in ("GPRMC", "GPGGA", "GNRMC", "GNGGA"):
                try:
                    t_sec = nmea_time_to_sec(fields[1])
                    if first_t is None:
                        first_t = t_sec
                    rel_t = t_sec - first_t
                    # Supporte le wrap minuit.
                    if rel_t < -43200:
                        rel_t += 86400
                    for (start, end) in fix_loss_windows:
                        if start <= rel_t <= end:
                            is_in_loss = True
                            break
                except Exception:
                    pass

            if type_ in ("GPRMC", "GNRMC") and len(fields) >= 7:
                stats["rmc"] += 1
                time_s = fields[1]
                status = fields[2]
                lat_s = fields[3]
                lat_h = fields[4]
                lon_s = fields[5]
                lon_h = fields[6]
                speed_knots = fields[7] if len(fields) > 7 else "0.0"
                course_deg = fields[8] if len(fields) > 8 else "0.0"
                date_s = fields[9] if len(fields) > 9 else ""
                if is_in_loss:
                    status = "V"
                    stats["lost"] += 1
                else:
                    lat = nmea_lat_to_deg(lat_s, lat_h)
                    lon = nmea_lon_to_deg(lon_s, lon_h)
                    lat2, lon2 = jitter_latlon(lat, lon, jitter_m, rng)
                    lat_s, lat_h = to_nmea_latitude(lat2)
                    lon_s, lon_h = to_nmea_longitude(lon2)
                    if jitter_m > 0:
                        stats["jittered"] += 1
                new_body = f"GPRMC,{time_s},{status},{lat_s},{lat_h},{lon_s},{lon_h}," \
                           f"{speed_knots},{course_deg},{date_s},,,A"
                fout.write(f"${new_body}*{nmea_checksum(new_body)}\r\n")

            elif type_ in ("GPGGA", "GNGGA") and len(fields) >= 10:
                stats["gga"] += 1
                time_s = fields[1]
                lat_s = fields[2]
                lat_h = fields[3]
                lon_s = fields[4]
                lon_h = fields[5]
                fix_q = fields[6] or "1"
                nb_sats = fields[7] or "08"
                hdop = fields[8] or "1.0"
                altitude = fields[9] if len(fields) > 9 else "0.0"
                alt_u = fields[10] if len(fields) > 10 else "M"
                geoid = fields[11] if len(fields) > 11 else "0.0"
                geoid_u = fields[12] if len(fields) > 12 else "M"
                if is_in_loss:
                    fix_q = "0"
                else:
                    lat = nmea_lat_to_deg(lat_s, lat_h)
                    lon = nmea_lon_to_deg(lon_s, lon_h)
                    lat2, lon2 = jitter_latlon(lat, lon, jitter_m, rng)
                    lat_s, lat_h = to_nmea_latitude(lat2)
                    lon_s, lon_h = to_nmea_longitude(lon2)
                    if hdop_max > 1.0:
                        new_hdop = rng.uniform(1.0, hdop_max)
                        hdop = f"{new_hdop:.1f}"
                new_body = f"GPGGA,{time_s},{lat_s},{lat_h},{lon_s},{lon_h}," \
                           f"{fix_q},{nb_sats},{hdop},{altitude},{alt_u}," \
                           f"{geoid},{geoid_u},,"
                fout.write(f"${new_body}*{nmea_checksum(new_body)}\r\n")

            else:
                fout.write(raw + "\r\n")

    return stats


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--in", dest="in_", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--jitter-m", type=float, default=0.0)
    p.add_argument("--hdop-max", type=float, default=1.0,
                   help="HDOP max aléatoire (1.0 = inchangé)")
    p.add_argument("--fix-loss", action="append", default=[],
                   help="fenêtre HH:MM:SS-HH:MM:SS (répétable)")
    p.add_argument("--seed", type=int, default=42)
    args = p.parse_args(argv)

    windows = []
    for w in args.fix_loss:
        parts = w.split("-")
        if len(parts) != 2:
            print(f"ERREUR : --fix-loss attend HH:MM:SS-HH:MM:SS, reçu : {w!r}", file=sys.stderr)
            return 2
        windows.append((hms_to_sec(parts[0]), hms_to_sec(parts[1])))

    in_path = Path(args.in_)
    out_path = Path(args.out)
    if not in_path.exists():
        print(f"Fichier introuvable : {in_path}", file=sys.stderr)
        return 2
    out_path.parent.mkdir(parents=True, exist_ok=True)

    stats = process(in_path, out_path, args.jitter_m, args.hdop_max, windows, args.seed)
    print(f"OK : {out_path}")
    print(f"  RMC traitées       : {stats['rmc']}")
    print(f"  GGA traitées       : {stats['gga']}")
    print(f"  Trames en perte    : {stats['lost']}")
    print(f"  Positions jittered : {stats['jittered']}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
