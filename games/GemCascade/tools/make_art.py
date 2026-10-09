#!/usr/bin/env python3
"""Gem Cascade HD art: seven faceted vector gems (each its own cut and colour), the nova orb, the
flame aura, the star glint, the cursor and a gem shard for bursts. All original.
    python3 games/GemCascade/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/gemcascade/*.png and games/GemCascade/art.h (namespace gcart).
One board cell is 117 px (the original's 26 x 4.5); gems are about 100 px, pivot at their centre."""
import os
import sys
import math
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
import skia  # noqa: E402
from hd import Sprite, poly, ellipse, circle, rrect, paint, lin, rad, star, mix  # noqa: E402

atlas = hd.Atlas(2048)
WHITE = (255, 255, 255)
S = 128            # sprite box (screen px); gems are drawn about 104 px across
R = 50             # gem radius
LIGHT = (-0.45, -0.7, 0.55)

# name, dark, mid, light (the mid colours match the original's particle colours)
GEMS = [
    ("RUBY", (88, 4, 34), (236, 32, 72), (255, 190, 200)),
    ("SAPPHIRE", (8, 22, 100), (40, 112, 240), (196, 232, 255)),
    ("EMERALD", (2, 60, 40), (30, 190, 96), (200, 255, 200)),
    ("TOPAZ", (130, 66, 0), (255, 204, 40), (255, 252, 200)),
    ("AMETHYST", (52, 10, 92), (168, 72, 236), (240, 208, 255)),
    ("PEARL", (98, 100, 140), (214, 220, 242), (255, 255, 255)),
    ("AMBER", (110, 30, 2), (252, 132, 28), (255, 228, 160)),
]


def norm(v):
    m = math.sqrt(sum(x * x for x in v))
    return tuple(x / m for x in v)


L = norm(LIGHT)


def ramp(g, b):
    """b in -1..1 -> dark .. mid .. light"""
    _, d, m, l = g
    if b < 0:
        return mix(m, d, min(1, -b))
    return mix(m, l, min(1, b))


def shade_of(cx, cy, fx, fy, z, jit):
    dx, dy = fx - cx, fy - cy
    n = norm((dx, dy, z * (abs(dx) + abs(dy) + 1e-3)))
    b = n[0] * L[0] + n[1] * L[1] + n[2] * L[2]
    return (b - 0.55) * 1.9 + jit


def regular(n, r, rot=0.0, sx=1.0, sy=1.0):
    return [(math.cos(rot + i * 2 * math.pi / n) * r * sx, math.sin(rot + i * 2 * math.pi / n) * r * sy) for i in range(n)]


def outline_of(k):
    if k == 0:   # ruby: round brilliant (octagon)
        return regular(8, R, math.pi / 8)
    if k == 1:   # sapphire: cushion square
        a, b = 44, 30
        return [(-b, -a), (b, -a), (a, -b), (a, b), (b, a), (-b, a), (-a, b), (-a, -b)]
    if k == 2:   # emerald: tall step cut
        return [(-24, -50), (24, -50), (38, -36), (38, 36), (24, 50), (-24, 50), (-38, 36), (-38, -36)]
    if k == 3:   # topaz: kite (pointed above and below)
        return [(0, -54), (44, -10), (0, 54), (-44, -10)]
    if k == 4:   # amethyst: trillion (rounded triangle cut as a hexagon)
        pts = []
        for i in range(3):
            a = -math.pi / 2 + i * 2 * math.pi / 3
            for d in (-0.16, 0.16):
                pts.append((math.cos(a + d) * 56, math.sin(a + d) * 56 + 8))
        return pts
    if k == 6:   # amber: hexagon
        return regular(6, R + 2, 0)
    return None


def lerp_pt(p, q, t):
    return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)


def drop_shadow(c, path):
    q = skia.Path(path)
    q.offset(0, 6)
    c.drawPath(q, paint((6, 2, 20), alpha=0.55, blur=6))


def glints(c, x, y, r):
    """the little four-point star that catches the light"""
    c.drawPath(circle(x, y, r * 0.55), paint(WHITE, alpha=0.6, blur=r * 0.35))
    c.drawPath(star(x, y, r, r * 0.16, 4, rot=-90), paint(WHITE))


