# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Flat 256x128 HOME banner (+48x48 icon) rendered from banner_scene_static.gltf with preview.py's renderer
(src/platform/3ds/banner.png + icon.png: the flat fallback when the 3D banner is off or fails).
The outputs are adaptations of the CC BY-SA 4.0 logo model (assets/logo/ATTRIBUTION.md) and are CC BY-SA 4.0."""
import os, sys, numpy as np
from PIL import Image, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import preview as P
P.W, P.H, P.SS = 800, 480, 2
P.BG = (255, 0, 255)
scene = os.path.join(P.REPO, 'src', 'platform', '3ds', 'banner3d', 'banner_scene_static.gltf')
parts = P.load_scene(scene)
parts = parts[0] if isinstance(parts, tuple) else parts
im = P.render(parts, angle=-14.0).convert('RGBA')
a = np.array(im).astype(int)
key = (np.abs(a[..., 0] - 255) < 40) & (a[..., 1] < 60) & (np.abs(a[..., 2] - 255) < 40)
a[key, 3] = 0
logo = Image.fromarray(a.astype(np.uint8)); logo = logo.crop(logo.getbbox())
def bg(w, h):
    g = Image.new('RGBA', (w, h)); px = g.load()
    for y in range(h):
        for x in range(w):
            t = y / h; d = ((x - w / 2) ** 2 / (w * .5) ** 2 + (y - h / 2) ** 2 / (h * .6) ** 2) ** .5; k = max(0, 1 - d)
            px[x, y] = (int(30 + 40 * t + 60 * k), int(70 + 50 * t + 80 * k), int(150 + 60 * t + 70 * k), 255)
    return g
def compose(w, h, maxw, maxh, sh):
    s = min(maxw / logo.width, maxh / logo.height)
    lg = logo.resize((round(logo.width * s), round(logo.height * s)), Image.LANCZOS)
    out = bg(w, h); x, y = (w - lg.width) // 2, (h - lg.height) // 2
    shadow = Image.new('RGBA', (w, h), (0, 0, 30, 0))
    m = Image.new('L', (w, h)); m.paste(lg.split()[3], (x, y))
    shadow.putalpha(m.filter(ImageFilter.GaussianBlur(sh)).point(lambda v: int(v * .55)))
    out.alpha_composite(shadow, (max(1, sh), max(1, sh))); out.alpha_composite(lg, (x, y))
    return out.convert('RGB')
d = os.path.join(P.REPO, 'src', 'platform', '3ds')
compose(256, 128, 236, 112, 3).save(os.path.join(d, 'banner.png'))
compose(48, 48, 46, 44, 1).save(os.path.join(d, 'icon.png'))
print('wrote banner.png + icon.png')
