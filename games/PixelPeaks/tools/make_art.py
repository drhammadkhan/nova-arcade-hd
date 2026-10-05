#!/usr/bin/env python3
"""Pixel Peaks HD art, drawn as vector shapes with Skia and packed into atlases.
    python3 games/PixelPeaks/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/pixelpeaks/*.png and games/PixelPeaks/art.h. All art is original.

The heroine is a rig of parts (head, torso, belt, arms, legs, ponytail, headband tails) that the game
poses every frame, so she animates smoothly at any size. Every part is drawn facing right, hanging
from its joint (the pivot): limbs point down, the torso and head point up.
Tiles are 72 px. Ground tiles come in 16 edge shapes per world (picked by autotile() in the game)."""
import os
import sys
import math
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, outlined, cel, poly, smooth, ellipse, circle, rrect, capsule, union, minus, intersect, offset, paint, lin, rad, shade, mix, INK  # noqa: E402
import skia  # noqa: E402

TS = 72
GI = (248, 248, 252)
GI_SH = (190, 198, 226)
SKIN = (255, 216, 188)
SKIN_SH = (238, 166, 140)
HAIR = (54, 38, 66)
HAIR_HL = (112, 86, 140)
RED = (226, 58, 78)
RED_SH = (160, 30, 60)
GOLD = (255, 204, 64)
GOLD_SH = (204, 132, 30)
LW = 2.6     # outline width for the heroine and foes

atlas = hd.Atlas(2048)


# ================================================================ the heroine
def part_limb(name, length, w0, w1, fill, fill_sh, end=None):
    """A sleeve or trouser segment hanging down from its joint at (0, 0)."""
    s = Sprite(w0 * 2 + 30, length + 30, pivot=(w0 + 15, 10))
    c = s.c
    c.translate(w0 + 15, 10)
    body = capsule(0, 0, 0, length, w0 / 2, w1 / 2)
    if end:
        body = union(body, end)
    outlined(c, body, fill, INK, LW)
    cel(c, body, rrect(-w0, -10, w0 * 0.75, length + 30, 4), fill_sh, 0.9)   # shadow down the back edge
    cel(c, body, capsule(w0 * 0.15, 2, w1 * 0.1, length - 4, w0 * 0.12), hd.shade(fill, 0.6), 0.8)
    atlas.add(name, s)


def hero_parts():
    # --- torso: the gi jacket from the hips (pivot) up to the neck at y = -32
    s = Sprite(70, 70, pivot=(35, 50))
    c = s.c
    c.translate(35, 50)
    body = smooth([(-15, 2), (-16, -10), (-15, -24), (-10, -33), (0, -36), (10, -33), (15, -24), (17, -10), (16, 2), (0, 4)])
    outlined(c, body, GI, INK, LW)
    cel(c, body, smooth([(-20, 6), (-20, -36), (-8, -36), (-11, -20), (-9, 6)]), GI_SH, 0.85)
    # the lapels crossing over the chest, with the undershirt showing at the neck
    c.drawPath(poly([(1, -35), (9, -32), (4, -20)]), paint(SKIN))
    c.drawPath(smooth([(-3, -35), (5, -24), (10, -12), (14, -4)], closed=False), paint(INK, stroke=2.2))
    c.drawPath(smooth([(10, -32), (6, -23), (4, -17)], closed=False), paint(INK, stroke=1.8))
    c.drawPath(smooth([(-6, -30), (-7, -16)], closed=False), paint(GI_SH, stroke=2))
    atlas.add("IMG_TORSO", s)

    # --- belt (drawn white; the game tints it) and its knot
    s = Sprite(60, 30, pivot=(30, 15))
    c = s.c
    c.translate(30, 15)
    band = smooth([(-15, -5), (0, -5.5), (15.5, -5), (15.5, 0), (0, 0.5), (-15, 0)], tension=0.2)
    outlined(c, band, (255, 255, 255), INK, 2.2)
    cel(c, band, rrect(-20, -1.6, 40, 4, 1), (205, 205, 205), 1)
    knot = ellipse(10, -2.5, 3.6, 3.3)
    outlined(c, knot, (255, 255, 255), INK, 1.8)
    cel(c, knot, ellipse(11, -0.5, 3.6, 2), (210, 210, 210), 1)
    atlas.add("IMG_BELT", s)
    s = Sprite(40, 40, pivot=(20, 8))
    c = s.c
    c.translate(20, 8)
    for dx, ln in ((-2, 11), (2.5, 9)):
        tail = smooth([(dx - 2, 0), (dx + 2, 0), (dx + 2.8, ln), (dx - 1.2, ln + 1)])
        outlined(c, tail, (255, 255, 255), INK, 1.8)
        cel(c, tail, rrect(dx - 4, 0, 2.8, ln + 3, 1), (205, 205, 205), 1)
    atlas.add("IMG_BELT_TAIL", s)

    # --- arms and legs
    hand = circle(0, 18, 5.6)
    part_limb("IMG_ARM_UP", 15, 13, 12, GI, GI_SH)
    s = Sprite(50, 50, pivot=(25, 10))      # forearm: wide sleeve, then the hand
    c = s.c
    c.translate(25, 10)
    sleeve = capsule(0, 0, 0, 11, 6.2, 7)
    outlined(c, hand, SKIN, INK, LW)
    cel(c, hand, circle(-3, 21, 4.5), SKIN_SH, 1)
    outlined(c, sleeve, GI, INK, LW)
    cel(c, sleeve, rrect(-12, -8, 8, 30, 3), GI_SH, 0.9)
    atlas.add("IMG_ARM_LO", s)
    part_limb("IMG_LEG_UP", 17, 15, 14, GI, GI_SH)
    s = Sprite(60, 60, pivot=(25, 10))      # shin with the bare foot pointing forward
    c = s.c
    c.translate(25, 10)
    foot = smooth([(-5, 15), (2, 14), (11, 16), (13, 20), (9, 22), (-4, 22), (-6, 19)])
    outlined(c, foot, SKIN, INK, LW)
    cel(c, foot, rrect(-8, 20, 24, 4, 1), SKIN_SH, 1)
    shin = capsule(0, 0, 0, 13, 7.3, 7.6)
    outlined(c, shin, GI, INK, LW)
    cel(c, shin, rrect(-12, -8, 8, 30, 3), GI_SH, 0.9)
    c.drawPath(smooth([(-6, 12), (0, 14), (7, 12)], closed=False), paint(GI_SH, stroke=1.6))
    atlas.add("IMG_LEG_LO", s)

    # --- ponytail (hangs from the back of the head) and the headband tails
    s = Sprite(50, 60, pivot=(25, 8))
    c = s.c
    c.translate(25, 8)
    tail = smooth([(-5, 0), (5, 0), (8, 10), (5, 24), (0, 32), (-3, 22), (-7, 10)])
    outlined(c, tail, HAIR, INK, LW)
    cel(c, tail, smooth([(1, 2), (5, 9), (3, 22), (0, 26), (1, 12)]), HAIR_HL, 0.8)
    tie = rrect(-6, -2, 12, 6, 3)
    outlined(c, tie, RED, INK, 2)
    atlas.add("IMG_PONYTAIL", s)
    s = Sprite(50, 50, pivot=(25, 6))
    c = s.c
    c.translate(25, 6)
    for dx, ln, w in ((-3, 24, 5.0), (3, 19, 4.4)):
        rib = smooth([(dx - w / 2, 0), (dx + w / 2, 0), (dx + w / 2 + 2, ln * 0.6), (dx + 1, ln), (dx - w / 2 + 1, ln - 2), (dx - w / 2, ln * 0.5)])
        outlined(c, rib, RED, INK, 2)
        cel(c, rib, rrect(dx - w, 0, w * 0.8, ln + 2, 1), RED_SH, 0.8)
    atlas.add("IMG_BAND_TAIL", s)

    for face in ("N", "BLINK", "HURT", "HAPPY", "FOCUS"):
        head(face)

    # --- curled up for the judo roll: a ball, spun by the game
    s = Sprite(90, 90, pivot=(45, 45))
    c = s.c
    c.translate(45, 45)
    ball = circle(0, 0, 31)
    outlined(c, ball, GI, INK, LW)
    cel(c, ball, circle(-8, 9, 31), GI_SH, 0.9)
    hair = intersect(ball, circle(15, -16, 24))
    c.drawPath(hair, paint(HAIR))
    cel(c, hair, circle(20, -24, 10), HAIR_HL, 0.8)
    c.drawPath(poly([(-30, -6), (-6, 14), (-8, 26)], closed=False), paint(GI_SH, stroke=2.2))
    c.drawPath(smooth([(-20, 20), (0, 28), (20, 18)], closed=False), paint(INK, stroke=1.8))
    knee = circle(-6, -8, 7)
    c.drawPath(knee, paint(SKIN))
    c.drawPath(knee, paint(INK, stroke=1.8))
    c.drawPath(ball, paint(INK, stroke=LW * 1.6))
    atlas.add("IMG_BALL", s)
    s = Sprite(90, 90, pivot=(45, 45))     # the belt across the ball (tinted)
    c = s.c
    c.translate(45, 45)
    band = intersect(circle(0, 0, 31.5), poly([(-34, 8), (34, -16), (34, -6), (-34, 18)]))
    outlined(c, band, (255, 255, 255), INK, 2)
    atlas.add("IMG_BALL_BELT", s)


