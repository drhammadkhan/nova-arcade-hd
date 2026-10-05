#!/usr/bin/env python3
"""Turbo Horizon HD art: the player's sports car seen from behind (straight and two steering frames),
four rival cars, roadside scenery for the three stages (palms, rocks, mesas, cacti, billboards, street
lamps, towers, chevron signs, bushes, the checkpoint gantry) and the parallax skylines. All original.
    python3 games/TurboHorizon/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/turbohorizon/*.png and games/TurboHorizon/art.h (namespace thart).

Sizes follow the original sprites times 4.5 (an original 72 x 136 palm is drawn 324 x 612), pivot at the
bottom centre; the game scales them with distance. Scenery is lit from the left. Scenery and rivals are
stored at 1x (they are rarely seen close), the player's car at 2x."""
import os
import sys
import math
import random
import skia

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, union, paint, lin, rad, capsule  # noqa: E402

F = 4.5
INK = (16, 12, 28)
atlas = hd.Atlas(2048)
_tf = skia.Typeface.MakeFromFile(os.path.join(ROOT, "tools/font/Fredoka.ttf"))


def font(size, weight=700):
    C = skia.FontArguments.VariationPosition.Coordinate
    fa = skia.FontArguments()
    fa.setVariationDesignPosition(skia.FontArguments.VariationPosition(skia.FontArguments.VariationPosition.Coordinates([C(0x77676874, float(weight))])))
    return skia.Font(_tf.makeClone(fa), size)


def word(c, s, cx, cy, size, fill, ink=INK, stroke=6):
    f = font(size)
    w = f.measureText(s)
    blob = skia.TextBlob.MakeFromString(s, f)
    y = cy + size * 0.36
    c.drawTextBlob(blob, cx - w / 2, y, paint(ink, stroke=stroke))
    c.drawTextBlob(blob, cx - w / 2, y, paint(fill))


def sprite(ow, oh, density=1, extra=0):
    """A sprite the size of an original ow x oh sprite, times 4.5; pivot at the bottom centre."""
    w, h = ow * F, oh * F
    s = Sprite(w + extra * 2, h + extra, pivot=(w / 2 + extra, h + extra), density=density)
    s.c.translate(extra, extra)
    return s, w, h


# ---------------------------------------------------------------- cars (rear views)
def tyre(c, x, y, w, h):
    c.drawPath(rrect(x, y, w, h, w * 0.25), paint((22, 20, 28)))
    for k in range(4):
        c.drawPath(rrect(x + w * 0.15, y + h * (0.12 + k * 0.22), w * 0.7, h * 0.07, 2), paint((44, 42, 52)))


