#!/usr/bin/env python3
"""Raidmaster 403 — journal de la carte pendant N secondes, sans terminal interactif.

Redémarre l'ESP32-S3 par la ligne RTS de l'USB Serial/JTAG (comme esptool, sans passer en mode
téléchargement), puis recopie ce que la carte écrit, couleurs ANSI retirées. Si le port disparaît
un instant (reset, usbipd), il est rouvert. Appelé par tools/fw.sh (log, flash) avec le Python
d'ESP-IDF, qui fournit pyserial.

    fw_log.py <port> [secondes] [--no-reset]

Licence : GPL-3.0.
"""

import re
import sys
import time

import serial

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def open_port(port, deadline):
    """Ouvre le port avec DTR et RTS au repos (pyserial les lèverait à l'ouverture : sur l'USB
    Serial/JTAG, ce sont les lignes BOOT et reset), comme le moniteur d'ESP-IDF."""
    while time.monotonic() < deadline:
        ser = serial.Serial()
        ser.port, ser.baudrate, ser.timeout = port, 115200, 0.2
        ser.dtr = False
        ser.rts = False
        try:
            ser.open()
            return ser
        except (serial.SerialException, OSError):
            time.sleep(0.2)
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__.strip().splitlines()[0])
        return 2
    port = args[0]
    secs = float(args[1]) if len(args) > 1 else 10.0
    deadline = time.monotonic() + secs

    ser = open_port(port, deadline)
    if ser is None:
        print(f"LOG : impossible d'ouvrir {port}")
        return 1
    if "--no-reset" not in sys.argv:
        ser.dtr = False             # pas de mode téléchargement (BOOT)
        ser.rts = True              # reset de la puce
        time.sleep(0.1)
        ser.rts = False
    print(f"LOG : {port}, {secs:g} s")

    pending = b""
    while time.monotonic() < deadline:
        try:
            chunk = ser.read(512)
        except (serial.SerialException, OSError):
            ser.close()
            ser = open_port(port, deadline)
            if ser is None:
                break
            continue
        pending += chunk
        *lines, pending = pending.split(b"\n")
        for raw in lines:
            print(ANSI.sub("", raw.decode("utf-8", "replace")).rstrip("\r"), flush=True)
    if pending:
        print(ANSI.sub("", pending.decode("utf-8", "replace")))
    if ser is not None:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
