"""Shared helpers for the HD art scripts: Skia vector drawing, trimming, an atlas packer and the C++
header writer. Every picture is drawn in screen pixels (1080p) and rendered at DENSITY times that, so
it stays sharp on Retina and 4K screens; the engine draws it back at 1/DENSITY.

    import hd
    s = hd.Sprite(120, 80, pivot=(60, 80))        # size and pivot in screen pixels
    hd.outlined(s.c, hd.ellipse(60, 50, 50, 28), (250, 240, 245), ink)
    atlas.add("IMG_BLOB", s)
    atlas.write(out_dir, header_path, "pp", "pixelpeaks")
"""
import os
import math
import random
import numpy as np
import skia

DENSITY = 2


# ---------------------------------------------------------------- colours and paints
def col(c, a=None):
    if len(c) == 4 and a is None:
        a = c[3] / 255.0
    return skia.Color4f(c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0 if a is None else a)


def colour_int(c, a=255):
    a = c[3] if len(c) == 4 else a
    return skia.ColorSetARGB(int(a), int(c[0]), int(c[1]), int(c[2]))


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def shade(c, k):
    """k < 0 darkens towards a cool shadow, k > 0 lightens towards a warm highlight."""
    if k < 0:
        return mix(c, (30, 20, 60), -k)
    return mix(c, (255, 250, 235), k)


def paint(color=None, shader=None, alpha=None, stroke=0, blur=0, cap_round=True):
    p = skia.Paint(AntiAlias=True)
    if shader is not None:
        p.setShader(shader)
        if alpha is not None:
            p.setAlphaf(alpha)
    elif color is not None:
        p.setColor4f(col(color, alpha))
    if stroke:
        p.setStyle(skia.Paint.kStroke_Style)
        p.setStrokeWidth(stroke)
        p.setStrokeJoin(skia.Paint.kRound_Join)
        if cap_round:
            p.setStrokeCap(skia.Paint.kRound_Cap)
    if blur:
        p.setMaskFilter(skia.MaskFilter.MakeBlur(skia.kNormal_BlurStyle, blur))
    return p


def lin(x0, y0, x1, y1, colors, pos=None):
    return skia.GradientShader.MakeLinear([skia.Point(x0, y0), skia.Point(x1, y1)],
                                          [colour_int(c) for c in colors], pos)


def rad(cx, cy, r, colors, pos=None):
    return skia.GradientShader.MakeRadial(skia.Point(cx, cy), r, [colour_int(c) for c in colors], pos)


# ---------------------------------------------------------------- paths
def poly(pts, closed=True):
    p = skia.Path()
    p.moveTo(*pts[0])
    for q in pts[1:]:
        p.lineTo(*q)
    if closed:
        p.close()
    return p


def smooth(pts, closed=True, tension=0.5):
    """Catmull-Rom spline through the points, as cubic Beziers."""
    p = skia.Path()
    n = len(pts)
    p.moveTo(*pts[0])
    rng = range(n) if closed else range(n - 1)
    for i in rng:
        p0 = pts[(i - 1) % n] if (closed or i > 0) else pts[0]
        p1 = pts[i]
        p2 = pts[(i + 1) % n]
        p3 = pts[(i + 2) % n] if (closed or i + 2 < n) else pts[-1]
        k = tension / 3.0 * 2
        c1 = (p1[0] + (p2[0] - p0[0]) * k / 2, p1[1] + (p2[1] - p0[1]) * k / 2)
        c2 = (p2[0] - (p3[0] - p1[0]) * k / 2, p2[1] - (p3[1] - p1[1]) * k / 2)
        p.cubicTo(c1[0], c1[1], c2[0], c2[1], p2[0], p2[1])
    if closed:
        p.close()
    return p


def ellipse(cx, cy, rx, ry):
    p = skia.Path()
    p.addOval(skia.Rect.MakeLTRB(cx - rx, cy - ry, cx + rx, cy + ry))
    return p


def circle(cx, cy, r):
    return ellipse(cx, cy, r, r)


def rrect(x, y, w, h, r):
    p = skia.Path()
    p.addRRect(skia.RRect.MakeRectXY(skia.Rect.MakeXYWH(x, y, w, h), r, r))
    return p


def capsule(x0, y0, x1, y1, r0, r1=None):
    """A limb: a rounded segment from (x0, y0) radius r0 to (x1, y1) radius r1."""
    r1 = r0 if r1 is None else r1
    a = math.atan2(y1 - y0, x1 - x0)
    nx, ny = -math.sin(a), math.cos(a)
    p = skia.Path()
    p.moveTo(x0 + nx * r0, y0 + ny * r0)
    p.lineTo(x1 + nx * r1, y1 + ny * r1)
    p.arcTo(skia.Rect.MakeLTRB(x1 - r1, y1 - r1, x1 + r1, y1 + r1), math.degrees(a) + 90, -180, False)
    p.lineTo(x0 - nx * r0, y0 - ny * r0)
    p.arcTo(skia.Rect.MakeLTRB(x0 - r0, y0 - r0, x0 + r0, y0 + r0), math.degrees(a) - 90, -180, False)
    p.close()
    return p


