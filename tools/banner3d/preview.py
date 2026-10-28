#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Software preview of banner_scene.gltf as the HOME Menu would roughly show it (numpy + Pillow only).

Camera = pycgfx's banner-camera.gltf: position (0, 1, 44.786), looking down -Z, yfov 30 deg, 400x240 (5:3).
Nodes named 'backdrop' / 'name' are treated as YAxial billboards.  The glTF 'sway' animation (rotation
channel of 'logo') is evaluated exactly as keyed (linear quaternion keys -> yaw).  Lighting is a guess (one directional light + ambient, matte), the real menu
light differs.  Stereo: two parallel cameras +-EYE/2 with the image planes shifted so z = 0 has zero parallax.

    python3 preview.py [scene.gltf] [out_dir]
Writes sway_m45/000/p45.png, sway_loop.gif (one loop), stereo_LR.png (parallel view),
stereo_RL_crossview.png, anaglyph.png (red/cyan) and a contact sheet overview.png.
"""
import json, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
CAM = np.array([0.0, 1.0, 44.786])
YFOV = np.radians(30.0)
ZNEAR = 26.5
W, H, SS = 400, 240, 2
EYE = 1.6
LIGHT = np.array([-0.35, 0.55, 1.0]); LIGHT /= np.linalg.norm(LIGHT)
BG = (34, 40, 52)


def load_scene(path):
    doc = json.load(open(path))
    base = os.path.dirname(path)
    blob = open(os.path.join(base, doc['buffers'][0]['uri']), 'rb').read()

    def acc(i):
        a = doc['accessors'][i]
        bv = doc['bufferViews'][a['bufferView']]
        n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32}[a['componentType']]
        off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        return np.frombuffer(blob, dt, a['count'] * n, off).reshape(a['count'], n) if n > 1 else \
            np.frombuffer(blob, dt, a['count'], off)

    tex = [np.asarray(Image.open(os.path.join(base, im['uri'])).convert('RGBA')).astype(np.float64) / 255
           for im in doc['images']]
    parts = []

    def walk(ni, chain, billboard):
        nd = doc['nodes'][ni]
        chain = chain + [(nd.get('name'), np.array(nd.get('translation', [0, 0, 0]), np.float64))]
        bb = billboard or nd.get('name') in ('backdrop', 'name')
        if 'mesh' in nd:
            for p in doc['meshes'][nd['mesh']]['primitives']:
                mat = doc['materials'][p['material']]
                ti = doc['textures'][mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
                pos = acc(p['attributes']['POSITION']).astype(np.float64)
                parts.append(dict(pos=pos, chain=chain, nrm=acc(p['attributes']['NORMAL']).astype(np.float64),
                                  uv=acc(p['attributes']['TEXCOORD_0']).astype(np.float64),
                                  idx=acc(p['indices']).astype(np.int64).reshape(-1, 3), tex=tex[ti],
                                  billboard=bb, cull=not mat.get('doubleSided', False)))
        for c in nd.get('children', []):
            walk(c, chain, bb)

    for n in doc['scenes'][doc.get('scene', 0)]['nodes']:
        walk(n, [], False)
    anim = {}
    for a in doc.get('animations', []):
        for ch in a['channels']:
            if ch['target']['path'] == 'rotation':
                smp = a['samplers'][ch['sampler']]
                anim[doc['nodes'][ch['target']['node']]['name']] = (acc(smp['input']).astype(np.float64),
                                                                    acc(smp['output']).astype(np.float64))
    return parts, anim


def anim_yaw(anim, node, t):
    """yaw (deg) of `node` at time t from linear quaternion keys (pure Y rotations)."""
    if node not in anim:
        return 0.0
    ts, q = anim[node]
    t = t % ts[-1]
    yaw = 2 * np.degrees(np.arctan2(q[:, 1], q[:, 3]))
    return float(np.interp(t, ts, yaw))


def rot_y(deg):
    r = np.radians(deg); c, s = np.cos(r), np.sin(r)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def render(parts, angle=0.0, eye=0.0, yaw=None):
    """angle: HOME-Menu-style rotation of the non-billboard parts; yaw: {node: deg} node animation."""
    yaw = yaw or {}
    w, h = W * SS, H * SS
    f = (h / 2) / np.tan(YFOV / 2)
    img = np.zeros((h, w, 3)); img[:] = np.array(BG) / 255
    zb = np.full((h, w), np.inf)
    R = rot_y(angle)
    cam = CAM + np.array([eye, 0, 0])
    shift = -eye * f / CAM[2]          # zero parallax on the z = 0 plane
    for p in parts:
        pos, nrm = p['pos'], p['nrm']
        for name, tr in reversed(p['chain']):          # innermost node first: rotate (animation) then translate
            if name in yaw:
                Ry = rot_y(yaw[name]); pos = pos @ Ry.T; nrm = nrm @ Ry.T
            pos = pos + tr
        if not p['billboard']:
            pos = pos @ R.T               # HOME rotation about the vertical axis through x = z = 0
            nrm = nrm @ R.T
        rel = pos - cam
        depth = -rel[:, 2]
        sx = w / 2 + f * rel[:, 0] / depth - shift
        sy = h / 2 - f * rel[:, 1] / depth
        shade = 0.30 + 0.80 * np.clip(nrm @ LIGHT, 0, 1)
        tex = p['tex']; th, tw = tex.shape[:2]
        for t in p['idx']:
            if depth[t].min() < ZNEAR:
                continue                  # near-plane clipped on the console too
            xs, ys = sx[t], sy[t]
            area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])
            if p['cull'] and area >= 0:   # screen y points down: CCW in world = negative area here
                continue
            if abs(area) < 1e-9:
                continue
            x0, x1 = max(int(np.floor(xs.min())), 0), min(int(np.ceil(xs.max())), w - 1)
            y0, y1 = max(int(np.floor(ys.min())), 0), min(int(np.ceil(ys.max())), h - 1)
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            l0 = ((xs[1] - gx) * (ys[2] - gy) - (xs[2] - gx) * (ys[1] - gy)) / area
            l1 = ((xs[2] - gx) * (ys[0] - gy) - (xs[0] - gx) * (ys[2] - gy)) / area
            l2 = 1 - l0 - l1
            m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not m.any():
                continue
            # perspective-correct interpolation
            iw = np.array([1 / depth[t[0]], 1 / depth[t[1]], 1 / depth[t[2]]])
            wsum = l0 * iw[0] + l1 * iw[1] + l2 * iw[2]
            z = 1 / wsum
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            m &= z < sub
            if not m.any():
                continue
            b0, b1, b2 = l0 * iw[0] * z, l1 * iw[1] * z, l2 * iw[2] * z
            uv = p['uv'][t]
            u = b0 * uv[0, 0] + b1 * uv[1, 0] + b2 * uv[2, 0]
            v = b0 * uv[0, 1] + b1 * uv[1, 1] + b2 * uv[2, 1]
            px = np.clip((u * tw).astype(int), 0, tw - 1); py = np.clip((v * th).astype(int), 0, th - 1)
            col = tex[py, px, :3]
            s = b0 * shade[t[0]] + b1 * shade[t[1]] + b2 * shade[t[2]]
            col = np.clip(col * s[..., None], 0, 1)
            sub[m] = z[m]
            img[y0:y1 + 1, x0:x1 + 1][m] = col[m]
    im = Image.fromarray((img * 255).astype(np.uint8)).resize((W, H), Image.LANCZOS)
    return im


def guides(im):
    """Mark the approximate HOME Menu overlay bands (title bar on top, camera / L+R strip at the bottom)."""
    im = im.copy(); d = ImageDraw.Draw(im)
    for y in (int(H * 0.12), int(H * 0.86)):
        for x in range(0, W, 8):
            d.line([(x, y), (x + 3, y)], fill=(90, 100, 120))
    return im


def main():
    scene = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, 'src', 'platform', '3ds', 'banner3d', 'banner_scene.gltf')
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, 'build', 'banner3d_preview')
    os.makedirs(out, exist_ok=True)
    pre = sys.argv[3] if len(sys.argv) > 3 else 'sway_'
    parts, anim = load_scene(scene)
    amp = 25
    if 'logo' in anim:
        q = anim['logo'][1]
        amp = int(round(np.max(np.abs(2 * np.degrees(np.arctan2(q[:, 1], q[:, 3]))))))
    shots = []
    for deg, tag in ((-amp, 'm%02d' % amp), (0, '000'), (amp, 'p%02d' % amp)):
        im = guides(render(parts, yaw={'logo': deg}))
        im.save(os.path.join(out, '%s%s.png' % (pre, tag))); shots.append(('logo yaw %+d deg' % deg, im))
    frames = []
    if anim:
        period = max(v[0][-1] for v in anim.values())
        n = int(period * 12)
        for k in range(n):                       # one loop at 12 fps, last frame != first frame
            t = period * k / n
            fr = guides(render(parts, yaw={nd: anim_yaw(anim, nd, t) for nd in anim}))
            ImageDraw.Draw(fr).text((4, 4), 't=%.2fs yaw %+.0f' % (t, anim_yaw(anim, 'logo', t)), fill=(200, 200, 200))
            frames.append(fr)
        frames[0].save(os.path.join(out, pre + 'loop.gif'), save_all=True, append_images=frames[1:],
                       duration=int(1000 / 12), loop=0)
    sy_ = min(15, amp)
    L = render(parts, 0, -EYE / 2, {'logo': sy_}); R = render(parts, 0, EYE / 2, {'logo': sy_})
    pair = Image.new('RGB', (W * 2 + 8, H), (0, 0, 0)); pair.paste(L, (0, 0)); pair.paste(R, (W + 8, 0))
    pair.save(os.path.join(out, pre + 'stereo_LR.png'))
    pair2 = Image.new('RGB', (W * 2 + 8, H), (0, 0, 0)); pair2.paste(R, (0, 0)); pair2.paste(L, (W + 8, 0))
    pair2.save(os.path.join(out, pre + 'stereo_RL_crossview.png'))
    la, ra = np.asarray(L.convert('L')), np.asarray(R.convert('L'))
    Image.fromarray(np.dstack([la, ra, ra])).save(os.path.join(out, pre + 'anaglyph.png'))
    sheet = Image.new('RGB', (W * 3, H * 2), (0, 0, 0))
    for i, (lab, im) in enumerate(shots):
        sheet.paste(im, (i * W, 0))
        ImageDraw.Draw(sheet).text((i * W + 4, 4), lab, fill=(255, 255, 255))
    sheet.paste(pair.resize((int((W * 2 + 8) * 0.9), int(H * 0.9))), (0, H + 16))
    ImageDraw.Draw(sheet).text((4, H + 2), 'stereo pair (parallel view, L | R), logo yaw +%d' % sy_, fill=(255, 255, 255))
    sheet.save(os.path.join(out, pre + 'overview.png'))
    print('wrote previews to', out, '(%d gif frames)' % len(frames))


if __name__ == '__main__':
    main()