def head(face):
    """The head, pivot at the neck. Faces: N, BLINK, HURT, HAPPY, FOCUS."""
    s = Sprite(90, 90, pivot=(45, 70))
    c = s.c
    c.translate(45, 70)
    skull = circle(2, -26, 23.5)
    jaw = smooth([(-6, -22), (6, -24), (20, -20), (22, -10), (14, -2), (4, 0), (-4, -6)])
    whole = union(skull, jaw)
    outlined(c, whole, SKIN, INK, LW)
    cel(c, whole, smooth([(-12, -30), (-2, -20), (6, -2), (-20, 0)]), SKIN_SH, 0.75)
    # hair: a cap over the top and back, a swept fringe over the forehead
    hair = smooth([(-22, -20), (-23, -36), (-12, -49), (4, -51), (19, -45), (26, -34), (25, -27), (17, -33), (13, -28), (8, -34), (2, -29),
                   (-4, -34), (-6, -24), (-10, -18), (-16, -10)])
    hair = intersect(union(hair, circle(-12, -24, 13)), offset(skull, 0, -1))
    hair = union(hair, smooth([(-20, -24), (-22, -10), (-18, -6), (-12, -14)]))
    c.drawPath(hair, paint(HAIR))
    cel(c, hair, smooth([(-8, -46), (8, -49), (18, -43), (10, -42), (-2, -41)]), HAIR_HL, 0.9)
    c.drawPath(hair, paint(INK, stroke=1.8))
    # ear
    ear = ellipse(-6, -21, 4, 5)
    outlined(c, ear, SKIN, INK, 1.8)
    c.drawPath(ellipse(-6, -21, 1.6, 2.4), paint(SKIN_SH))
    # the red headband across the forehead
    band = intersect(skull, poly([(-30, -36), (30, -42), (30, -36), (-30, -30)]))
    c.drawPath(band, paint(RED))
    cel(c, band, rrect(-30, -34, 60, 3, 1), RED_SH, 0.8)
    c.drawPath(band, paint(INK, stroke=1.6))
    c.drawPath(circle(13.5, -40, 3.2), paint(GOLD))   # a little gold crest on the band
    c.drawPath(circle(13.5, -40, 3.2), paint(INK, stroke=1.2))
    # face
    eyes = [(8.5, -24, 0.82), (19, -24, 1.0)]
    if face in ("N", "FOCUS"):
        for ex, ey, k in eyes:
            e = ellipse(ex, ey, 2.9 * k, 4.6)
            c.drawPath(e, paint((40, 28, 60)))
            c.drawPath(ellipse(ex + 0.4, ey + 1.6, 2.0 * k, 2.2), paint((96, 70, 150)))
            c.drawPath(circle(ex + 0.8 * k, ey - 1.8, 1.2), paint((255, 255, 255)))
        if face == "FOCUS":   # determined brows
            c.drawPath(poly([(5, -31), (11, -29.5)], closed=False), paint(HAIR, stroke=2))
            c.drawPath(poly([(16, -29.5), (22, -31)], closed=False), paint(HAIR, stroke=2))
            c.drawPath(poly([(13, -12), (18, -12.5)], closed=False), paint((150, 60, 70), stroke=1.8))
        else:
            c.drawPath(smooth([(12, -13), (14.5, -11), (17, -12.5)], closed=False), paint((150, 60, 70), stroke=1.8))
    elif face == "BLINK":
        for ex, ey, k in eyes:
            c.drawPath(smooth([(ex - 3 * k, ey + 1), (ex, ey + 2.4), (ex + 3 * k, ey + 1)], closed=False), paint((40, 28, 60), stroke=1.9))
        c.drawPath(smooth([(12, -13), (14.5, -11), (17, -12.5)], closed=False), paint((150, 60, 70), stroke=1.8))
    elif face == "HAPPY":
        for ex, ey, k in eyes:
            c.drawPath(smooth([(ex - 3 * k, ey + 1.5), (ex, ey - 2), (ex + 3 * k, ey + 1.5)], closed=False), paint((40, 28, 60), stroke=2.1))
        m = smooth([(11, -14), (18, -14.5), (15, -9.5)])
        c.drawPath(m, paint((200, 70, 90)))
        c.drawPath(m, paint((120, 40, 60), stroke=1.2))
    elif face == "HURT":
        for ex, ey, k in eyes:
            c.drawPath(poly([(ex - 2.6 * k, ey - 3), (ex + 2.4 * k, ey), (ex - 2.6 * k, ey + 3)], closed=False), paint((40, 28, 60), stroke=2))
        c.drawPath(ellipse(15, -11.5, 2.4, 2.8), paint((120, 40, 60)))
    c.drawPath(ellipse(6, -16.5, 3.4, 1.8), paint((255, 150, 160), alpha=0.7))
    c.drawPath(ellipse(20.5, -16.5, 2.6, 1.8), paint((255, 150, 160), alpha=0.7))
    atlas.add("IMG_HEAD_" + face, s)


# ================================================================ foes
MOCHI_COLS = [((255, 236, 242), (246, 176, 200)), ((214, 240, 196), (138, 196, 120)), ((246, 248, 255), (178, 196, 230))]


