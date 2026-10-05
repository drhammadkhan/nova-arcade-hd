#!/usr/bin/env python3
"""Volt Rally HD art: the volt paddle (a chrome shell with a glass core window, the core drawn white so
the game tints it per player), the plasma ball (cool and smash-hot, two flicker frames each), the
rival badges and their glyphs. All original.
    python3 games/VoltRally/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/voltrally/*.png and games/VoltRally/art.h (namespace vrart).
The paddle is the original's 6 x 34 hitbox scaled 4.5x (27 x 153) with a small overhang; pivots are
at the centre of every picture."""
import os
import sys
import math
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, poly, smooth, ellipse, circle, rrect, union, minus, paint, lin, rad, star  # noqa: E402

INK = (14, 12, 34)
WHITE = (255, 255, 255)
atlas = hd.Atlas(2048)

PW, PH = 32, 162          # drawn paddle size (hitbox 27 x 153)


def paddle():
    # the energy core: white, glossy, tinted by the game
    s = Sprite(PW, PH, pivot=(PW / 2, PH / 2), trim=False)
    c = s.c
    core = rrect(6, 14, PW - 12, PH - 28, 9)
    c.drawPath(core, paint(shader=lin(6, 0, PW - 6, 0, [(200, 200, 210), (255, 255, 255), (255, 255, 255), (170, 170, 185)], [0, 0.35, 0.55, 1])))
    c.drawPath(rrect(10, 18, 4, PH - 36, 2), paint(WHITE, alpha=0.9))
    for i in range(7):   # cell lines inside the core
        y = 26 + i * 18
        c.drawPath(poly([(7, y), (PW - 7, y)], closed=False), paint((150, 150, 170), stroke=1.2, alpha=0.6))
    atlas.add("IMG_CORE", s)

    # the chrome shell with a window cut out of it
    s = Sprite(PW + 8, PH + 8, pivot=((PW + 8) / 2, (PH + 8) / 2), trim=False)
    c = s.c
    c.translate(4, 4)
    shell = rrect(0, 0, PW, PH, 13)
    window = rrect(7, 15, PW - 14, PH - 30, 8)
    body = minus(shell, window)
    c.drawPath(shell, paint(INK, stroke=5))
    c.drawPath(body, paint(shader=lin(0, 0, PW, 0, [(70, 76, 104), (200, 210, 235), (120, 128, 160), (40, 44, 70)], [0, 0.3, 0.65, 1])))
    for y0 in (2, PH - 14):   # end caps
        cap = rrect(2, y0, PW - 4, 12, 6)
        c.drawPath(cap, paint(shader=lin(0, 0, PW, 0, [(150, 160, 190), (255, 255, 255), (170, 180, 210), (80, 86, 120)], [0, 0.32, 0.6, 1])))
        c.drawPath(circle(PW / 2, y0 + 6, 2.2), paint((60, 64, 90)))
    # the grip clamp across the middle of the window
    clamp = rrect(3, PH / 2 - 7, PW - 6, 14, 5)
    c.drawPath(clamp, paint(INK, stroke=2.5))
    c.drawPath(clamp, paint(shader=lin(0, 0, PW, 0, [(90, 96, 128), (230, 236, 255), (110, 118, 150)], [0, 0.35, 1])))
    c.drawPath(rrect(6, PH / 2 - 1.2, PW - 12, 2.4, 1.2), paint((50, 54, 80)))
    c.drawPath(window, paint(INK, stroke=2))
    c.drawPath(rrect(3, 18, 2.5, PH - 36, 1.2), paint(WHITE, alpha=0.55))
    atlas.add("IMG_SHELL", s)


def bolt_path(cx, cy, k):
    pts = [(-0.25, -1), (0.45, -1), (0.08, -0.12), (0.5, -0.12), (-0.35, 1), (-0.05, 0.12), (-0.45, 0.12)]
    return poly([(cx + x * k, cy + y * k) for x, y in pts])


def ball(name, colors, filament, seed):
    R = 26
    s = Sprite(2 * R + 8, 2 * R + 8, pivot=(R + 4, R + 4))
    c = s.c
    c.translate(R + 4, R + 4)
    c.drawPath(circle(0, 0, R), paint(shader=rad(-6, -8, R * 1.5, colors, [0, 0.35, 0.75, 1])))
    rnd = random.Random(seed)
    c.save()
    c.clipPath(circle(0, 0, R - 2), hd.skia.ClipOp.kIntersect, True)
    for i in range(5):   # electric filaments from the core to the shell
        a = rnd.uniform(0, math.tau)
        pts = [(0, 0)]
        for j in range(1, 5):
            r = R * j / 4.0
            aa = a + rnd.uniform(-0.45, 0.45)
            pts.append((math.cos(aa) * r, math.sin(aa) * r))
        p = poly(pts, closed=False)
        c.drawPath(p, paint(filament, stroke=3.2, blur=2, alpha=0.7))
        c.drawPath(p, paint(WHITE, stroke=1.3))
    c.restore()
    c.drawPath(circle(0, 0, R * 0.32), paint(WHITE, blur=3))
    c.drawPath(ellipse(-8, -11, 9, 5), paint(WHITE, alpha=0.75))
    c.drawPath(circle(0, 0, R), paint(filament, stroke=2, alpha=0.8))
    atlas.add(name, s)


