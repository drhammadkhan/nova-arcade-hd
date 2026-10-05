#!/usr/bin/env python3
"""Hop Rush HD art, top-down: the hop-bot, cars, a racer, a truck, a dozer, logs and lily pads.
All original.
    python3 games/HopRush/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/hoprush/*.png and games/HopRush/art.h (namespace hrart).
One tile is 72 px (the original's 16 x 4.5). Vehicles face right (the game flips them); car and racer
bodies are drawn white so the game can tint them, with their glass and lights on a separate layer."""
import os
import sys
import math

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, union, paint, lin, rad  # noqa: E402

T = 72
INK = (20, 14, 34)
atlas = hd.Atlas(2048)


def wheels(c, xs, w, h, y0, y1):
    for x in xs:
        for y in (y0, y1):
            c.drawPath(rrect(x, y, w, h, 4), paint((24, 20, 34)))


def car(name, length, racer=False):
    W = length * T
    s = Sprite(W, T, pivot=(0, 0), trim=False)
    c = s.c
    wheels(c, [10, W - 30], 20, 10, 10, T - 20)
    body = rrect(4, 14, W - 8, T - 28, 16)
    outlined(c, body, (255, 255, 255), INK, 2.6, shader=lin(0, 14, 0, T - 14, [(255, 255, 255), (220, 220, 230), (170, 170, 185)]))
    if racer:
        c.drawPath(poly([(W - 4, T / 2 - 6), (W + 2, T / 2), (W - 4, T / 2 + 6)]), paint((255, 255, 255)))
    atlas.add("IMG_%s_BODY" % name, s)
    s = Sprite(W, T, pivot=(0, 0), trim=False)   # glass, lights and stripes, not tinted
    c = s.c
    c.drawPath(rrect(W * 0.52, 20, W * 0.2, T - 40, 8), paint(shader=lin(0, 20, 0, T - 20, [(220, 250, 255), (110, 190, 240)])))
    c.drawPath(rrect(W * 0.16, 22, W * 0.14, T - 44, 6), paint((110, 170, 220)))
    c.drawPath(rrect(W - 12, 18, 6, 9, 3), paint((255, 255, 210)))
    c.drawPath(rrect(W - 12, T - 27, 6, 9, 3), paint((255, 255, 210)))
    c.drawPath(rrect(6, 18, 5, 9, 2), paint((255, 70, 70)))
    c.drawPath(rrect(6, T - 27, 5, 9, 2), paint((255, 70, 70)))
    if racer:
        c.drawPath(rrect(6, T / 2 - 4, W - 14, 8, 4), paint((255, 255, 255), alpha=0.9))
    else:
        c.drawPath(rrect(10, 16, W - 24, 5, 2), paint((255, 255, 255), alpha=0.5))
    atlas.add("IMG_%s_TOP" % name, s)


def truck():
    W = 3 * T
    s = Sprite(W, T, pivot=(0, 0), trim=False)
    c = s.c
    wheels(c, [14, 60, 110, W - 44], 22, 10, 8, T - 18)
    box = rrect(4, 10, W - 66, T - 20, 8)
    outlined(c, box, (200, 210, 230), INK, 2.6, shader=lin(0, 10, 0, T - 10, [(240, 245, 255), (190, 200, 225), (130, 140, 170)]))
    for k in range(1, 4):
        c.drawPath(poly([(4 + k * (W - 66) / 4, 12), (4 + k * (W - 66) / 4, T - 12)], closed=False), paint((140, 150, 180), stroke=2.4))
    cab = rrect(W - 60, 14, 56, T - 28, 12)
    outlined(c, cab, (240, 80, 90), INK, 2.6, shader=lin(0, 14, 0, T - 14, [(255, 150, 150), (230, 70, 80), (150, 30, 50)]))
    c.drawPath(rrect(W - 30, 20, 16, T - 40, 6), paint((180, 230, 255)))
    c.drawPath(rrect(W - 10, 18, 5, 9, 2), paint((255, 255, 210)))
    c.drawPath(rrect(W - 10, T - 27, 5, 9, 2), paint((255, 255, 210)))
    atlas.add("IMG_TRUCK", s)


