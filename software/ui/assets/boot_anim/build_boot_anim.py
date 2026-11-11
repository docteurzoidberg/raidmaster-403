#!/usr/bin/env python3
"""Raidmaster 403 — construit l'animation de démarrage (format binaire « RMBA ») depuis une planche.

Entrée : une planche PNG (images rangées de gauche à droite puis de haut en bas, COLS par ligne),
rendue dans Blender (variante retenue : HD sur grille lisse, glow fort, un tour calé sur le jingle ;
chemin de la planche lue au build : cmake/boot_anim.cmake). La scène et la planche ne sont pas publiées.

Sortie : fichier binaire petit-boutiste, identique pour le simulateur et l'ESP32 (partition flash
dédiée, lue en place), décodé par ui/boot_anim.c. Deux formats (--format, à trancher) :
    en-tête 16 octets : magic "RMBA", u16 version (1 ou 2), u16 largeur, u16 hauteur,
                        u16 nb images, u16 images/s, u16 réservé
    palette 256 × 4 octets (B, G, R, A), commune à toutes les images
  v1 (5,5 Mo) :
    images  nb × (largeur × hauteur) octets, index de palette (1 octet par pixel)
  v2 (1,1 Mo, mêmes pixels) :
    table   (nb + 1) × u32 : début du code de chaque image depuis le début des codes, puis la fin
    codes   images d'index (ligne par ligne) codées sans perte : la première en entier, chacune
            des suivantes par différence avec la précédente. Code = 1 octet, type sur les 2 bits de
            poids fort, longueur n sur les 6 autres (63 : n = 63 + entier LEB128 qui suit) :
              00 SAUT   n pixels identiques à l'image précédente (jamais dans la première)
              01 APLAT  n pixels d'un même index (1 octet)
              10 COPIE  n pixels, n octets d'index

Palette globale calculée sur toute l'animation (quantification médiane, sans tramage). En v2, le
fichier est relu et comparé pixel à pixel aux images avant d'être écrit.

Usage : build_boot_anim.py <planche.png> <sortie.bin> [--format 2] [--frames 36] [--cols 6]
        [--width 480] [--height 320] [--fps 12]
Prérequis : Pillow et numpy (CMake cherche un .venv à la racine du dépôt, voir le README).
Licence : GPL-3.0.
"""
import argparse
import struct
import sys

import numpy as np
from PIL import Image

OP_SKIP, OP_FILL, OP_COPY = 0, 1, 2
MIN_SKIP = 2        # saut plus court : absorbé dans une copie
MIN_FILL = 3        # aplat plus court : idem
LEN_ESC = 63


def run_remaining(a):
    """Pour chaque position, longueur restante de la suite de valeurs égales qui la contient."""
    n = len(a)
    starts = np.flatnonzero(np.concatenate(([True], a[1:] != a[:-1])))
    ends = np.append(starts[1:], n)
    run_id = np.cumsum(np.concatenate(([True], a[1:] != a[:-1]))) - 1
    return ends[run_id] - np.arange(n)


def code(op, n, out):
    if n < LEN_ESC:
        out.append(op << 6 | n)
        return
    out.append(op << 6 | LEN_ESC)
    v = n - LEN_ESC
    while v >= 0x80:
        out.append(v & 0x7F | 0x80)
        v >>= 7
    out.append(v)


def encode(cur, prev):
    """Codes d'une image (prev None : image complète). Gourmand : saut s'il est assez long, sinon
    aplat s'il est assez long, sinon copie jusqu'au prochain saut ou aplat possible."""
    n = len(cur)
    same = np.zeros(n, bool) if prev is None else cur == prev
    skip_rem = np.where(same, run_remaining(same), 0)
    fill_rem = run_remaining(cur)
    start_ok = (skip_rem >= MIN_SKIP) | (fill_rem >= MIN_FILL)
    starts = np.flatnonzero(start_ok)
    out = bytearray()
    i = 0
    while i < n:
        if skip_rem[i] >= MIN_SKIP:
            code(OP_SKIP, int(skip_rem[i]), out)
            i += int(skip_rem[i])
        elif fill_rem[i] >= MIN_FILL:
            code(OP_FILL, int(fill_rem[i]), out)
            out.append(int(cur[i]))
            i += int(fill_rem[i])
        else:
            k = np.searchsorted(starts, i, side="right")
            j = int(starts[k]) if k < len(starts) else n
            code(OP_COPY, j - i, out)
            out += cur[i:j].tobytes()
            i = j
    return bytes(out)