def faceted(k, rng):
    g = GEMS[k]
    O = outline_of(k)
    n = len(O)
    s = Sprite(S, S, pivot=(S / 2, S / 2))
    c = s.c
    c.translate(S / 2, S / 2)
    body = poly(O)
    drop_shadow(c, body)
    if k == 2:   # step cut: concentric bands of long facets
        rings = [O, [(x * 0.78, y * 0.82) for x, y in O], [(x * 0.56, y * 0.64) for x, y in O]]
        for ri in range(2):
            A, B = rings[ri], rings[ri + 1]
            for i in range(n):
                j = (i + 1) % n
                fx = (A[i][0] + A[j][0] + B[i][0] + B[j][0]) / 4
                fy = (A[i][1] + A[j][1] + B[i][1] + B[j][1]) / 4
                b = shade_of(0, 0, fx, fy, 0.5 + ri * 0.5, rng.uniform(-0.12, 0.12))
                c.drawPath(poly([A[i], A[j], B[j], B[i]]), paint(ramp(g, b)))
        T = rings[2]
        c.drawPath(poly(T), paint(shader=lin(-20, -30, 20, 30, [ramp(g, 0.5), ramp(g, -0.1), ramp(g, 0.25)])))
        # a few reflections in the table
        for t in (0.3, 0.6):
            c.drawPath(poly([lerp_pt(T[7], T[0], t), lerp_pt(T[3], T[4], t)], closed=False), paint(WHITE, stroke=1.2, alpha=0.25))
        edges = [poly(r) for r in rings]
        for e in edges:
            c.drawPath(e, paint(WHITE, stroke=1.1, alpha=0.3))
        for i in range(n):
            c.drawPath(poly([rings[0][i], rings[2][i]], closed=False), paint(WHITE, stroke=1.0, alpha=0.25))
    else:
        tscale = 0.5 if k != 3 else 0.42
        M = [lerp_pt((0, 0), p, 0.78) for p in O]
        T = [lerp_pt((0, 0), p, tscale) for p in O]
        T = [(x - 2, y - 3) for x, y in T]   # the table sits a touch towards the light
        facets = []
        for i in range(n):
            j = (i + 1) % n
            om = lerp_pt(O[i], O[j], 0.5)
            facets.append(([O[i], om, M[i]], 0.35))
            facets.append(([om, O[j], M[j]], 0.35))
            facets.append(([M[i], om, M[j]], 0.45))
            facets.append(([M[i], M[j], T[j], T[i]], 0.8))
        for pts, z in facets:
            fx = sum(p[0] for p in pts) / len(pts)
            fy = sum(p[1] for p in pts) / len(pts)
            b = shade_of(0, 0, fx, fy, z, rng.uniform(-0.18, 0.18))
            # gems throw light back from the far side: some dark facets flare up
            if b < -0.4 and rng.random() < 0.3:
                b = 0.35
            c.drawPath(poly(pts), paint(ramp(g, b)))
        c.drawPath(poly(T), paint(shader=lin(T[0][0] - 30, -30, 30, 30, [ramp(g, 0.7), ramp(g, 0.05), ramp(g, -0.25), ramp(g, 0.3)])))
        # crown lines
        for pts, z in facets:
            c.drawPath(poly(pts), paint(WHITE, stroke=0.9, alpha=0.22))
        c.drawPath(poly(T), paint(WHITE, stroke=1.4, alpha=0.5))
    # rim: dark outline, then a thin inner light along the upper-left edges
    c.drawPath(body, paint(g[1], stroke=1.6, alpha=0.9))
    c.drawPath(body, paint(mix(g[1], (10, 4, 30), 0.75), stroke=3.4, alpha=0.9))
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    rim = skia.Path(body)
    rim.offset(2.5, 3.5)
    c.drawPath(skia.Op(body, rim, skia.PathOp.kDifference_PathOp), paint(WHITE, alpha=0.55))
    # a soft gloss sweep across the top
    c.drawPath(ellipse(-14, -26, 30, 14), paint(WHITE, alpha=0.18, blur=4))
    c.restore()
    glints(c, -18, -22, 13)
    return s


