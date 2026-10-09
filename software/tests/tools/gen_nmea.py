#!/usr/bin/env python3
"""
Génère une trace NMEA synthétique propre (sans bruit) pour les scénarios de simulation.

Pure stdlib Python 3.8+. Format : RMC + GGA à chaque échantillon, pas de VTG/GSA
(l'agent simulation pourra les ajouter si le parseur en a besoin).

Modes :
  * linear      : ligne droite, vitesse et cap constants
  * stationary  : immobile (position unique répétée, uniquement RMC/GGA à vitesse 0)
  * circular    : cercle de rayon donné, cap qui tourne régulièrement

Toutes les positions sont exprimées en WGS84 degrés décimaux. Les timestamps démarrent
par défaut à l'heure courante UTC, mais `--start-utc` permet de forcer une valeur.

Exemple :
    python3 gen_nmea.py --mode linear --duration 3600 --rate 10 \\
        --speed-kmh 90 --heading-deg 90 \\
        --start-lat 48.8566 --start-lon 2.3522 \\
        --hdop 1.0 --out stage_highway_1h.nmea

Licence : GPL-3.0 (voir ../LICENSE).

STAGING v0 — à revoir par l'agent simulation. Voir tests/tools/README.md.
"""

import argparse
import datetime as dt
import math
import sys
from pathlib import Path

EARTH_R_M = 6371008.8  # rayon moyen WGS84 en mètres

# ---------------------------------------------------------------------------- #
# Utilitaires NMEA
# ---------------------------------------------------------------------------- #


def nmea_checksum(sentence_no_dollar: str) -> str:
    """Calcule le checksum XOR NMEA d'une phrase sans le $ initial ni le *XX final."""
    cs = 0
    for ch in sentence_no_dollar:
        cs ^= ord(ch)
    return f"{cs:02X}"


def to_nmea_latitude(lat_deg: float) -> tuple[str, str]:
    """48.8566 -> ('4851.3960', 'N')  — ddmm.mmmm."""
    hemi = "N" if lat_deg >= 0 else "S"
    lat_abs = abs(lat_deg)
    deg = int(lat_abs)
    minutes = (lat_abs - deg) * 60.0
    return f"{deg:02d}{minutes:07.4f}", hemi


def to_nmea_longitude(lon_deg: float) -> tuple[str, str]:
    """2.3522 -> ('00221.1320', 'E')  — dddmm.mmmm."""
    hemi = "E" if lon_deg >= 0 else "W"
    lon_abs = abs(lon_deg)
    deg = int(lon_abs)
    minutes = (lon_abs - deg) * 60.0
    return f"{deg:03d}{minutes:07.4f}", hemi


def build_rmc(ts: dt.datetime, lat: float, lon: float,
              speed_knots: float, course_deg: float,
              fix_valid: bool = True) -> str:
    """$GPRMC — position + vitesse (knots) + cap vrai (deg)."""
    time_s = ts.strftime("%H%M%S.") + f"{ts.microsecond // 1000:03d}"[:2]
    date_s = ts.strftime("%d%m%y")
    lat_s, lat_h = to_nmea_latitude(lat)
    lon_s, lon_h = to_nmea_longitude(lon)
    status = "A" if fix_valid else "V"
    fields = f"GPRMC,{time_s},{status},{lat_s},{lat_h},{lon_s},{lon_h}," \
             f"{speed_knots:.2f},{course_deg:.2f},{date_s},,,A"
    cs = nmea_checksum(fields)
    return f"${fields}*{cs}"


def build_gga(ts: dt.datetime, lat: float, lon: float,
              fix_quality: int, nb_sats: int, hdop: float,
              altitude_m: float = 100.0) -> str:
    """$GPGGA — qualité fix (0=invalid, 1=GPS, 2=DGPS, ...) + HDOP + alt."""
    time_s = ts.strftime("%H%M%S.") + f"{ts.microsecond // 1000:03d}"[:2]
    lat_s, lat_h = to_nmea_latitude(lat)
    lon_s, lon_h = to_nmea_longitude(lon)
    fields = f"GPGGA,{time_s},{lat_s},{lat_h},{lon_s},{lon_h}," \
             f"{fix_quality},{nb_sats:02d},{hdop:.1f},{altitude_m:.1f},M,0.0,M,,"
    cs = nmea_checksum(fields)
    return f"${fields}*{cs}"


# ---------------------------------------------------------------------------- #
# Déplacement sur la sphère (petites distances)
# ---------------------------------------------------------------------------- #


def move_on_sphere(lat_deg: float, lon_deg: float,
                   heading_deg: float, distance_m: float) -> tuple[float, float]:
    """
    Avance `distance_m` depuis (lat, lon) dans la direction `heading_deg` (0=N, 90=E).
    Approximation sphérique (erreur < 1 cm sur 100 m), suffisante pour un tripmaster.
    """
    if distance_m == 0:
        return lat_deg, lon_deg

    lat1 = math.radians(lat_deg)
    lon1 = math.radians(lon_deg)
    brng = math.radians(heading_deg)
    d_over_r = distance_m / EARTH_R_M

    lat2 = math.asin(
        math.sin(lat1) * math.cos(d_over_r)
        + math.cos(lat1) * math.sin(d_over_r) * math.cos(brng)
    )
    lon2 = lon1 + math.atan2(
        math.sin(brng) * math.sin(d_over_r) * math.cos(lat1),
        math.cos(d_over_r) - math.sin(lat1) * math.sin(lat2),
    )
    return math.degrees(lat2), (math.degrees(lon2) + 540) % 360 - 180


