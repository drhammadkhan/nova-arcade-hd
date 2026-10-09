#!/usr/bin/env python3
"""Alien Tide HD art: three alien species (two animation frames each), the bonus saucer and the
player's cannon, drawn as vector shapes. All original.
    python3 games/AlienTide/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/alientide/*.png and games/AlienTide/art.h (namespace atart).
Each alien fits the original's 16 x 12 cell scaled 4.5x (72 x 54), pivot at its centre."""
import os
import sys
import math

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, union, paint, lin, rad, capsule  # noqa: E402

INK = (20, 12, 36)
atlas = hd.Atlas(2048)
CW, CH = 72, 54


def eyes(c, pts, r, look=0):
    for x, y in pts:
        c.drawPath(circle(x, y, r), paint((255, 255, 255)))
        c.drawPath(circle(x + look, y + r * 0.2, r * 0.55), paint(INK))
        c.drawPath(circle(x - r * 0.3 + look, y - r * 0.35, r * 0.22), paint((255, 255, 255)))


def orb(frame):
    """The top row: a floating brain-orb with a crown of feelers (pink, 30 points)."""
    s = Sprite(CW + 20, CH + 20, pivot=((CW + 20) / 2, (CH + 20) / 2))
    c = s.c
    c.translate((CW + 20) / 2, (CH + 20) / 2)
    for i in range(5):   # feelers
        a = math.radians(-150 + i * 30 + (8 if frame else -8))
        c.drawPath(smooth([(math.cos(a) * 14, math.sin(a) * 14), (math.cos(a) * 26, math.sin(a) * 26 - 4), (math.cos(a) * 32, math.sin(a) * 32)], closed=False), paint((255, 130, 220), stroke=4))
        c.drawPath(circle(math.cos(a) * 32, math.sin(a) * 32, 4.5), paint((255, 230, 120)))
    body = ellipse(0, 2, 24, 20)
    outlined(c, body, (255, 120, 210), INK, 2.6, shader=rad(-8, -8, 30, [(255, 210, 240), (255, 110, 210), (170, 40, 140)]))
    c.drawPath(smooth([(-16, -6), (-8, -12), (0, -6), (8, -12), (16, -6)], closed=False), paint((200, 60, 160), stroke=2.4))
    eyes(c, [(-8, 4), (8, 4)], 6, 1 if frame else -1)
    c.drawPath(smooth([(-6, 14), (0, 17 if frame else 15), (6, 14)], closed=False), paint(INK, stroke=2.4))
    atlas.add("IMG_ORB%d" % frame, s)


def mantis(frame):
    """Rows two and three: a cyan mantis with scything arms (20 points)."""
    s = Sprite(CW + 20, CH + 20, pivot=((CW + 20) / 2, (CH + 20) / 2))
    c = s.c
    c.translate((CW + 20) / 2, (CH + 20) / 2)
    up = -1 if frame else 1
    for d in (-1, 1):
        arm = smooth([(d * 12, 0), (d * 26, -10 * up), (d * 34, 4 + 6 * up), (d * 30, 14)], closed=False)
        c.drawPath(arm, paint(INK, stroke=9))
        c.drawPath(arm, paint((70, 210, 240), stroke=5))
        leg = smooth([(d * 8, 14), (d * 16, 24), (d * (20 if frame else 14), 30)], closed=False)
        c.drawPath(leg, paint(INK, stroke=7))
        c.drawPath(leg, paint((40, 150, 200), stroke=3.5))
    body = smooth([(-14, 14), (-18, -4), (-8, -20), (8, -20), (18, -4), (14, 14), (0, 18)], tension=0.4)
    outlined(c, body, (80, 210, 240), INK, 2.6, shader=lin(0, -20, 0, 18, [(180, 250, 255), (70, 200, 240), (20, 110, 170)]))
    c.drawPath(poly([(-7, -18), (-12, -30), (-4, -20)]), paint((80, 210, 240)))
    c.drawPath(poly([(7, -18), (12, -30), (4, -20)]), paint((80, 210, 240)))
    for x in (-7, 7):
        c.drawPath(ellipse(x, -6, 5.5, 7), paint((255, 240, 120)))
        c.drawPath(ellipse(x, -4, 2.5, 4), paint(INK))
    c.drawPath(poly([(-5, 8), (5, 8)], closed=False), paint(INK, stroke=2.4))
    atlas.add("IMG_MANTIS%d" % frame, s)


