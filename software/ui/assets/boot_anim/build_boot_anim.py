#!/usr/bin/env python3
"""Raidmaster 403 — construit l'animation de démarrage (format binaire « RMBA ») depuis une planche.

Entrée : une planche PNG (images rangées de gauche à droite puis de haut en bas, COLS par ligne),
rendue dans Blender depuis ui/assets/boot_anim/boot_anim.blend (variante retenue : HD sur grille
lisse, glow fort → docs/artefacts/boot-anim/2e_selection_hd_glow_fort.png).

Sortie : fichier binaire petit-boutiste, identique pour le simulateur et l'ESP32 (partition flash
dédiée, lue en place) :
    en-tête 16 octets : magic "RMBA", u16 version=1, u16 largeur, u16 hauteur, u16 nb images,
                        u16 images/s, u16 réservé
    palette 256 × 4 octets (B, G, R, A), commune à toutes les images
    images  nb × (largeur × hauteur) octets, index de palette (1 octet par pixel)

Palette globale calculée sur toute l'animation (quantification médiane, sans tramage).

Usage : build_boot_anim.py <planche.png> <sortie.bin> [--frames 36] [--cols 6] [--width 480]
        [--height 320] [--fps 12]
Prérequis : Pillow (venv du projet 403 : ~/403/.venv).
Licence : GPL-3.0.
"""
import argparse
import struct
import sys

from PIL import Image


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sheet")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=36)
    ap.add_argument("--cols", type=int, default=6)
    ap.add_argument("--width", type=int, default=480)
    ap.add_argument("--height", type=int, default=320)
    ap.add_argument("--fps", type=int, default=12)
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

    with open(a.out, "wb") as fo:
        fo.write(b"RMBA" + struct.pack("<HHHHHH", 1, w, h, len(frames), a.fps, 0))
        for k in range(256):
            rr, gg, bb = pal[3 * k:3 * k + 3]
            fo.write(bytes((bb, gg, rr, 255)))
        for f in frames:
            q = f.quantize(palette=pal_img, dither=Image.Dither.NONE)
            fo.write(q.tobytes())
    print(f"build_boot_anim : {a.out} ({len(frames)} images {w}×{h}, {a.fps} i/s)", file=sys.stderr)


if __name__ == "__main__":
    main()