def car(name, ow, oh, body, style, yaw=0, density=1):
    """body: (light, mid, dark). yaw 0..2 shows the right flank (the game mirrors for left turns)."""
    s, W, H = sprite(ow, oh, density, extra=4)
    c = s.c
    light, mid, dark = body
    sh = yaw * W * 0.035                     # how far the upper body swings over
    side = yaw * W * 0.045                   # visible right flank
    # shadow
    c.drawPath(ellipse(W / 2, H - 4, W * 0.52, H * 0.07), paint((0, 0, 0), alpha=0.35, blur=6))
    tw = W / 7.0
    ty0 = H * (0.6 if style != "van" else 0.66)
    tyre(c, 2, ty0, tw, H - ty0 - 1)
    tyre(c, W - 2 - tw - side * 0.3, ty0, tw, H - ty0 - 1)
    if style == "van":
        bt = H * 0.04
    elif style == "pickup":
        bt = H * 0.36
    else:
        bt = H * 0.40
    bb = H * 0.88
    c.drawPath(rrect(W * 0.12, bb - 10, W * 0.76, H * 0.07 + 10, 6), paint((26, 24, 34)))   # underside
    # lower body (trunk and bumper)
    lower = smooth([(4, bb), (2, bt + H * 0.18), (W * 0.08, bt + H * 0.02), (W * 0.92, bt + H * 0.02),
                    (W - 2 + side * 0.4, bt + H * 0.18), (W - 4 + side * 0.4, bb)], tension=0.25)
    if style == "van":
        lower = rrect(3, bt, W - 6 + side * 0.4, bb - bt, W * 0.08)
    outlined(c, lower, mid, INK, 2.4, shader=lin(0, bt, 0, bb, [light, mid, dark]))
    if side > 0:   # the right flank, in shadow
        cel(c, lower, smooth([(W - side * 1.3, bt + H * 0.06), (W + 10, bt), (W + 10, bb), (W - side * 0.9, bb)], tension=0.1), hd.shade(dark, -0.3), 0.8)
    # cabin (not on a van: its whole back is the body)
    if style != "van":
        cb = bt + H * 0.04
        if style == "pickup":
            ct, cl, cr = H * 0.02, W * 0.22, W * 0.78
        elif style == "sport":
            ct, cl, cr = H * 0.10, W * 0.20, W * 0.80
        else:
            ct, cl, cr = H * 0.04, W * 0.17, W * 0.83
        cab = smooth([(W * 0.08 + sh, cb), (cl + sh, ct + H * 0.05), (cl + W * 0.06 + sh, ct), (cr - W * 0.06 + sh, ct),
                      (cr + sh, ct + H * 0.05), (W * 0.92 + sh + side * 0.3, cb)], tension=0.2)
        outlined(c, cab, mid, INK, 2.4, shader=lin(0, ct, 0, cb, [light, mid]))
        glass = smooth([(W * 0.15 + sh, cb - H * 0.03), (cl + W * 0.03 + sh, ct + H * 0.07), (cr - W * 0.03 + sh, ct + H * 0.07),
                        (W * 0.85 + sh, cb - H * 0.03)], tension=0.15)
        c.drawPath(glass, paint(shader=lin(0, ct, 0, cb, [(150, 190, 230), (40, 60, 100), (16, 22, 44)])))
        c.drawPath(poly([(cl + W * 0.12 + sh, ct + H * 0.08), (cl + W * 0.2 + sh, ct + H * 0.08), (cl + W * 0.08 + sh, cb - H * 0.04),
                         (cl + sh, cb - H * 0.04)]), paint((255, 255, 255), alpha=0.18))
    else:
        for x0, x1 in ((W * 0.1, W * 0.48), (W * 0.52, W * 0.9)):   # twin rear windows
            c.drawPath(rrect(x0 + sh * 0.4, H * 0.10, x1 - x0, H * 0.26, 8), paint(shader=lin(0, H * 0.1, 0, H * 0.36, [(150, 190, 230), (30, 50, 90)])))
        c.drawPath(rrect(W * 0.495, H * 0.06, W * 0.01, H * 0.62, 1), paint(INK, alpha=0.6))
        c.drawPath(rrect(W * 0.4, H * 0.45, W * 0.06, H * 0.03, 2), paint((60, 60, 70)))
    if style == "pickup":   # tailgate panel lines
        c.drawPath(rrect(W * 0.1, bt + H * 0.1, W * 0.8, H * 0.05, 3), paint(dark))
    # shoulder highlight
    c.drawPath(rrect(W * 0.1, bt + H * 0.045, W * 0.8, H * 0.03, 3), paint(hd.shade(light, 0.5), alpha=0.8))
    # tail lights
    ly = bt + H * (0.13 if style != "van" else 0.42)
    lh = H * (0.09 if style != "van" else 0.12)
    lw = W * (0.24 if style == "sport" else 0.16)
    for x0 in (W * 0.06, W * 0.94 - lw + side * 0.3):
        c.drawPath(rrect(x0, ly, lw, lh, lh * 0.4), paint(INK))
        c.drawPath(rrect(x0 + 2, ly + 2, lw - 4, lh - 4, lh * 0.35), paint(shader=lin(0, ly, 0, ly + lh, [(255, 190, 170), (240, 40, 50), (150, 10, 30)])))
        c.drawPath(rrect(x0 + lw * 0.15, ly + lh * 0.2, lw * 0.4, lh * 0.18, 2), paint((255, 230, 220), alpha=0.8))
    if style == "sport":   # light bar between the tail lights
        c.drawPath(rrect(W * 0.3, ly + lh * 0.3, W * 0.4 + side * 0.2, lh * 0.4, 3), paint((140, 16, 30)))
    # number plate
    px, pw, ph = W * 0.5 - W * 0.09 + side * 0.2, W * 0.18, H * 0.1
    py = bb - H * 0.21
    c.drawPath(rrect(px, py, pw, ph, 3), paint(INK))
    c.drawPath(rrect(px + 2, py + 2, pw - 4, ph - 4, 2), paint((245, 240, 220)))
    for k in range(4):
        c.drawPath(rrect(px + pw * (0.14 + k * 0.19), py + ph * 0.3, pw * 0.12, ph * 0.4, 1), paint((40, 40, 60)))
    # bumper
    c.drawPath(rrect(W * 0.04, bb - H * 0.07, W * 0.92 + side * 0.4, H * 0.06, 4), paint(hd.shade(dark, -0.4)))
    if style == "sport":
        for x in (W * 0.28, W * 0.72):   # exhausts and a diffuser
            c.drawPath(ellipse(x + side * 0.2, bb - H * 0.03, W * 0.035, H * 0.03), paint((60, 60, 70)))
            c.drawPath(ellipse(x + side * 0.2, bb - H * 0.03, W * 0.022, H * 0.018), paint((10, 10, 14)))
        # rear wing
        wy = bt - H * 0.02
        for x in (W * 0.22, W * 0.78):
            c.drawPath(rrect(x - 4 + sh * 0.6, wy, 8, H * 0.08, 2), paint(INK))
        wing = rrect(W * 0.06 + sh * 0.8, wy - H * 0.05, W * 0.88, H * 0.06, 6)
        outlined(c, wing, dark, INK, 2.2, shader=lin(0, wy - H * 0.05, 0, wy + H * 0.01, [light, dark]))
    atlas.add(name, s)


