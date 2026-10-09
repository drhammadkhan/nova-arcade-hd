#!/usr/bin/env python3
"""Maze Munch HD art: the neon maze (three big layers made from the original's layout), the munch-bot
with every mouth opening, the flame-shaped wisps, power crystals and the bonus gem. All original.
    python3 games/MazeMunch/tools/make_art.py [--preview DIR]      (from the repo root)
Writes assets/mazemunch/*.png and games/MazeMunch/art.h (namespace mmart).

One maze tile is 45 px (the original's 10 x 4.5). The maze layers cover the 28 x 22 tile maze plus a
GM-pixel margin for the glow, and are drawn with their top-left corner at (MOX - GM, MOY - GM).
  MAZE_FILL  the inside of the walls (dark, drawn as is)
  MAZE_GLOW  a soft white glow round the tubes (the game tints it and adds it)
  MAZE_TUBE  the neon tubes, grey with a white core (the game tints it)
Wisp bodies and the bonus gem are drawn in greys so the game can tint them."""
import os
import sys
import math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art"))
import hd  # noqa: E402
from hd import Sprite, poly, smooth, ellipse, circle, paint, lin, rad, minus  # noqa: E402
import skia  # noqa: E402

T = 45            # one tile on screen
GM = 40           # glow margin round the maze layers
atlas = hd.Atlas(2048)

# The original's maze, unchanged (games/MazeMunch/game.h in Nova Arcade)
MAZE = [
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##          ##.#     ",
    "######.## ###--### ##.######",
    "      .   #      #   .      ",
    "######.## ######## ##.######",
    "     #.##          ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......  .......##..o#",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
]
COLS, ROWS, TUNNEL = 28, 22, 10


# ---------------------------------------------------------------- the maze
def solid_grid():
    """Walls, plus the empty spaces nobody can reach (so they fill in as solid blocks)."""
    seen = set()
    stack = [(13, 16)]
    while stack:
        c, r = stack.pop()
        if (c, r) in seen:
            continue
        seen.add((c, r))
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nc, nr = c + dc, r + dr
            if nr == TUNNEL:
                nc %= COLS
            if 0 <= nc < COLS and 0 <= nr < ROWS and MAZE[nr][nc] != "#":
                stack.append((nc, nr))
    g = np.ones((ROWS + 2, COLS + 2), np.float32)   # one cell of border all round, solid
    for r in range(ROWS):
        for c in range(COLS):
            g[r + 1, c + 1] = 0.0 if (c, r) in seen else 1.0
    g[TUNNEL + 1, 0] = g[TUNNEL + 1, COLS + 1] = 0.0
    return g


def blur(a, sigma):
    """Gaussian blur through the FFT, padding with the edge values so nothing wraps."""
    p = int(sigma * 4) + 2
    b = np.pad(a, p, mode="edge")
    fy = np.fft.fftfreq(b.shape[0])[:, None]
    fx = np.fft.rfftfreq(b.shape[1])[None, :]
    g = np.exp(-2 * math.pi ** 2 * sigma ** 2 * (fx ** 2 + fy ** 2))
    out = np.fft.irfft2(np.fft.rfft2(b) * g, s=b.shape)
    return out[p:-p, p:-p]


def phi(x):
    return 0.5 * (1 + math.erf(x / math.sqrt(2)))