def jelly(frame):
    """Rows four and five: a green jellyfish with wavy tentacles (10 points)."""
    s = Sprite(CW + 20, CH + 20, pivot=((CW + 20) / 2, (CH + 20) / 2))
    c = s.c
    c.translate((CW + 20) / 2, (CH + 20) / 2)
    for i in range(5):
        x = -20 + i * 10
        w = 5 if (i + frame) % 2 else -5
        t = smooth([(x, 4), (x + w, 14), (x - w, 22), (x + w * 0.5, 30)], closed=False)
        c.drawPath(t, paint(INK, stroke=7))
        c.drawPath(t, paint((110, 230, 120), stroke=3.5))
    bell = smooth([(-28, 8), (-26, -10), (-12, -22), (12, -22), (26, -10), (28, 8), (14, 4), (0, 10), (-14, 4)], tension=0.4)
    outlined(c, bell, (100, 220, 110), INK, 2.6, shader=lin(0, -22, 0, 10, [(210, 255, 190), (100, 220, 110), (30, 130, 80)]))
    c.drawPath(ellipse(-10, -12, 7, 4), paint((255, 255, 255), alpha=0.6))
    eyes(c, [(-9, -4), (9, -4)], 5.5, 1 if frame else -1)
    atlas.add("IMG_JELLY%d" % frame, s)


def saucer(frame):
    W = 28 * 4.5
    s = Sprite(W + 20, 70, pivot=(10, 10))
    c = s.c
    c.translate(10, 10)
    c.drawPath(ellipse(W / 2, 18, 26, 18), paint(shader=rad(W / 2 - 6, 10, 30, [(230, 255, 255), (120, 220, 250), (40, 110, 170)])))
    body = smooth([(4, 32), (30, 20), (W - 30, 20), (W - 4, 32), (W - 30, 44), (30, 44)], tension=0.35)
    outlined(c, body, (255, 140, 60), INK, 2.6, shader=lin(0, 20, 0, 44, [(255, 210, 140), (255, 130, 60), (180, 60, 30)]))
    for i in range(5):
        on = (i + frame) % 2 == 0
        c.drawPath(circle(22 + i * 20, 33, 4.5), paint((255, 255, 160) if on else (120, 40, 30)))
    c.drawPath(ellipse(W / 2, 46, 20, 4), paint((255, 230, 140), alpha=0.6))
    atlas.add("IMG_SAUCER%d" % frame, s)


def cannon():
    W, H = 18 * 4.5, 10 * 4.5
    s = Sprite(W + 10, H + 10, pivot=(5, 5))
    c = s.c
    c.translate(5, 5)
    base = smooth([(2, H), (6, H * 0.45), (W * 0.3, H * 0.35), (W * 0.7, H * 0.35), (W - 6, H * 0.45), (W - 2, H)], tension=0.25)
    outlined(c, base, (90, 220, 110), INK, 2.6, shader=lin(0, H * 0.35, 0, H, [(190, 255, 170), (80, 210, 110), (30, 120, 70)]))
    barrel = rrect(W / 2 - 6, 0, 12, H * 0.5, 4)
    outlined(c, barrel, (200, 255, 200), INK, 2.4)
    c.drawPath(rrect(W / 2 - 3, 2, 6, 6, 2), paint((255, 255, 255)))
    c.drawPath(rrect(12, H * 0.62, W - 24, 6, 3), paint((40, 140, 80)))
    atlas.add("IMG_CANNON", s)


def main():
    for f in range(2):
        orb(f)
        mantis(f)
        jelly(f)
        saucer(f)
    cannon()
    atlas.write(os.path.join(ROOT, "assets", "alientide"), os.path.join(ROOT, "games", "AlienTide", "art.h"), "atart", "alientide")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "at_sheet.png"))


if __name__ == "__main__":
    main()