def mochi():
    for w, (top, bot) in enumerate(MOCHI_COLS):
        for blink in (0, 1):
            s = Sprite(110, 90, pivot=(55, 80))
            c = s.c
            c.translate(55, 80)
            body = smooth([(-34, 0), (-35, -14), (-26, -38), (0, -48), (26, -38), (35, -14), (34, 0), (0, 2)])
            c.drawPath(ellipse(0, 2, 34, 5), paint((0, 0, 0), alpha=0.18))
            outlined(c, body, top, INK, LW, shader=lin(0, -48, 0, 2, [shade(top, 0.3), top, bot]))
            cel(c, body, ellipse(-14, -32, 11, 6), (255, 255, 255), 0.85)
            if w == 0:   # a sakura petal stuck on top
                c.drawPath(ellipse(10, -46, 6, 3.5), paint((255, 150, 190)))
            if w == 1:   # a matcha leaf
                c.drawPath(smooth([(6, -46), (16, -54), (20, -48), (12, -44)]), paint((90, 160, 80)))
            if w == 2:   # a dusting of snow
                c.drawPath(smooth([(-18, -40), (-6, -48), (12, -46), (22, -38), (8, -40), (-6, -38)]), paint((255, 255, 255)))
            if blink:
                for ex in (-11, 9):
                    c.drawPath(poly([(ex - 4, -22), (ex + 4, -22)], closed=False), paint(INK, stroke=2.4))
            else:
                for ex in (-11, 9):
                    c.drawPath(ellipse(ex, -23, 3.6, 5), paint(INK))
                    c.drawPath(circle(ex + 1, -25, 1.4), paint((255, 255, 255)))
            c.drawPath(smooth([(-4, -14), (-1, -11.5), (2, -14)], closed=False), paint(INK, stroke=2))
            c.drawPath(ellipse(-19, -15, 5, 3), paint((255, 130, 150), alpha=0.6))
            c.drawPath(ellipse(17, -15, 5, 3), paint((255, 130, 150), alpha=0.6))
            atlas.add("IMG_MOCHI%d_%d" % (w, blink), s)
        # squashed flat
        s = Sprite(120, 50, pivot=(60, 40))
        c = s.c
        c.translate(60, 40)
        flat = smooth([(-42, 0), (-38, -10), (0, -16), (38, -10), (42, 0), (0, 2)])
        outlined(c, flat, top, INK, LW, shader=lin(0, -16, 0, 2, [top, bot]))
        for ex in (-12, 10):
            c.drawPath(poly([(ex - 4, -10), (ex + 4, -6), (ex - 4, -4)], closed=False), paint(INK, stroke=2))
        atlas.add("IMG_MOCHI%d_FLAT" % w, s)


def crow():
    body_c = (54, 58, 92)
    for flap in range(3):
        pass
    s = Sprite(120, 90, pivot=(60, 50))
    c = s.c
    c.translate(60, 50)
    tail = poly([(-26, -4), (-46, -14), (-48, -2), (-44, 8), (-24, 6)])
    outlined(c, tail, shade(body_c, -0.2), INK, LW)
    body = smooth([(-30, 2), (-18, -14), (4, -18), (20, -16), (30, -6), (24, 8), (4, 14), (-18, 12)])
    outlined(c, body, body_c, INK, LW, shader=lin(0, -18, 0, 14, [shade(body_c, 0.25), body_c, shade(body_c, -0.3)]))
    cel(c, body, ellipse(6, -12, 14, 4), (130, 140, 200), 0.6)
    beak = poly([(26, -10), (42, -4), (26, 0)])
    outlined(c, beak, GOLD, INK, 2)
    c.drawPath(poly([(26, -4.5), (40, -4.5)], closed=False), paint(GOLD_SH, stroke=1.4))
    c.drawPath(circle(18, -9, 5.2), paint((255, 255, 255)))
    c.drawPath(circle(19.5, -9, 2.8), paint(INK))
    c.drawPath(poly([(12, -16), (24, -13)], closed=False), paint(INK, stroke=2.4))   # a cross brow
    for fx in (-4, 4):
        c.drawPath(poly([(fx, 13), (fx - 2, 22), (fx + 3, 22)], closed=False), paint(GOLD_SH, stroke=2.2))
    atlas.add("IMG_CROW", s)
    s = Sprite(110, 80, pivot=(30, 20))     # the wing, pivot at the shoulder; the game flaps it
    c = s.c
    c.translate(30, 20)
    wing = smooth([(-6, -4), (14, -10), (40, -6), (60, 4), (44, 6), (50, 12), (32, 12), (34, 18), (14, 12), (-4, 6)])
    outlined(c, wing, shade(body_c, 0.15), INK, LW)
    cel(c, wing, smooth([(0, 4), (30, 2), (50, 10), (20, 14)]), shade(body_c, -0.3), 0.8)
    atlas.add("IMG_CROW_WING", s)


def chestnut():
    shell = (150, 92, 54)
    s = Sprite(110, 110, pivot=(55, 55))
    c = s.c
    c.translate(55, 55)
    spikes = []
    rnd = random.Random(7)
    for i in range(22):
        a = i / 22 * math.tau
        r1 = 37 + rnd.uniform(-1, 3)
        spikes.append((math.cos(a) * r1, math.sin(a) * r1))
        a2 = (i + 0.5) / 22 * math.tau
        spikes.append((math.cos(a2) * 26, math.sin(a2) * 26))
    sp = poly(spikes)
    outlined(c, sp, (120, 160, 70), INK, 2.2, shader=rad(-8, -10, 40, [(176, 210, 104), (100, 140, 60)]))
    ball = circle(0, 0, 27)
    outlined(c, ball, shell, INK, LW, shader=rad(-8, -10, 34, [(206, 140, 84), shell, (100, 58, 36)]))
    cel(c, ball, ellipse(-9, -14, 10, 5), (255, 220, 170), 0.6)
    atlas.add("IMG_CHESTNUT", s)
    for angry in (0, 1):
        s = Sprite(60, 40, pivot=(30, 20))
        c = s.c
        c.translate(30, 20)
        for ex in (-8, 8):
            c.drawPath(ellipse(ex, 0, 5, 6), paint((255, 255, 255)))
            c.drawPath(ellipse(ex + 1.5, 1, 2.8, 3.6), paint(INK))
            c.drawPath(ellipse(ex, 0, 5, 6), paint(INK, stroke=1.6))
        if angry:
            c.drawPath(poly([(-14, -9), (-3, -5)], closed=False), paint(INK, stroke=3))
            c.drawPath(poly([(14, -9), (3, -5)], closed=False), paint(INK, stroke=3))
        c.drawPath(smooth([(-5, 11), (0, 9), (5, 11)], closed=False), paint(INK, stroke=2))
        atlas.add("IMG_CHESTNUT_FACE%d" % angry, s)


# ================================================================ worlds: colours and ground tiles
WORLDS = [
    dict(name="blossom", soil=(196, 128, 86), soil2=(150, 88, 64), deep=(112, 62, 52), grass=(126, 204, 92), grass2=(72, 160, 80),
         speck=(255, 168, 200), stone=(172, 164, 180)),
    dict(name="bamboo", soil=(124, 98, 78), soil2=(92, 70, 62), deep=(66, 48, 52), grass=(92, 184, 118), grass2=(44, 128, 96),
         speck=(255, 236, 140), stone=(140, 150, 150)),
    dict(name="peaks", soil=(132, 142, 168), soil2=(98, 106, 136), deep=(72, 76, 108), grass=(250, 252, 255), grass2=(190, 210, 238),
         speck=(255, 255, 255), stone=(150, 160, 184)),
]
OV = 12   # how far a tile can draw past its square