def cars():
    red = ((255, 120, 110), (220, 30, 50), (110, 10, 30))
    for yaw in range(3):
        car("IMG_PCAR%d" % yaw, 80, 38, red, "sport", yaw, density=2)
    car("IMG_RIVAL0", 70, 36, ((150, 200, 255), (50, 100, 220), (20, 30, 100)), "coupe")
    car("IMG_RIVAL1", 62, 52, ((255, 240, 150), (245, 190, 40), (150, 90, 10)), "van")
    car("IMG_RIVAL2", 70, 38, ((240, 240, 250), (170, 172, 190), (80, 80, 100)), "sedan")
    car("IMG_RIVAL3", 72, 42, ((170, 240, 150), (50, 160, 90), (16, 70, 50)), "pickup")
    atlas.table("RIVAL", ["IMG_RIVAL%d" % i for i in range(4)])
    atlas.table("PCAR", ["IMG_PCAR%d" % i for i in range(3)])


# ---------------------------------------------------------------- scenery
GREEN = [(190, 245, 140), (80, 190, 90), (24, 100, 60)]
BARK = [(200, 150, 100), (140, 90, 56), (70, 44, 30)]
ROCKC = [(255, 190, 130), (210, 100, 60), (110, 40, 30)]


def palm(name, seed):
    rng = random.Random(seed)
    s, W, H = sprite(72, 136, extra=40)
    c = s.c
    bx, top = W / 2 + rng.uniform(-12, 12), 26 * F
    lean = rng.choice([-1, 1]) * rng.uniform(36, 60)
    n = 30
    spine = [(bx + lean * (1 - i / n) ** 2 - lean * 0.2, H - 2 - i / n * (H - top)) for i in range(n + 1)]
    left, right = [], []
    for i, (x, y) in enumerate(spine):
        r = (3.8 - i / n * 1.5) * F
        left.append((x - r, y)); right.append((x + r, y))
    trunk = smooth(left + right[::-1], tension=0.2)
    outlined(c, trunk, BARK[1], INK, 2.4, shader=lin(bx - 20, 0, bx + 20, 0, [BARK[0], BARK[1], BARK[2]]))
    c.save(); c.clipPath(trunk, skia.ClipOp.kIntersect, True)
    for i in range(1, n, 2):   # bark rings
        x, y = spine[i]
        r = (3.8 - i / n * 1.5) * F
        c.drawPath(smooth([(x - r, y - 4), (x, y + 3), (x + r, y - 4)], closed=False, tension=0.5), paint(BARK[2], stroke=3, alpha=0.75))
    c.restore()
    tx, ty = spine[-1]

    def frond(a, length, droop, shade_k):
        pts = []
        for j in range(15):
            t = j / 14
            d = t * length
            pts.append((tx + math.cos(a) * d, ty + math.sin(a) * d + droop * d * d / length))
        base = hd.shade(GREEN[1], shade_k)
        dark = hd.shade(GREEN[2], shade_k)
        lite = hd.shade(GREEN[0], shade_k)
        for j in range(1, 14):   # leaflets, swept back from the tip and drooping
            x, y = pts[j]
            dx, dy = pts[j + 1][0] - x, pts[j + 1][1] - y
            l = math.hypot(dx, dy) or 1
            ux, uy = dx / l, dy / l
            t = j / 14
            ll = (5 + 22 * math.sin(t * math.pi * 0.9 + 0.2)) * (1 - t * 0.35)
            for sgn in (-1, 1):
                nx, ny = -uy * sgn, ux * sgn
                ex, ey = x + (nx * 0.8 + ux * 0.55) * ll, y + (ny * 0.8 + uy * 0.55) * ll + ll * 0.5
                leaf = poly([(x, y), (ex, ey)], closed=False)
                c.drawPath(leaf, paint(INK, stroke=9))
        for j in range(1, 14):
            x, y = pts[j]
            dx, dy = pts[j + 1][0] - x, pts[j + 1][1] - y
            l = math.hypot(dx, dy) or 1
            ux, uy = dx / l, dy / l
            t = j / 14
            ll = (5 + 22 * math.sin(t * math.pi * 0.9 + 0.2)) * (1 - t * 0.35)
            for sgn in (-1, 1):
                nx, ny = -uy * sgn, ux * sgn
                ex, ey = x + (nx * 0.8 + ux * 0.55) * ll, y + (ny * 0.8 + uy * 0.55) * ll + ll * 0.5
                c.drawPath(poly([(x, y), (ex, ey)], closed=False), paint(lite if sgn < 0 else base, stroke=5))
        sp = smooth(pts, closed=False)
        c.drawPath(sp, paint(INK, stroke=8))
        c.drawPath(sp, paint(dark, stroke=4))

    fr = []
    for k in range(9):
        a = math.radians(-180 + 22.5 * k + rng.uniform(-6, 6))
        up = 1 - abs(k - 4) / 4.0
        fr.append((up, a, rng.uniform(30, 38) * F * (0.8 + 0.2 * (1 - up)), rng.uniform(0.35, 0.6) * (1.2 - up * 0.6)))
    for up, a, length, droop in sorted(fr, key=lambda f: f[0]):   # back fronds first, darker
        frond(a, length, droop, -0.15 if up > 0.6 else 0)
    for d in (-9, 9, 0):
        c.drawPath(circle(tx + d, ty + 12, 8), paint(INK))
        c.drawPath(circle(tx + d, ty + 12, 6), paint(shader=rad(tx + d - 2, ty + 10, 8, [(170, 120, 60), (90, 56, 30)])))
    atlas.add(name, s)