def dozer():
    W = 2 * T
    s = Sprite(W, T, pivot=(0, 0), trim=False)
    c = s.c
    for y in (4, T - 18):   # caterpillar tracks
        tr = rrect(10, y, W - 34, 14, 7)
        outlined(c, tr, (40, 40, 56), INK, 2)
        for k in range(8):
            c.drawPath(poly([(16 + k * 12, y + 2), (16 + k * 12, y + 12)], closed=False), paint((90, 90, 110), stroke=2))
    body = rrect(18, 14, W - 52, T - 28, 8)
    outlined(c, body, (255, 210, 60), INK, 2.6, shader=lin(0, 14, 0, T - 14, [(255, 240, 150), (255, 200, 50), (200, 130, 20)]))
    c.drawPath(rrect(30, 22, 30, T - 44, 6), paint((60, 70, 100)))
    blade = rrect(W - 26, 2, 16, T - 4, 6)
    outlined(c, blade, (180, 190, 210), INK, 2.6, shader=lin(W - 26, 0, W - 10, 0, [(230, 235, 245), (130, 140, 160)]))
    for k in range(2):
        c.drawPath(poly([(18, 20 + k * 30), (4, 20 + k * 30)], closed=False), paint((255, 150, 40), stroke=4))
    atlas.add("IMG_DOZER", s)


def log():
    """A log is drawn as a left end, middle pieces and a right end, each one tile."""
    for part in ("L", "M", "R"):
        s = Sprite(T + 8, T, pivot=(4, 0), trim=False)
        c = s.c
        c.translate(4, 0)
        x0 = 6 if part == "L" else -4
        x1 = T - 6 if part == "R" else T + 4
        bark = rrect(x0, 8, x1 - x0, T - 16, 22 if part != "M" else 0)
        if part == "L":
            r = hd.skia.RRect()
            r.setRectRadii(hd.skia.Rect.MakeLTRB(x0, 8, x1, T - 8), [hd.skia.Point(22, 22), hd.skia.Point(0, 0), hd.skia.Point(0, 0), hd.skia.Point(22, 22)])
            bark = hd.skia.Path(); bark.addRRect(r)
        if part == "R":
            r = hd.skia.RRect()
            r.setRectRadii(hd.skia.Rect.MakeLTRB(x0, 8, x1, T - 8), [hd.skia.Point(0, 0), hd.skia.Point(22, 22), hd.skia.Point(22, 22), hd.skia.Point(0, 0)])
            bark = hd.skia.Path(); bark.addRRect(r)
        c.save()
        c.clipRect(hd.skia.Rect.MakeLTRB(0 if part != "L" else -10, 0, T if part != "R" else T + 10, T))
        c.drawPath(bark, paint(INK, stroke=5.2))
        c.drawPath(bark, paint(shader=lin(0, 8, 0, T - 8, [(190, 130, 90), (140, 90, 64), (90, 54, 40)])))
        c.clipPath(bark, hd.skia.ClipOp.kIntersect, True)
        for k in range(3):
            c.drawPath(smooth([(-4, 20 + k * 12), (T * 0.3, 18 + k * 12 + (k % 2) * 3), (T * 0.7, 21 + k * 12), (T + 4, 19 + k * 12)], closed=False),
                       paint((100, 64, 46), stroke=2.4))
        c.restore()
        if part == "R":
            c.drawPath(ellipse(T - 12, T / 2, 10, T / 2 - 10), paint((230, 190, 140)))
            for r in (6, 3):
                c.drawPath(ellipse(T - 12, T / 2, r * 0.9, r * 2.2), paint((170, 120, 80), stroke=1.6))
        atlas.add("IMG_LOG_" + part, s)


