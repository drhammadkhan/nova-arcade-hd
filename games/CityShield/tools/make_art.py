#!/usr/bin/env python3
"""City Shield HD art: six night-time city blocks with lit windows (and their ruins), the missile
bases (a bunker plus a separate launcher barrel that turns to aim), the interceptor, ammo icons, the
bomber, the satellite, the moon, and three full-width layers (the ground, a far skyline silhouette and
its windows). All original, drawn as vectors.
    python3 games/CityShield/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/cityshield/*.png and games/CityShield/art.h (namespace csart).
The game keeps the original's 320 x 240 units, drawn 4.5x on a centred 4:3 field (x offset 240).
The ground layer follows the original's ground profile, which is also written to art.h (GROUND_TOP),
since the warheads land on it."""
import os
import sys
import math
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, union, minus, paint, lin, rad  # noqa: E402

K = 4.5
OX = (1920 - 320 * K) / 2
INK = (12, 10, 26)
atlas = hd.Atlas(2048)

# the original's layout (units)
GROUND_Y, FLAT = 206, 222
BASE_X = [24, 160, 296]
CITY_X = [58, 90, 122, 182, 214, 246]
CITY_W, CITY_H = 28, 22
LAYER_TOP = 860            # screen row where the ground picture starts


def ground_top(u, exact=True):
    """The original's ground profile (rows below GROUND_Y) at field x = u. exact: the integer table the
    game logic uses; otherwise a smooth version for drawing."""
    if exact:
        y = FLAT - GROUND_Y + 1 + int(1.2 * math.sin(u * 0.11) + 0.8 * math.sin(u * 0.037 + 2))
    else:
        y = FLAT - GROUND_Y + 1 + 1.2 * math.sin(u * 0.11) + 0.8 * math.sin(u * 0.037 + 2) - 0.5
    for bx in BASE_X:
        d = abs(u - bx)
        if d < 30:
            m = 4 + (d / 30) ** 2 * 13 - (3 if d < 12 else 0) * (1 - d / 12)
            y = min(y, int(m) if exact else m)
    return max(2, y)


# ---------------------------------------------------------------- the cities
WIN_WARM = [(255, 214, 120), (255, 190, 90), (255, 236, 170)]
WIN_COOL = [(120, 220, 255), (170, 240, 255)]