def pearl(rng):
    g = GEMS[5]
    s = Sprite(S, S, pivot=(S / 2, S / 2))
    c = s.c
    c.translate(S / 2, S / 2)
    body = circle(0, 0, 46)
    drop_shadow(c, body)
    c.drawPath(body, paint(shader=rad(-14, -18, 70, [(255, 255, 255), g[2], (170, 170, 210), g[1]], [0, 0.35, 0.75, 1])))
    # iridescent sheen
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    sweep = skia.GradientShader.MakeSweep(0, 0, [hd.colour_int(x) for x in [(255, 170, 220), (160, 240, 255), (255, 240, 170), (210, 170, 255), (255, 170, 220)]])
    c.drawPath(circle(0, 0, 46), paint(shader=sweep, alpha=0.28))
    c.drawPath(circle(8, 12, 40), paint((140, 140, 190), alpha=0.25, blur=10))
    c.drawPath(ellipse(10, 30, 26, 10), paint((255, 230, 250), alpha=0.5, blur=5))   # bounce light
    c.restore()
    c.drawPath(body, paint((80, 80, 120), stroke=3.2, alpha=0.9))
    c.drawPath(ellipse(-15, -20, 16, 10), paint(WHITE, alpha=0.85, blur=3))
    glints(c, -18, -22, 13)
    return s


def nova():
    """The nova: a rainbow crystal orb. The body rotates in game; the shine stays put."""
    s = Sprite(S, S, pivot=(S / 2, S / 2))
    c = s.c
    c.translate(S / 2, S / 2)
    body = circle(0, 0, 48)
    cols = [(255, 80, 120), (255, 200, 60), (90, 240, 120), (60, 200, 255), (170, 100, 255), (255, 80, 200), (255, 80, 120)]
    sweep = skia.GradientShader.MakeSweep(0, 0, [hd.colour_int(x) for x in cols])
    c.drawPath(body, paint(shader=sweep))
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    # disco facets: a ring of triangles lightening and darkening the colours
    rng = random.Random(7)
    for ring_i, (r0, r1, cnt) in enumerate(((48, 34, 12), (34, 18, 9), (18, 0, 6))):
        for i in range(cnt):
            a0 = i * 2 * math.pi / cnt + ring_i * 0.3
            a1 = (i + 1) * 2 * math.pi / cnt + ring_i * 0.3
            p = [(math.cos(a0) * r0, math.sin(a0) * r0), (math.cos(a1) * r0, math.sin(a1) * r0),
                 (math.cos((a0 + a1) / 2) * r1, math.sin((a0 + a1) / 2) * r1)]
            v = rng.random()
            c.drawPath(poly(p), paint(WHITE if v > 0.5 else (20, 0, 40), alpha=0.1 + 0.25 * abs(v - 0.5)))
            c.drawPath(poly(p), paint(WHITE, stroke=0.8, alpha=0.3))
    c.drawPath(circle(0, 0, 22), paint(WHITE, alpha=0.55, blur=10))
    c.restore()
    atlas.add("IMG_NOVA", s)
    s = Sprite(S, S, pivot=(S / 2, S / 2))
    c = s.c
    c.translate(S / 2, S / 2)
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    c.drawPath(circle(10, 14, 50), paint((30, 0, 60), alpha=0.35, blur=12))
    c.restore()
    c.drawPath(body, paint((40, 10, 70), stroke=3.2, alpha=0.9))
    c.drawPath(ellipse(-15, -20, 17, 10), paint(WHITE, alpha=0.8, blur=3))
    glints(c, -18, -22, 15)
    atlas.add("IMG_NOVA_SHINE", s)


