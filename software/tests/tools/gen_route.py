#!/usr/bin/env python3
"""
Raidmaster 403 — génère une trace NMEA à partir d'un itinéraire décrit en texte (route.txt).

Sert aux scénarios de `tests/fixtures/scenarios/` : la trace est régénérée au build dans le dossier
de build (déterministe : même fichier, même graine → mêmes octets), seul l'itinéraire est versionné.

    python3 tests/tools/gen_route.py route.txt --out trace.nmea

Imprime ensuite la distance « vérité » (intégrale de la vitesse sur le trajet réel, avant bruit) à
chaque repère `mark` et à la fin : c'est la référence à reporter dans `actions.txt`.

Format de route.txt : une directive par ligne, « # » commente. Durées et instants en secondes de
trace, vitesses en km/h, caps en degrés.

  Réglages (avant les segments)
    start <lat> <lon> <AAAA-MM-JJTHH:MM:SS>   départ et heure UTC (défaut Paris, 2026-10-09T06:00:00)
    heading <deg>                             cap initial (défaut 90)
    rate <hz>                                 fréquence des époques, 1 à 10 (défaut 1)
    seed <n>                                  graine du bruit (défaut 403)
    hdop <valeur>                             HDOP nominal (défaut 1.0)
    sats <n>                                  satellites nominaux (défaut 10)
    jitter <m>                                bruit de position gaussien (écart-type, défaut 0)
    speed_noise <km/h>                        bruit gaussien sur la vitesse annoncée (défaut 0)

  Segments (mis bout à bout)
    drive <durée> <v0> <v1> [virage °/s]      vitesse linéaire de v0 à v1, virage constant
    stop <durée>                              à l'arrêt
    wander <durée> <vmin> <vmax> <virage max °/s> [période s]
                                              vitesse et virage qui ondulent (route sinueuse)
    repeat <n> … end                          répète le bloc

  Événements (instants absolus de trace)
    loss <t0> <t1>                            sans fix : RMC « V », GGA qualité 0
    silence <t0> <t1>                         module muet (aucune phrase)
    hdop_at <t0> <t1> <valeur>                HDOP forcé sur l'intervalle
    fix2d <t0> <t1>                           fix 2D : GSA mode 2, 4 satellites, pas d'altitude
    mark <nom>                                (entre deux segments) imprime la distance vérité ici

Licence : GPL-3.0.
"""

import argparse
import datetime as dt
import math
import random
import sys

R_EARTH = 6371008.8
SUB_HZ = 10                    # pas d'intégration interne


def checksum(body):
    cs = 0
    for c in body.encode("ascii"):
        cs ^= c
    return f"{cs:02X}"


def sentence(body):
    return f"${body}*{checksum(body)}\r\n"


def nmea_coord(deg, is_lat):
    hemi = ("N" if deg >= 0 else "S") if is_lat else ("E" if deg >= 0 else "W")
    deg = abs(deg)
    d = int(deg)
    m = round((deg - d) * 60.0, 5)
    if m >= 60.0:
        d += 1
        m = 0.0
    return (f"{d:02d}{m:08.5f}" if is_lat else f"{d:03d}{m:08.5f}") + "," + hemi


def parse(path):
    cfg = {"lat": 48.8566, "lon": 2.3522, "utc": "2026-10-09T06:00:00", "heading": 90.0, "rate": 1,
           "seed": 403, "hdop": 1.0, "sats": 10, "jitter": 0.0, "speed_noise": 0.0}
    segs, events = [], []
    stack = [segs]
    with open(path, encoding="utf-8") as f:
        for no, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].split()
            if not line:
                continue
            k, a = line[0], line[1:]
            try:
                if k == "start":
                    cfg["lat"], cfg["lon"], cfg["utc"] = float(a[0]), float(a[1]), a[2]
                elif k in ("heading", "hdop", "jitter", "speed_noise"):
                    cfg[k] = float(a[0])
                elif k in ("rate", "seed", "sats"):
                    cfg[k] = int(a[0])
                elif k == "drive":
                    stack[-1].append(("drive", float(a[0]), float(a[1]), float(a[2]),
                                      float(a[3]) if len(a) > 3 else 0.0))
                elif k == "stop":
                    stack[-1].append(("drive", float(a[0]), 0.0, 0.0, 0.0))
                elif k == "wander":
                    stack[-1].append(("wander", float(a[0]), float(a[1]), float(a[2]), float(a[3]),
                                      float(a[4]) if len(a) > 4 else 60.0))
                elif k == "mark":
                    stack[-1].append(("mark", a[0]))
                elif k == "repeat":
                    block = []
                    stack[-1].append(("repeat", int(a[0]), block))
                    stack.append(block)
                elif k == "end":
                    stack.pop()
                elif k in ("loss", "silence", "fix2d"):
                    events.append((k, float(a[0]), float(a[1]), None))
                elif k == "hdop_at":
                    events.append((k, float(a[0]), float(a[1]), float(a[2])))
                else:
                    raise ValueError(f"directive inconnue « {k} »")
            except (IndexError, ValueError) as e:
                sys.exit(f"{path}:{no}: {e}")
    if len(stack) != 1:
        sys.exit(f"{path}: « repeat » sans « end »")
    if not 1 <= cfg["rate"] <= 10 or SUB_HZ % cfg["rate"] != 0:
        sys.exit(f"{path}: rate doit diviser {SUB_HZ}")
    return cfg, segs, events


def flatten(segs):
    for s in segs:
        if s[0] == "repeat":
            for _ in range(s[1]):
                yield from flatten(s[2])
        else:
            yield s