def ground_tile(w, mask, variant):
    W = WORLDS[w]
    up, left, right, down = mask & 1, mask & 2, mask & 4, mask & 8
    s = Sprite(TS + OV * 2, TS + OV * 2, pivot=(OV, OV), trim=False)
    c = s.c
    c.translate(OV, OV)
    rnd = random.Random(w * 100 + mask * 7 + variant * 1000)
    R = 16
    # the solid shape: rounded where two open sides meet, pushed past every closed side
    x0 = 0 if left else -OV
    x1 = TS if right else TS + OV
    y0 = 0 if up else -OV
    y1 = TS if down else TS + OV
    shape = skia.Path()
    rr = skia.RRect()
    radii = [skia.Point(R, R) if (up and left) else skia.Point(0, 0),
             skia.Point(R, R) if (up and right) else skia.Point(0, 0),
             skia.Point(R, R) if (down and right) else skia.Point(0, 0),
             skia.Point(R, R) if (down and left) else skia.Point(0, 0)]
    rr.setRectRadii(skia.Rect.MakeLTRB(x0, y0, x1, y1), radii)
    shape.addRRect(rr)
    clip = skia.Rect.MakeLTRB(-OV if left else 0, -OV if up else 0, TS + OV if right else TS, TS + OV if down else TS)
    c.save()
    c.clipRect(clip)
    c.drawPath(shape, paint(mix(W["deep"], INK, 0.4), stroke=LW * 2))
    top_c = W["soil"] if up else W["soil2"]
    bot_c = W["deep"] if down else W["soil2"]
    c.drawPath(shape, paint(shader=lin(0, 0, 0, TS, [top_c, W["soil2"], bot_c], [0, 0.55 if up else 0.5, 1])))
    # texture: pebbles and strata, kept inside the shape
    c.save()
    c.clipPath(shape, skia.ClipOp.kIntersect, True)
    for _ in range(5 + variant * 2):
        px, py = rnd.uniform(4, TS - 4), rnd.uniform(18 if up else 2, TS - 4)
        r = rnd.uniform(2.5, 6)
        c.drawPath(ellipse(px, py, r * 1.4, r), paint(shade(W["soil2"], -0.25), alpha=0.7))
        c.drawPath(ellipse(px - r * 0.3, py - r * 0.3, r * 0.7, r * 0.45), paint(shade(W["soil"], 0.3), alpha=0.5))
    if variant == 1:   # a buried stone
        st = smooth([(14, 40), (30, 30), (50, 36), (52, 52), (32, 58), (16, 54)])
        c.drawPath(st, paint(W["stone"]))
        cel(c, st, ellipse(30, 36, 14, 5), shade(W["stone"], 0.4), 0.8)
        c.drawPath(st, paint(INK, stroke=1.6, alpha=0.6))
    if variant == 2:   # a root
        c.drawPath(smooth([(-4, 30), (20, 38), (38, 34), (60, 48), (76, 46)], closed=False), paint(shade(W["soil2"], -0.4), stroke=4))
    if left:
        c.drawRect(skia.Rect.MakeLTRB(0, 0, 8, TS + OV), paint(shade(W["soil"], 0.25), alpha=0.35))
    if right:
        c.drawRect(skia.Rect.MakeLTRB(TS - 10, 0, TS, TS + OV), paint(W["deep"], alpha=0.35))
    c.restore()
    c.drawPath(shape, paint(INK, stroke=LW, alpha=0.9))
    c.restore()
    # the grass (or snow) cap along an open top, overhanging a little at open ends
    if up:
        ext_l = -7 if left else -OV
        ext_r = TS + 7 if right else TS + OV
        pts = [(ext_l, 8)]
        n = 9
        for i in range(n + 1):
            x = ext_l + (ext_r - ext_l) * i / n
            pts.append((x, 14 + rnd.uniform(-2, 4) + (3 if i % 2 else 0)))
        pts.append((ext_r, 8))
        top = [(ext_r, 2)]
        for i in range(n, -1, -1):
            x = ext_l + (ext_r - ext_l) * i / n
            top.append((x, -1 + rnd.uniform(-1.5, 1.5)))
        cap = smooth(pts + top)
        c.save()
        c.clipRect(skia.Rect.MakeLTRB(-OV if left else 0, -OV, TS + OV if right else TS, TS))
        c.drawPath(cap, paint(INK, stroke=LW * 2))
        c.drawPath(cap, paint(shader=lin(0, -2, 0, 16, [shade(W["grass"], 0.25), W["grass"], W["grass2"]])))
        # blades / drifts poking up
        for i in range(7):
            bx = rnd.uniform(4, TS - 4)
            if w == 2:
                c.drawPath(ellipse(bx, 0, rnd.uniform(5, 9), 3), paint((255, 255, 255)))
            else:
                h = rnd.uniform(4, 8)
                blade = poly([(bx - 2.5, 2), (bx + rnd.uniform(-2, 2), -h), (bx + 2.5, 2)])
                c.drawPath(blade, paint(INK, stroke=1.8))
                c.drawPath(blade, paint(shade(W["grass"], 0.15)))
        for i in range(3):
            c.drawPath(circle(rnd.uniform(6, TS - 6), rnd.uniform(3, 9), 1.6), paint(W["speck"]))
        c.drawPath(smooth([(ext_l + 6, 3), (TS / 2, 1), (ext_r - 6, 3)], closed=False), paint(shade(W["grass"], 0.5), stroke=2, alpha=0.7))
        c.restore()
    return s


