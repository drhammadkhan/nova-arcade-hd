#!/usr/bin/env python3
"""Shared art for every Nova Arcade HD game: glossy blocks, bricks and orbs drawn in white (games tint
them), particle shapes, and a title logo for each game.
    python3 tools/art/common_art.py [--preview DIR]      (from the repo root)
Writes assets/common/*.png and engine/common_art.h (namespace cart)."""
import os
import sys
import math
import skia

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import hd  # noqa: E402
from hd import Sprite, paint, lin, rad, rrect, circle, ellipse, poly, smooth, star, intersect, offset  # noqa: E402

atlas = hd.Atlas(4096)
WHITE = (255, 255, 255)


def block():
    """A glossy bevelled square, 64 px, white: tinted per piece colour."""
    S = 64
    s = Sprite(S, S, pivot=(0, 0), trim=False)
    c = s.c
    body = rrect(1, 1, S - 2, S - 2, 9)
    c.drawPath(body, paint(shader=lin(0, 0, S, S, [(255, 255, 255), (214, 214, 222), (150, 150, 170)])))
    inner = rrect(8, 8, S - 16, S - 16, 6)
    c.drawPath(inner, paint(shader=lin(0, 8, 0, S - 8, [(250, 250, 255), (200, 200, 214)])))
    c.drawPath(poly([(1, 1), (S - 1, 1), (S - 8, 8), (8, 8)]), paint((255, 255, 255), alpha=0.6))
    c.drawPath(poly([(S - 1, 1), (S - 1, S - 1), (S - 8, S - 8), (S - 8, 8)]), paint((120, 120, 140), alpha=0.35))
    c.drawPath(poly([(1, S - 1), (S - 1, S - 1), (S - 8, S - 8), (8, S - 8)]), paint((90, 90, 110), alpha=0.45))
    c.drawPath(ellipse(20, 17, 9, 4.5), paint((255, 255, 255), alpha=0.8))
    c.drawPath(body, paint((40, 40, 60), stroke=1.6, alpha=0.6))
    atlas.add("IMG_BLOCK", s)


def brick():
    """A glossy rounded brick, 120 x 44, white."""
    W, H = 120, 44
    s = Sprite(W, H, pivot=(0, 0), trim=False)
    c = s.c
    body = rrect(1, 1, W - 2, H - 2, 12)
    c.drawPath(body, paint(shader=lin(0, 0, 0, H, [(255, 255, 255), (220, 220, 230), (160, 160, 178)])))
    c.drawPath(rrect(6, 4, W - 12, H * 0.38, 9), paint((255, 255, 255), alpha=0.75))
    c.drawPath(rrect(6, H - 9, W - 12, 5, 3), paint((110, 110, 130), alpha=0.35))
    c.drawPath(body, paint((40, 40, 60), stroke=1.6, alpha=0.6))
    atlas.add("IMG_BRICK", s)


def orb():
    """A glossy sphere, 64 px, white, pivot in the middle."""
    s = Sprite(64, 64, pivot=(32, 32))
    c = s.c
    c.translate(32, 32)
    c.drawPath(circle(0, 0, 30), paint(shader=rad(-9, -11, 44, [(255, 255, 255), (214, 214, 226), (130, 130, 150)])))
    c.drawPath(ellipse(-9, -13, 11, 6.5), paint((255, 255, 255), alpha=0.85))
    c.drawPath(circle(0, 0, 30), paint((40, 40, 60), stroke=1.5, alpha=0.5))
    atlas.add("IMG_ORB", s)


def fx():
    s = Sprite(64, 64, pivot=(32, 32))
    s.c.translate(32, 32)
    s.c.drawPath(star(0, 0, 30, 5, 4), paint(WHITE))
    s.c.drawPath(circle(0, 0, 6), paint(WHITE, blur=3))
    atlas.add("IMG_SPARK", s)
    s = Sprite(128, 128, pivot=(64, 64))
    s.c.translate(64, 64)
    s.c.drawPath(circle(0, 0, 54), paint(WHITE, stroke=6, blur=4))
    s.c.drawPath(circle(0, 0, 54), paint(WHITE, stroke=3))
    atlas.add("IMG_RING", s)
    s = Sprite(96, 96, pivot=(48, 48))   # a soft puff for smoke and dust
    s.c.translate(48, 48)
    for (x, y, r) in ((-10, 4, 24), (12, 0, 22), (0, -12, 22), (2, 10, 20)):
        s.c.drawPath(circle(x, y, r), paint(WHITE, alpha=0.55, blur=8))
    atlas.add("IMG_PUFF", s)
    s = Sprite(40, 12, pivot=(20, 6))    # a streak for shots and speed lines
    s.c.translate(20, 6)
    s.c.drawPath(rrect(-18, -3, 36, 6, 3), paint(WHITE, blur=2))
    s.c.drawPath(rrect(-14, -1.5, 28, 3, 1.5), paint(WHITE))
    atlas.add("IMG_STREAK", s)


