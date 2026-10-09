#!/usr/bin/env python3
"""
Lit un module u-blox branché en USB/FTDI et enregistre les trames NMEA brutes.

Utile quand on reçoit physiquement le NEO-M9N : on le pose sur un rebord de fenêtre
avec son antenne, on branche via FTDI (ou USB natif u-blox), et on capture 30 min de
NMEA « vrai » pour alimenter le catalogue de scénarios avec le bruit propre du module.

Nécessite `pyserial` (voir `requirements.txt`).

STAGING v0 — à revoir par l'agent simulation (notamment : stratégie d'auto-découverte
de la vitesse série, détection automatique du port, interface interactive).
"""

import argparse
import sys
import time
from pathlib import Path


def main(argv):
    try:
        import serial  # type: ignore
    except ImportError:
        print("ERREUR : pyserial manquant. Installer : pip install pyserial", file=sys.stderr)
        return 2

    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True, help="port série, ex: /dev/ttyUSB0")
    p.add_argument("--baud", type=int, default=9600, help="vitesse (défaut 9600, NEO-M9N par défaut)")
    p.add_argument("--duration", type=float, default=60.0, help="durée d'enregistrement en secondes")
    p.add_argument("--out", type=str, required=True, help="fichier .nmea en sortie")
    args = p.parse_args(argv)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1.0)
    except Exception as e:
        print(f"ERREUR ouverture port {args.port} @ {args.baud} : {e}", file=sys.stderr)
        return 2

    start = time.time()
    nb_rmc = 0
    nb_gga = 0
    nb_other = 0

    try:
        with out_path.open("w", encoding="ascii", newline="") as fh:
            while (time.time() - start) < args.duration:
                try:
                    line = ser.readline().decode("ascii", errors="replace").strip()
                except Exception as e:
                    print(f"WARN lecture série : {e}", file=sys.stderr)
                    continue
                if not line.startswith("$"):
                    continue
                fh.write(line + "\r\n")
                if "GPRMC" in line or "GNRMC" in line:
                    nb_rmc += 1
                elif "GPGGA" in line or "GNGGA" in line:
                    nb_gga += 1
                else:
                    nb_other += 1
    finally:
        ser.close()

    elapsed = time.time() - start
    print(f"OK : {out_path} — {nb_rmc} RMC, {nb_gga} GGA, {nb_other} autres, "
          f"{elapsed:.1f}s écoulés")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