def samples(cfg, segs):
    """Échantillons internes à SUB_HZ : (t, v_kmh, turn_dps) et repères (t, nom)."""
    dt_s = 1.0 / SUB_HZ
    t = 0.0
    out, marks = [], []
    for s in flatten(segs):
        if s[0] == "mark":
            marks.append((t, s[1]))
            continue
        dur = s[1]
        n = int(round(dur * SUB_HZ))
        for i in range(n):
            u = (i + 0.5) / n if n else 0.0
            if s[0] == "drive":
                v = s[2] + (s[3] - s[2]) * u
                turn = s[4]
            else:
                _, _, vmin, vmax, tmax, per = s
                tt = i * dt_s
                v = vmin + (vmax - vmin) * 0.5 * (1 + math.sin(2 * math.pi * tt / per))
                turn = tmax * math.sin(2 * math.pi * tt / (per * 0.37)) * math.cos(2 * math.pi * tt / (per * 1.7))
            out.append((t, v, turn))
            t += dt_s
    return out, marks, t


def in_event(events, kind, t):
    for k, t0, t1, val in events:
        if k == kind and t0 <= t < t1:
            return val if val is not None else True
    return None


def generate(cfg, segs, events, out):
    rng = random.Random(cfg["seed"])
    sub, marks, total_s = samples(cfg, segs)
    t0 = dt.datetime.fromisoformat(cfg["utc"].replace("Z", "+00:00"))
    if t0.tzinfo is None:
        t0 = t0.replace(tzinfo=dt.timezone.utc)
    lat, lon, head = cfg["lat"], cfg["lon"], cfg["heading"]
    dist = 0.0
    dt_s = 1.0 / SUB_HZ
    every = SUB_HZ // cfg["rate"]
    mark_d = {}
    mi = 0
    lines = 0
    # Dernière époque à l'instant final, avec la vitesse du dernier pas.
    last = (total_s, sub[-1][1] if sub else 0.0, 0.0)
    for idx, (t, v, turn) in enumerate(sub + [last]):
        while mi < len(marks) and marks[mi][0] <= t + 1e-9:
            mark_d[marks[mi][1]] = (marks[mi][0], dist)
            mi += 1
        if idx % every == 0:
            ts = t0 + dt.timedelta(seconds=round(t, 2))
            hms = ts.strftime("%H%M%S") + f".{ts.microsecond // 10000:02d}"
            date = ts.strftime("%d%m%y")
            if not in_event(events, "silence", t):
                if in_event(events, "loss", t):
                    out.write(sentence(f"GPRMC,{hms},V,,,,,,,{date},,,N"))
                    out.write(sentence(f"GPGGA,{hms},,,,,0,00,99.99,,,,,,"))
                    out.write(sentence("GPGSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99"))
                else:
                    hd = in_event(events, "hdop_at", t) or cfg["hdop"]
                    two_d = in_event(events, "fix2d", t)
                    sats = 4 if two_d else cfg["sats"]
                    j = cfg["jitter"]
                    nlat, nlon = lat, lon
                    if j > 0:
                        nlat += rng.gauss(0, j) / R_EARTH * 180 / math.pi
                        nlon += rng.gauss(0, j) / (R_EARTH * math.cos(math.radians(lat))) * 180 / math.pi
                        nlon = (nlon + 540.0) % 360.0 - 180.0
                    la, lo = nmea_coord(nlat, True), nmea_coord(nlon, False)
                    sv = v
                    if cfg["speed_noise"] > 0:
                        sv = max(0.0, v + rng.gauss(0, cfg["speed_noise"]))
                    kn = sv / 1.852
                    crs = f"{head:.1f}" if v > 1.0 else ""
                    out.write(sentence(f"GPRMC,{hms},A,{la},{lo},{kn:.2f},{crs},{date},,,A"))
                    alt = "" if two_d else "100.0"
                    out.write(sentence(f"GPGGA,{hms},{la},{lo},1,{sats:02d},{hd:.1f},{alt},M,{46.0 if alt else ''},M,,"))
                    out.write(sentence(f"GPGSA,A,{2 if two_d else 3},01,02,03,04,,,,,,,,,{hd * 1.3:.1f},{hd:.1f},"
                                       f"{'' if two_d else f'{hd:.1f}'}"))
                lines += 1
        if idx == len(sub):
            break
        # Avance d'un pas : cap au milieu du pas, déplacement à la vitesse du pas.
        h = head + turn * dt_s / 2
        d = v / 3.6 * dt_s
        lat += d * math.cos(math.radians(h)) / R_EARTH * 180 / math.pi
        lon += d * math.sin(math.radians(h)) / (R_EARTH * math.cos(math.radians(lat))) * 180 / math.pi
        lon = (lon + 540.0) % 360.0 - 180.0
        head = (head + turn * dt_s) % 360.0
        dist += d
    return dist, total_s, mark_d, lines


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    p.add_argument("route")
    p.add_argument("--out", required=True)
    p.add_argument("--quiet", action="store_true", help="n'imprime pas les repères")
    a = p.parse_args()
    cfg, segs, events = parse(a.route)
    with open(a.out, "w", encoding="ascii", newline="") as f:
        dist, total_s, marks, epochs = generate(cfg, segs, events, f)
    if not a.quiet:
        for name, (t, d) in marks.items():
            print(f"mark {name:<16} t={t:9.1f} s  vérité {d / 1000:10.4f} km")
        print(f"fin  {'':<16} t={total_s:9.1f} s  vérité {dist / 1000:10.4f} km  ({epochs} époques)")


if __name__ == "__main__":
    main()