# ---------------------------------------------------------------- logos
LOGOS = [
    # id, text, gradient (top -> bottom), outline, glow
    ("BLOCKFALL", "BLOCKFALL", [(140, 240, 255), (90, 140, 255), (190, 90, 255)], (30, 20, 70), (120, 160, 255)),
    ("BRICKSTORM", "BRICK STORM", [(255, 240, 150), (255, 150, 60), (240, 60, 90)], (60, 20, 40), (255, 140, 80)),
    ("NEONSERPENT", "NEON SERPENT", [(200, 255, 150), (60, 230, 140), (20, 170, 200)], (10, 40, 50), (80, 255, 170)),
    ("ASTRODRIFT", "ASTRO DRIFT", [(255, 255, 255), (150, 210, 255), (130, 120, 255)], (20, 20, 60), (140, 180, 255)),
    ("NOVALANCE", "NOVA LANCE", [(255, 250, 200), (255, 190, 80), (255, 80, 120)], (50, 15, 50), (255, 150, 90)),
    ("ALIENTIDE", "ALIEN TIDE", [(220, 255, 140), (90, 240, 120), (40, 180, 160)], (15, 40, 40), (120, 255, 140)),
    ("HOPRUSH", "HOP RUSH", [(200, 255, 140), (110, 220, 90), (40, 160, 90)], (20, 50, 30), (160, 255, 120)),
    ("MAZEMUNCH", "MAZE MUNCH", [(255, 250, 160), (255, 214, 60), (255, 140, 40)], (60, 30, 20), (255, 220, 90)),
    ("VOLTRALLY", "VOLT RALLY", [(255, 255, 170), (255, 220, 60), (60, 200, 255)], (20, 30, 60), (120, 220, 255)),
    ("GEMCASCADE", "GEM CASCADE", [(255, 200, 250), (220, 120, 255), (110, 110, 255)], (40, 20, 70), (230, 140, 255)),
    ("CITYSHIELD", "CITY SHIELD", [(200, 240, 255), (100, 190, 255), (60, 100, 230)], (15, 25, 60), (110, 190, 255)),
    ("TURBOHORIZON", "TURBO HORIZON", [(255, 240, 160), (255, 120, 90), (220, 60, 160)], (50, 15, 60), (255, 120, 140)),
]


def font(weight, size):
    tf = skia.Typeface.MakeFromFile(os.path.join(ROOT, "tools/font/Fredoka.ttf"))
    C = skia.FontArguments.VariationPosition.Coordinate
    fa = skia.FontArguments()
    fa.setVariationDesignPosition(skia.FontArguments.VariationPosition(skia.FontArguments.VariationPosition.Coordinates([C(0x77676874, float(weight))])))
    return skia.Font(tf.makeClone(fa), size)


def logos():
    f = font(700, 150)
    for gid, txt, grad, ink, glow in LOGOS:
        w = f.measureText(txt)
        W, H = int(w + 160), 260
        s = Sprite(W, H, pivot=(W / 2, H / 2), density=1)   # big and soft-edged: 1x keeps the textures small
        c = s.c
        x, y = (W - w) / 2, H / 2 + 54
        blob = skia.TextBlob.MakeFromString(txt, f)
        c.drawTextBlob(blob, x, y, paint(glow, stroke=30, blur=22, alpha=0.55))
        c.drawTextBlob(blob, x + 8, y + 12, paint((10, 6, 24), stroke=26, alpha=0.55))
        c.drawTextBlob(blob, x, y, paint(ink, stroke=26))
        c.drawTextBlob(blob, x, y, paint(shader=lin(0, y - 112, 0, y + 6, grad)))
        # a glassy highlight across the top half of the letters
        c.save()
        c.clipRect(skia.Rect.MakeLTRB(0, 0, W, y - 62))
        c.drawTextBlob(blob, x, y, paint((255, 255, 255), alpha=0.28))
        c.restore()
        c.drawTextBlob(blob, x, y - 3, paint((255, 255, 255), stroke=2.5, alpha=0.45))
        atlas.add("IMG_LOGO_" + gid, s)


def main():
    block()
    brick()
    orb()
    fx()
    logos()
    atlas.write(os.path.join(ROOT, "assets", "common"), os.path.join(ROOT, "engine", "common_art.h"), "cart", "common")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "common_sheet.png"))


if __name__ == "__main__":
    main()