def rock():
    s, W, H = sprite(72, 40, extra=4)
    c = s.c
    pts = [(6, H), (10, H * 0.55), (W * 0.18, H * 0.2), (W * 0.36, H * 0.06), (W * 0.55, H * 0.12), (W * 0.72, H * 0.04),
           (W * 0.88, H * 0.3), (W - 6, H * 0.6), (W - 4, H)]
    body = smooth(pts, tension=0.35)
    outlined(c, body, ROCKC[1], INK, 2.6, shader=lin(0, 0, W, H, [ROCKC[0], ROCKC[1], ROCKC[2]]))
    for k in range(5):   # strata
        y = H * (0.3 + k * 0.15)
        cel(c, body, smooth([(0, y), (W * 0.3, y - 10), (W * 0.6, y + 6), (W, y - 8), (W, y + 6), (W * 0.6, y + 16), (W * 0.3, y), (0, y + 8)], tension=0.4),
            ROCKC[2], 0.25)
    cel(c, body, ellipse(W * 0.3, H * 0.2, W * 0.25, H * 0.15), (255, 230, 190), 0.35)
    atlas.add("IMG_ROCK", s)


def mesa():
    s, W, H = sprite(120, 64, extra=4)
    c = s.c
    body = poly([(0, H), (14 * F, 9 * F), (20 * F, 7 * F), (60 * F, 8 * F), (98 * F, 6 * F), (106 * F, 9 * F), (W, H)])
    outlined(c, body, ROCKC[1], INK, 2.6, shader=lin(0, 0, W, 0, [ROCKC[0], ROCKC[1], ROCKC[2]]))
    for k in range(6):
        y = H * (0.25 + k * 0.13)
        cel(c, body, poly([(0, y), (W, y - 6), (W, y + 10), (0, y + 14)]), ROCKC[2] if k % 2 else (255, 210, 160), 0.22)
    cel(c, body, poly([(W * 0.7, 0), (W, 0), (W, H), (W * 0.82, H)]), (60, 20, 40), 0.3)
    for x in range(int(W * 0.15), int(W * 0.85), 26):   # erosion gullies
        c.drawPath(poly([(x, 12 * F), (x + 6, H * 0.6), (x - 4, H)], closed=False), paint(ROCKC[2], stroke=3, alpha=0.4))
    atlas.add("IMG_MESA", s)