def tower(c, rng, x, w, h, base_y, hue):
    """One building: a gradient facade, a moonlit left edge, a grid of windows and a roof detail."""
    top = base_y - h
    body_c = hd.mix((34, 34, 74), hue, 0.25)
    shape = rrect(x, top, w, h + 4, 1.5)
    style = rng.randrange(4)
    if style == 1 and w > 18:     # a stepped crown
        shape = union(rrect(x, top + 12, w, h - 8, 1.5), rrect(x + w * 0.2, top, w * 0.6, 14, 1.5))
    elif style == 2 and w > 16:   # a slanted roof
        shape = union(rrect(x, top + 10, w, h - 6, 1.5), poly([(x, top + 10.5), (x + w, top), (x + w, top + 11)]))
    outlined(c, shape, body_c, INK, 1.6, shader=lin(x, top, x, base_y, [hd.mix(body_c, (120, 120, 200), 0.35), body_c, hd.mix(body_c, (8, 8, 20), 0.5)]))
    cel(c, shape, rrect(x - 2, top - 20, 3.2, h + 30, 0), (180, 190, 255), 0.35)          # moonlit edge
    cel(c, shape, rrect(x + w - 4, top - 20, 6, h + 30, 0), (6, 6, 18), 0.35)             # shadow side
    # windows
    ww, wh, gx, gy = (3.0, 4.0, 6.0, 7.5) if w < 22 else (3.6, 4.4, 7.0, 8.0)
    cols = max(1, int((w - 5) // gx))
    x0 = x + (w - (cols - 1) * gx - ww) / 2
    lit_rate = rng.uniform(0.35, 0.7)
    cool = rng.random() < 0.3
    wy = top + (16 if style in (1, 2) else 7)
    while wy < base_y - 7:
        for i in range(cols):
            if rng.random() < lit_rate:
                colr = rng.choice(WIN_COOL if cool and rng.random() < 0.7 else WIN_WARM)
                c.drawPath(rrect(x0 + i * gx - 1.5, wy - 1.5, ww + 3, wh + 3, 2), paint(colr, alpha=0.25))
                c.drawPath(rrect(x0 + i * gx, wy, ww, wh, 0.8), paint(colr))
            else:
                c.drawPath(rrect(x0 + i * gx, wy, ww, wh, 0.8), paint((18, 18, 40)))
        wy += gy
    # roof furniture
    r = rng.random()
    if r < 0.4:      # antenna (its light blinks at run time)
        ax = x + w * rng.uniform(0.3, 0.7)
        c.drawPath(poly([(ax, top + 2), (ax, top - 16)], closed=False), paint((90, 90, 130), stroke=1.6))
        return (ax, top - 17)
    if r < 0.6 and w > 16:   # a water tank
        c.drawPath(rrect(x + 3, top - 7, 8, 7, 2), paint((60, 56, 90)))
        c.drawPath(poly([(x + 4, top), (x + 4, top - 2), (x + 10, top - 2), (x + 10, top)], closed=False), paint(INK, stroke=1))
    elif r < 0.8:   # a neon band
        colr = rng.choice([(255, 80, 170), (90, 230, 255), (255, 170, 60)])
        yy = top + h * rng.uniform(0.25, 0.45)
        c.drawPath(rrect(x + 2, yy - 2, w - 4, 6, 3), paint(colr, alpha=0.35))
        c.drawPath(rrect(x + 3, yy, w - 6, 2.4, 1.2), paint(hd.mix(colr, (255, 255, 255), 0.4)))
    return None


CITY_PX_W = 140
CITY_PX_H = 128


def city(i):
    rng = random.Random(100 + i * 7)
    s = Sprite(CITY_PX_W + 20, CITY_PX_H + 30, pivot=(CITY_PX_W / 2 + 10, CITY_PX_H + 20))
    c = s.c
    c.translate(10, 20)
    hue = [(90, 60, 160), (40, 80, 150), (110, 50, 120), (50, 70, 140), (80, 60, 170), (60, 90, 130)][i]
    base_y = CITY_PX_H
    # back row (shorter, darker) then the front row
    lights = []
    for row in range(2):
        x = rng.uniform(0, 8) if row == 0 else 0
        while x < CITY_PX_W - 10:
            w = rng.uniform(16, 30)
            if x + w > CITY_PX_W:
                w = CITY_PX_W - x
            if row == 0:
                h = rng.uniform(60, CITY_PX_H - 6)
                c.save()
                tower(c, rng, x, w, h, base_y - 14, hd.mix(hue, (10, 10, 30), 0.4))
                c.restore()
            else:
                h = rng.uniform(34, 92)
                if rng.random() < 0.18:
                    h = rng.uniform(98, CITY_PX_H)
                a = tower(c, rng, x, w, h, base_y, hue)
                if a:
                    lights.append(a)
            x += w + (rng.uniform(4, 14) if row == 0 else rng.uniform(-2, 3))
    # street level: a warm glow line and a few cars' lights
    c.drawPath(rrect(-2, base_y - 4, CITY_PX_W + 4, 6, 2), paint((26, 22, 50)))
    c.drawPath(rrect(0, base_y - 4, CITY_PX_W, 2, 1), paint((255, 200, 120), alpha=0.6))
    atlas.add("IMG_CITY%d" % i, s)
    return [(x + 10 - (CITY_PX_W / 2 + 10), y + 20 - (CITY_PX_H + 20)) for x, y in lights]


def ruin(i):
    rng = random.Random(300 + i)
    s = Sprite(CITY_PX_W + 20, 70, pivot=(CITY_PX_W / 2 + 10, 60))
    c = s.c
    c.translate(10, 10)
    base_y = 50
    pts = [(0, base_y)]
    x = 0
    while x < CITY_PX_W:
        h = rng.uniform(4, 34) if rng.random() < 0.6 else rng.uniform(2, 10)
        pts.append((x, base_y - h * rng.uniform(0.5, 1)))
        x += rng.uniform(5, 14)
        pts.append((min(x, CITY_PX_W), base_y - h))
    pts.append((CITY_PX_W, base_y))
    shape = poly(pts)
    outlined(c, shape, (50, 40, 54), INK, 1.6, shader=lin(0, base_y - 34, 0, base_y, [(80, 64, 76), (40, 30, 44), (20, 16, 24)]))
    # broken girders and glowing embers
    for _ in range(6):
        gx = rng.uniform(6, CITY_PX_W - 6)
        gy = base_y - rng.uniform(6, 24)
        a = rng.uniform(-1.2, -0.3) if rng.random() < 0.5 else rng.uniform(-2.8, -1.9)
        L = rng.uniform(8, 18)
        c.drawPath(poly([(gx, gy), (gx + math.cos(a) * L, gy + math.sin(a) * L)], closed=False), paint((70, 60, 70), stroke=2))
    for _ in range(14):
        ex, ey = rng.uniform(4, CITY_PX_W - 4), base_y - rng.uniform(1, 12)
        r = rng.uniform(1.2, 2.6)
        c.drawPath(circle(ex, ey, r * 2.2), paint((255, 110, 40), alpha=0.3))
        c.drawPath(circle(ex, ey, r), paint((255, 190, 90)))
    c.drawPath(rrect(-2, base_y - 4, CITY_PX_W + 4, 6, 2), paint((26, 22, 34)))
    atlas.add("IMG_RUIN%d" % i, s)


# ---------------------------------------------------------------- bases
BASE_W = 128


def base():
    s = Sprite(BASE_W + 20, 70, pivot=(BASE_W / 2 + 10, 60))
    c = s.c
    c.translate(10, 10)
    cx = BASE_W / 2
    # bunker: a low armoured dome on a plinth
    plinth = rrect(4, 34, BASE_W - 8, 16, 5)
    outlined(c, plinth, (70, 80, 110), INK, 2, shader=lin(0, 34, 0, 50, [(120, 130, 170), (60, 66, 96), (34, 36, 60)]))
    for k in range(5):
        c.drawPath(rrect(14 + k * 22, 40, 12, 4, 2), paint((255, 200, 90) if k % 2 == 0 else (90, 230, 255), alpha=0.9))
    dome = minus(ellipse(cx, 36, 40, 30), rrect(0, 36, BASE_W, 30, 0))
    outlined(c, dome, (90, 110, 150), INK, 2, shader=rad(cx - 12, 14, 46, [(200, 220, 255), (100, 120, 170), (40, 46, 80)]))
    c.drawPath(smooth([(cx - 30, 30), (cx - 18, 16), (cx, 10)], closed=False), paint((230, 240, 255), stroke=2.2, alpha=0.6))
    c.drawPath(rrect(cx - 12, 4, 24, 10, 4), paint((40, 46, 80)))     # the turret ring
    c.drawPath(rrect(cx - 12, 4, 24, 3, 2), paint((140, 160, 210)))
    atlas.add("IMG_BASE", s)

    s = Sprite(30, 60, pivot=(15, 54))   # the launcher barrel, pointing up, pivot at its foot
    c = s.c
    b = rrect(9, 6, 12, 48, 4)
    outlined(c, b, (150, 170, 210), INK, 2, shader=lin(9, 0, 21, 0, [(220, 230, 255), (130, 150, 200), (60, 70, 110)]))
    c.drawPath(rrect(8, 4, 14, 8, 3), paint((60, 70, 110)))
    c.drawPath(rrect(10, 20, 10, 3, 1), paint((90, 230, 255)))
    atlas.add("IMG_BARREL", s)

    s = Sprite(BASE_W + 20, 50, pivot=(BASE_W / 2 + 10, 40))
    c = s.c
    c.translate(10, 10)
    rng = random.Random(9)
    pts = [(2, 30)]
    for i in range(14):
        x = 4 + i * (BASE_W - 8) / 13
        pts.append((x, 30 - rng.uniform(2, 14)))
    pts.append((BASE_W - 2, 30))
    outlined(c, poly(pts), (50, 50, 66), INK, 1.8, shader=lin(0, 14, 0, 30, [(84, 80, 100), (36, 34, 50)]))
    for _ in range(5):
        x = rng.uniform(16, BASE_W - 16)
        c.drawPath(smooth([(x, 22), (x + rng.uniform(-10, 10), 10), (x + rng.uniform(-14, 14), 4)], closed=False), paint((90, 96, 120), stroke=2.4))
    for _ in range(8):
        c.drawPath(circle(rng.uniform(8, BASE_W - 8), rng.uniform(22, 29), 1.6), paint((255, 170, 70)))
    atlas.add("IMG_BASE_RUIN", s)


def missile():
    """The interceptor: pointing right, pivot at its centre."""
    s = Sprite(44, 18, pivot=(22, 9))
    c = s.c
    body = union(rrect(6, 6, 28, 6, 3), poly([(32, 6), (42, 9), (32, 12)]))
    outlined(c, body, (230, 240, 255), INK, 1.4, shader=lin(0, 6, 0, 12, [(255, 255, 255), (170, 200, 240)]))
    c.drawPath(poly([(6, 6), (2, 2), (12, 6)]), paint((90, 220, 255)))
    c.drawPath(poly([(6, 12), (2, 16), (12, 12)]), paint((90, 220, 255)))
    c.drawPath(rrect(26, 6.5, 3, 5, 1), paint((90, 220, 255)))
    atlas.add("IMG_MISSILE", s)
    # the ammo icon: a small upright missile, pivot at its foot
    s = Sprite(14, 34, pivot=(7, 32))
    c = s.c
    body = union(rrect(4, 8, 6, 22, 2.5), poly([(4, 9), (7, 1), (10, 9)]))
    outlined(c, body, (230, 240, 255), INK, 1.2, shader=lin(4, 0, 10, 0, [(255, 255, 255), (160, 190, 230)]))
    c.drawPath(poly([(4, 24), (1, 31), (5, 29)]), paint((90, 220, 255)))
    c.drawPath(poly([(10, 24), (13, 31), (9, 29)]), paint((90, 220, 255)))
    c.drawPath(rrect(4.5, 12, 5, 2.4, 1), paint((255, 90, 110)))
    atlas.add("IMG_AMMO", s)


# ---------------------------------------------------------------- flyers and the moon
def bomber():
    """Facing right, pivot at its centre."""
    W, H = 24 * K, 10 * K
    s = Sprite(W + 20, H + 20, pivot=(W / 2 + 10, H / 2 + 10))
    c = s.c
    c.translate(10, 10)
    wing = poly([(W * 0.62, H * 0.45), (W * 0.34, H * 0.05), (W * 0.22, H * 0.08), (W * 0.36, H * 0.5)])
    outlined(c, wing, (60, 66, 96), INK, 2, shader=lin(0, 0, 0, H * 0.5, [(110, 120, 160), (50, 54, 84)]))
    body = smooth([(2, H * 0.52), (W * 0.15, H * 0.36), (W * 0.7, H * 0.34), (W * 0.94, H * 0.48), (W * 0.7, H * 0.68), (W * 0.15, H * 0.66)], tension=0.35)
    outlined(c, body, (80, 90, 130), INK, 2, shader=lin(0, H * 0.34, 0, H * 0.68, [(170, 180, 220), (80, 90, 130), (40, 44, 72)]))
    tail = poly([(W * 0.04, H * 0.5), (W * 0.0, H * 0.12), (W * 0.12, H * 0.14), (W * 0.2, H * 0.45)])
    outlined(c, tail, (70, 76, 110), INK, 2)
    wing2 = poly([(W * 0.6, H * 0.6), (W * 0.36, H * 0.98), (W * 0.24, H * 0.95), (W * 0.38, H * 0.56)])
    outlined(c, wing2, (50, 54, 84), INK, 2, shader=lin(0, H * 0.6, 0, H, [(80, 86, 120), (36, 40, 64)]))
    c.drawPath(ellipse(W * 0.78, H * 0.45, 7, 3.4), paint((120, 230, 255)))
    c.drawPath(rrect(W * 0.3, H * 0.47, W * 0.36, 2.2, 1), paint((20, 20, 40)))
    atlas.add("IMG_BOMBER", s)


def satellite():
    W, H = 22 * K, 12 * K
    s = Sprite(W + 20, H + 20, pivot=(W / 2 + 10, H / 2 + 10))
    c = s.c
    c.translate(10, 10)
    cx, cy = W / 2, H / 2
    c.drawPath(poly([(10, cy), (W - 10, cy)], closed=False), paint((140, 140, 160), stroke=2.5))
    for x0 in (2, W - 34):
        panel = rrect(x0, cy - 13, 32, 26, 2)
        outlined(c, panel, (40, 70, 150), INK, 1.8, shader=lin(x0, cy - 13, x0 + 32, cy + 13, [(110, 160, 255), (40, 70, 160), (20, 30, 90)]))
        for k in range(1, 4):
            c.drawPath(poly([(x0 + k * 8, cy - 12), (x0 + k * 8, cy + 12)], closed=False), paint((20, 30, 70), stroke=1))
        c.drawPath(poly([(x0 + 1, cy), (x0 + 31, cy)], closed=False), paint((20, 30, 70), stroke=1))
    core = rrect(cx - 13, cy - 13, 26, 26, 5)
    outlined(c, core, (230, 180, 70), INK, 2, shader=lin(cx - 13, cy - 13, cx + 13, cy + 13, [(255, 236, 150), (220, 160, 60), (130, 80, 30)]))
    c.drawPath(poly([(cx, cy - 13), (cx, cy - 24)], closed=False), paint((200, 200, 220), stroke=1.8))
    c.drawPath(ellipse(cx, cy + 2, 6, 6), paint((60, 40, 30)))
    c.drawPath(ellipse(cx - 1.5, cy, 2.5, 2.5), paint((255, 90, 90)))
    atlas.add("IMG_SAT", s)


def moon():
    R = 62
    s = Sprite(R * 2 + 8, R * 2 + 8, pivot=(R + 4, R + 4))
    c = s.c
    c.translate(R + 4, R + 4)
    disc = circle(0, 0, R)
    c.drawPath(disc, paint(shader=rad(-R * 0.35, -R * 0.35, R * 1.5, [(255, 252, 236), (226, 222, 214), (150, 150, 176)])))
    rng = random.Random(3)
    for _ in range(11):
        x, y, r = rng.uniform(-R * 0.75, R * 0.75), rng.uniform(-R * 0.75, R * 0.75), rng.uniform(4, 13)
        if x * x + y * y > (R - r) ** 2:
            continue
        cel(c, disc, circle(x, y, r), (170, 166, 180), 0.55)
        cel(c, disc, circle(x - r * 0.25, y - r * 0.25, r * 0.75), (140, 136, 156), 0.45)
    # the terminator: a soft shadow on the lower right
    cel(c, disc, circle(R * 0.55, R * 0.45, R * 1.05), (60, 60, 100), 0.35)
    atlas.add("IMG_MOON", s)


# ---------------------------------------------------------------- full-width layers
def ground_layer():
    H = 1080 - LAYER_TOP
    s = Sprite(1920, H, density=1, trim=False)
    c = s.c
    pts = []
    for x in range(0, 1921, 4):
        u = (x - OX) / K
        y = (GROUND_Y + ground_top(u, exact=False)) * K - LAYER_TOP
        pts.append((x, y))
    path = poly(pts + [(1920, H), (0, H)])
    c.drawPath(path, paint(shader=lin(0, 60, 0, H, [(46, 46, 82), (26, 24, 48), (12, 10, 24)])))
    c.save()
    c.clipPath(path, skia_op(), True)
    rng = random.Random(11)
    # strata
    for k in range(10):
        off = 26 + k * 18
        q = [(x, y + off + 4 * math.sin(x * 0.01 + k)) for x, y in pts]
        c.drawPath(smooth(q[::6], closed=False), paint((70, 70, 120), stroke=2, alpha=0.18))
    # rocks
    for _ in range(140):
        x = rng.uniform(0, 1920)
        i = int(x / 4)
        y = pts[min(i, len(pts) - 1)][1] + rng.uniform(16, 130)
        r = rng.uniform(3, 9)
        c.drawPath(ellipse(x, y, r * 1.4, r), paint((60, 60, 100), alpha=0.6))
        c.drawPath(ellipse(x - r * 0.3, y - r * 0.4, r * 0.8, r * 0.4), paint((110, 110, 160), alpha=0.4))
    c.restore()
    # the lit rim (city light catching the crest)
    rim = smooth(pts[::2], closed=False)
    c.drawPath(rim, paint((150, 150, 220), stroke=4))
    c.drawPath(rim, paint((90, 230, 255), stroke=10, alpha=0.12))
    for _ in range(260):   # grass tufts
        x = rng.uniform(0, 1920)
        y = pts[min(int(x / 4), len(pts) - 1)][1]
        h = rng.uniform(4, 10)
        c.drawPath(poly([(x - 2, y + 1), (x + rng.uniform(-3, 3), y - h), (x + 2, y + 1)]), paint((80, 110, 140)))
    atlas.add_big("IMG_GROUND", s, "ground.png")


def skia_op():
    import skia
    return skia.ClipOp.kIntersect


SKY_H = 330


def skyline_layers():
    """The far city: a silhouette (white, tinted per wave) and its windows (white, drawn added)."""
    rng = random.Random(21)
    sil = Sprite(1920, SKY_H, density=1, trim=False)
    win = Sprite(1920, SKY_H, density=1, trim=False)
    cs, cw = sil.c, win.c
    blds = []
    x = -10
    while x < 1930:
        w = rng.uniform(26, 80)
        h = rng.uniform(40, 150) + (rng.uniform(40, 140) if rng.random() < 0.18 else 0)
        blds.append((x, w, h))
        x += w + rng.uniform(-8, 6)
    for x, w, h in blds:
        top = SKY_H - h
        cs.drawPath(rrect(x, top, w, h + 4, 1), paint((255, 255, 255)))
        if rng.random() < 0.25:
            cs.drawPath(poly([(x + w / 2, top - rng.uniform(16, 50)), (x + w / 2, top)], closed=False), paint((255, 255, 255), stroke=2.5))
        if rng.random() < 0.2:
            cs.drawPath(poly([(x, top), (x + w / 2, top - w * 0.4), (x + w, top)]), paint((255, 255, 255)))
        lit = rng.uniform(0.08, 0.3)
        for wy in range(int(top + 8), SKY_H - 6, 9):
            for wx in range(int(x + 5), int(x + w - 6), 8):
                if rng.random() < lit:
                    b = rng.uniform(0.5, 1.0)
                    cw.drawPath(rrect(wx, wy, 3.5, 4.5, 0.6), paint((255, 255, 255), alpha=b))
    atlas.add_big("IMG_SKYLINE", sil, "skyline.png")
    atlas.add_big("IMG_SKYLINE_WIN", win, "skyline_win.png")


def write_extra(header, lights):
    tops = [ground_top(x) for x in range(320)]
    lines = open(header).read().rstrip().split("\n")
    end = lines.pop()   # "}  // namespace csart"
    lines.append("// the original's ground profile: rows below GROUND_Y where the ground starts, per field column")
    lines.append("static const uint8_t GROUND_TOP[320] = {%s};" % ",".join(map(str, tops)))
    lines.append("static const float LAYER_TOP = %d;   // screen row of IMG_GROUND's top edge" % LAYER_TOP)
    lines.append("// antenna-light positions per city, relative to the city sprite's pivot (screen pixels)")
    n = max(1, max(len(l) for l in lights))
    lines.append("static const int NLIGHTS = %d;" % n)
    rows = []
    for l in lights:
        l = l + [(-999, -999)] * (n - len(l))
        rows.append("{%s}" % ", ".join("{%.1ff, %.1ff}" % p for p in l))
    lines.append("static const float CITY_LIGHTS[6][%d][2] = {%s};" % (n, ", ".join(rows)))
    lines.append(end)
    open(header, "w").write("\n".join(lines) + "\n")


def main():
    lights = [city(i) for i in range(6)]
    for i in range(6):
        ruin(i)
    atlas.table("CITIES", ["IMG_CITY%d" % i for i in range(6)])
    atlas.table("RUINS", ["IMG_RUIN%d" % i for i in range(6)])
    base()
    missile()
    bomber()
    satellite()
    moon()
    ground_layer()
    skyline_layers()
    header = os.path.join(ROOT, "games", "CityShield", "art.h")
    atlas.write(os.path.join(ROOT, "assets", "cityshield"), header, "csart", "cityshield")
    write_extra(header, lights)
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "cs_sheet.png"))


if __name__ == "__main__":
    main()