def flame():
    """A ring of flame tongues, white-hot inside: drawn additively behind a flame gem."""
    s = Sprite(180, 180, pivot=(90, 90))
    c = s.c
    c.translate(90, 90)
    n = 9
    for layer, (rr, col, a) in enumerate(((84, (255, 90, 20), 0.75), (72, (255, 170, 40), 0.85), (60, (255, 240, 170), 0.9))):
        p = skia.Path()
        for i in range(n * 2 + 1):
            ang = i * math.pi / n + layer * 0.2
            r = rr if i % 2 == 0 else rr * 0.68
            x, y = math.cos(ang) * r, math.sin(ang) * r
            if i == 0:
                p.moveTo(x, y)
            else:
                pa = (i - 0.5) * math.pi / n + layer * 0.2
                cr = (rr * 0.92) if i % 2 == 0 else rr * 0.8
                p.quadTo(math.cos(pa + 0.12) * cr, math.sin(pa + 0.12) * cr, x, y)
        p.close()
        c.drawPath(p, paint(col, alpha=a, blur=3 + layer))
    atlas.add("IMG_FLAME", s)


def star_glint():
    s = Sprite(160, 160, pivot=(80, 80))
    c = s.c
    c.translate(80, 80)
    c.drawPath(star(0, 0, 76, 10, 4), paint((150, 240, 255), alpha=0.7, blur=6))
    c.drawPath(star(0, 0, 70, 6, 4), paint(WHITE))
    c.drawPath(circle(0, 0, 12), paint(WHITE, blur=5))
    atlas.add("IMG_GLINT", s)


def star_badge():
    """the star gem's mark: a small five-point star set into the gem"""
    s = Sprite(60, 60, pivot=(30, 30))
    c = s.c
    c.translate(30, 30)
    c.drawPath(star(0, 0, 25, 11, 5), paint((60, 220, 255), alpha=0.9, blur=5))
    c.drawPath(star(0, 0, 22, 9.5, 5), paint((20, 40, 90), stroke=4))
    c.drawPath(star(0, 0, 22, 9.5, 5), paint(shader=rad(-5, -6, 26, [WHITE, (190, 245, 255), (70, 190, 255)])))
    atlas.add("IMG_STARMARK", s)


def cursor():
    C = 124
    s = Sprite(C + 20, C + 20, pivot=((C + 20) / 2, (C + 20) / 2))
    c = s.c
    c.translate((C + 20) / 2, (C + 20) / 2)
    h, L_ = C / 2, 34
    p = skia.Path()
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.moveTo(sx * h, sy * (h - L_))
            p.lineTo(sx * h, sy * (h - 12))
            p.quadTo(sx * h, sy * h, sx * (h - 12), sy * h)
            p.lineTo(sx * (h - L_), sy * h)
    c.drawPath(p, paint(WHITE, stroke=10, alpha=0.5, blur=5))
    c.drawPath(p, paint(WHITE, stroke=6))
    atlas.add("IMG_CURSOR", s)
    s = Sprite(C + 20, C + 20, pivot=((C + 20) / 2, (C + 20) / 2))
    c = s.c
    c.translate((C + 20) / 2, (C + 20) / 2)
    frame = rrect(-h, -h, C, C, 20)
    c.drawPath(frame, paint(WHITE, stroke=12, alpha=0.5, blur=6))
    c.drawPath(frame, paint(WHITE, stroke=6))
    atlas.add("IMG_SELECT", s)


def shard():
    s = Sprite(30, 30, pivot=(15, 15))
    c = s.c
    c.translate(15, 15)
    a, b, d = (0, -13), (11, 9), (-10, 7)
    m = (1, 1)
    c.drawPath(poly([a, b, m]), paint((255, 255, 255)))
    c.drawPath(poly([b, d, m]), paint((150, 150, 170)))
    c.drawPath(poly([d, a, m]), paint((215, 215, 228)))
    atlas.add("IMG_SHARD", s)


def main():
    rng = random.Random(1234)
    names = []
    for k, g in enumerate(GEMS):
        spr = pearl(rng) if k == 5 else faceted(k, rng)
        atlas.add("IMG_GEM_" + g[0], spr)
        names.append("IMG_GEM_" + g[0])
    atlas.table("GEM", names)
    nova()
    flame()
    star_glint()
    star_badge()
    cursor()
    shard()
    atlas.write(os.path.join(ROOT, "assets", "gemcascade"), os.path.join(ROOT, "games", "GemCascade", "art.h"), "gcart", "gemcascade")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "gc_sheet.png"), bg=(30, 22, 60))


if __name__ == "__main__":
    main()