def cactus():
    s, W, H = sprite(40, 72, extra=4)
    c = s.c

    def arm(path):
        outlined(c, path, GREEN[1], INK, 2.6, shader=lin(0, 0, W, 0, [GREEN[0], GREEN[1], GREEN[2]]))

    arm(union(capsule(8 * F, 22 * F, 8 * F, 42 * F, 3.5 * F), capsule(8 * F, 42 * F, 18 * F, 42 * F, 3.5 * F)))
    arm(union(capsule(32 * F, 30 * F, 32 * F, 48 * F, 3.5 * F), capsule(32 * F, 48 * F, 22 * F, 48 * F, 3.5 * F)))
    trunk = capsule(20 * F, 6 * F, 20 * F, H + 10, 5.5 * F)
    arm(trunk)
    for dx in (-12, -4, 4, 12):   # ribs
        cel(c, trunk, poly([(20 * F + dx, 0), (20 * F + dx, H)], closed=False), GREEN[2], 0)
        c.save(); c.clipPath(trunk, skia.ClipOp.kIntersect, True)
        c.drawPath(poly([(20 * F + dx, 6 * F), (20 * F + dx, H)], closed=False), paint(GREEN[2], stroke=2, alpha=0.5))
        c.restore()
    rng = random.Random(3)
    for _ in range(14):
        c.drawPath(circle(rng.uniform(14 * F, 26 * F), rng.uniform(8 * F, H - 10), 2), paint((255, 240, 160)))
    c.drawPath(circle(20 * F, 3 * F, 7), paint((255, 120, 160)))   # a flower on top
    atlas.add("IMG_CACTUS", s)


def billboard(kind):
    s, W, H = sprite(96, 76, extra=4)
    c = s.c
    for px in (18 * F, 74 * F):
        c.drawPath(rrect(px, 40 * F, 5 * F, H - 40 * F, 4), paint(INK))
        c.drawPath(rrect(px + 3, 40 * F, 5 * F - 6, H - 40 * F, 3), paint(shader=lin(px, 0, px + 5 * F, 0, [(220, 220, 235), (110, 110, 130)])))
    board = rrect(4 * F, 4 * F, W - 8 * F, 42 * F, 10)
    grads = [[(255, 230, 120), (255, 120, 60), (220, 40, 140)], [(40, 60, 180), (40, 150, 230), (120, 240, 255)],
             [(30, 120, 70), (80, 200, 100), (250, 230, 110)]]
    outlined(c, board, (255, 255, 255), INK, 4, shader=lin(0, 4 * F, 0, 46 * F, grads[kind]))
    cel(c, board, poly([(0, 0), (W * 0.6, 0), (W * 0.3, H), (0, H)]), (255, 255, 255), 0.12)
    txt = ["NOVA", "TURBO", "ARCADE"][kind]
    word(c, txt, W / 2, 21 * F, 25 * F if kind == 0 else 19 * F, (255, 255, 255), INK, 7)
    c.drawPath(rrect(8 * F, 37 * F, W - 16 * F, 2 * F, 3), paint((255, 255, 255) if kind != 1 else (255, 230, 90)))
    for x in range(int(10 * F), int(W - 10 * F), int(12 * F)):   # lamps on top
        c.drawPath(rrect(x, 1 * F, 2 * F, 4 * F, 2), paint((90, 90, 110)))
    atlas.add("IMG_BILL%d" % kind, s)


def lamp():
    s, W, H = sprite(40, 120, extra=6)
    c = s.c
    pole = rrect(4 * F, 14 * F, 4 * F, H - 14 * F, 6)
    outlined(c, pole, (150, 150, 170), INK, 2.4, shader=lin(4 * F, 0, 8 * F, 0, [(230, 230, 245), (120, 120, 140)]))
    c.drawPath(rrect(2 * F, H - 6 * F, 8 * F, 6 * F, 4), paint((70, 70, 90)))
    armp = smooth([(6 * F, 16 * F), (14 * F, 10 * F), (24 * F, 9 * F), (32 * F, 10 * F)], closed=False)
    c.drawPath(armp, paint(INK, stroke=4.5 * F))
    c.drawPath(armp, paint((180, 180, 200), stroke=3 * F))
    head = rrect(24 * F, 7 * F, 14 * F, 5 * F, 8)
    outlined(c, head, (120, 120, 140), INK, 2.4)
    c.drawPath(rrect(25 * F, 11 * F, 12 * F, 2.2 * F, 4), paint((255, 250, 220)))
    atlas.add("IMG_LAMP", s)


