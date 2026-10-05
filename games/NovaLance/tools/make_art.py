#!/usr/bin/env python3
"""Nova Lance HD art: the fighter, the enemy craft, the boss mothership and the ringed planet, drawn as
vector shapes. All original.
    python3 games/NovaLance/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/novalance/*.png and games/NovaLance/art.h (namespace nlart).
Sizes are the original sprites' sizes times 4.5; every craft faces left except the player."""
import os
import sys
import math

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, union, intersect, paint, lin, rad  # noqa: E402

K = 4.5
INK = (24, 16, 44)
atlas = hd.Atlas(2048)


def player():
    W, H = 32 * K, 16 * K
    s = Sprite(W, H, pivot=(0, 0), trim=False)
    c = s.c
    # rear fins
    fin_t = poly([(20, 22), (44, 6), (60, 24)])
    fin_b = poly([(20, 50), (44, 66), (60, 48)])
    for f in (fin_t, fin_b):
        outlined(c, f, (60, 70, 120), INK, 2.6, shader=lin(0, 0, 60, 0, [(90, 100, 160), (50, 50, 100)]))
    hull = smooth([(8, 30), (30, 22), (80, 20), (128, 30), (142, 36), (128, 42), (80, 52), (30, 50), (8, 42)], tension=0.35)
    outlined(c, hull, (220, 230, 245), INK, 2.8, shader=lin(0, 18, 0, 54, [(255, 255, 255), (200, 210, 230), (110, 120, 160)]))
    cel(c, hull, rrect(10, 40, 130, 14, 6), (120, 130, 175), 0.8)
    c.drawPath(poly([(30, 36), (126, 36)], closed=False), paint((70, 210, 250), stroke=3))
    canopy = smooth([(70, 26), (96, 24), (112, 32), (96, 36), (72, 34)], tension=0.4)
    outlined(c, canopy, (230, 80, 200), INK, 2, shader=lin(70, 24, 110, 36, [(255, 170, 240), (200, 50, 170), (90, 20, 100)]))
    c.drawPath(ellipse(86, 28, 8, 2.2), paint((255, 255, 255), alpha=0.8))
    # wing pod cannon
    pod = rrect(48, 48, 46, 10, 5)
    outlined(c, pod, (90, 100, 150), INK, 2.2)
    c.drawPath(rrect(88, 50, 10, 6, 2), paint((255, 200, 90)))
    # engine nozzle glow at the back
    c.drawPath(rrect(2, 30, 12, 12, 4), paint((60, 220, 255)))
    c.drawPath(rrect(4, 33, 6, 6, 3), paint((220, 250, 255)))
    atlas.add("IMG_PLAYER", s)


def drone():
    W = 18 * K
    for frame in range(2):
        s = Sprite(W, W, pivot=(W / 2, W / 2), trim=False)
        c = s.c
        c.translate(W / 2, W / 2)
        for i in range(6):   # spikes
            a = math.radians(i * 60 + frame * 30)
            spike = poly([(math.cos(a - 0.25) * 26, math.sin(a - 0.25) * 26), (math.cos(a) * 39, math.sin(a) * 39), (math.cos(a + 0.25) * 26, math.sin(a + 0.25) * 26)])
            outlined(c, spike, (120, 120, 160), INK, 2)
        body = circle(0, 0, 28)
        outlined(c, body, (110, 60, 160), INK, 2.6, shader=rad(-8, -10, 40, [(200, 150, 255), (120, 60, 180), (50, 20, 80)]))
        c.drawPath(circle(0, 0, 18), paint((30, 14, 50)))
        eye = circle(0, 0, 11)
        c.drawPath(eye, paint(shader=rad(0, 0, 12, [(255, 255, 220), (255, 90, 120), (160, 20, 60)])))
        c.drawPath(circle(-3, -3, 3), paint((255, 255, 255)))
        c.drawPath(ellipse(-10, -14, 8, 4), paint((255, 255, 255), alpha=0.5))
        atlas.add("IMG_DRONE%d" % frame, s)


def dart():
    W, H = 22 * K, 13 * K
    s = Sprite(W, H, pivot=(0, 0), trim=False)
    c = s.c
    body = poly([(4, H / 2), (40, 14), (W - 6, 20), (W - 10, H / 2), (W - 6, H - 20), (40, H - 14)])
    outlined(c, body, (240, 70, 80), INK, 2.6, shader=lin(0, 0, 0, H, [(255, 150, 140), (230, 60, 70), (120, 20, 40)]))
    for y, d in ((6, -1), (H - 6, 1)):
        wing = poly([(46, H / 2 + d * 6), (W - 4, y), (W - 22, H / 2 + d * 6)])
        outlined(c, wing, (180, 40, 60), INK, 2.2)
    c.drawPath(ellipse(36, H / 2, 10, 4.5), paint((255, 230, 120)))
    c.drawPath(rrect(W - 12, H / 2 - 5, 10, 10, 3), paint((255, 160, 60)))
    atlas.add("IMG_DART", s)


