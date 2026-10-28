#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Draws the project's own neutral HOME Menu artwork: src/platform/3ds/neutral/banner.png (256x128) + icon.png (48x48).
Use it instead of the logo artwork with:  make cia BANNER3D=0 ICON=$PWD/neutral/icon.png BANNER_PNG=$PWD/neutral/banner.png

A faceted green gem on a blue gradient with the text "sims3ds". Original artwork of this project (MPL-2.0); it uses
no logo or mark of EA/Maxis.  Needs Pillow and any TTF/OTF font (default: Inter Bold or DejaVu Sans Bold if found).

    python3 tools/banner/make_flat_banner.py [--font /path/to/Font-Bold.ttf]
"""
import argparse
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(HERE, '..', '..', 'src', 'platform', '3ds', 'neutral'))
FONTS = ['/usr/share/fonts/opentype/inter/Inter-Bold.otf', '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
         '/Library/Fonts/Arial Bold.ttf', 'C:/Windows/Fonts/arialbd.ttf']
SS = 4  # supersampling


def gradient(w, h):
    im = Image.new('RGB', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            t = y / max(1, h - 1)
            dx, dy = (x - w * 0.5) / (w * 0.6), (y - h * 0.45) / (h * 0.7)
            k = max(0.0, 1.0 - (dx * dx + dy * dy) ** 0.5)
            px[x, y] = (int(18 + 20 * t + 40 * k), int(52 + 40 * t + 70 * k), int(120 + 50 * t + 80 * k))
    return im


def gem(draw, cx, cy, r):
    """Faceted diamond (two pyramids) pointing up and down, lit from the upper left."""
    top, bot = (cx, cy - r * 1.55), (cx, cy + r * 1.55)
    left, right, front = (cx - r, cy), (cx + r, cy), (cx, cy + r * 0.12)
    faces = [((top, left, front), (126, 230, 120)), ((top, front, right), (70, 190, 72)),
             ((bot, front, left), (46, 160, 58)), ((bot, right, front), (24, 112, 40))]
    for pts, col in faces:
        draw.polygon(pts, fill=col)
    edge = (14, 70, 26)
    for a, b in [(top, left), (top, right), (bot, left), (bot, right), (left, front), (front, right),
                 (top, front), (bot, front)]:
        draw.line([a, b], fill=edge, width=max(1, int(r * 0.06)))


def shadowed(base, layer, blur):
    sh = Image.new('RGBA', base.size, (0, 0, 0, 0))
    sh.putalpha(layer.split()[3].filter(ImageFilter.GaussianBlur(blur)).point(lambda v: int(v * 0.5)))
    base.alpha_composite(sh, (max(1, blur // 2), max(1, blur // 2)))
    base.alpha_composite(layer)


def banner(font_path):
    w, h = 256 * SS, 128 * SS
    base = gradient(256, 128).resize((w, h), Image.BICUBIC).convert('RGBA')
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    gem(d, 52 * SS, 60 * SS, 21 * SS)
    f1 = ImageFont.truetype(font_path, 38 * SS)
    f2 = ImageFont.truetype(font_path, 12 * SS)
    d.text((88 * SS, 34 * SS), 'sims3ds', font=f1, fill=(255, 255, 255))
    d.text((90 * SS, 82 * SS), 'unofficial New 3DS port', font=f2, fill=(205, 225, 255))
    shadowed(base, layer, 3 * SS)
    return base.convert('RGB').resize((256, 128), Image.LANCZOS)


def icon():
    w = 48 * SS
    base = gradient(48, 48).resize((w, w), Image.BICUBIC).convert('RGBA')
    layer = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    gem(ImageDraw.Draw(layer), 24 * SS, 24 * SS, 12.5 * SS)
    shadowed(base, layer, 2 * SS)
    return base.convert('RGB').resize((48, 48), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--font', default=next((f for f in FONTS if os.path.exists(f)), None))
    ap.add_argument('--out', default=OUT)
    a = ap.parse_args()
    if not a.font:
        raise SystemExit('no font found: pass --font /path/to/a/bold.ttf')
    banner(a.font).save(os.path.join(a.out, 'banner.png'))
    icon().save(os.path.join(a.out, 'icon.png'))
    print('wrote', os.path.join(a.out, 'banner.png'), 'and icon.png')


if __name__ == '__main__':
    main()