def tower(name, seed):
    rng = random.Random(seed)
    s, W, H = sprite(64, 150, extra=6)
    c = s.c
    top = (20 + rng.randint(0, 20)) * F
    body = rrect(4 * F, top, W - 8 * F, H - top + 10, 6)
    outlined(c, body, (50, 44, 90), INK, 3, shader=lin(4 * F, 0, W - 4 * F, 0, [(90, 80, 150), (50, 44, 90), (24, 20, 50)]))
    neon = [(255, 80, 200), (110, 240, 255)][seed % 2]
    c.save(); c.clipPath(body, skia.ClipOp.kIntersect, True)
    y = top + 8 * F
    while y < H - 6 * F:
        x = 10 * F
        while x < W - 10 * F:
            if rng.random() < 0.6:
                warm = rng.random() < 0.7
                cc = (255, 210, 110) if warm else (140, 230, 255)
                c.drawPath(rrect(x, y, 3.2 * F, 3.2 * F, 2), paint(cc, alpha=rng.uniform(0.7, 1)))
            else:
                c.drawPath(rrect(x, y, 3.2 * F, 3.2 * F, 2), paint((20, 16, 40)))
            x += 6 * F
        y += 6 * F
    c.drawPath(poly([(W * 0.7, 0), (W, 0), (W, H), (W * 0.7, H)]), paint((10, 6, 30), alpha=0.35))
    c.restore()
    c.drawPath(rrect(4 * F, top, W - 8 * F, 3 * F, 3), paint(neon, blur=6))
    c.drawPath(rrect(4 * F, top, W - 8 * F, 2.4 * F, 3), paint(neon))
    c.drawPath(rrect(W / 2 - 4, top - 14 * F, 8, 14 * F, 3), paint((150, 150, 170)))
    c.drawPath(circle(W / 2, top - 15 * F, 7), paint((255, 60, 70)))
    atlas.add(name, s)


def chevron():
    s, W, H = sprite(32, 44, extra=4)
    c = s.c
    c.drawPath(rrect(14 * F, 22 * F, 4 * F, H - 22 * F, 4), paint(INK))
    c.drawPath(rrect(14 * F + 3, 22 * F, 4 * F - 6, H - 22 * F, 3), paint((200, 200, 215)))
    board = rrect(2 * F, 2 * F, W - 4 * F, 22 * F, 10)
    outlined(c, board, (255, 210, 40), INK, 4, shader=lin(0, 2 * F, 0, 24 * F, [(255, 240, 120), (250, 190, 30)]))
    for i in range(3):   # pointing left
        x = 6 * F + i * 8 * F
        c.drawPath(poly([(x + 6 * F, 5 * F), (x, 13 * F), (x + 6 * F, 21 * F), (x + 9 * F, 21 * F), (x + 3 * F, 13 * F), (x + 9 * F, 5 * F)]), paint(INK))
    atlas.add("IMG_CHEVRON", s)


def bush():
    s, W, H = sprite(48, 26, extra=4)
    c = s.c
    blobs = [(12, 16, 9), (24, 12, 11), (36, 16, 9), (20, 19, 7), (30, 19, 7)]
    body = union(*[circle(x * F, y * F, r * F) for x, y, r in blobs], rrect(4 * F, 18 * F, 40 * F, 8 * F, 10))
    outlined(c, body, GREEN[1], INK, 2.6, shader=lin(0, 0, W * 0.8, H, [GREEN[0], GREEN[1], GREEN[2]]))
    for x, y, r in blobs:
        cel(c, body, circle(x * F - r * 1.2, y * F - r * 1.5, r * F * 0.6), (230, 255, 190), 0.35)
    rng = random.Random(5)
    for _ in range(8):
        c.drawPath(circle(rng.uniform(8 * F, 40 * F), rng.uniform(8 * F, 20 * F), 5), paint((255, 150, 190)))
    atlas.add("IMG_BUSH", s)