def pod():
    W = 22 * K
    for frame in range(2):
        s = Sprite(W, W, pivot=(0, 0), trim=False)
        c = s.c
        body = rrect(6, 6, W - 12, W - 12, 26)
        outlined(c, body, (90, 110, 130), INK, 2.8, shader=lin(0, 0, W, W, [(170, 190, 210), (90, 110, 130), (40, 50, 70)]))
        for k in range(3):   # armour bands
            c.drawPath(poly([(14 + k * 26, 8), (14 + k * 26, W - 8)], closed=False), paint((60, 70, 90), stroke=3))
        hatch = ellipse(24, W / 2, 16, 20 if frame else 8)
        c.drawPath(hatch, paint((30, 20, 40)))
        if frame:
            c.drawPath(circle(24, W / 2, 11), paint(shader=rad(24, W / 2, 12, [(255, 255, 220), (255, 160, 60), (200, 60, 30)])))
        c.drawPath(ellipse(50, 20, 22, 6), paint((255, 255, 255), alpha=0.35))
        atlas.add("IMG_POD%d" % frame, s)


def boss():
    W, H = 84 * K, 62 * K
    s = Sprite(W, H, pivot=(0, 0), trim=False)
    c = s.c
    # upper and lower prongs
    for d in (-1, 1):
        cy = H / 2 + d * 92
        prong = smooth([(30, cy - 26), (200, cy - 34 * (1 if d < 0 else 0.6)), (W - 10, cy - 14), (W - 10, cy + 14), (200, cy + 34 * (0.6 if d < 0 else 1)), (40, cy + 26)], tension=0.25)
        outlined(c, prong, (80, 70, 120), INK, 3, shader=lin(0, cy - 34, 0, cy + 34, [(150, 140, 200), (80, 70, 120), (40, 30, 70)]))
        c.drawPath(rrect(60, cy - 6, 200, 12, 6), paint((50, 40, 80)))
        for k in range(5):
            c.drawPath(rrect(70 + k * 40, cy - 3, 22, 6, 3), paint((255, 120, 80) if k % 2 else (90, 220, 255)))
    hull = smooth([(20, H / 2 - 60), (140, H / 2 - 90), (300, H / 2 - 70), (W - 4, H / 2 - 30), (W - 4, H / 2 + 30), (300, H / 2 + 70), (140, H / 2 + 90), (20, H / 2 + 60), (6, H / 2)], tension=0.3)
    outlined(c, hull, (110, 100, 160), INK, 3.2, shader=lin(0, 0, 0, H, [(190, 180, 230), (110, 100, 160), (50, 40, 90)]))
    cel(c, hull, rrect(0, H / 2 + 30, W, 80, 10), (60, 50, 100), 0.6)
    for k in range(4):   # plating lines
        x = 120 + k * 56
        c.drawPath(smooth([(x, H / 2 - 70 + k * 6), (x + 20, H / 2), (x, H / 2 + 70 - k * 6)], closed=False), paint((70, 60, 110), stroke=3))
    # the core (the boss's weak point sits at its front-left, where it fires from)
    core = circle(60, H / 2 + 4, 40)
    outlined(c, core, (255, 80, 120), INK, 3, shader=rad(52, H / 2 - 4, 46, [(255, 240, 220), (255, 90, 140), (150, 20, 80)]))
    c.drawPath(circle(48, H / 2 - 8, 10), paint((255, 255, 255), alpha=0.8))
    c.drawPath(rrect(130, H / 2 - 14, 150, 28, 14), paint((40, 30, 70)))
    c.drawPath(rrect(140, H / 2 - 6, 130, 12, 6), paint(shader=lin(140, 0, 270, 0, [(90, 220, 255), (200, 120, 255)])))
    atlas.add("IMG_BOSS", s)


def planet():
    W, H = 84 * K, 60 * K
    s = Sprite(W, H, pivot=(0, 0), trim=False)
    c = s.c
    cx, cy, r = W / 2, H / 2, 100
    ring_back = intersect(ellipse(cx, cy, 180, 40), poly([(0, 0), (W, 0), (W, cy), (0, cy)]))
    c.drawPath(ring_back, paint((200, 150, 220), stroke=10, alpha=0.6))
    ball = circle(cx, cy, r)
    c.drawPath(ball, paint(shader=rad(cx - 40, cy - 40, 150, [(255, 190, 170), (210, 90, 140), (80, 30, 90)])))
    c.save()
    c.clipPath(ball, hd.skia.ClipOp.kIntersect, True)
    for k in range(5):
        c.drawPath(ellipse(cx, cy - 60 + k * 30, 140, 8), paint((255, 230, 220), alpha=0.18))
    c.drawPath(circle(cx + 40, cy + 30, 110), paint((40, 10, 60), alpha=0.35))
    c.restore()
    ring_front = intersect(ellipse(cx, cy, 180, 40), poly([(0, cy), (W, cy), (W, H), (0, H)]))
    c.drawPath(ring_front, paint((240, 200, 255), stroke=10, alpha=0.9))
    c.drawPath(ellipse(cx, cy, 160, 34), paint((180, 120, 210), stroke=4, alpha=0.6))
    atlas.add("IMG_PLANET", s)


def main():
    player()
    drone()
    dart()
    pod()
    boss()
    planet()
    atlas.write(os.path.join(ROOT, "assets", "novalance"), os.path.join(ROOT, "games", "NovaLance", "art.h"), "nlart", "novalance")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "nl_sheet.png"))


if __name__ == "__main__":
    main()
