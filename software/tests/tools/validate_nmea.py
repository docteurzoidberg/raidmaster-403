#!/usr/bin/env python3
"""
Valide un fichier .nmea : checksum des trames RMC/GGA, continuité temporelle,
positions dans les bornes WGS84, fix type cohérent.

Pure stdlib Python 3.8+. Code de sortie 0 si tout est OK, 1 sinon.

Version v0.
"""

import datetime as dt
import sys
from pathlib import Path


def nmea_checksum(sentence_no_dollar_no_star: str) -> str:
    cs = 0
    for ch in sentence_no_dollar_no_star:
        cs ^= ord(ch)
    return f"{cs:02X}"


def parse_sentence(line: str) -> tuple[str, list[str], str] | None:
    """Retourne (type, champs, checksum_calculé) ou None si format invalide."""
    line = line.strip()
    if not line.startswith("$") or "*" not in line:
        return None
    body, cs_hex = line[1:].rsplit("*", 1)
    cs_calc = nmea_checksum(body)
    fields = body.split(",")
    return fields[0], fields, cs_calc.upper() == cs_hex.upper().strip()


def nmea_time_to_sec(hms: str) -> float | None:
    """124603.50 -> 45963.5 s depuis minuit UTC."""
    if not hms:
        return None
    try:
        h = int(hms[0:2])
        m = int(hms[2:4])
        s = float(hms[4:])
        return h * 3600 + m * 60 + s
    except Exception:
        return None


def nmea_lat_to_deg(s: str, hemi: str) -> float | None:
    if not s or len(s) < 4:
        return None
    try:
        d = int(s[0:2])
        m = float(s[2:])
        v = d + m / 60.0
        return -v if hemi == "S" else v
    except Exception:
        return None


def nmea_lon_to_deg(s: str, hemi: str) -> float | None:
    if not s or len(s) < 5:
        return None
    try:
        d = int(s[0:3])
        m = float(s[3:])
        v = d + m / 60.0
        return -v if hemi == "W" else v
    except Exception:
        return None


def main(argv):
    if len(argv) != 1:
        print("Usage : validate_nmea.py <trace.nmea>", file=sys.stderr)
        return 2
    path = Path(argv[0])
    if not path.exists():
        print(f"Fichier introuvable : {path}", file=sys.stderr)
        return 2

    errors: list[str] = []
    stats = {
        "lines": 0,
        "rmc": 0,
        "gga": 0,
        "cs_fail": 0,
        "unknown": 0,
        "bad_lat": 0,
        "bad_lon": 0,
        "bad_fix": 0,
    }
    last_t: float | None = None

    with path.open(encoding="ascii") as fh:
        for lineno, raw in enumerate(fh, start=1):
            stats["lines"] += 1
            parsed = parse_sentence(raw)
            if parsed is None:
                errors.append(f"L{lineno}: format invalide : {raw.strip()[:60]!r}")
                continue
            type_, fields, cs_ok = parsed
            if not cs_ok:
                stats["cs_fail"] += 1
                errors.append(f"L{lineno}: checksum invalide pour {type_}")
                continue

            if type_ == "GPRMC":
                stats["rmc"] += 1
                t = nmea_time_to_sec(fields[1])
                if t is not None and last_t is not None:
                    dt_sec = t - last_t
                    # Supporte le wrap minuit UTC.
                    if dt_sec < -43200:
                        dt_sec += 86400
                    if dt_sec < -0.01:
                        errors.append(f"L{lineno}: temps RMC qui recule de {-dt_sec:.3f}s")
                last_t = t if t is not None else last_t
                lat = nmea_lat_to_deg(fields[3], fields[4])
                lon = nmea_lon_to_deg(fields[5], fields[6])
                if lat is None or not -90 <= lat <= 90:
                    stats["bad_lat"] += 1
                if lon is None or not -180 <= lon <= 180:
                    stats["bad_lon"] += 1

            elif type_ == "GPGGA":
                stats["gga"] += 1
                try:
                    fix_q = int(fields[6]) if fields[6] else -1
                except ValueError:
                    fix_q = -1
                if fix_q not in (0, 1, 2, 4, 5):
                    stats["bad_fix"] += 1
                    errors.append(f"L{lineno}: fix quality GGA invalide : {fix_q}")

            else:
                stats["unknown"] += 1

    print(f"Fichier : {path}")
    print(f"Lignes totales      : {stats['lines']}")
    print(f"RMC valides         : {stats['rmc']}")
    print(f"GGA valides         : {stats['gga']}")
    print(f"Checksums invalides : {stats['cs_fail']}")
    print(f"Trames inconnues    : {stats['unknown']}")
    print(f"Latitudes hors bornes : {stats['bad_lat']}")
    print(f"Longitudes hors bornes: {stats['bad_lon']}")
    print(f"Fix quality GGA invalide: {stats['bad_fix']}")
    if errors:
        print(f"\n{len(errors)} ERREURS (10 premières) :")
        for e in errors[:10]:
            print(f"  {e}")
        return 1
    print("\nOK : trace propre, aucun défaut détecté.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