def decode(stream, buf):
    """Relecture (contrôle) : applique les codes d'une image à buf (image précédente)."""
    p = pos = 0
    while p < len(stream):
        c = stream[p]
        p += 1
        op, n = c >> 6, c & 0x3F
        if n == LEN_ESC:
            v, sh = 0, 0
            while True:
                b = stream[p]
                p += 1
                v |= (b & 0x7F) << sh
                sh += 7
                if b < 0x80:
                    break
            n += v
        if op == OP_FILL:
            buf[pos:pos + n] = stream[p]
            p += 1
        elif op == OP_COPY:
            buf[pos:pos + n] = np.frombuffer(stream, np.uint8, n, p)
            p += n
        elif op != OP_SKIP:
            raise ValueError(f"code {op} inconnu")
        pos += n
    if pos != len(buf):
        raise ValueError(f"image incomplète : {pos} pixels sur {len(buf)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sheet")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=36)
    ap.add_argument("--cols", type=int, default=6)
    ap.add_argument("--width", type=int, default=480)
    ap.add_argument("--height", type=int, default=320)
    ap.add_argument("--fps", type=int, default=12)
    ap.add_argument("--format", type=int, choices=(1, 2), default=2)
    a = ap.parse_args()

    sheet = Image.open(a.sheet).convert("RGB")
    w, h = a.width, a.height
    frames = []
    for i in range(a.frames):
        r, c = divmod(i, a.cols)
        frames.append(sheet.crop((c * w, r * h, (c + 1) * w, (r + 1) * h)))

    # Palette commune : quantification de toutes les images empilées.
    stack = Image.new("RGB", (w, h * len(frames)))
    for i, f in enumerate(frames):
        stack.paste(f, (0, i * h))
    pal_img = stack.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = pal_img.getpalette()[:256 * 3]
    pal += [0] * (256 * 3 - len(pal))

    idx = [np.frombuffer(f.quantize(palette=pal_img, dither=Image.Dither.NONE).tobytes(), np.uint8)
           for f in frames]
    raw = len(frames) * w * h
    header = b"RMBA" + struct.pack("<HHHHHH", a.format, w, h, len(frames), a.fps, 0)
    header += b"".join(bytes((pal[3 * k + 2], pal[3 * k + 1], pal[3 * k], 255)) for k in range(256))
    if a.format == 1:
        with open(a.out, "wb") as fo:
            fo.write(header)
            for f in idx:
                fo.write(f.tobytes())
        print(f"build_boot_anim : {a.out} (v1, {len(frames)} images {w}×{h}, {a.fps} i/s, "
              f"{raw / 1e6:.2f} Mo)", file=sys.stderr)
        return

    streams = [encode(cur, idx[i - 1] if i else None) for i, cur in enumerate(idx)]

    buf = np.zeros(w * h, np.uint8)
    for i, s in enumerate(streams):
        decode(s, buf)
        if not np.array_equal(buf, idx[i]):
            sys.exit(f"build_boot_anim : image {i} mal relue, fichier non écrit")

    offsets = [0]
    for s in streams:
        offsets.append(offsets[-1] + len(s))
    with open(a.out, "wb") as fo:
        fo.write(header)
        fo.write(struct.pack(f"<{len(offsets)}I", *offsets))
        for s in streams:
            fo.write(s)
    print(f"build_boot_anim : {a.out} (v2, {len(frames)} images {w}×{h}, {a.fps} i/s, "
          f"{offsets[-1] / 1e6:.2f} Mo de codes au lieu de {raw / 1e6:.2f} Mo, relu sans écart)",
          file=sys.stderr)


if __name__ == "__main__":
    main()