def tiles():
    for w in range(3):
        for m in range(16):
            atlas.add("IMG_G%d_%d" % (w, m), ground_tile(w, m, 0))
        atlas.add("IMG_G%d_S" % w, ground_tile(w, 0, 1))
        atlas.add("IMG_G%d_R" % w, ground_tile(w, 0, 2))
        atlas.table("GROUND%d" % w, ["IMG_G%d_%d" % (w, m) for m in range(16)] + ["IMG_G%d_S" % w, "IMG_G%d_R" % w])
    # stone block
    s = Sprite(TS + 8, TS + 8, pivot=(4, 4))
    c = s.c
    c.translate(4, 4)
    blk = rrect(1, 1, TS - 2, TS - 2, 8)
    outlined(c, blk, (160, 160, 178), INK, LW, shader=lin(0, 0, TS, TS, [(196, 196, 214), (150, 150, 172), (112, 110, 138)]))
    inner = rrect(10, 10, TS - 20, TS - 20, 5)
    c.drawPath(inner, paint((140, 140, 164)))
    c.drawPath(poly([(10, TS - 10), (10, 10), (TS - 10, 10)], closed=False), paint((116, 114, 140), stroke=2.5))
    c.drawPath(poly([(TS - 10, 10), (TS - 10, TS - 10), (10, TS - 10)], closed=False), paint((210, 210, 226), stroke=2.5))
    c.drawPath(poly([(18, 30), (30, 36), (28, 48)], closed=False), paint((110, 108, 134), stroke=1.6))
    atlas.add("IMG_BLOCK", s)
    # planks: a single, a left end, a middle and a right end
    for kind in ("ONE", "L", "M", "R"):
        s = Sprite(TS + 16, 44, pivot=(8, 6), trim=False)
        c = s.c
        c.translate(8, 6)
        x0 = 4 if kind in ("ONE", "L") else -2
        x1 = TS - 4 if kind in ("ONE", "R") else TS + 2
        board = rrect(x0, 0, x1 - x0, 18, 6 if kind != "M" else 0)
        if kind in ("ONE", "L", "R"):
            r = skia.RRect()
            rl = 6 if kind in ("ONE", "L") else 0
            rr_ = 6 if kind in ("ONE", "R") else 0
            r.setRectRadii(skia.Rect.MakeLTRB(x0, 0, x1, 18), [skia.Point(rl, rl), skia.Point(rr_, rr_), skia.Point(rr_, rr_), skia.Point(rl, rl)])
            board = skia.Path()
            board.addRRect(r)
        c.save()
        c.clipRect(skia.Rect.MakeLTRB(0 if kind in ("ONE", "L") else 0, -6, TS if kind in ("ONE", "R") else TS, 40))
        c.drawPath(board, paint(INK, stroke=LW * 2))
        c.drawPath(board, paint(shader=lin(0, 0, 0, 18, [(222, 166, 104), (186, 124, 74), (150, 92, 56)])))
        c.drawPath(poly([(x0, 6), (x1, 6)], closed=False), paint((156, 98, 60), stroke=1.4))
        c.drawPath(poly([(x0, 2.5), (x1, 2.5)], closed=False), paint((240, 196, 140), stroke=1.6))
        c.restore()
        for nx in (14, TS - 14):
            c.drawPath(circle(nx, 9, 2), paint((90, 60, 50)))
        if kind in ("ONE", "L", "R"):   # rope bindings at the ends
            ex = 10 if kind == "L" else TS - 10 if kind == "R" else None
            for bx in ([10, TS - 10] if kind == "ONE" else [ex]):
                c.drawPath(rrect(bx - 3, -2, 6, 22, 2), paint((230, 210, 160)))
                c.drawPath(rrect(bx - 3, -2, 6, 22, 2), paint(INK, stroke=1.4))
        atlas.add("IMG_PLANK_" + kind, s)
    # spikes: sharpened bamboo stakes on a stone footing
    s = Sprite(TS + 8, TS + 8, pivot=(4, 4))
    c = s.c
    c.translate(4, 4)
    for i, (x, h) in enumerate(((12, 44), (28, 54), (44, 46), (60, 52))):
        stake = poly([(x - 6.5, TS - 10), (x - 6.5, TS - h + 12), (x, TS - h), (x + 6.5, TS - h + 12), (x + 6.5, TS - 10)])
        outlined(c, stake, (200, 210, 120), INK, 2.2, shader=lin(x - 6, 0, x + 6, 0, [(226, 232, 150), (170, 186, 90)]))
        c.drawPath(poly([(x - 6.5, TS - h + 12), (x, TS - h), (x + 6.5, TS - h + 12)]), paint((246, 240, 214)))
        c.drawPath(poly([(x - 6.5, TS - h + 12), (x, TS - h), (x + 6.5, TS - h + 12)], closed=False), paint(INK, stroke=1.6))
        c.drawPath(poly([(x - 6, TS - 28), (x + 6, TS - 28)], closed=False), paint((140, 150, 70), stroke=1.6))
    base = rrect(0, TS - 14, TS, 14, 5)
    outlined(c, base, (150, 150, 168), INK, LW)
    atlas.add("IMG_SPIKES", s)
    # lucky boxes: red lacquer with a gold crest; used = plain dark wood
    for kind in ("BOX", "BOXHEART", "USED"):
        s = Sprite(TS + 8, TS + 8, pivot=(4, 4))
        c = s.c
        c.translate(4, 4)
        bx = rrect(1, 1, TS - 2, TS - 2, 10)
        if kind == "USED":
            outlined(c, bx, (120, 86, 70), INK, LW, shader=lin(0, 0, 0, TS, [(146, 108, 84), (100, 70, 60)]))
            c.drawPath(rrect(9, 9, TS - 18, TS - 18, 6), paint((90, 62, 54)))
            for p in ((10, 10), (TS - 10, 10), (10, TS - 10), (TS - 10, TS - 10)):
                c.drawPath(circle(*p, 3), paint((70, 50, 46)))
        else:
            outlined(c, bx, RED, INK, LW, shader=lin(0, 0, 0, TS, [(246, 92, 96), (204, 44, 70), (150, 26, 58)]))
            c.drawPath(rrect(7, 7, TS - 14, TS - 14, 7), paint(GOLD_SH, stroke=2.5))
            cel(c, bx, rrect(4, 4, TS - 8, 12, 6), (255, 160, 160), 0.5)
            for p in ((12, 12), (TS - 12, 12), (12, TS - 12), (TS - 12, TS - 12)):
                c.drawPath(circle(*p, 3.2), paint(GOLD))
            if kind == "BOX":   # a gold mon with a swirl
                c.drawPath(circle(TS / 2, TS / 2, 15), paint(GOLD))
                c.drawPath(circle(TS / 2, TS / 2, 15), paint(GOLD_SH, stroke=2))
                c.drawPath(smooth([(TS / 2 - 7, TS / 2 + 3), (TS / 2 - 4, TS / 2 - 7), (TS / 2 + 5, TS / 2 - 6), (TS / 2 + 6, TS / 2 + 3), (TS / 2, TS / 2 + 5), (TS / 2 - 1, TS / 2)], closed=False),
                           paint(GOLD_SH, stroke=3))
            else:
                ht = heart_path(TS / 2, TS / 2 + 1, 15)
                c.drawPath(ht, paint((255, 238, 240)))
                c.drawPath(ht, paint(GOLD_SH, stroke=2))
        atlas.add("IMG_" + kind, s)


def heart_path(cx, cy, r):
    return smooth([(cx, cy + r), (cx - r * 1.05, cy - r * 0.05), (cx - r * 0.85, cy - r * 0.85), (cx - r * 0.3, cy - r * 0.95), (cx, cy - r * 0.5),
                   (cx + r * 0.3, cy - r * 0.95), (cx + r * 0.85, cy - r * 0.85), (cx + r * 1.05, cy - r * 0.05)], tension=0.45)


