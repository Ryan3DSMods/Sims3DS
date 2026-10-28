#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Build the HOME-menu 3D banner scene (glTF 2.0) from an STL logo model.

Default input: assets/logo/thesimslogo_red_backer.stl = "Sims game logo 3D" by 3DTwinkie (https://www.printables.com/@3DTwinkie) on Printables
(https://www.printables.com/model/1630221-sims-game-logo-3d), CC BY-SA 4.0 - see assets/logo/ATTRIBUTION.md.
The outputs (banner_scene*.gltf/.bin, banner_palette.png) are adaptations of that model and are CC BY-SA 4.0 too.
The "The Sims" logo design itself is a trademark of Electronic Arts.

Output (default: src/platform/3ds/banner3d/):
    banner_scene.gltf / banner_scene.bin / banner_palette.png   [+ banner_backdrop.png with --backdrop]

Scene layout (Epic0522's hardware-tested "Level 1/2" structure for pycgfx):
    COMMON                 root node (pycgfx names the CMDL "COMMON" anyway)
    +-- world              empty: per the guide everything under 'world' follows the HOME Menu's own rotation,
    |                      which we cannot limit, so nothing visible lives here
    +-- logo               the extruded "The Sims" logo (sibling of world), centred on its own vertical axis,
    |                      node translation (0, WORLD_Y, 0); rigid node animation 'sway':
    |                      yaw = SWAY_DEG * sin(2*pi*t/SWAY_PERIOD), 30 fps, explicit first/last frame,
    |                      seamless loop, LINEAR, T/R/S channels (T and S constant) -> pycgfx CANM 'COMMON',
    |                      ordinary Transform (5) member 'logo'.  No skin, no JOINTS/WEIGHTS.
    |   (--billboard-parent: COMMON -> name (YAxial billboard, identity, empty) -> logo, so that a HOME Menu
    |    rotation of the whole model is cancelled and only our +-SWAY_DEG sway remains - untested variant)
    +-- backdrop           (only with --backdrop) gradient card behind the logo; convert_banner3d.py sets
                           it to YAxial billboard (mode 5) so it always stays behind the logo
    --static: no animation (logo still a sibling of world, so it should not rotate at all)

Units: glTF/CGFX scene units.  HOME Menu camera (pycgfx banner-camera.gltf): position (0, 1, 44.786),
looking down -Z, yfov 30 deg, aspect 5:3, znear 26.5 -> at z = 0 the top screen shows x in [-20, 20],
y in [-11, 13]; nothing may come closer than z = 18.3 or it is clipped by the near plane.

Colours: baked 256x128 front-view texture (see make_rim_texture): white letters with a thin red outline; front
faces (normal.z > 0.8) and the inlay-step walls inside the letters map planar, the extrusion sides and the back
sample a solid red texel.  Lighting is left to the HOME Menu (diffuse
lighting needs the NORMAL attribute), so the material is matte: metallic 0, roughness 1 (+ the specular
stage is removed after conversion by convert_banner3d.py).

Needs numpy + Pillow + scipy (ndimage: hole filling, distance transform for the rim texture).
"""
import argparse, json, os, struct, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
DEFAULT_STL = os.path.join(REPO, 'assets', 'logo', 'thesimslogo_red_backer.stl')
DEFAULT_OUT = os.path.join(REPO, 'src', 'platform', '3ds', 'banner3d')

TARGET_WIDTH = 27.2      # logo width (scene units): largest that stays inside the HOME overlay bands over the +-25 deg sway
WORLD_Y = 1.4            # logo centre height (fits between the top title bar and the bottom strip)
SWAY_DEG = 25.0          # max yaw either side (user: 25 degrees)
SWAY_PERIOD = 5.0        # seconds per full left-right-left cycle
SWAY_FPS = 30
OUTLINE = True           # thin red outline built from GEOMETRY (v10 texture/material unchanged); False = v10
UV_RED, UV_WHITE = (0.25, 0.5), (0.75, 0.5)   # centres of the red / white halves of the 16x16 palette (v10)
UV_PER_MM = 0.045        # outline field -> u: u = 0.5 + 0.045 * F(mm), clamped to the palette
UV_MIN, UV_MAX = 1.0 / 32, 31.0 / 32           # stay inside the outermost texel centres
TEX_W, TEX_H = 256, 128  # resolution of the (internal, not exported) front-view outline field
TEX_MARGIN = 4
TEX_SS = 4               # supersampling of that field (0.16 mm per sample)
REFINE_MM = 1.2          # never split edges shorter than this
ISO_TOL_MM = 0.2        # split an edge crossing the outline if the field deviates this much from linear on it
LINE_MM = 1.9            # thin red outline along each letter's outer contour (~2.2 % of the big S's height)
LINE_OVERRIDES = [((95.0, 130.0, 60.0), 2.8)]   # (centre-x range mm, min height mm) -> width: the M
NO_LINE_MAX_H = 20.0     # single (ring-less) parts lower than this = "The": light red outline instead
LINE_THE_MM = 0.75       # "The" outline width (user: thin, same red as the rest)
UV_THE_V = 0.875         # palette row 14 (light red | white)
LINE_MIN_MM = 1.0        # >= 1.5 texels of the 256x128 texture so it survives RGBA4 + filtering
LINE_FRAC_HALF = 0.3     # thin strokes: 30 % of the half stroke width (but at least LINE_MIN_MM) ...
LINE_MAX_HALF = 0.6      # ... and never more than 60 % of it, so white always remains
WALL_INSIDE_MM = 1.0     # side walls projecting deeper than this inside a letter are inlay steps -> white
SMALL_POCKET_FRAC = 0.15  # pockets smaller than this fraction of their ring's outline are holes (window panes)
PANE_MIN_HALF_MM = 1.5    # a real pane is at least 3 mm wide
CLOSE_PX = 4               # morphological closing (supersampled px) to bridge ring/pocket seams
RING_MAX_HALF_MM = 1.6     # parts thinner than 2 x this and taller than 20 mm are outline rings (red)
CREASE_DEG = 40.0        # smooth red side walls below this angle, hard edges above
WHITE_NZ = 0.8           # face normal.z threshold for "letter face"
WHITE_MIN_Z_MM = 50.0    # letter faces lie above the base plate (base front face is at z = 47.6 mm)
RED = (238, 17, 34)      # exactly representable in RGBA4 (pycgfx's texture format)
WHITE = (255, 255, 238)
LIGHT_RED = RED          # "The" outline uses the normal red (user asked for thinner, not lighter)


def load_stl(path):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d, 80)[0]
    if 84 + 50 * n != len(d):
        raise SystemExit('%s: not a binary STL (size mismatch)' % path)
    a = np.frombuffer(d[84:84 + 50 * n], dtype=np.dtype([('n', '<3f4'), ('v', '<9f4'), ('a', '<u2')]))
    return a['v'].reshape(-1, 3, 3).astype(np.float64)


def build_logo(tris_mm):
    """-> positions (N,3) f32, normals (N,3) f32, uvs (N,2) f32, indices (M,) u16, info dict."""
    v = tris_mm.reshape(-1, 3)
    lo, hi = v.min(0), v.max(0)
    centre = (lo + hi) / 2
    scale = TARGET_WIDTH / (hi[0] - lo[0])
    fn = np.cross(tris_mm[:, 1] - tris_mm[:, 0], tris_mm[:, 2] - tris_mm[:, 0])
    area = np.linalg.norm(fn, axis=1)
    keep = area > 1e-10
    tris_mm, fn, area = tris_mm[keep], fn[keep], area[keep]
    fn = fn / area[:, None]
    zc = tris_mm[:, :, 2].mean(1)
    white = (fn[:, 2] > WHITE_NZ) & (zc > WHITE_MIN_Z_MM)
    front = fn[:, 2] > WHITE_NZ           # every front-facing face (letters + base plate) gets planar UVs

    # weld positions -> vertex ids
    q = np.round(tris_mm.reshape(-1, 3) * 1e4).astype(np.int64)
    _, pid = np.unique(q, axis=0, return_inverse=True)
    pid = pid.reshape(-1, 3)
    npos = pid.max() + 1
    # per-corner normal: area-weighted mean of incident faces of the same colour class within the crease angle
    cos_crease = np.cos(np.radians(CREASE_DEG))
    incident = [[] for _ in range(npos)]
    for t in range(len(pid)):
        for k in range(3):
            incident[pid[t, k]].append(t)
    corner_n = np.zeros((len(pid), 3, 3))
    for t in range(len(pid)):
        for k in range(3):
            if white[t]:
                corner_n[t, k] = fn[t]            # letter faces: flat (as v10)
                continue
            acc = np.zeros(3)
            for u in incident[pid[t, k]]:
                if white[u] == white[t] and fn[u] @ fn[t] >= cos_crease:
                    acc += fn[u] * area[u]
            corner_n[t, k] = acc / (np.linalg.norm(acc) + 1e-12)
    # build the welded vertex buffer keyed on (position, quantised normal, colour); v10 palette UVs
    p_scene = (tris_mm - centre) * scale
    pos, nrm, uvs, idx = [], [], [], []
    key_to_idx = {}

    def emit(p_mm, n, uv):
        key = (tuple(np.round(p_mm * 1e4).astype(np.int64)), tuple(np.round(n * 64).astype(int)),
               round(uv[0], 5), round(uv[1], 5))
        j = key_to_idx.get(key)
        if j is None:
            j = len(pos)
            key_to_idx[key] = j
            pos.append((p_mm - centre) * scale)
            nrm.append(n)
            uvs.append(uv)
        idx.append(j)

    extra = {}
    if OUTLINE:
        tris_out, extra = outline_geometry(tris_mm, fn, white, front, corner_n, lo, hi)
        for (p3, n3, u3) in tris_out:
            for k in range(3):
                emit(p3[k], n3[k], u3[k])
    else:                                  # exactly the v10 colouring: letter faces white, everything else red
        for t in range(len(pid)):
            for k in range(3):
                emit(tris_mm[t, k], corner_n[t, k], UV_WHITE if white[t] else UV_RED)
    pos = np.array(pos, np.float32)
    nrm = np.array(nrm, np.float32)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    uvs = np.array(uvs, np.float32)
    idx = np.array(idx, np.uint32)
    if len(pos) > 65535:
        raise SystemExit('too many vertices for 16-bit indices: %d' % len(pos))
    # sanity: winding agrees with the outward STL normals (glTF front face = CCW)
    tri = pos[idx.reshape(-1, 3)]
    wn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    tn = np.array(nrm, np.float64)[idx.reshape(-1, 3)].sum(1)
    agree = np.mean(np.sum(wn * tn, axis=1) > 0)        # winding vs. (outward) vertex normals
    # closedness: every undirected edge used exactly twice
    e = np.sort(np.concatenate([pid[:, [0, 1]], pid[:, [1, 2]], pid[:, [2, 0]]]), axis=1)
    _, cnt = np.unique(e, axis=0, return_counts=True)
    info = dict(outline=OUTLINE, **extra)
    info.update(triangles=len(idx) // 3, vertices=len(pos), white_tris=int(white.sum()),
                scale_units_per_mm=scale, size_units=((hi - lo) * scale).round(3).tolist(),
                winding_ccw_outward=float(agree), open_edges=int((cnt != 2).sum()))
    return pos, nrm, uvs, idx.astype(np.uint16), info


def make_palette(path, the_rows=True):
    """The v10 (hardware-tested) 16x16 RGBA palette: left half red, right half white.  v15 adds a light red in
    the left half of the bottom rows 12-15 (v >= 0.75 in glTF, rows from the image top) for the "The" outline;
    "The" samples v = 0.875 (row 14), everything else still v = 0.5 (rows 7/8 = unchanged red | white)."""
    im = Image.new('RGBA', (16, 16), RED + (255,))
    im.paste(Image.new('RGBA', (8, 16), WHITE + (255,)), (8, 0))
    if the_rows:
        im.paste(Image.new('RGBA', (8, 4), LIGHT_RED + (255,)), (0, 12))
    im.save(path)


def outline_geometry(tris, fn, white, front, corner_n, lo, hi):
    """Thin red outline in geometry.  Signed field F (mm, > 0 white) from make_rim_texture; letter faces and
    the inlay-step walls inside the letters are (1) refined where they approach the outline's inner edge -
    edges split at their midpoint, decided per edge so neighbours always agree (no T-junctions) - and (2) cut
    exactly along the F = 0 iso-line (crossing points are per-edge too).  Returns [(pos3 mm, normals3, red)]."""
    _, _, inside_mm, field, (panes, the_tris) = make_rim_texture(tris, fn, white, front, lo, hi)
    n_t = len(tris)
    planar = np.zeros(n_t, int)         # 0 fixed colour, 1 field on the red/white rows, 2 field on the light-red rows
    for t in range(n_t):
        if white[t] and t not in panes:
            planar[t] = 2 if t in the_tris else 1
    # every non-letter face (extrusion sides, inlay step walls, back, base plate) is solid red exactly as in v10
    fixed_red = {t: not white[t] for t in range(n_t)}                   # panes: white
    verts, vid = [], {}

    def vreg(p):
        k = tuple(np.round(p * 1e4).astype(np.int64))
        j = vid.get(k)
        if j is None:
            j = vid[k] = len(verts)
            verts.append(np.asarray(p, np.float64))
        return j
    fcache = {}

    def F(j):
        v = fcache.get(j)
        if v is None:
            v = fcache[j] = field(verts[j])
        return v
    # triangle: [ids(3), normals(3x3), planar, fixed_red]
    T = [[[vreg(tris[t, k]) for k in range(3)], corner_n[t].copy(), planar[t], fixed_red[t]] for t in range(n_t)]

    def near(a, b):
        """split an edge if the outline crosses it in a way a single linear cut would get wrong"""
        pa, pb = verts[a], verts[b]
        L = np.linalg.norm(pa - pb)
        if L <= REFINE_MM:
            return False
        ss = np.linspace(0, 1, 9)
        fs = np.array([field(pa + (pb - pa) * t) for t in ss])
        sg = fs >= 0
        changes = int((sg[1:] != sg[:-1]).sum())
        if changes == 0:
            return False                  # whole edge on one side (interior bumps ignored)
        if changes > 1:
            return True
        lin = fs[0] + (fs[-1] - fs[0]) * ss
        return bool(np.abs(fs - lin).max() > ISO_TOL_MM)
    for it in range(8):
        split = {}
        for ids, _, pl, _ in T:
            if not pl:
                continue
            for k in range(3):
                a, b = ids[k], ids[(k + 1) % 3]
                e = (min(a, b), max(a, b))
                if e not in split and near(a, b):
                    split[e] = None
        if not split:
            break
        for e in split:
            split[e] = vreg((verts[e[0]] + verts[e[1]]) / 2)
        out = []
        for ids, nr, pl, fr in T:
            m = [split.get((min(ids[k], ids[(k + 1) % 3]), max(ids[k], ids[(k + 1) % 3]))) for k in range(3)]
            nm = [None if m[k] is None else (nr[k] + nr[(k + 1) % 3]) for k in range(3)]
            nm = [None if x is None else x / (np.linalg.norm(x) + 1e-12) for x in nm]
            cnt = sum(x is not None for x in m)
            if cnt == 0:
                out.append([ids, nr, pl, fr]); continue
            if cnt == 3:
                a, b, c = ids
                out += [[[a, m[0], m[2]], np.array([nr[0], nm[0], nm[2]]), pl, fr],
                        [[m[0], b, m[1]], np.array([nm[0], nr[1], nm[1]]), pl, fr],
                        [[m[2], m[1], c], np.array([nm[2], nm[1], nr[2]]), pl, fr],
                        [[m[0], m[1], m[2]], np.array([nm[0], nm[1], nm[2]]), pl, fr]]
                continue
            # rotate so that edge 0 (ids[0]->ids[1]) is split
            r = next(k for k in range(3) if m[k] is not None)
            if cnt == 2 and m[(r + 2) % 3] is not None:
                r = (r + 2) % 3
            ids = ids[r:] + ids[:r]; nr = np.roll(nr, -r, axis=0); m = m[r:] + m[:r]; nm = nm[r:] + nm[:r]
            a, b, c = ids
            if cnt == 1:
                out += [[[a, m[0], c], np.array([nr[0], nm[0], nr[2]]), pl, fr],
                        [[m[0], b, c], np.array([nm[0], nr[1], nr[2]]), pl, fr]]
            else:                        # edges 0 (a-b) and 1 (b-c) split
                out += [[[m[0], b, m[1]], np.array([nm[0], nr[1], nm[1]]), pl, fr],
                        [[a, m[0], m[1]], np.array([nr[0], nm[0], nm[1]]), pl, fr],
                        [[a, m[1], c], np.array([nr[0], nm[1], nr[2]]), pl, fr]]
        T = out
    # colour by UV, no cutting: u = 0.5 + k*F on the 16x16 palette (red texels u < 0.5, white u > 0.5, nearest
    # filtering) -> the red/white boundary falls exactly on the linearly interpolated F = 0 line inside each
    # triangle, vertices stay shared, and the material/texture are byte-identical to v10
    res = []
    crossing = 0
    for ids, nr, pl, fr in T:
        P = [verts[i] for i in ids]
        if not pl:
            uv = UV_RED if fr else UV_WHITE
            res.append((P, nr, (uv, uv, uv)))
            continue
        f = [F(i) for i in ids]
        if min(f) < 0 <= max(f):
            crossing += 1
        v = UV_THE_V if pl == 2 else UV_RED[1]
        res.append((P, nr, tuple((float(np.clip(0.5 + UV_PER_MM * x, UV_MIN, UV_MAX)), v) for x in f)))
    keep = []
    for P, nr, u in res:
        if np.linalg.norm(np.cross(P[1] - P[0], P[2] - P[0])) > 1e-9:
            keep.append((P, nr / (np.linalg.norm(nr, axis=1, keepdims=True) + 1e-12), u))
    return keep, dict(outline_crossing_tris=crossing, refined_tris=len(T) - n_t)


def make_rim_texture(tris, fn, white, front, lo, hi):
    """Front-view (planar x,y) texture: white letters with a red rim of fixed width along each letter's outline.

    Letter parts = connected components of the white (letter-face) triangles.  Most STL letters are a raised
    ring (~2-3 mm) around a recessed inlay pocket; ring + pockets are merged into one letter silhouette, which is
    white except ONE thin red line (LINE_MM, capped on thin strokes) along its outer contour and real holes.
    Small pockets inside a normal face (window panes) are white holes framed by that line.  Per texel the
    topmost part (z-buffer) decides.  Everything outside the letters (base plate, background) is red.
    Returns (PIL RGBA image, function xyz_mm -> (u, v) in glTF convention, v = 0 at the image top)."""
    from scipy import ndimage
    W, H = TEX_W * TEX_SS, TEX_H * TEX_SS
    sx = (W - 2 * TEX_MARGIN * TEX_SS) / (hi[0] - lo[0])
    sy = (H - 2 * TEX_MARGIN * TEX_SS) / (hi[1] - lo[1])
    m = TEX_MARGIN * TEX_SS

    def to_px(p):                        # mm -> supersampled pixel coords (row 0 = top)
        return m + (p[..., 0] - lo[0]) * sx, m + (hi[1] - p[..., 1]) * sy

    def to_uv(p):
        x, y = to_px(np.asarray(p, np.float64))
        return (float(x / W), float(y / H))

    # connected components of letter faces (shared welded vertices)
    q = np.round(tris.reshape(-1, 3) * 1e4).astype(np.int64)
    _, pid = np.unique(q, axis=0, return_inverse=True)
    pid = pid.reshape(-1, 3)
    par = list(range(pid.max() + 1))

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    wt = np.where(white)[0]
    for t in wt:
        a, b, c = pid[t]
        par[find(b)] = find(a); par[find(c)] = find(a)
    comps = {}
    for t in wt:
        comps.setdefault(find(pid[t, 0]), []).append(t)
    comps = list(comps.values())

    # per-component mask + z-buffer raster (barycentric, pixel centres)
    gy, gx = np.mgrid[0:H, 0:W].astype(np.float64) + 0.5
    zbuf = np.full((H, W), -np.inf)
    owner = np.full((H, W), -1)
    masks = []
    for ci, ts in enumerate(comps):
        mk = np.zeros((H, W), bool)
        for t in ts:
            xs, ys = to_px(tris[t])
            x0, x1 = int(max(np.floor(xs.min()), 0)), int(min(np.ceil(xs.max()), W - 1))
            y0, y1 = int(max(np.floor(ys.min()), 0)), int(min(np.ceil(ys.max()), H - 1))
            if x0 > x1 or y0 > y1:
                continue
            X, Y = gx[y0:y1 + 1, x0:x1 + 1], gy[y0:y1 + 1, x0:x1 + 1]
            d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(d) < 1e-12:
                continue
            l0 = ((ys[1] - ys[2]) * (X - xs[2]) + (xs[2] - xs[1]) * (Y - ys[2])) / d
            l1 = ((ys[2] - ys[0]) * (X - xs[2]) + (xs[0] - xs[2]) * (Y - ys[2])) / d
            l2 = 1 - l0 - l1
            e = -1e-6
            inside = (l0 >= e) & (l1 >= e) & (l2 >= e)
            z = l0 * tris[t, 0, 2] + l1 * tris[t, 1, 2] + l2 * tris[t, 2, 2]
            mk[y0:y1 + 1, x0:x1 + 1] |= inside
            zb = zbuf[y0:y1 + 1, x0:x1 + 1]
            win = inside & (z > zb)
            zb[win] = z[win]
            owner[y0:y1 + 1, x0:x1 + 1][win] = ci
        masks.append(mk)
    # group pockets with the ring whose filled outline contains them (tightest containing ring, then its root)
    areas = [mk.sum() for mk in masks]
    filled = [ndimage.binary_fill_holes(ndimage.binary_closing(mk, iterations=3) | mk) for mk in masks]
    fa = [f.sum() for f in filled]
    parent = {}
    for i in range(len(comps)):
        cands = [j for j in range(len(comps)) if j != i and fa[j] > fa[i] and areas[i] and
                 (masks[i] & filled[j]).sum() >= 0.9 * areas[i]]
        parent[i] = min(cands, key=lambda j: fa[j]) if cands else None
    # A 'ring' = the STL's raised outline band around a recessed inlay pocket (pockets fill most of its outline,
    # or it is a long thin band).  Ring + its pockets form ONE letter: a single red band along the letter's
    # outer silhouette (and real holes/counters), everything inside white - the ring/pocket seam is not drawn.
    # Small pockets inside a normal face (window panes in the house) stay holes of that face (so the face's
    # band frames them, as in the reference) and are white themselves.
    child_area = {}
    for i, pnt in parent.items():
        if pnt is not None:
            child_area[pnt] = child_area.get(pnt, 0) + areas[i]
    half_of = {}
    for i in range(len(comps)):
        if areas[i]:
            half_of[i] = float(ndimage.distance_transform_edt(masks[i], sampling=(1 / sy, 1 / sx)).max())
    hgt = {}
    for i in range(len(comps)):
        rows = np.where(masks[i].any(1))[0]
        hgt[i] = (rows.max() - rows.min() + 1) / sy if len(rows) else 0
    ring = {j for j, ca in child_area.items() if ca >= 0.5 * fa[j]}
    ring |= {j for j in range(len(comps)) if j in child_area and half_of.get(j, 9) < RING_MAX_HALF_MM and hgt[j] > 20}
    letters = []                                   # (members, silhouette)
    done = set()
    pane_set = set()
    for j in ring:
        kids = [i for i in range(len(comps)) if parent[i] == j]
        small = [i for i in kids if areas[i] < SMALL_POCKET_FRAC * fa[j]    # window panes etc.: stay holes
                 and half_of.get(i, 0) >= PANE_MIN_HALF_MM]                # (thin slivers join the letter)
        white_only_r = set(small)
        mem = [j] + [i for i in kids if i not in white_only_r]
        sil = np.zeros((H, W), bool)
        for i in mem:
            sil |= masks[i]
        letters.append((mem, ndimage.binary_closing(sil, iterations=CLOSE_PX) | sil))
        done.update(mem)
        done.update(small)
        pane_set.update(small)
    white_only = set(pane_set)
    for i in range(len(comps)):
        if i in done or not areas[i]:
            continue
        if parent[i] is not None and parent[i] not in ring:
            white_only.add(i)                      # pane inside a face: white, framed by the face's band
            continue
        if half_of.get(i, 0) < 0.8:               # hair-thin sliver faces: plain white (as in v10)
            white_only.add(i)
            continue
        letters.append(([i], ndimage.binary_closing(masks[i], iterations=CLOSE_PX) | masks[i]))
    tex = np.zeros((H, W, 3))
    tex[:] = np.array(RED) / 255
    white = np.array(WHITE) / 255
    widths = {}
    plain = set()
    inside = np.zeros((H, W))
    fmap = np.full((H, W), -10.0)       # signed outline field in mm: > 0 white, < 0 red (outside: far)
    for mem, sil in letters:
        dist = ndimage.distance_transform_edt(sil, sampling=(1 / sy, 1 / sx))   # mm (anisotropic texels)
        inside = np.maximum(inside, dist)
        half = float(dist.max())
        rows = np.where(sil.any(1))[0]
        h_mm = (rows.max() - rows.min() + 1) / sy
        cx_mm = lo[0] + (np.where(sil.any(0))[0].mean() / W * W - m) / sx
        want = LINE_MM
        for (x0_, x1_, hmin), wmm in LINE_OVERRIDES:
            if x0_ <= cx_mm <= x1_ and h_mm >= hmin:
                want = wmm
        rim_mm = min(want, max(LINE_MIN_MM, LINE_FRAC_HALF * half), LINE_MAX_HALF * half)
        if len(mem) == 1 and h_mm < NO_LINE_MAX_H:          # "The": thin LIGHT red line (own palette rows)
            rim_mm = min(LINE_THE_MM, LINE_MAX_HALF * half)
            plain.update(mem)
        widths[mem[0]] = ('letter' if len(mem) > 1 else 'part', round(h_mm, 1), round(rim_mm, 2), round(cx_mm, 1))
        # owner == -1 inside the closed silhouette = rasterisation cracks along the ring/pocket seam
        sel = sil & (np.isin(owner, mem) | (owner < 0))
        tex[sel & (dist >= rim_mm)] = white
        fmap[sel] = (dist[sel] - rim_mm) if rim_mm > 0 else 2.0
    for i in white_only:
        tex[owner == i] = white
        fmap[owner == i] = 2.0
        widths[i] = ('pane', round(hgt[i], 1), 0.0)
    img = Image.fromarray((tex * 255).round().astype(np.uint8)).resize((TEX_W, TEX_H), Image.BOX)
    img = img.convert('RGBA')
    make_rim_texture.groups = widths

    def inside_mm(p):
        x, y = to_px(np.asarray(p, np.float64))
        return float(inside[min(max(int(y), 0), H - 1), min(max(int(x), 0), W - 1)])

    def field(p):                        # bilinear sample of fmap at a point (mm)
        x, y = to_px(np.asarray(p, np.float64))
        x, y = min(max(x - 0.5, 0), W - 1.001), min(max(y - 0.5, 0), H - 1.001)
        x0, y0 = int(x), int(y)
        fx, fy = x - x0, y - y0
        f = fmap[y0:y0 + 2, x0:x0 + 2]
        return float((f[0, 0] * (1 - fx) + f[0, 1] * fx) * (1 - fy) + (f[1, 0] * (1 - fx) + f[1, 1] * fx) * fy)
    panes = set()
    for i in set(white_only):           # panes, slivers: fixed white UV (no field sampling at the edge)
        panes.update(int(t) for t in comps[i])
    the_tris = set()
    for i in plain:
        the_tris.update(int(t) for t in comps[i])
    return img, to_uv, inside_mm, field, (panes, the_tris)


def make_backdrop_png(path):
    w, h = 128, 64
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    t = y / (h - 1)
    d = np.sqrt(((x - w / 2) / (w * 0.5)) ** 2 + ((y - h * 0.45) / (h * 0.6)) ** 2)
    g = np.clip(1 - d, 0, 1)
    r = 30 + 40 * t + 60 * g
    gg = 70 + 50 * t + 80 * g
    b = 150 + 60 * t + 70 * g
    a = np.full_like(r, 255)
    Image.fromarray(np.dstack([r, gg, b, a]).clip(0, 255).astype(np.uint8)).save(path)


class GltfBuilder:
    def __init__(self):
        self.bin = bytearray()
        self.views, self.accessors = [], []

    def add(self, arr, comp, typ, target=None, minmax=False):
        while len(self.bin) % 4:
            self.bin += b'\0'
        off = len(self.bin)
        data = np.ascontiguousarray(arr).tobytes()
        self.bin += data
        bv = dict(buffer=0, byteOffset=off, byteLength=len(data))
        if target:
            bv['target'] = target
        self.views.append(bv)
        a = dict(bufferView=len(self.views) - 1, componentType=comp, count=int(len(arr)), type=typ)
        if minmax:
            a['min'] = np.atleast_1d(np.asarray(arr).min(0)).astype(float).tolist()
            a['max'] = np.atleast_1d(np.asarray(arr).max(0)).astype(float).tolist()
        self.accessors.append(a)
        return len(self.accessors) - 1


def sway_keys():
    n = int(round(SWAY_PERIOD * SWAY_FPS))
    t = np.arange(n + 1, dtype=np.float64) / SWAY_FPS          # frames 0..n inclusive, key n == key 0
    yaw = np.radians(SWAY_DEG) * np.sin(2 * np.pi * t / SWAY_PERIOD)
    yaw[-1] = 0.0
    q = np.stack([np.zeros_like(yaw), np.sin(yaw / 2), np.zeros_like(yaw), np.cos(yaw / 2)], 1)   # x,y,z,w
    return t.astype(np.float32), q.astype(np.float32)


def write_gltf(out_dir, pos, nrm, uvs, idx, backdrop, animate=True, billboard_parent=False):
    g = GltfBuilder()
    meshes, materials, textures, images, nodes = [], [], [], [], []
    samplers = [dict(magFilter=9728, minFilter=9728, wrapS=33071, wrapT=33071)]   # nearest, clamp (as v10)
    images.append(dict(uri='banner_palette.png', name='COMMON1'))
    textures.append(dict(sampler=0, source=0))
    materials.append(dict(name='mt_sims_logo', doubleSided=False,
                          pbrMetallicRoughness=dict(baseColorTexture=dict(index=0), metallicFactor=0.0,
                                                    roughnessFactor=1.0)))
    prim = dict(attributes=dict(POSITION=g.add(pos, 5126, 'VEC3', 34962, True),
                                NORMAL=g.add(nrm, 5126, 'VEC3', 34962),
                                TEXCOORD_0=g.add(uvs, 5126, 'VEC2', 34962)),
                indices=g.add(idx, 5123, 'SCALAR', 34963), material=0, mode=4)
    meshes.append(dict(name='logo', primitives=[prim]))
    logo_t = [0.0, WORLD_Y, 0.0]
    nodes.append(dict(name='COMMON', children=[1, 2]))
    nodes.append(dict(name='world'))
    if billboard_parent:
        nodes.append(dict(name='name', children=[3]))
        nodes.append(dict(name='logo', mesh=0, translation=logo_t))
        logo_node = 3
    else:
        nodes.append(dict(name='logo', mesh=0, translation=logo_t))
        logo_node = 2
    animations = []
    if animate:
        t, q = sway_keys()
        ti = g.add(t, 5126, 'SCALAR', None, True)
        ri = g.add(q, 5126, 'VEC4')
        t2 = np.array([0.0, t[-1]], np.float32)
        t2i = g.add(t2, 5126, 'SCALAR', None, True)
        tri = g.add(np.array([logo_t, logo_t], np.float32), 5126, 'VEC3')
        sci = g.add(np.ones((2, 3), np.float32), 5126, 'VEC3')
        animations.append(dict(name='sway', samplers=[
            dict(input=ti, output=ri, interpolation='LINEAR'),
            dict(input=t2i, output=tri, interpolation='LINEAR'),
            dict(input=t2i, output=sci, interpolation='LINEAR')], channels=[
            dict(sampler=0, target=dict(node=logo_node, path='rotation')),
            dict(sampler=1, target=dict(node=logo_node, path='translation')),
            dict(sampler=2, target=dict(node=logo_node, path='scale'))]))
    if backdrop:
        # 38 x 19 card, 13 units behind (logo half-width is 12, so it never crosses it while rotating) the rotation axis, baked into the vertices (identity node, as the guide
        # requires for billboard nodes).  Facing +Z (towards the camera), CCW.
        z, w2, h2 = -13.0, 19.0, 9.5
        bp = np.array([[-w2, WORLD_Y - h2, z], [w2, WORLD_Y - h2, z], [w2, WORLD_Y + h2, z], [-w2, WORLD_Y + h2, z]], np.float32)
        bn = np.tile(np.array([[0, 0, 1]], np.float32), (4, 1))
        bt = np.array([[0, 1], [1, 1], [1, 0], [0, 0]], np.float32)   # glTF uv: v = 0 is the image top
        bi = np.array([0, 1, 2, 0, 2, 3], np.uint16)
        images.append(dict(uri='banner_backdrop.png', name='COMMON2'))
        samplers.append(dict(magFilter=9729, minFilter=9729, wrapS=33071, wrapT=33071))
        textures.append(dict(sampler=1, source=1))
        materials.append(dict(name='mt_backdrop', doubleSided=False,
                              pbrMetallicRoughness=dict(baseColorTexture=dict(index=1), metallicFactor=0.0,
                                                        roughnessFactor=1.0)))
        prim = dict(attributes=dict(POSITION=g.add(bp, 5126, 'VEC3', 34962, True),
                                    NORMAL=g.add(bn, 5126, 'VEC3', 34962),
                                    TEXCOORD_0=g.add(bt, 5126, 'VEC2', 34962)),
                    indices=g.add(bi, 5123, 'SCALAR', 34963), material=1, mode=4)
        meshes.append(dict(name='backdrop', primitives=[prim]))
        nodes.append(dict(name='backdrop', mesh=1))
        nodes[0]['children'].append(len(nodes) - 1)
    doc = dict(asset=dict(version='2.0', generator='sims3ds tools/banner3d/make_scene.py'),
               scene=0, scenes=[dict(name='COMMON', nodes=[0])], nodes=nodes, meshes=meshes,
               materials=materials, textures=textures, images=images, samplers=samplers,
               accessors=g.accessors, bufferViews=g.views,
               buffers=[dict(uri='banner_scene.bin', byteLength=len(g.bin))])
    # banner_scene_static.gltf: identical scene without the animation (same .bin; the animation accessors
    # are simply unused) - make_banner3d.sh falls back to it if the animated conversion fails.
    with open(os.path.join(out_dir, 'banner_scene_static.gltf'), 'w') as f:
        json.dump(doc, f, indent=1)
    if animations:
        doc['animations'] = animations
    with open(os.path.join(out_dir, 'banner_scene.bin'), 'wb') as f:
        f.write(g.bin)
    with open(os.path.join(out_dir, 'banner_scene.gltf'), 'w') as f:
        json.dump(doc, f, indent=1)
    return len(g.bin)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stl', default=DEFAULT_STL)
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--backdrop', action='store_true', help='add a billboarded gradient card behind the logo')
    ap.add_argument('--static', action='store_true', help='no sway animation')
    ap.add_argument('--no-outline', action='store_true', help='v10 look: letter faces white, no red outline')
    ap.add_argument('--billboard-parent', action='store_true',
                    help="put the logo under an empty YAxial-billboard node 'name' (cancels a whole-model HOME rotation)")
    a = ap.parse_args()
    global OUTLINE
    if a.no_outline:
        OUTLINE = False
    os.makedirs(a.out, exist_ok=True)
    if not os.path.exists(a.stl):
        raise SystemExit('STL not found: %s\nExpected the model in assets/logo/ (see assets/logo/ATTRIBUTION.md) or pass --stl <file>.' % a.stl)
    pos, nrm, uvs, idx, info = build_logo(load_stl(a.stl))
    make_palette(os.path.join(a.out, 'banner_palette.png'), the_rows=OUTLINE)
    if OUTLINE:
        info['outline_parts_kind_h_mm_line_mm'] = sorted(make_rim_texture.groups.values(), key=lambda x: -x[1])
    if a.backdrop:
        make_backdrop_png(os.path.join(a.out, 'banner_backdrop.png'))
    elif os.path.exists(os.path.join(a.out, 'banner_backdrop.png')):
        os.remove(os.path.join(a.out, 'banner_backdrop.png'))
    nbin = write_gltf(a.out, pos, nrm, uvs, idx, a.backdrop, not a.static, a.billboard_parent)
    # pycgfx stores position/normal/uv as float streams + u16 indices; texture 16x16 RGBA4
    est = info["vertices"] * 32 + len(idx) * 2 + 16 * 16 * 2 + 12000
    info.update(bin_bytes=nbin, estimated_cgfx_bytes=est + (0 if a.static else 4000), backdrop=a.backdrop,
                animation=None if a.static else 'yaw +-%g deg, %g s, %d fps, %d keys' % (
                    SWAY_DEG, SWAY_PERIOD, SWAY_FPS, int(round(SWAY_PERIOD * SWAY_FPS)) + 1),
                billboard_parent=a.billboard_parent)
    print(json.dumps(info, indent=1))
    if info['open_edges']:
        print('WARNING: mesh is not closed (%d open/non-manifold edges)' % info['open_edges'])
    if est > 0x7C000:
        print('WARNING: estimated CGFX size is close to / above the 512 KiB HOME Menu limit')


if __name__ == '__main__':
    main()