def union(*paths):
    out = paths[0]
    for q in paths[1:]:
        out = skia.Op(out, q, skia.PathOp.kUnion_PathOp)
    return out


def minus(a, b):
    return skia.Op(a, b, skia.PathOp.kDifference_PathOp)


def intersect(a, b):
    return skia.Op(a, b, skia.PathOp.kIntersect_PathOp)


def offset(path, dx, dy):
    q = skia.Path(path)
    q.offset(dx, dy)
    return q


def star(cx, cy, r_out, r_in, n, rot=-90):
    pts = []
    for i in range(n * 2):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians(rot + i * 180.0 / n)
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return poly(pts)


# ---------------------------------------------------------------- drawing with the house style
INK = (44, 30, 58)      # outline colour for characters and props
LINE = 3.0              # outline width in screen pixels


def outlined(c, path, fill, ink=INK, width=LINE, shader=None):
    """Fill a shape with a dark outline round it (the outline sits half outside)."""
    if width > 0:
        c.drawPath(path, paint(ink, stroke=width * 2))
    c.drawPath(path, paint(fill, shader=shader))


def cel(c, path, light_path, color, alpha=1.0):
    """Paint a highlight or shadow shape, clipped to a body shape."""
    c.save()
    c.clipPath(path, skia.ClipOp.kIntersect, True)
    c.drawPath(light_path, paint(color, alpha=alpha))
    c.restore()


def soft(c, path, color, blur, alpha=1.0):
    c.drawPath(path, paint(color, alpha=alpha, blur=blur))


# ---------------------------------------------------------------- sprites
class Sprite:
    """A picture drawn in screen-pixel coordinates; pivot is where the engine places it."""

    def __init__(self, w, h, pivot=(0, 0), density=DENSITY, trim=True):
        self.w, self.h, self.pivot, self.density, self.trim = w, h, pivot, density, trim
        self.surface = skia.Surface(int(math.ceil(w * density)), int(math.ceil(h * density)))
        self.c = self.surface.getCanvas()
        self.c.clear(skia.Color4f(0, 0, 0, 0))
        self.c.scale(density, density)

    def pixels(self):
        img = self.surface.makeImageSnapshot()
        return img.toarray(colorType=skia.kRGBA_8888_ColorType, alphaType=skia.kUnpremul_AlphaType)

    def result(self):
        """(rgba array, pivot in array pixels, scale) after trimming empty borders."""
        a = self.pixels()
        px, py = self.pivot[0] * self.density, self.pivot[1] * self.density
        if self.trim:
            alpha = a[:, :, 3]
            ys, xs = np.nonzero(alpha > 0)
            if len(xs) == 0:
                a = a[:1, :1]
                return a, (0, 0), 1.0 / self.density
            x0, x1, y0, y1 = max(0, xs.min() - 1), min(a.shape[1], xs.max() + 2), max(0, ys.min() - 1), min(a.shape[0], ys.max() + 2)
            a = a[y0:y1, x0:x1]
            px, py = px - x0, py - y0
        return a, (px, py), 1.0 / self.density

    def save_png(self, path):
        a = self.pixels()
        skia.Image.fromarray(np.ascontiguousarray(a), colorType=skia.kRGBA_8888_ColorType,
                             alphaType=skia.kUnpremul_AlphaType).save(path, skia.kPNG)