# ================================================================ pickups and HUD
def pickups():
    s = Sprite(60, 60, pivot=(30, 30))   # a gold mon coin with a square hole
    c = s.c
    c.translate(30, 30)
    coin = circle(0, 0, 21)
    hole = rrect(-6, -6, 12, 12, 1.5)
    body = minus(coin, hole)
    outlined(c, body, GOLD, (120, 70, 20), 2.4, shader=rad(-7, -8, 28, [(255, 244, 170), GOLD, GOLD_SH]))
    c.drawPath(circle(0, 0, 15.5), paint(GOLD_SH, stroke=1.6))
    for a in range(4):
        ang = a * math.pi / 2 + math.pi / 4
        c.drawPath(circle(math.cos(ang) * 11, math.sin(ang) * 11, 1.8), paint(GOLD_SH))
    cel(c, body, ellipse(-8, -12, 7, 3.5), (255, 255, 230), 0.8)
    atlas.add("IMG_COIN", s)
    s = Sprite(90, 70, pivot=(45, 35))   # the secret scroll
    c = s.c
    c.translate(45, 35)
    paper = rrect(-26, -15, 52, 30, 3)
    outlined(c, paper, (250, 238, 210), INK, LW, shader=lin(0, -15, 0, 15, [(255, 248, 228), (232, 210, 170)]))
    for y in (-7, -1, 5):
        c.drawPath(poly([(-17, y), (17, y)], closed=False), paint((150, 120, 100), stroke=1.4, alpha=0.6))
    for x in (-30, 30):
        rod = rrect(x - 5, -20, 10, 40, 4)
        outlined(c, rod, (170, 40, 60), INK, 2.2, shader=lin(x - 5, 0, x + 5, 0, [(220, 70, 90), (140, 30, 50)]))
        c.drawPath(rrect(x - 3, -24, 6, 6, 2), paint(GOLD))
        c.drawPath(rrect(x - 3, 18, 6, 6, 2), paint(GOLD))
    c.drawPath(smooth([(-4, 14), (0, 26), (4, 20), (6, 30)], closed=False), paint((226, 58, 78), stroke=2.4))
    atlas.add("IMG_SCROLL", s)
    for full in (1, 0):
        s = Sprite(50, 50, pivot=(25, 25))
        c = s.c
        c.translate(25, 25)
        ht = heart_path(0, 1, 17)
        if full:
            outlined(c, ht, (236, 60, 86), INK, 2.6, shader=rad(-6, -8, 24, [(255, 130, 140), (236, 60, 86), (180, 30, 70)]))
            c.drawPath(ellipse(-7, -7, 4.5, 3), paint((255, 230, 230), alpha=0.9))
        else:
            outlined(c, ht, (70, 60, 100), INK, 2.6)
        atlas.add("IMG_HEART" if full else "IMG_HEART_EMPTY", s)
    # a sparkle and a petal, a leaf and a firefly for particles and weather
    s = Sprite(40, 40, pivot=(20, 20))
    s.c.translate(20, 20)
    s.c.drawPath(hd.star(0, 0, 18, 3.6, 4), paint((255, 255, 255)))
    atlas.add("IMG_SPARK", s)
    s = Sprite(24, 24, pivot=(12, 12))
    s.c.translate(12, 12)
    pet = smooth([(0, -7), (5, -2), (3, 6), (0, 4), (-3, 6), (-5, -2)])
    s.c.drawPath(pet, paint(shader=lin(0, -7, 0, 6, [(255, 214, 230), (255, 150, 190)])))
    atlas.add("IMG_PETAL", s)
    s = Sprite(30, 24, pivot=(15, 12))
    s.c.translate(15, 12)
    leaf = smooth([(-11, 0), (0, -5), (11, 0), (0, 5)])
    s.c.drawPath(leaf, paint((120, 190, 90)))
    s.c.drawPath(poly([(-10, 0), (10, 0)], closed=False), paint((70, 140, 70), stroke=1.2))
    atlas.add("IMG_LEAF", s)
    s = Sprite(40, 40, pivot=(20, 20))
    s.c.translate(20, 20)
    s.c.drawPath(circle(0, 0, 9), paint((255, 255, 255), blur=4))
    s.c.drawPath(circle(0, 0, 5), paint((255, 255, 255)))
    atlas.add("IMG_DOT", s)


# ================================================================ scenery
def tree_sakura():
    s = Sprite(340, 380, pivot=(170, 370))
    c = s.c
    c.translate(170, 370)
    trunk = smooth([(-16, 0), (-10, -80), (-24, -150), (-40, -190), (-26, -194), (-4, -160), (6, -200), (18, -196), (12, -150), (36, -190),
                    (48, -184), (20, -120), (14, -60), (20, 0)])
    outlined(c, trunk, (112, 72, 76), INK, LW, shader=lin(-20, 0, 20, 0, [(140, 96, 96), (96, 60, 66)]))
    rnd = random.Random(3)
    blobs = [(-80, -210, 62), (0, -260, 78), (82, -214, 64), (-40, -290, 58), (52, -294, 60), (-110, -250, 44), (118, -256, 46), (0, -190, 54)]
    canopy = union(*[circle(x, y, r) for x, y, r in blobs])
    outlined(c, canopy, (255, 180, 206), INK, LW, shader=rad(-20, -300, 220, [(255, 226, 236), (255, 178, 206), (232, 128, 170)]))
    for x, y, r in blobs:   # puffs of lighter blossom
        cel(c, canopy, circle(x - r * 0.25, y - r * 0.3, r * 0.6), (255, 226, 236), 0.6)
    for _ in range(70):
        a, d = rnd.uniform(0, math.tau), rnd.uniform(0, 1)
        x, y = rnd.uniform(-150, 150), rnd.uniform(-330, -170)
        if canopy.contains(x, y):
            c.drawPath(circle(x, y, rnd.uniform(2, 4)), paint(rnd.choice([(255, 250, 252), (236, 120, 160), (255, 210, 226)])))
    cel(c, canopy, ellipse(30, -170, 160, 40), (220, 110, 160), 0.45)
    atlas.add("IMG_SAKURA", s)


def tree_bamboo():
    s = Sprite(260, 560, pivot=(130, 550))
    c = s.c
    c.translate(130, 550)
    rnd = random.Random(5)
    for (x, h, w, tone) in ((-60, 470, 15, 0.0), (40, 530, 17, 0.1), (-10, 500, 18, 0.2), (80, 420, 13, -0.1)):
        stalk = rrect(x - w / 2, -h, w, h, w / 2)
        base = shade((110, 190, 110), tone)
        outlined(c, stalk, base, INK, 2.4, shader=lin(x - w / 2, 0, x + w / 2, 0, [shade(base, 0.35), base, shade(base, -0.3)]))
        y = -30
        while y > -h + 20:
            c.drawPath(poly([(x - w / 2, y), (x + w / 2, y)], closed=False), paint(INK, stroke=2.2))
            c.drawPath(poly([(x - w / 2 + 1, y - 3), (x + w / 2 - 1, y - 3)], closed=False), paint(shade(base, 0.45), stroke=2))
            if rnd.random() < 0.5:
                d = rnd.choice([-1, 1])
                for k in range(rnd.randint(2, 3)):
                    ln = rnd.uniform(30, 46)
                    ang = math.radians(rnd.uniform(15, 40))
                    tip = (x + d * math.cos(ang) * ln, y - 6 + math.sin(ang) * ln * 0.6 + k * 6)
                    lf = smooth([(x + d * w / 2, y - 6), ((x + tip[0]) / 2, tip[1] - 7), tip, ((x + tip[0]) / 2, tip[1] + 3)])
                    outlined(c, lf, (96, 176, 96), INK, 1.8, shader=lin(x, 0, tip[0], 0, [(120, 200, 110), (70, 140, 80)]))
            y -= rnd.uniform(70, 95)
    atlas.add("IMG_BAMBOO", s)


def tree_pine():
    s = Sprite(320, 440, pivot=(160, 430))
    c = s.c
    c.translate(160, 430)
    trunk = rrect(-12, -120, 24, 120, 6)
    outlined(c, trunk, (110, 80, 72), INK, LW)
    layers = [(-56, 150, 70), (-126, 124, 64), (-190, 100, 58), (-248, 74, 54), (-300, 48, 46)]
    for y, half, h in layers:
        tier = smooth([(-half, y), (-half * 0.6, y - h * 0.55), (0, y - h * 1.15), (half * 0.6, y - h * 0.55), (half, y), (0, y + 12)], tension=0.35)
        outlined(c, tier, (60, 120, 100), INK, LW, shader=lin(-half, 0, half, 0, [(86, 150, 120), (52, 102, 92), (40, 80, 80)]))
        snow = intersect(tier, smooth([(-half, y - 10), (-half * 0.4, y - h * 0.5), (0, y - h * 1.3), (half * 0.5, y - h * 0.6), (half * 0.9, y - 6), (0, y - h * 0.3)]))
        c.drawPath(snow, paint(shader=lin(0, y - h, 0, y, [(255, 255, 255), (214, 228, 248)])))
        c.drawPath(tier, paint(INK, stroke=LW, alpha=0.9))
    atlas.add("IMG_PINE", s)


