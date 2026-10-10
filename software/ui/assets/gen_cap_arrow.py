#!/usr/bin/env python3
"""Raidmaster 403 — génère ui/img_cap_arrow.c (flèche de cap, style 07 « isométrique 3D »).

Deux sprites ARGB8888 64×64 (anti-aliasing par suréchantillonnage 4×4, stdlib seule) :
  - img_cap_arrow        : flèche ambre + facette claire (face gauche éclairée) ;
  - img_cap_arrow_shadow : ombre portée noire 50 %, dessinée décalée sous la flèche.
Les deux sont pivotés séparément autour de leur centre, ce qui garde l'ombre toujours
décalée dans la même direction (lumière fixe) quel que soit le cap.

Géométrie reprise du SVG validé (viewBox 100, pointe vers le haut = Nord).
Usage : python3 ui/assets/gen_cap_arrow.py  (réécrit ui/img_cap_arrow.c)
Licence : GPL-3.0.
"""
import os

SIZE = 64
SCALE = 0.6           # viewBox 100 → rayon max ~31 px, tient dans 64 px en rotation
SS = 4                # suréchantillonnage par axe

ARROW = [(50, 10), (86, 86), (50, 70), (14, 86)]
FACET = [(50, 10), (50, 70), (14, 86)]
AMBER = (0xFF, 0xB8, 0x4D)
FACET_RGB = (0xFF, 0xD2, 0x8A)
FACET_OPA = 0.7


def to_px(poly):
    return [((x - 50) * SCALE + SIZE / 2, (y - 50) * SCALE + SIZE / 2) for x, y in poly]


def inside(poly, x, y):
    c = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def coverage(poly, px, py):
    hit = 0
    for sy in range(SS):
        for sx in range(SS):
            if inside(poly, px + (sx + 0.5) / SS, py + (sy + 0.5) / SS):
                hit += 1
    return hit / (SS * SS)


def render_arrow():
    arrow, facet = to_px(ARROW), to_px(FACET)
    out = []
    for py in range(SIZE):
        for px in range(SIZE):
            a = coverage(arrow, px, py)
            f = coverage(facet, px, py) * FACET_OPA
            rgb = [AMBER[i] * (1 - f) + FACET_RGB[i] * f for i in range(3)]
            out.append((rgb, a))
    return out


def render_shadow():
    arrow = to_px(ARROW)
    return [((0, 0, 0), coverage(arrow, px, py) * 0.5)
            for py in range(SIZE) for px in range(SIZE)]


def c_array(name, pixels):
    data = []
    for (r, g, b), a in pixels:
        # ARGB8888 little-endian en mémoire : B, G, R, A
        data += [round(b), round(g), round(r), round(a * 255)]
    lines = []
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{v:02X}" for v in data[i:i + 16]) + ",")
    body = "\n".join(lines)
    return f"""static const uint8_t {name}_map[] = {{
{body}
}};

const lv_image_dsc_t {name} = {{
    .header = {{
        .magic  = LV_IMAGE_HEADER_MAGIC,
        .cf     = LV_COLOR_FORMAT_ARGB8888,
        .w      = {SIZE},
        .h      = {SIZE},
        .stride = {SIZE * 4},
    }},
    .data_size = sizeof({name}_map),
    .data      = {name}_map,
}};
"""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    dst = os.path.join(here, "..", "img_cap_arrow.c")
    with open(dst, "w") as f:
        f.write("/*\n * Raidmaster 403 — sprites flèche de cap (style 07 isométrique 3D).\n"
                " * FICHIER GÉNÉRÉ par ui/assets/gen_cap_arrow.py — ne pas éditer à la main.\n"
                " * Licence : GPL-3.0.\n */\n\n#include \"img_cap_arrow.h\"\n\n")
        f.write(c_array("img_cap_arrow", render_arrow()))
        f.write("\n")
        f.write(c_array("img_cap_arrow_shadow", render_shadow()))
    print("écrit", os.path.normpath(dst))


if __name__ == "__main__":
    main()
