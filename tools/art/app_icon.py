#!/usr/bin/env python3
"""The app icon: a snowy peak under a four-point nova star, on a rounded square.
    python3 tools/art/app_icon.py      (from the repo root)
Writes platform/icon.png (1024 px, also the web page's icon) and platform/mac/NovaArcadeHD.icns."""
import os
import sys
import skia
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import hd  # noqa: E402
from hd import paint, lin, rad, smooth, poly, star, rrect  # noqa: E402

S = 1024
surf = skia.Surface(S, S)
c = surf.getCanvas()
c.clear(skia.Color4f(0, 0, 0, 0))
m = 100   # macOS icons leave a margin round the rounded square
body = rrect(m, m, S - 2 * m, S - 2 * m, 185)
c.drawPath(hd.offset(body, 0, 14), paint((0, 0, 0), alpha=0.35, blur=18))
c.drawPath(body, paint(shader=lin(0, m, 0, S - m, [(60, 44, 150), (38, 22, 92), (20, 12, 52)])))
c.save()
c.clipPath(body, skia.ClipOp.kIntersect, True)
c.drawCircle(512, 380, 330, paint(shader=rad(512, 380, 330, [(140, 110, 255, 150), (140, 110, 255, 0)])))
back = smooth([(60, 940), (300, 560), (380, 610), (520, 430), (700, 640), (780, 590), (980, 940)], tension=0.2)
c.drawPath(back, paint((70, 60, 150)))
peak = poly([(150, 940), (512, 470), (874, 940)])
c.drawPath(peak, paint(shader=lin(300, 470, 700, 940, [(160, 190, 255), (90, 110, 210), (50, 60, 150)])))
snow = poly([(512, 470), (420, 590), (465, 575), (505, 615), (548, 572), (600, 590)])
c.drawPath(snow, paint((255, 255, 255)))
c.drawPath(peak, paint((24, 14, 50), stroke=16))
c.drawRect(skia.Rect.MakeLTRB(0, 860, S, S), paint(shader=lin(0, 860, 0, S - m, [(255, 150, 200, 0), (255, 150, 200, 110)])))
c.restore()
c.drawPath(star(512, 300, 170, 30, 4), paint((255, 230, 120), blur=14, alpha=0.7))
c.drawPath(star(512, 300, 150, 26, 4), paint((255, 240, 170)))
c.drawPath(star(512, 300, 80, 16, 4, rot=-45), paint((255, 255, 255), alpha=0.9))
c.drawPath(body, paint((255, 255, 255), stroke=6, alpha=0.15))
out = os.path.join(ROOT, "platform", "icon.png")
os.makedirs(os.path.join(ROOT, "platform", "mac"), exist_ok=True)
surf.makeImageSnapshot().save(out, skia.kPNG)
Image.open(out).save(os.path.join(ROOT, "platform", "mac", "NovaArcadeHD.icns"),
                     sizes=[(16, 16), (32, 32), (64, 64), (128, 128), (256, 256), (512, 512), (1024, 1024)])
print("wrote platform/icon.png and platform/mac/NovaArcadeHD.icns")