def banner(name, txt):
    s, W, H = sprite(176, 26, extra=4)
    c = s.c
    body = rrect(0, 0, W, H, 10)
    outlined(c, body, (220, 30, 50), INK, 4, shader=lin(0, 0, 0, H, [(255, 90, 90), (210, 20, 50), (150, 10, 40)]))
    c.save(); c.clipPath(body, skia.ClipOp.kIntersect, True)
    sq = 4 * F
    for row, y in enumerate((0, H - sq)):
        c.drawPath(rrect(0, y, W, sq, 0), paint(INK))
        for k in range(int(W / sq) + 1):
            if (k + row) % 2 == 0:
                c.drawPath(rrect(k * sq, y, sq, sq, 0), paint((255, 255, 255)))
    c.restore()
    word(c, txt, W / 2, H / 2 - 2, 17 * F, (255, 255, 255), INK, 7)
    atlas.add(name, s)


def post():
    s, W, H = sprite(10, 90, extra=4)
    c = s.c
    p = rrect(1 * F, 0, 8 * F, H, 6)
    outlined(c, p, (180, 180, 200), INK, 2.6, shader=lin(F, 0, 9 * F, 0, [(240, 240, 250), (170, 170, 190), (90, 90, 110)]))
    for y in range(0, int(H), int(10 * F)):
        c.drawPath(rrect(1 * F, y, 8 * F, 4, 0), paint((70, 70, 90)))
    atlas.add("IMG_POST", s)


SCENERY = ["IMG_PALM0", "IMG_PALM1", "IMG_ROCK", "IMG_MESA", "IMG_CACTUS", "IMG_BILL0", "IMG_BILL1", "IMG_BILL2",
           "IMG_LAMP", "IMG_TOWER0", "IMG_TOWER1", "IMG_CHEVRON", "IMG_BUSH", "IMG_BANNERC", "IMG_BANNERS", "IMG_POST"]


def scenery():
    palm("IMG_PALM0", 1)
    palm("IMG_PALM1", 7)
    rock()
    mesa()
    cactus()
    for k in range(3):
        billboard(k)
    lamp()
    tower("IMG_TOWER0", 1)
    tower("IMG_TOWER1", 2)
    chevron()
    bush()
    banner("IMG_BANNERC", "CHECKPOINT")
    banner("IMG_BANNERS", "START")
    post()
    atlas.table("SCENERY", SCENERY)


# ---------------------------------------------------------------- parallax layers (tile horizontally)
LW = 2304    # 512 original pixels * 4.5, so the layers wrap like the original's


def layer(name, h, fn):
    s = Sprite(LW, h, density=1, trim=False)
    fn(s.c, h)
    atlas.add_big(name, s, name.lower().replace("img_", "") + ".png")


def ridge(heights, h, base_extra=0):
    pts = [(0, h)]
    for x in range(0, LW + 1, 6):
        pts.append((x, h - heights(x)))
    pts.append((LW, h))
    return poly(pts)


def tau(x):
    return x / LW * 2 * math.pi


def far0(c, h):    # Sunset Coast: violet mountains
    def hgt(x):
        a = tau(x)
        return (34 + 16 * math.sin(a * 3) + 9 * math.sin(a * 7 + 1) + 4 * math.sin(a * 17)) * F + 60
    p = ridge(hgt, h)
    c.drawPath(p, paint(shader=lin(0, h - 300, 0, h, [(190, 110, 170), (130, 64, 140), (110, 60, 130)])))
    # snowy, sunlit ridges
    c.save(); c.clipPath(p, skia.ClipOp.kIntersect, True)
    rim = ridge(lambda x: hgt(x) - 14, h)
    c.drawPath(skia.Op(p, rim, skia.PathOp.kDifference_PathOp), paint((255, 190, 170), alpha=0.7))
    for x in range(0, LW, 90):   # gullies
        top = h - hgt(x)
        c.drawPath(poly([(x, top + 10), (x + 26, top + 90), (x + 10, h)], closed=False), paint((90, 40, 110), stroke=5, alpha=0.35))
    c.restore()


def near0(c, h):   # the sea along the coast
    def hgt(x):
        return (6 + 3 * math.sin(tau(x) * 9)) * F + 40
    p = ridge(hgt, h)
    c.drawPath(p, paint(shader=lin(0, h - 80, 0, h, [(110, 150, 200), (60, 110, 170), (40, 80, 140)])))
    c.save(); c.clipPath(p, skia.ClipOp.kIntersect, True)
    rng = random.Random(2)
    for _ in range(140):
        x, y = rng.uniform(0, LW), rng.uniform(h - 60, h - 4)
        c.drawPath(rrect(x, y, rng.uniform(20, 70), 3, 1.5), paint((255, 200, 160), alpha=rng.uniform(0.3, 0.8)))
    c.restore()