def badge():
    R = 60
    s = Sprite(2 * R + 10, 2 * R + 10, pivot=(R + 5, R + 5))
    c = s.c
    c.translate(R + 5, R + 5)
    hexa = poly([(math.cos(math.radians(30 + 60 * i)) * R, math.sin(math.radians(30 + 60 * i)) * R) for i in range(6)])
    inner = poly([(math.cos(math.radians(30 + 60 * i)) * (R - 10), math.sin(math.radians(30 + 60 * i)) * (R - 10)) for i in range(6)])
    c.drawPath(hexa, paint(INK, stroke=6))
    c.drawPath(hexa, paint(shader=lin(0, -R, 0, R, [(255, 255, 255), (200, 200, 212), (130, 130, 150)])))
    c.drawPath(inner, paint(shader=lin(0, -R, 0, R, [(150, 150, 168), (225, 225, 235), (255, 255, 255)])))
    c.save()
    c.clipPath(inner, hd.skia.ClipOp.kIntersect, True)
    c.drawPath(ellipse(-10, -R * 0.75, R * 0.9, R * 0.45), paint(WHITE, alpha=0.45))
    c.restore()
    atlas.add("IMG_BADGE", s)


def glyphs():
    G = 70
    def sprite():
        s = Sprite(G, G, pivot=(G / 2, G / 2), trim=False)
        s.c.translate(G / 2, G / 2)
        return s, s.c
    # SPARKY: a lightning bolt
    s, c = sprite()
    c.drawPath(bolt_path(0, 0, 30), paint(WHITE))
    atlas.add("IMG_G0", s)
    # FLUX: three waves
    s, c = sprite()
    for k in (-1, 0, 1):
        pts = [(x, k * 14 + 7 * math.sin(x / 9.0)) for x in range(-28, 29, 2)]
        c.drawPath(poly(pts, closed=False), paint(WHITE, stroke=6))
    atlas.add("IMG_G1", s)
    # SURGE: a double chevron
    s, c = sprite()
    for y in (-10, 10):
        c.drawPath(poly([(-24, y + 12), (0, y - 10), (24, y + 12)], closed=False), paint(WHITE, stroke=8))
    atlas.add("IMG_G2", s)
    # ARCLIGHT: two prongs with an arc between them
    s, c = sprite()
    c.drawPath(rrect(-26, 4, 9, 26, 3), paint(WHITE))
    c.drawPath(rrect(17, 4, 9, 26, 3), paint(WHITE))
    arc = poly([(-21, 4), (-14, -12), (-6, -4), (0, -24), (6, -8), (13, -16), (21, 4)], closed=False)
    c.drawPath(arc, paint(WHITE, stroke=5))
    c.drawPath(star(0, -24, 10, 3, 4), paint(WHITE))
    atlas.add("IMG_G3", s)
    # DYNAMO: a gear
    s, c = sprite()
    pts = []
    for i in range(40):
        a = math.tau * i / 40
        r = 30 if (i // 2) % 2 == 0 else 23
        pts.append((math.cos(a) * r, math.sin(a) * r))
    c.drawPath(minus(poly(pts), circle(0, 0, 10)), paint(WHITE))
    atlas.add("IMG_G4", s)
    # OVERLOAD: a burst with a bolt cut out
    s, c = sprite()
    c.drawPath(minus(star(0, 0, 33, 20, 10), bolt_path(0, 0, 17)), paint(WHITE))
    atlas.add("IMG_G5", s)
    # the player: a plug
    s, c = sprite()
    c.drawPath(rrect(-13, -30, 7, 18, 3), paint(WHITE))
    c.drawPath(rrect(6, -30, 7, 18, 3), paint(WHITE))
    c.drawPath(union(rrect(-22, -14, 44, 26, 8), rrect(-8, 8, 16, 22, 5)), paint(WHITE))
    atlas.add("IMG_GYOU", s)
    atlas.table("GLYPH", ["IMG_G%d" % i for i in range(6)])


def main():
    paddle()
    ball("IMG_BALL0", [WHITE, (190, 245, 255), (60, 170, 255), (30, 60, 170)], (120, 220, 255), 3)
    ball("IMG_BALL1", [WHITE, (190, 245, 255), (60, 170, 255), (30, 60, 170)], (120, 220, 255), 11)
    ball("IMG_HOT0", [WHITE, (255, 240, 170), (255, 190, 50), (200, 80, 20)], (255, 220, 90), 5)
    ball("IMG_HOT1", [WHITE, (255, 240, 170), (255, 190, 50), (200, 80, 20)], (255, 220, 90), 17)
    atlas.table("BALL", ["IMG_BALL0", "IMG_BALL1"])
    atlas.table("HOT", ["IMG_HOT0", "IMG_HOT1"])
    badge()
    glyphs()
    atlas.write(os.path.join(ROOT, "assets", "voltrally"), os.path.join(ROOT, "games", "VoltRally", "art.h"), "vrart", "voltrally")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "vr_sheet.png"))


if __name__ == "__main__":
    main()