def maze_layers(D=2):
    g = solid_grid()
    w, h = (COLS * T + 2 * GM) * D, (ROWS * T + 2 * GM) * D
    xs = (np.arange(w) + 0.5) / D - GM
    ys = (np.arange(h) + 0.5) / D - GM
    ci = np.clip(np.floor(xs / T).astype(int) + 1, 0, COLS + 1)
    ri = np.clip(np.floor(ys / T).astype(int) + 1, 0, ROWS + 1)
    mask = g[ri[:, None], ci[None, :]]
    # outside the maze the tunnel row stays open right to the edge
    tun = (ys >= TUNNEL * T) & (ys < (TUNNEL + 1) * T)
    out = (xs < 0) | (xs >= COLS * T)
    mask[np.ix_(tun, out)] = 0.0
    sigma, inset = 12.0 * D, 12.0 * D
    v = blur(mask, sigma)
    L = phi(inset / sigma)
    gy, gx = np.gradient(v)
    gm = np.sqrt(gx * gx + gy * gy) + 1e-7
    dist = (v - L) / gm          # signed distance to the tube's centre line, + inside the walls
    dist = np.clip(dist, -60 * D, 60 * D)
    ad = np.abs(dist)
    tube = np.clip(3.4 * D + 0.5 - ad, 0, 1)
    core = np.clip(1.3 * D + 0.5 - ad, 0, 1)
    # a fainter second line inside the wall gives the tubes some depth
    inner = np.clip(1.1 * D + 0.5 - np.abs(dist - 9 * D), 0, 1) * (v > L)
    lum = 0.6 + 0.4 * core
    a = np.maximum(tube, inner * 0.45)
    lum = np.where(tube >= inner * 0.45, lum, 0.75)
    tube_rgba = np.zeros((h, w, 4), np.uint8)
    tube_rgba[:, :, 0] = tube_rgba[:, :, 1] = tube_rgba[:, :, 2] = (lum * 255).astype(np.uint8)
    tube_rgba[:, :, 3] = (a * 255).astype(np.uint8)
    # the fill and the glow are soft, so they are kept at 1x
    small = lambda x: x.reshape(h // D, D, w // D, D).mean(axis=(1, 3))
    tube1, dist1 = small(tube), small(dist) / D
    glow = np.clip(blur(tube1, 7.0) * 1.6 + blur(tube1, 22.0) * 1.1, 0, 1)
    glow_rgba = np.zeros((h // D, w // D, 4), np.uint8)
    glow_rgba[:, :, :3] = 255
    glow_rgba[:, :, 3] = (glow * 255).astype(np.uint8)
    inside = np.clip(dist1 + 0.5, 0, 1)
    rim = np.exp(-np.maximum(dist1, 0) / 16.0)
    deep, edge = np.array([12, 9, 34], np.float32), np.array([38, 26, 92], np.float32)
    fc = deep[None, None, :] + (edge - deep)[None, None, :] * rim[:, :, None]
    fill_rgba = np.zeros((h // D, w // D, 4), np.uint8)
    fill_rgba[:, :, :3] = np.clip(fc, 0, 255).astype(np.uint8)
    # fade the fill out towards the maze's outer edge, so the layer has no visible border
    x1 = np.arange(w // D) + 0.5 - GM
    y1 = np.arange(h // D) + 0.5 - GM
    ix = np.minimum(x1, COLS * T - x1)
    iy = np.minimum(y1, ROWS * T - y1)
    fade = np.clip(np.minimum(ix[None, :], iy[:, None]) / 26.0, 0, 1)
    fill_rgba[:, :, 3] = (inside * fade * 235).astype(np.uint8)
    return fill_rgba, glow_rgba, tube_rgba


class Big:
    def __init__(self, a, density):
        self.a, self.density = a, density

    def pixels(self):
        return self.a


# ---------------------------------------------------------------- the munch-bot
R = 26


def muncher(i, n):
    """Frame i of n: the mouth's half angle runs from shut to wide open (and on, for the death)."""
    half = i * 180.0 / (n - 1)
    s = Sprite(R * 2 + 12, R * 2 + 12, pivot=(R + 6, R + 6), trim=False)
    c = s.c
    c.translate(R + 6, R + 6)
    body = circle(0, 0, R)
    if half > 0.5:
        a = math.radians(min(half, 179.5))
        pts = [(-4, 0)]
        steps = 12
        for k in range(steps + 1):
            t = -a + 2 * a * k / steps
            pts.append((math.cos(t) * (R + 8) * 1.5, math.sin(t) * (R + 8) * 1.5))
        body = minus(body, poly(pts))
    c.drawPath(body, paint((18, 70, 36), stroke=5))
    c.drawPath(body, paint(shader=rad(-9, -11, R * 1.5, [(240, 255, 210), (182, 255, 110), (70, 170, 60), (34, 100, 44)], [0, 0.35, 0.8, 1])))
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    c.drawPath(ellipse(-8, -13, 9, 5), paint((255, 255, 255), alpha=0.55))
    c.drawPath(circle(0, 0, R), paint((20, 80, 40), stroke=3, alpha=0.5))
    # the eye, above and behind the mouth
    ex, ey = -3, -14
    c.drawPath(circle(ex, ey, 6), paint((250, 255, 240)))
    c.drawPath(circle(ex + 1.5, ey + 0.5, 3.6), paint((14, 22, 40)))
    c.drawPath(circle(ex + 0.2, ey - 1.4, 1.4), paint((255, 255, 255)))
    c.restore()
    return s


# ---------------------------------------------------------------- the wisps
def wisp_body(f):
    """A flame-shaped wisp in greys (the game tints it): round shoulders, flickering tips, a rippling hem."""
    s = Sprite(70, 84, pivot=(35, 44), trim=False)
    c = s.c
    c.translate(35, 44)
    ph = f * math.pi / 2
    pts = []
    for k in range(9):                      # the hem, left to right
        x = -25 + k * 50 / 8.0
        y = 22 + 4 * math.sin(k * math.pi / 2 + ph)
        pts.append((x, y))
    w1, w2, w3 = math.sin(ph) * 3, math.cos(ph) * 3, math.sin(ph + 1.3) * 3
    pts += [(26, 8), (25, -8), (19, -19), (14 + w1, -31), (8, -24), (1 + w2, -39), (-5, -26), (-12 + w3, -34),
            (-18, -21), (-25, -8), (-26, 8)]
    body = smooth(pts, tension=0.55)
    c.drawPath(body, paint((70, 70, 80), stroke=4.5))
    c.drawPath(body, paint(shader=lin(0, -38, 0, 26, [(255, 255, 255), (238, 238, 238), (190, 190, 196), (140, 140, 150)], [0, 0.35, 0.75, 1])))
    c.save()
    c.clipPath(body, skia.ClipOp.kIntersect, True)
    inner = smooth([(-14, -14), (-6 + w2 * 0.5, -30), (2, -16), (-2, 4), (-12, 6)], tension=0.5)
    c.drawPath(inner, paint((255, 255, 255), alpha=0.6, blur=3))
    c.drawPath(ellipse(0, 26, 30, 8), paint((60, 60, 70), alpha=0.35, blur=4))
    c.restore()
    return s


def fright_face():
    s = Sprite(48, 36, pivot=(24, 18), trim=False)
    c = s.c
    c.translate(24, 18)
    for x in (-9, 9):
        c.drawPath(hd.rrect(x - 3.5, -9, 7, 8, 3), paint((255, 255, 255)))
    zig = [(-15, 9), (-10, 5), (-5, 9), (0, 5), (5, 9), (10, 5), (15, 9)]
    c.drawPath(poly(zig, closed=False), paint((255, 255, 255), stroke=3.2))
    return s


# ---------------------------------------------------------------- crystals and the bonus gem
def crystal():
    """A power crystal: a lime hexagonal gem with facets."""
    s = Sprite(44, 52, pivot=(22, 26), trim=False)
    c = s.c
    c.translate(22, 26)
    top, mid, bot, w = -22, -6, 22, 15
    outline = poly([(0, top), (w, mid), (w * 0.7, 12), (0, bot), (-w * 0.7, 12), (-w, mid)])
    c.drawPath(outline, paint((30, 90, 40), stroke=4))
    faces = [
        ([(0, top), (w, mid), (0, -2)], (220, 255, 170)),
        ([(0, top), (0, -2), (-w, mid)], (250, 255, 225)),
        ([(-w, mid), (0, -2), (-w * 0.7, 12)], (182, 255, 110)),
        ([(w, mid), (w * 0.7, 12), (0, -2)], (120, 220, 80)),
        ([(-w * 0.7, 12), (0, -2), (0, bot)], (140, 230, 90)),
        ([(0, -2), (w * 0.7, 12), (0, bot)], (80, 180, 70)),
    ]
    for pts, colr in faces:
        c.drawPath(poly(pts), paint(colr))
    c.drawPath(poly([(-3, -16), (3, -16), (0, -8)]), paint((255, 255, 255), alpha=0.8))
    return s


def gem():
    """The bonus gem, a brilliant-cut stone in greys (tinted pink and gold by turns)."""
    s = Sprite(64, 60, pivot=(32, 30), trim=False)
    c = s.c
    c.translate(32, 30)
    crown, girdle, tip, w, tw = -20, -8, 24, 27, 14
    shape = poly([(-tw, crown), (tw, crown), (w, girdle), (0, tip), (-w, girdle)])
    c.drawPath(shape, paint((70, 66, 80), stroke=4))
    c.drawPath(shape, paint((210, 210, 220)))
    faces = [
        ([(-tw, crown), (tw, crown), (6, girdle), (-6, girdle)], (255, 255, 255)),
        ([(-tw, crown), (-6, girdle), (-w, girdle)], (235, 235, 240)),
        ([(tw, crown), (w, girdle), (6, girdle)], (200, 200, 210)),
        ([(-w, girdle), (-6, girdle), (0, tip)], (225, 225, 232)),
        ([(-6, girdle), (6, girdle), (0, tip)], (190, 190, 200)),
        ([(6, girdle), (w, girdle), (0, tip)], (150, 150, 165)),
    ]
    for pts, colr in faces:
        c.drawPath(poly(pts), paint(colr))
        c.drawPath(poly(pts), paint((120, 120, 135), stroke=1.2, alpha=0.6))
    c.drawPath(poly([(-9, -17), (-3, -17), (-7, -11)]), paint((255, 255, 255)))
    return s


def main():
    n = 16
    for i in range(n):
        atlas.add("IMG_MUNCH%d" % i, muncher(i, n))
    atlas.table("MUNCH", ["IMG_MUNCH%d" % i for i in range(n)])
    for f in range(4):
        atlas.add("IMG_WISP%d" % f, wisp_body(f))
    atlas.table("WISP", ["IMG_WISP%d" % f for f in range(4)])
    atlas.add("IMG_FRIGHT", fright_face())
    atlas.add("IMG_CRYSTAL", crystal())
    atlas.add("IMG_GEM", gem())
    fill, glow, tube = maze_layers()
    atlas.add_big("IMG_MAZE_FILL", Big(fill, 1), "maze_fill.png")
    atlas.add_big("IMG_MAZE_GLOW", Big(glow, 1), "maze_glow.png")
    atlas.add_big("IMG_MAZE_TUBE", Big(tube, 2), "maze_tube.png")
    atlas.write(os.path.join(ROOT, "assets", "mazemunch"), os.path.join(ROOT, "games", "MazeMunch", "art.h"), "mmart", "mazemunch")
    if "--preview" in sys.argv:
        d = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(d, exist_ok=True)
        hd.contact_sheet(atlas, os.path.join(d, "mm_sheet.png"), bg=(20, 18, 50))
        # the three maze layers over the background, roughly as the game draws them
        h, w = fill.shape[:2]
        bg = np.zeros((h, w, 3), np.float32) + np.array([8, 6, 26], np.float32)
        tint = np.array([120, 100, 255], np.float32) / 255
        a = fill[:, :, 3:4] / 255.0
        bg = bg * (1 - a) + fill[:, :, :3] * a
        bg += glow[:, :, 3:4] / 255.0 * 0.55 * 255 * tint
        t2 = tube.reshape(h, 2, w, 2, 4).mean(axis=(1, 3))
        ta = t2[:, :, 3:4] / 255.0
        bg = bg * (1 - ta) + t2[:, :, :3] * tint * ta
        bg += t2[:, :, :3] * ta * 0.4
        out = np.clip(bg, 0, 255).astype(np.uint8)
        rgba = np.concatenate([out, np.full((h, w, 1), 255, np.uint8)], axis=2)
        skia.Image.fromarray(np.ascontiguousarray(rgba), colorType=skia.kRGBA_8888_ColorType).save(os.path.join(d, "mm_maze.png"), skia.kPNG)


if __name__ == "__main__":
    main()