def bleed(a, iterations=6):
    """Spread colour into fully transparent pixels next to drawn ones, so linear filtering at the
    edges never pulls in black."""
    a = a.copy()
    rgb = a[:, :, :3].astype(np.float32)
    known = a[:, :, 3] > 0
    for _ in range(iterations):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(known.shape, np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            sk = np.roll(known, (dy, dx), (0, 1))
            sr = np.roll(rgb, (dy, dx), (0, 1))
            acc += sr * sk[:, :, None]
            cnt += sk
        grow = (~known) & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][:, None]
        known = known | grow
    a[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    return a


class Atlas:
    PAD = 3

    def __init__(self, page=2048):
        self.page = page
        self.items = []      # (name, array, pivot, scale)
        self.bigs = []       # standalone textures: (name, array, scale, filename)
        self.arrays = []     # (name, [names]) for animation tables

    def add(self, name, sprite_or_result):
        r = sprite_or_result.result() if isinstance(sprite_or_result, Sprite) else sprite_or_result
        self.items.append((name, r[0], r[1], r[2]))

    def add_big(self, name, sprite, filename):
        """A large picture (a background layer) kept in its own texture."""
        a = sprite.pixels()
        self.bigs.append((name, a, 1.0 / sprite.density, filename))

    def table(self, name, names):
        self.arrays.append((name, names))

    def pack(self):
        order = sorted(range(len(self.items)), key=lambda i: -self.items[i][1].shape[0])
        pages, places = [], {}
        shelves = []   # per page: list of [y, height, x]
        for i in order:
            name, a, piv, sc = self.items[i]
            h, w = a.shape[0] + self.PAD * 2, a.shape[1] + self.PAD * 2
            assert w <= self.page and h <= self.page, "%s is too big for an atlas page" % name
            placed = False
            for pi, sh in enumerate(shelves):
                for s in sh:
                    if h <= s[1] and s[2] + w <= self.page:
                        places[i] = (pi, s[2], s[0]); s[2] += w; placed = True
                        break
                if placed:
                    break
                top = sh[-1][0] + sh[-1][1] if sh else 0
                if top + h <= self.page:
                    sh.append([top, h, w]); places[i] = (pi, 0, top); placed = True
                    break
            if not placed:
                shelves.append([[0, h, w]]); places[i] = (len(shelves) - 1, 0, 0)
        heights = [max(s[0] + s[1] for s in sh) for sh in shelves]
        for ph in heights:
            pages.append(np.zeros((ph, self.page, 4), np.uint8))
        for i, (pi, x, y) in places.items():
            a = bleed(self.items[i][1])
            hh, ww = a.shape[:2]
            pages[pi][y + self.PAD:y + self.PAD + hh, x + self.PAD:x + self.PAD + ww] = a
        return pages, places

    def write(self, out_dir, header, ns, asset_prefix):
        os.makedirs(out_dir, exist_ok=True)
        pages, places = self.pack()
        files = []
        for pi, pg in enumerate(pages):
            fn = "atlas%d.png" % pi
            skia.Image.fromarray(np.ascontiguousarray(pg), colorType=skia.kRGBA_8888_ColorType,
                                 alphaType=skia.kUnpremul_AlphaType).save(os.path.join(out_dir, fn), skia.kPNG)
            files.append(fn)
        for name, a, sc, fn in self.bigs:
            skia.Image.fromarray(np.ascontiguousarray(bleed(a, 2)), colorType=skia.kRGBA_8888_ColorType,
                                 alphaType=skia.kUnpremul_AlphaType).save(os.path.join(out_dir, fn), skia.kPNG)
            files.append(fn)
        out = ["// AUTO-GENERATED by an art script - edit the script, not this file",
               "#pragma once", '#include "nova.h"', "namespace %s {" % ns,
               "static int TEX[%d] = {%s};" % (len(files), ", ".join(["-1"] * len(files))),
               "static const char* const TEX_FILES[%d] = {%s};" % (len(files), ", ".join('"%s/%s"' % (asset_prefix, f) for f in files)),
               "static const int NTEX = %d;" % len(files)]
        for i, (name, a, piv, sc) in enumerate(self.items):
            pi, x, y = places[i]
            h, w = a.shape[:2]
            out.append("static const nova::Img %s = {&TEX[%d], %d, %d, %d, %d, %.1ff, %.1ff, %.4ff};" % (
                name, pi, x + self.PAD, y + self.PAD, w, h, piv[0], piv[1], sc))
        for bi, (name, a, sc, fn) in enumerate(self.bigs):
            h, w = a.shape[:2]
            out.append("static const nova::Img %s = {&TEX[%d], 0, 0, %d, %d, 0, 0, %.4ff};" % (name, len(pages) + bi, w, h, sc))
        for name, names in self.arrays:
            out.append("static const nova::Img* const %s[%d] = {%s};" % (name, len(names), ", ".join("&" + n for n in names)))
        out.append("}  // namespace %s" % ns)
        open(header, "w").write("\n".join(out) + "\n")
        total = sum(p.shape[0] * p.shape[1] for p in pages)
        print("atlas: %d sprites on %d pages (%.1f MPix), %d big pictures -> %s" % (len(self.items), len(pages), total / 1e6, len(self.bigs), out_dir))


def contact_sheet(atlas, path, bg=(90, 110, 140)):
    """A preview of every sprite, for checking the art by eye."""
    items = atlas.items
    cell = 220
    cols = 9
    rows = (len(items) + cols - 1) // cols
    surf = skia.Surface(cols * cell, rows * cell)
    c = surf.getCanvas()
    c.clear(col(bg))
    font = skia.Font(None, 13)
    for i, (name, a, piv, sc) in enumerate(items):
        x, y = (i % cols) * cell, (i // cols) * cell
        img = skia.Image.fromarray(np.ascontiguousarray(a), colorType=skia.kRGBA_8888_ColorType, alphaType=skia.kUnpremul_AlphaType)
        h, w = a.shape[:2]
        k = min(1.0, (cell - 30) / max(w, h))
        c.drawImageRect(img, skia.Rect.MakeXYWH(x + 5, y + 5, w * k, h * k), skia.SamplingOptions(skia.FilterMode.kLinear))
        c.drawString(name.replace("IMG_", ""), x + 4, y + cell - 8, font, paint((255, 255, 255)))
    surf.makeImageSnapshot().save(path, skia.kPNG)