def props():
    # stone lantern (toro)
    s = Sprite(110, 160, pivot=(55, 150))
    c = s.c
    c.translate(55, 150)
    stone = (178, 176, 186)
    sh = lambda path: outlined(c, path, stone, INK, LW, shader=lin(-30, 0, 30, 0, [shade(stone, 0.3), stone, shade(stone, -0.3)]))
    sh(rrect(-26, -16, 52, 16, 4))
    sh(rrect(-9, -62, 18, 48, 3))
    sh(rrect(-24, -76, 48, 16, 4))
    sh(rrect(-18, -110, 36, 36, 4))
    c.drawPath(rrect(-10, -102, 20, 20, 2), paint((60, 50, 70)))
    roof = smooth([(-40, -110), (-24, -128), (0, -136), (24, -128), (40, -110), (0, -116)], tension=0.4)
    sh(roof)
    sh(circle(0, -140, 7))
    atlas.add("IMG_TORO", s)
    # bush, flowers, rock
    s = Sprite(170, 100, pivot=(85, 92))
    c = s.c
    c.translate(85, 92)
    bush = union(circle(-40, -26, 30), circle(0, -44, 40), circle(42, -28, 30), rrect(-70, -30, 140, 30, 8))
    outlined(c, bush, (96, 176, 96), INK, LW, shader=lin(0, -84, 0, 0, [(150, 214, 120), (90, 170, 96), (52, 120, 80)]))
    for x, y, r in ((-44, -36, 14), (-2, -60, 18), (40, -38, 14)):
        cel(c, bush, circle(x, y, r), (180, 230, 140), 0.6)
    atlas.add("IMG_BUSH", s)
    s = Sprite(130, 70, pivot=(65, 64))
    c = s.c
    c.translate(65, 64)
    rnd = random.Random(11)
    for i in range(7):
        x = -50 + i * 16 + rnd.uniform(-4, 4)
        h = rnd.uniform(20, 40)
        c.drawPath(smooth([(x, 0), (x + rnd.uniform(-5, 5), -h / 2), (x + rnd.uniform(-4, 4), -h)], closed=False), paint((70, 140, 80), stroke=2.4))
        col = rnd.choice([(255, 236, 120), (255, 160, 200), (255, 255, 255), (180, 160, 255)])
        fl = hd.star(x, -h, 8, 4.5, 5)
        outlined(c, fl, col, INK, 1.6)
        c.drawPath(circle(x, -h, 2.4), paint((255, 190, 60)))
    atlas.add("IMG_FLOWERS", s)
    s = Sprite(130, 90, pivot=(65, 84))
    c = s.c
    c.translate(65, 84)
    rock = smooth([(-48, 0), (-50, -20), (-30, -44), (4, -52), (36, -40), (50, -14), (48, 0)])
    outlined(c, rock, (150, 150, 168), INK, LW, shader=lin(-40, -50, 40, 0, [(200, 200, 216), (140, 140, 160), (100, 98, 124)]))
    c.drawPath(smooth([(-10, -40), (6, -28), (2, -14)], closed=False), paint((110, 108, 134), stroke=2))
    atlas.add("IMG_ROCK", s)
    # checkpoint: a paper lantern on a post
    for lit in (0, 1):
        s = Sprite(110, 200, pivot=(55, 190))
        c = s.c
        c.translate(55, 190)
        post = rrect(-5, -150, 10, 150, 3)
        outlined(c, post, (120, 82, 60), INK, LW)
        arm = rrect(-5, -152, 34, 8, 3)
        outlined(c, arm, (120, 82, 60), INK, LW)
        c.drawPath(poly([(24, -146), (24, -134)], closed=False), paint(INK, stroke=2))
        body = ellipse(24, -104, 22, 30)
        paper = (255, 168, 80) if lit else (226, 220, 210)
        paper2 = (255, 220, 140) if lit else (200, 192, 186)
        outlined(c, body, paper, INK, LW, shader=rad(20, -110, 34, [paper2, paper, shade(paper, -0.3)]))
        for k in range(-2, 3):
            c.drawPath(ellipse(24, -104, 22 * math.cos(k * 0.45), 30), paint(shade(paper, -0.25), stroke=1.4))
        c.drawPath(rrect(12, -138, 24, 8, 2), paint((60, 44, 50)))
        c.drawPath(rrect(12, -78, 24, 8, 2), paint((60, 44, 50)))
        c.drawPath(poly([(24, -70), (24, -58)], closed=False), paint((226, 58, 78), stroke=3))
        if lit:
            c.drawPath(circle(24, -104, 7), paint((255, 250, 220)))
        atlas.add("IMG_LANTERN%d" % lit, s)
    # the goal: a torii gate
    s = Sprite(400, 440, pivot=(200, 430))
    c = s.c
    c.translate(200, 430)
    red = (232, 70, 54)
    red_sh = (170, 40, 44)
    for x in (-118, 118):
        pillar = smooth([(x - 13, 0), (x - 11, -330), (x + 11, -330), (x + 13, 0)], tension=0.1)
        outlined(c, pillar, red, INK, LW, shader=lin(x - 13, 0, x + 13, 0, [shade(red, 0.3), red, red_sh]))
        foot = rrect(x - 18, -24, 36, 24, 4)
        outlined(c, foot, (50, 40, 50), INK, LW)
    nuki = rrect(-160, -270, 320, 22, 4)
    outlined(c, nuki, red, INK, LW, shader=lin(0, -270, 0, -248, [shade(red, 0.3), red_sh]))
    tie = rrect(-14, -330, 28, 62, 3)
    outlined(c, tie, red, INK, LW)
    c.drawPath(rrect(-9, -322, 18, 44, 2), paint((40, 34, 52)))
    c.drawPath(rrect(-6, -318, 12, 36, 2), paint(GOLD, stroke=1.6))
    shimaki = rrect(-176, -346, 352, 22, 5)
    outlined(c, shimaki, red, INK, LW)
    kasagi = smooth([(-200, -362), (-170, -372), (0, -380), (170, -372), (200, -362), (196, -350), (0, -360), (-196, -350)], tension=0.3)
    outlined(c, kasagi, (44, 36, 56), INK, LW, shader=lin(0, -380, 0, -350, [(84, 74, 100), (34, 28, 46)]))
    atlas.add("IMG_TORII", s)


# ================================================================ backgrounds (one texture each, 1x density)
SKIES = [
    dict(top=(110, 178, 244), bot=(255, 214, 196), far=(150, 170, 220), far2=(196, 196, 230), mid=(120, 196, 140), mid2=(88, 160, 120)),
    dict(top=(70, 60, 140), bot=(255, 150, 120), far=(110, 90, 150), far2=(170, 120, 160), mid=(54, 110, 96), mid2=(34, 80, 80)),
    dict(top=(118, 150, 200), bot=(226, 236, 250), far=(150, 166, 204), far2=(204, 214, 236), mid=(96, 122, 150), mid2=(70, 92, 124)),
]
BG_W = 2048


def wrap_noise(rnd, n):
    """Smooth periodic noise: a sum of sines with whole-number frequencies, so the layer tiles."""
    terms = [(k, rnd.uniform(0, math.tau), rnd.uniform(0.4, 1.0) / k) for k in (1, 2, 3, 5, 7, 11)]
    return lambda x: sum(a * math.sin(k * x / BG_W * math.tau + ph) for k, ph, a in terms)