def pad():
    s = Sprite(T, T, pivot=(T / 2, T / 2))
    c = s.c
    c.translate(T / 2, T / 2)
    leaf = hd.minus(circle(0, 0, 30), poly([(0, 0), (40, -12), (40, 12)]))
    outlined(c, leaf, (80, 210, 100), INK, 2.4, shader=rad(-8, -8, 34, [(170, 250, 150), (80, 200, 100), (30, 120, 70)]))
    for a in (-120, -60, 0, 60, 120, 180):
        r = math.radians(a)
        c.drawPath(poly([(0, 0), (math.cos(r) * 26, math.sin(r) * 26)], closed=False), paint((50, 150, 80), stroke=1.6))
    c.drawPath(circle(-12, 10, 6), paint((255, 170, 210)))
    c.drawPath(circle(-12, 10, 2.5), paint((255, 240, 120)))
    atlas.add("IMG_PAD", s)


def bot(name, squash=False, hop=False):
    s = Sprite(T + 20, T + 20, pivot=(10 + T / 2, 10 + T / 2))
    c = s.c
    c.translate(10 + T / 2, 10 + T / 2)
    if squash:
        flat = ellipse(0, 10, 32, 12)
        outlined(c, flat, (70, 210, 240), INK, 2.6)
        for x in (-12, 12):
            c.drawPath(poly([(x - 5, 6), (x + 5, 14)], closed=False), paint(INK, stroke=3))
            c.drawPath(poly([(x - 5, 14), (x + 5, 6)], closed=False), paint(INK, stroke=3))
        atlas.add("IMG_" + name, s)
        return
    k = 1.08 if hop else 1.0
    for x in (-18, 18):   # feet
        foot = ellipse(x, 24 if not hop else 28, 10, 7)
        outlined(c, foot, (40, 130, 170), INK, 2.2)
    body = rrect(-24 * k, -22 * k, 48 * k, 46 * k, 18)
    outlined(c, body, (70, 210, 240), INK, 2.8, shader=lin(0, -22, 0, 24, [(190, 250, 255), (70, 200, 240), (30, 120, 170)]))
    c.drawPath(rrect(-16, -18, 32, 9, 5), paint((255, 255, 255), alpha=0.5))
    c.drawPath(poly([(0, -22), (0, -34)], closed=False), paint(INK, stroke=3))
    c.drawPath(circle(0, -36, 6), paint((255, 120, 210)))
    c.drawPath(circle(-2, -38, 2), paint((255, 255, 255)))
    for x in (-10, 10):
        c.drawPath(circle(x, -2, 8), paint((255, 255, 255)))
        c.drawPath(circle(x, -4 if hop else -1, 4), paint(INK))
    c.drawPath(rrect(-8, 10, 16, 4, 2), paint((40, 130, 170)))
    atlas.add("IMG_" + name, s)


def gem():
    s = Sprite(60, 60, pivot=(30, 30))
    c = s.c
    c.translate(30, 30)
    g = poly([(0, -24), (20, -6), (0, 24), (-20, -6)])
    outlined(c, g, (255, 120, 210), INK, 2.4, shader=lin(-20, -24, 20, 24, [(255, 220, 250), (255, 110, 210), (170, 40, 150)]))
    c.drawPath(poly([(-20, -6), (20, -6)], closed=False), paint((255, 200, 240), stroke=2))
    c.drawPath(poly([(0, -24), (-6, -6), (0, 24)], closed=False), paint((255, 200, 240), stroke=1.6))
    atlas.add("IMG_GEM", s)


def main():
    car("CAR", 1)
    car("RACER", 1, racer=True)
    truck()
    dozer()
    log()
    pad()
    bot("BOT")
    bot("BOT_HOP", hop=True)
    bot("BOT_FLAT", squash=True)
    gem()
    atlas.write(os.path.join(ROOT, "assets", "hoprush"), os.path.join(ROOT, "games", "HopRush", "art.h"), "hrart", "hoprush")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "hr_sheet.png"))


if __name__ == "__main__":
    main()