# ---------------------------------------------------------------------------- #
# Génération par mode
# ---------------------------------------------------------------------------- #


def gen_linear(args, fh):
    """Trajectoire rectiligne, vitesse et cap constants."""
    rate = args.rate
    dt_s = 1.0 / rate
    total_samples = int(args.duration * rate)

    lat = args.start_lat
    lon = args.start_lon
    speed_kmh = args.speed_kmh
    speed_mps = speed_kmh / 3.6
    speed_knots = speed_kmh / 1.852
    heading = args.heading_deg
    hdop = args.hdop

    for i in range(total_samples):
        ts = args.start_utc + dt.timedelta(seconds=i * dt_s)

        fh.write(build_rmc(ts, lat, lon, speed_knots, heading) + "\r\n")
        fh.write(build_gga(ts, lat, lon, 1, args.sats, hdop) + "\r\n")

        lat, lon = move_on_sphere(lat, lon, heading, speed_mps * dt_s)


def gen_stationary(args, fh):
    """Immobile, vitesse 0."""
    rate = args.rate
    dt_s = 1.0 / rate
    total_samples = int(args.duration * rate)

    lat = args.start_lat
    lon = args.start_lon
    hdop = args.hdop

    for i in range(total_samples):
        ts = args.start_utc + dt.timedelta(seconds=i * dt_s)
        fh.write(build_rmc(ts, lat, lon, 0.0, 0.0) + "\r\n")
        fh.write(build_gga(ts, lat, lon, 1, args.sats, hdop) + "\r\n")


def gen_circular(args, fh):
    """Cercle de rayon donné, vitesse tangentielle constante, cap qui tourne."""
    rate = args.rate
    dt_s = 1.0 / rate
    total_samples = int(args.duration * rate)

    radius_m = args.radius_m
    if radius_m <= 0:
        raise ValueError("--radius-m doit être > 0 pour le mode circular")
    speed_kmh = args.speed_kmh
    speed_mps = speed_kmh / 3.6
    speed_knots = speed_kmh / 1.852
    hdop = args.hdop

    # Centre du cercle : plein nord à `radius_m` du point de départ.
    center_lat, center_lon = move_on_sphere(
        args.start_lat, args.start_lon, 0.0, radius_m
    )

    # Vitesse angulaire (rad/s)
    omega = speed_mps / radius_m

    for i in range(total_samples):
        ts = args.start_utc + dt.timedelta(seconds=i * dt_s)
        # Angle depuis le centre (0 au sud = position de départ, croît en sens horaire).
        theta = -math.pi / 2 + omega * i * dt_s
        # Nouvelle position : à `radius_m` du centre, dans la direction theta (0=N, 90=E).
        heading_from_center_deg = (math.degrees(theta) + 360) % 360
        lat, lon = move_on_sphere(
            center_lat, center_lon, heading_from_center_deg, radius_m
        )
        # Cap tangentiel = perpendiculaire au rayon, sens horaire.
        heading = (heading_from_center_deg + 90) % 360

        fh.write(build_rmc(ts, lat, lon, speed_knots, heading) + "\r\n")
        fh.write(build_gga(ts, lat, lon, 1, args.sats, hdop) + "\r\n")


MODES = {
    "linear": gen_linear,
    "stationary": gen_stationary,
    "circular": gen_circular,
}


# ---------------------------------------------------------------------------- #
# CLI
# ---------------------------------------------------------------------------- #


def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--mode", choices=MODES.keys(), required=True)
    p.add_argument("--duration", type=float, required=True, help="durée simulée en secondes")
    p.add_argument("--rate", type=int, default=10, help="fréquence Hz (défaut 10)")
    p.add_argument("--start-lat", type=float, default=48.8566, help="latitude initiale (défaut Paris)")
    p.add_argument("--start-lon", type=float, default=2.3522, help="longitude initiale")
    p.add_argument("--start-utc", type=str, default=None,
                   help="timestamp UTC ISO début, défaut now()")
    p.add_argument("--speed-kmh", type=float, default=90.0, help="vitesse km/h (linear/circular)")
    p.add_argument("--heading-deg", type=float, default=90.0, help="cap vrai 0-360 (linear)")
    p.add_argument("--radius-m", type=float, default=500.0, help="rayon m (circular)")
    p.add_argument("--hdop", type=float, default=1.0, help="HDOP (défaut 1.0)")
    p.add_argument("--sats", type=int, default=10, help="nombre de satellites visibles")
    p.add_argument("--out", type=str, required=True, help="chemin fichier .nmea en sortie")

    args = p.parse_args(argv)
    if args.start_utc is None:
        args.start_utc = dt.datetime.now(dt.timezone.utc).replace(microsecond=0)
    else:
        args.start_utc = dt.datetime.fromisoformat(args.start_utc).astimezone(dt.timezone.utc)
    return args


def main(argv):
    args = parse_args(argv)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="ascii", newline="") as fh:
        MODES[args.mode](args, fh)
    total_samples = int(args.duration * args.rate)
    print(f"OK : {out_path} — {total_samples} échantillons, "
          f"{total_samples * 2} trames NMEA, mode={args.mode}, "
          f"durée={args.duration}s @ {args.rate}Hz")


if __name__ == "__main__":
    main(sys.argv[1:])