def backgrounds():
    for w in range(3):
        S = SKIES[w]
        rnd = random.Random(40 + w)
        # far mountains: big peaks, atmospheric, with snow
        s = Sprite(BG_W, 560, density=1, trim=False)
        c = s.c
        f1 = wrap_noise(rnd, 6)
        for layer, (colr, base, amp, peaks) in enumerate(((S["far2"], 300, 140, 5), (S["far"], 360, 170, 7))):
            pts = [(0, 560)]
            for i in range(0, BG_W + 1, 8):
                x = i
                # sharp peaks from a periodic triangle wave plus noise
                tri = 1 - abs(((x / BG_W * peaks) % 1) * 2 - 1)
                y = base - amp * (tri ** 1.6) - 40 * f1(x * (layer + 1))
                pts.append((x, y))
            pts.append((BG_W, 560))
            path = poly(pts)
            top_c = mix(colr, S["bot"], 0.25 if layer == 0 else 0.0)
            c.drawPath(path, paint(shader=lin(0, base - amp, 0, 560, [top_c, mix(colr, S["bot"], 0.55)])))
            # snow caps on the higher peaks
            snow = intersect(path, poly([(0, 0), (BG_W, 0), (BG_W, base - amp * 0.62), (0, base - amp * 0.62)]))
            c.drawPath(snow, paint(mix((255, 255, 255), colr, 0.25 if w != 1 else 0.5), alpha=0.85))
        if w == 0:   # a far pagoda on a ridge
            px, py = 1500, 300
            for k in range(4):
                c.drawPath(rrect(px - 26 + k * 4, py - k * 30 - 18, 52 - k * 8, 18, 2), paint(mix(S["far"], (90, 60, 90), 0.4)))
                c.drawPath(poly([(px - 44 + k * 6, py - k * 30 - 18), (px, py - k * 30 - 34), (px + 44 - k * 6, py - k * 30 - 18)]), paint(mix(S["far"], (70, 50, 80), 0.5)))
        hd.Sprite.save_png  # (kept for reference)
        atlas.add_big("IMG_BG_FAR%d" % w, s, "bg_far%d.png" % w)
        # mid hills with tree silhouettes
        s = Sprite(BG_W, 460, density=1, trim=False)
        c = s.c
        f2 = wrap_noise(rnd, 6)
        pts = [(0, 460)]
        for i in range(0, BG_W + 1, 8):
            pts.append((i, 210 - 70 * f2(i)))
        pts.append((BG_W, 460))
        hill = poly(pts)
        c.drawPath(hill, paint(shader=lin(0, 140, 0, 460, [S["mid"], S["mid2"]])))
        for _ in range(60):
            x = rnd.uniform(0, BG_W)
            y = 210 - 70 * f2(x) + 6
            for xx in (x, x - BG_W, x + BG_W):
                if w == 0:   # round sakura silhouettes
                    col_t = mix((236, 150, 186), S["mid"], rnd.uniform(0.2, 0.55))
                    r = rnd.uniform(18, 34)
                    c.drawPath(rrect(xx - 3, y - r - 4, 6, r + 6, 2), paint(mix((110, 70, 80), S["mid"], 0.5)))
                    c.drawPath(union(circle(xx, y - r - 6, r), circle(xx - r * 0.7, y - r * 0.8, r * 0.7), circle(xx + r * 0.7, y - r * 0.8, r * 0.7)), paint(col_t))
                elif w == 1:   # bamboo stalks
                    h = rnd.uniform(90, 200)
                    c.drawPath(rrect(xx - 4, y - h, 8, h, 4), paint(mix((40, 100, 80), S["mid"], rnd.uniform(0, 0.4))))
                    c.drawPath(smooth([(xx, y - h + 10), (xx + 20, y - h - 4), (xx + 34, y - h + 4)], closed=False), paint(mix((40, 100, 80), S["mid"], 0.3), stroke=5))
                else:   # snowy pines
                    h = rnd.uniform(50, 100)
                    pine = poly([(xx - h * 0.32, y), (xx, y - h), (xx + h * 0.32, y)])
                    c.drawPath(pine, paint(mix((46, 80, 90), S["mid"], rnd.uniform(0.1, 0.4))))
                    c.drawPath(intersect(pine, poly([(xx - h, y - h * 0.55), (xx + h, y - h * 0.7), (xx + h, y - h * 1.2), (xx - h, y - h * 1.2)])), paint((236, 244, 255), alpha=0.9))
        atlas.add_big("IMG_BG_MID%d" % w, s, "bg_mid%d.png" % w)
    # clouds
    for i in range(3):
        rnd = random.Random(90 + i)
        s = Sprite(420, 170, pivot=(210, 150))
        c = s.c
        c.translate(210, 150)
        blobs = [(rnd.uniform(-140, 140), rnd.uniform(-60, -20), rnd.uniform(36, 66)) for _ in range(7)]
        cloud = union(*[circle(x, y, r) for x, y, r in blobs], rrect(-170, -40, 340, 40, 20))
        c.drawPath(cloud, paint(shader=lin(0, -130, 0, 0, [(255, 255, 255), (232, 236, 252)])))
        cel(c, cloud, offset(cloud, 0, 18), (214, 220, 244), 0.6)
        atlas.add("IMG_CLOUD%d" % i, s)
    atlas.table("CLOUDS", ["IMG_CLOUD%d" % i for i in range(3)])


# ================================================================ the logo
def logo():
    tf = skia.Typeface.MakeFromFile(os.path.join(ROOT, "tools/font/Fredoka.ttf"))
    C = skia.FontArguments.VariationPosition.Coordinate
    fa = skia.FontArguments()
    fa.setVariationDesignPosition(skia.FontArguments.VariationPosition(skia.FontArguments.VariationPosition.Coordinates([C(0x77676874, 700.0)])))
    font = skia.Font(tf.makeClone(fa), 150)
    s = Sprite(1100, 300, pivot=(550, 150))
    c = s.c
    c.translate(550, 150)
    # a mountain behind the words
    mt = smooth([(-230, 60), (-90, -110), (-40, -70), (20, -138), (110, -40), (240, 60)], tension=0.25)
    c.drawPath(mt, paint(shader=lin(0, -138, 0, 60, [(255, 255, 255), (150, 190, 255), (90, 120, 220)])))
    c.drawPath(mt, paint(INK, stroke=8))
    blob = skia.TextBlob.MakeFromString("PIXEL PEAKS", font)
    wdt = font.measureText("PIXEL PEAKS")
    x, y = -wdt / 2, 60
    c.drawTextBlob(blob, x + 8, y + 12, paint((30, 16, 50), alpha=0.5))
    c.drawTextBlob(blob, x, y, paint((44, 22, 70), stroke=26))
    c.drawTextBlob(blob, x, y, paint(shader=lin(0, y - 110, 0, y, [(255, 246, 170), (255, 196, 70), (246, 110, 70)])))
    c.drawTextBlob(blob, x, y - 4, paint((255, 255, 255), stroke=2.5, alpha=0.5))
    atlas.add("IMG_LOGO", s)


def main():
    hero_parts()
    mochi()
    crow()
    chestnut()
    tiles()
    pickups()
    tree_sakura()
    tree_bamboo()
    tree_pine()
    props()
    backgrounds()
    logo()
    out = os.path.join(ROOT, "assets", "pixelpeaks")
    atlas.write(out, os.path.join(ROOT, "games", "PixelPeaks", "art.h"), "ppart", "pixelpeaks")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "pp_sheet.png"))
        print("preview in", d)


if __name__ == "__main__":
    main()