def far1(c, h):    # Canyon Run: flat-topped mesas
    def hgt(x):
        a = tau(x)
        m = math.sin(a * 4 + 0.5)
        return (46 + 2.5 * math.sin(a * 9) + 1.5 * math.sin(a * 23) if m > 0.3 else 18 + 8 * math.sin(a * 11)) * F + 50
    pts = [(0, h)]
    for x in range(0, LW + 1, 3):
        pts.append((x, h - hgt(x)))
    pts.append((LW, h))
    p = poly(pts)
    c.drawPath(p, paint(shader=lin(0, h - 300, 0, h, [(240, 140, 90), (200, 96, 60), (180, 90, 70)])))
    c.save(); c.clipPath(p, skia.ClipOp.kIntersect, True)
    for k in range(10):
        y = h - 260 + k * 26
        c.drawPath(rrect(0, y, LW, 9, 0), paint((140, 60, 40), alpha=0.25))
    c.restore()


def near1(c, h):
    def hgt(x):
        a = tau(x)
        return (14 + 10 * abs(math.sin(a * 6)) + 3 * math.sin(a * 23)) * F + 40
    p = ridge(hgt, h)
    c.drawPath(p, paint(shader=lin(0, h - 200, 0, h, [(190, 90, 60), (160, 70, 50), (140, 64, 50)])))
    c.save(); c.clipPath(p, skia.ClipOp.kIntersect, True)
    rim = ridge(lambda x: hgt(x) - 10, h)
    c.drawPath(skia.Op(p, rim, skia.PathOp.kDifference_PathOp), paint((255, 190, 130), alpha=0.6))
    c.restore()


def city(c, h, seed, base, lit, win_cols, wmin, wmax, hmin, hmax, step):
    rng = random.Random(seed)
    x = 0
    while x < LW:
        w = rng.randint(wmin, wmax)
        w = min(w, LW - x)
        bh = rng.randint(hmin, hmax)
        top = h - bh
        c.drawPath(rrect(x, top, w + 1, bh, 0), paint(shader=lin(0, top, 0, h, [hd.shade(base, 0.08), base])))
        if rng.random() < 0.3:
            c.drawPath(rrect(x + w / 2 - 2, top - 30, 4, 30, 0), paint(base))
            c.drawPath(circle(x + w / 2, top - 32, 4), paint((255, 70, 80)))
        if rng.random() < 0.35:   # a neon rim
            c.drawPath(rrect(x, top, w, 4, 0), paint(rng.choice([(255, 80, 200), (110, 240, 255)]), alpha=0.9))
        for wy in range(int(top + 12), h - 6, step):
            for wx in range(int(x + 8), int(x + w - 8), step):
                if rng.random() < lit:
                    c.drawPath(rrect(wx, wy, step * 0.45, step * 0.45, 1), paint(rng.choice(win_cols), alpha=rng.uniform(0.5, 1)))
        x += w + rng.randint(0, 10)


def far2(c, h):    # Neon Night: the skyline
    city(c, h, 11, (30, 22, 60), 0.35, [(150, 220, 255), (120, 160, 255)], 50, 130, 120, 320, 14)


def near2(c, h):
    city(c, h, 23, (16, 12, 36), 0.45, [(255, 210, 110), (255, 170, 90)], 70, 170, 60, 200, 18)


def layers():
    layer("IMG_FAR0", 420, far0)
    layer("IMG_NEAR0", 140, near0)
    layer("IMG_FAR1", 420, far1)
    layer("IMG_NEAR1", 260, near1)
    layer("IMG_FAR2", 420, far2)
    layer("IMG_NEAR2", 260, near2)


def main():
    cars()
    scenery()
    layers()
    atlas.write(os.path.join(ROOT, "assets", "turbohorizon"), os.path.join(ROOT, "games", "TurboHorizon", "art.h"), "thart", "turbohorizon")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "th_sheet.png"))
        for name, a, sc, fn in atlas.bigs:
            skia.Image.fromarray(a, colorType=skia.kRGBA_8888_ColorType, alphaType=skia.kUnpremul_AlphaType).save(os.path.join(d, fn), skia.kPNG)


if __name__ == "__main__":
    main()
