#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Independent CGFX (NintendoWare CTR graphics) parser / validator.

Only what a HOME-menu banner uses is decoded in depth: header, DATA, DICT (incl. patricia tree check),
CMDL (animation-group descriptions, meshes, materials (MTOB + texture mappers + SHDR + fragment shader),
SOBJ shapes (primitive sets, index streams, interleaved vertex streams, vertex params), mesh-node
visibilities) and TXOB image textures (IMAG data, tiled/Morton pixel formats).

Every self-relative pointer that is followed is recorded and checked (inside the file, inside the right
block, alignment).  `python3 cgfx_parse.py file.cgfx` prints a dump + all problems found.
Layout knowledge: 3dbrew CGFX page, EveryFileExplorer (Gericom) 3DS/NintendoWare/GFX/*.cs, SPICA.
"""
import struct, sys

FMT_NAMES = {0: 'RGBA8', 1: 'RGB8', 2: 'RGBA5551', 3: 'RGB565', 4: 'RGBA4', 5: 'LA8', 6: 'HILO8', 7: 'L8',
             8: 'A8', 9: 'LA4', 10: 'L4', 11: 'A4', 12: 'ETC1', 13: 'ETC1A4'}
FMT_BPP = {0: 32, 1: 24, 2: 16, 3: 16, 4: 16, 5: 16, 6: 16, 7: 8, 8: 8, 9: 8, 10: 4, 11: 4, 12: 4, 13: 8}
GLTYPE = {0x1400: ('b', 1), 0x1401: ('B', 1), 0x1402: ('h', 2), 0x1406: ('f', 4)}
SLOT_NAMES = ['models', 'textures', 'luts', 'materials', 'shaders', 'cameras', 'lights', 'fogs', 'scenes',
              'skeletal anims', 'material anims', 'visibility anims', 'camera anims', 'light anims', 'emitters']
USAGE = ['Position', 'Normal', 'Tangent', 'Color', 'TexCoord0', 'TexCoord1', 'TexCoord2', 'BoneIndex',
         'BoneWeight']


class CgfxError(Exception):
    pass


class Reader:
    def __init__(self, data):
        self.d = data
        self.problems = []
        self.pointers = []          # (field_addr, target, what)
        self.ranges = []            # (start, end, what) object extents we decoded (for overlap checks)
        self.notes = []             # informational (not decoded parts)

    def u8(self, o): return self.d[o]
    def u16(self, o): return struct.unpack_from('<H', self.d, o)[0]
    def s16(self, o): return struct.unpack_from('<h', self.d, o)[0]
    def u32(self, o): return struct.unpack_from('<I', self.d, o)[0]
    def s32(self, o): return struct.unpack_from('<i', self.d, o)[0]
    def f32(self, o): return struct.unpack_from('<f', self.d, o)[0]
    def fv(self, o, n): return list(struct.unpack_from('<%df' % n, self.d, o))
    def magic(self, o): return self.d[o:o + 4].decode('latin1')

    def err(self, msg):
        self.problems.append(msg)

    def ptr(self, o, what, optional=False, signed=False):
        v = self.s32(o) if signed else self.u32(o)
        if v == 0:
            if not optional:
                self.err('null pointer %s @0x%x' % (what, o))
            return 0
        t = o + v
        self.pointers.append((o, t, what))
        if not (0 <= t < len(self.d)):
            self.err('pointer %s @0x%x -> 0x%x outside file (size 0x%x)' % (what, o, t, len(self.d)))
            raise CgfxError('bad pointer ' + what)
        return t

    def cstr(self, o):
        e = self.d.index(b'\0', o)
        return self.d[o:e].decode('ascii')

    def strp(self, o, what, optional=False):
        t = self.ptr(o, what + '.name', optional)
        return self.cstr(t) if t else None


# ----------------------------------------------------------------------------------------------- DICT
def patricia_check(names, nodes):
    """Re-implements the NW4C patricia lookup and verifies every name is found via the tree."""
    maxlen = max((len(n) for n in names), default=0)

    def bit(name, b):
        i = b >> 3
        if i >= len(name):
            return 0
        return (ord(name[i]) >> (b & 7)) & 1  # NW4C: byte index from the start, LSB-first (EFE GetBit)

    bad = []
    for want in names:
        # NW4C lookup: start at root's left child, descend while refbit decreases
        cur = nodes[0]
        nxt = nodes[cur['left']]
        prev_ref = cur['refbit']
        steps = 0
        while nxt['refbit'] < prev_ref and steps < 64:
            prev_ref = nxt['refbit']
            # names are padded to the dict's max length; bits are counted from the *end*? see note below
            b = nxt['refbit']
            nxt = nodes[nxt['right'] if bit_end(want, b, maxlen) else nxt['left']]
            steps += 1
        if nxt.get('name') != want:
            bad.append(want)
    return bad


def bit_end(name, b, length):
    """NW4C refbit: bit b counts from the end of the (zero padded to `length`) string, i.e. byte
    index (length-1 - b//8) when reading the name reversed.  EFE pads names to the longest length and
    GetBit uses name[bit/8] - which is the same thing because EFE's padded string is compared reversed?
    We implement the variant that the official files use: byte = name[len-1-(b>>3)]... resolved by
    trying the template (see validate) - this function implements the EFE convention exactly."""
    i = b >> 3
    s = name.ljust(length, '\0')
    if i >= len(s):
        return 0
    return (ord(s[i]) >> (b & 7)) & 1


def read_dict(r, o, what):
    if r.magic(o) != 'DICT':
        r.err('%s: expected DICT at 0x%x, got %r' % (what, o, r.magic(o)))
        raise CgfxError('no DICT')
    size = r.u32(o + 4)
    n = r.u32(o + 8)
    if size != 0x1C + 0x10 * n:
        r.err('%s: DICT size 0x%x != 0x1c+0x10*%d' % (what, size, n))
    nodes = []
    for i in range(n + 1):
        no = o + 0xC + 0x10 * i
        nd = dict(refbit=r.u32(no), left=r.u16(no + 4), right=r.u16(no + 6))
        if i == 0:
            if nd['refbit'] != 0xFFFFFFFF:
                r.err('%s: root refbit 0x%x' % (what, nd['refbit']))
            if r.u32(no + 8) or r.u32(no + 12):
                r.err('%s: root node has name/data' % what)
        else:
            nd['name'] = r.strp(no + 8, what + '[%d]' % (i - 1))
            nd['data'] = r.ptr(no + 12, what + '[%s].data' % nd['name'])
        if nd['left'] > n or nd['right'] > n:
            r.err('%s: node %d child index out of range' % (what, i))
        nodes.append(nd)
    r.ranges.append((o, o + size, 'DICT ' + what))
    names = [nd['name'] for nd in nodes[1:]]
    if len(set(names)) != len(names):
        r.err('%s: duplicate names' % what)
    bad = patricia_check(names, nodes)
    if bad:
        r.err('%s: patricia lookup fails for %s' % (what, bad))
    return [(nd['name'], nd['data']) for nd in nodes[1:]], nodes


# ----------------------------------------------------------------------------------------------- TXOB
def tiled_index(x, y, w):
    return (((y >> 3) * (w >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                                     ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))


def decode_texture(data, w, h, fmt):
    """Decode a tiled texture to an RGBA numpy array (row 0 = top of the image as the GPU samples it with
    t=1; the PICA's t axis goes bottom->top, NW4C stores row 0 of the tiles at t=1?  we return the image in
    memory order: tile row 0 = first rows in memory = image TOP as stored by bannertool)."""
    import numpy as np
    out = np.zeros((h, w, 4), np.uint8)
    if fmt in (12, 13):
        from etc1 import decode_etc1
        return decode_etc1(data, w, h, fmt == 13)
    bpp = FMT_BPP[fmt] // 8
    for y in range(h):
        for x in range(w):
            i = tiled_index(x, y, w) * bpp
            if fmt == 4:      # RGBA4 (stored as u16 R<<12|G<<8|B<<4|A)
                v = data[i] | data[i + 1] << 8
                out[y, x] = [((v >> 12) & 15) * 17, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17]
            elif fmt == 3:    # RGB565
                v = data[i] | data[i + 1] << 8
                out[y, x] = [((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, 255]
            elif fmt == 0:    # RGBA8 stored A,B,G,R
                out[y, x] = [data[i + 3], data[i + 2], data[i + 1], data[i]]
            elif fmt == 1:    # RGB8 stored B,G,R
                out[y, x] = [data[i + 2], data[i + 1], data[i], 255]
            elif fmt == 2:    # RGBA5551
                v = data[i] | data[i + 1] << 8
                out[y, x] = [((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31,
                             255 * (v & 1)]
            else:
                raise CgfxError('decode of format %d not implemented' % fmt)
    return out


def read_txob(r, o, what, imag):
    t = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'TXOB':
        r.err('%s: not TXOB' % what)
        raise CgfxError('TXOB')
    t['revision'] = r.u32(o + 8)
    t['name'] = r.strp(o + 0xC, what)
    if t['type'] == 0x20000004:                       # reference texture
        t['kind'] = 'reference'
        t['linked'] = r.strp(o + 0x18, what + '.link')
        t['linked_ptr'] = r.u32(o + 0x1C)
        r.ranges.append((o, o + 0x20, 'TXOB-ref ' + what))
    elif t['type'] == 0x20000011:                     # image texture
        t['kind'] = 'image'
        t['height'], t['width'], t['glformat'], t['gltype'], t['levels'], t['texobj'], t['locflag'], t['hwformat'] = \
            struct.unpack_from('<8I', r.d, o + 0x18)
        img = r.ptr(o + 0x38, what + '.image')
        t['img_at'] = img
        ih, iw, size = r.u32(img), r.u32(img + 4), r.u32(img + 8)
        dp = r.ptr(img + 0xC, what + '.pixels')
        t['img_dynalloc'], t['bpp'], t['img_locaddr'], t['img_memaddr'] = struct.unpack_from('<4I', r.d, img + 0x10)
        t['data_at'], t['data_size'] = dp, size
        r.ranges.append((o, o + 0x3C, 'TXOB ' + what))
        r.ranges.append((img, img + 0x20, 'TXOB image ' + what))
        w, h, f = t['width'], t['height'], t['hwformat']
        if (iw, ih) != (w, h):
            r.err('%s: image ctr size %dx%d != texture %dx%d' % (what, iw, ih, w, h))
        for v, n in ((w, 'width'), (h, 'height')):
            if v < 8 or v > 1024 or v & (v - 1):
                r.err('%s: %s %d not a power of two in 8..1024' % (what, n, v))
        if f not in FMT_BPP:
            r.err('%s: unknown hw format %d' % (what, f))
        else:
            exp = sum((w >> l) * (h >> l) for l in range(t['levels'])) * FMT_BPP[f] // 8
            if size != exp:
                r.err('%s: data size 0x%x expected 0x%x' % (what, size, exp))
            if t['bpp'] != FMT_BPP[f]:
                r.err('%s: bpp field %d != %d' % (what, t['bpp'], FMT_BPP[f]))
        if imag and not (imag[0] <= dp and dp + size <= imag[1]):
            r.err('%s: pixel data 0x%x..0x%x not inside IMAG' % (what, dp, dp + size))
        if dp % 128:
            r.err('%s: pixel data 0x%x not 128-byte aligned' % (what, dp))
        if dp + size > len(r.d):
            r.err('%s: pixel data past EOF' % what)
        t['pixels'] = r.d[dp:dp + size]
    else:
        t['kind'] = 'type 0x%x' % t['type']
        r.err('%s: unsupported TXOB type 0x%x' % (what, t['type']))
    return t


# ----------------------------------------------------------------------------------------------- MTOB
def read_texmapper(r, o, what, owner):
    m = dict(at=o, type=r.u32(o), dynalloc=r.u32(o + 4))
    m['txob'] = read_txob(r, r.ptr(o + 8, what + '.txob'), what + '.txob', None)
    s = r.ptr(o + 0xC, what + '.sampler')
    m['cmd'] = list(struct.unpack_from('<14I', r.d, o + 0x10))
    m['cmd_size'] = r.u32(o + 0x48)
    # sampler: type, owner (signed self-relative back pointer), minfilter, border(4f), lodbias
    m['sampler_type'] = r.u32(s)
    so = s + 4 + r.s32(s + 4)
    if so != o:
        r.err('%s: sampler owner -> 0x%x, expected texmapper 0x%x' % (what, so, o))
    m['minfilter'] = r.u32(s + 8)
    m['border'] = r.fv(s + 0xC, 4)
    m['lodbias'] = r.f32(s + 0x1C)
    r.ranges.append((o, o + 0x4C, 'TexMapper ' + what))
    r.ranges.append((s, s + 0x20, 'Sampler ' + what))
    pica_check_texmapper(r, m, what)
    return m


def pica_check_texmapper(r, m, what):
    """The texture mapper carries a ready-made PICA200 command list for texture unit 0:
       words: [?] [hdr 0x8E? ...] - we decode the standard (value, header) pairs and check the size reg 0x82
       matches the referenced texture later (done in validate)."""
    words = m['cmd']
    regs = {}
    i = 0
    while i + 1 < len(words):
        val, hdr = words[i], words[i + 1]
        reg = hdr & 0xFFFF
        n = (hdr >> 20) & 0x7FF
        consec = hdr >> 31
        if hdr == 0 and val == 0:
            break
        params = [val] + words[i + 2:i + 2 + n]
        for k, p in enumerate(params):
            regs[reg + (k if consec else 0)] = p
        i += 2 + n + (n & 1)   # parameters are padded to 8 bytes
    m['regs'] = regs


def read_shdr(r, o, what):
    s = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'SHDR':
        r.err('%s: not SHDR' % what)
    s['name'] = r.strp(o + 0xC, what, optional=True)
    s['linked'] = r.strp(o + 0x18, what + '.linked')
    r.ranges.append((o, o + 0x20, 'SHDR ' + what))
    return s


def read_fragshader(r, o, what):
    f = dict(at=o)
    f['buffer_color'] = r.fv(o, 4)
    f['lighting'] = list(struct.unpack_from('<6I', r.d, o + 0x10))
    lt = r.ptr(o + 0x28, what + '.lighting_table')
    f['combiners'] = []
    for i in range(6):
        c = o + 0x2C + i * 0x1C
        f['combiners'].append(dict(constant=r.u32(c), src_rgb=r.u16(c + 4), src_a=r.u16(c + 6),
                                   addr=r.u32(c + 8), operands=r.u32(c + 0xC), comb_rgb=r.u16(c + 0x10),
                                   comb_a=r.u16(c + 0x12), const_rgba=r.u32(c + 0x14), scale_rgb=r.u16(c + 0x18),
                                   scale_a=r.u16(c + 0x1A)))
    at = o + 0x2C + 6 * 0x1C
    f['alpha_test'] = (r.u32(at), r.u32(at + 4))
    f['buffer_cmds'] = list(struct.unpack_from('<6I', r.d, at + 8))
    end = at + 8 + 24
    r.ranges.append((o, end, 'FragShader ' + what))
    f['lut_ptrs'] = []
    for i in range(6):
        p = r.ptr(lt + 4 * i, what + '.lut%d' % i, optional=True)
        f['lut_ptrs'].append(p)
    r.ranges.append((lt, lt + 24, 'FragLightTable ' + what))
    return f


def read_mtob(r, o, what):
    m = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'MTOB':
        r.err('%s: not MTOB' % what)
        raise CgfxError('MTOB')
    m['revision'] = r.u32(o + 8)
    m['name'] = r.strp(o + 0xC, what)
    m['flags'], m['texcoord_cfg'], m['translucency'] = struct.unpack_from('<3I', r.d, o + 0x18)
    mc = o + 0x24
    m['colors_f'] = [r.fv(mc + 16 * i, 4) for i in range(11)]
    m['colors_u8'] = list(struct.unpack_from('<11I', r.d, mc + 0xB0))
    rz = mc + 0xB0 + 0x2C + 4
    m['raster'] = dict(flags=r.u32(rz), cull=r.u32(rz + 4), poly_off=r.f32(rz + 8), cmd=(r.u32(rz + 0xC), r.u32(rz + 0x10)))
    fo = rz + 0x14
    m['depth'] = list(struct.unpack_from('<5I', r.d, fo))
    m['blend_mode'] = r.u32(fo + 0x14)
    m['blend_color'] = r.fv(fo + 0x18, 4)
    m['blend_cmds'] = list(struct.unpack_from('<6I', r.d, fo + 0x28))
    m['stencil'] = list(struct.unpack_from('<4I', r.d, fo + 0x40))
    tc = fo + 0x50
    m['n_texcoords'] = r.u32(tc)
    m['texcoords'] = []
    for i in range(3):
        c = tc + 4 + i * 0x58
        m['texcoords'].append(dict(src=r.u32(c), method=r.u32(c + 4), camera=r.s32(c + 8), mtxmode=r.u32(c + 0xC),
                                   scale=r.fv(c + 0x10, 2), rotate=r.f32(c + 0x18), translate=r.fv(c + 0x1C, 2)))
    p = tc + 4 + 3 * 0x58
    m['texmappers'] = []
    for i in range(3):
        t = r.ptr(p + 4 * i, what + '.texmapper%d' % i, optional=True)
        m['texmappers'].append(read_texmapper(r, t, what + '.tm%d' % i, o) if t else None)
    if r.u32(p + 12):
        r.err('%s: procedural texture mapper present (unsupported)' % what)
    m['shader'] = read_shdr(r, r.ptr(p + 16, what + '.shader'), what + '.shdr')
    m['frag'] = read_fragshader(r, r.ptr(p + 20, what + '.frag'), what + '.frag')
    m['shader_prog_idx'], m['n_shader_params'], m['shader_params_ptr'], m['lightset'], m['fog'] = \
        struct.unpack_from('<5I', r.d, p + 24)
    m['hashes_at'] = p + 44
    m['hashes'] = list(struct.unpack_from('<13I', r.d, p + 44))
    m['material_id'] = r.u32(p + 44 + 52)
    m['end'] = p + 44 + 56
    r.ranges.append((o, m['end'], 'MTOB ' + what))
    return m


# ----------------------------------------------------------------------------------------------- SOBJ
def read_mesh(r, o, what, model_at):
    m = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'SOBJ':
        r.err('%s: not SOBJ' % what)
        raise CgfxError('mesh')
    m['name'] = r.strp(o + 0xC, what, optional=True)
    m['shape_idx'], m['material_idx'] = r.u32(o + 0x18), r.u32(o + 0x1C)
    own = o + 0x20 + r.s32(o + 0x20)
    if own != model_at:
        r.err('%s: owner -> 0x%x != model 0x%x' % (what, own, model_at))
    m['visible'], m['priority'], m['node_vis_idx'] = r.u8(o + 0x24), r.u8(o + 0x25), r.s16(o + 0x26)
    m['raw'] = list(struct.unpack_from('<18I', r.d, o + 0x28))
    m['node_name'] = r.strp(o + 0x70, what + '.node', optional=True)
    m['tail'] = list(struct.unpack_from('<3I', r.d, o + 0x74))
    r.ranges.append((o, o + 0x80, 'Mesh ' + what))
    return m


def read_bbox(r, o, what):
    t = r.u32(o)
    if t == 0x80000000:
        r.ranges.append((o, o + 0x40, 'OBB ' + what))
        return dict(type=t, center=r.fv(o + 4, 3), orient=r.fv(o + 0x10, 9), size=r.fv(o + 0x34, 3))
    r.err('%s: bounding volume type 0x%x' % (what, t))
    return dict(type=t)


def read_shape(r, o, what, imag):
    s = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'SOBJ':
        r.err('%s: not SOBJ' % what)
        raise CgfxError('shape')
    s['name'] = r.strp(o + 0xC, what, optional=True)
    s['flags'] = r.u32(o + 0x18)
    s['bbox'] = read_bbox(r, r.ptr(o + 0x1C, what + '.bbox'), what)
    s['pos_offset'] = r.fv(o + 0x20, 3)
    nps = r.u32(o + 0x2C)
    psa = r.ptr(o + 0x30, what + '.primsets')
    s['base_address'] = r.u32(o + 0x34)
    nva = r.u32(o + 0x38)
    vaa = r.ptr(o + 0x3C, what + '.vtxattrs')
    s['blendshape'] = r.u32(o + 0x40)
    r.ranges.append((o, o + 0x44, 'Shape ' + what))
    s['primsets'] = []
    for i in range(nps):
        po = r.ptr(psa + 4 * i, what + '.ps%d' % i)
        ps = dict(at=po, n_bones=r.u32(po), skinning=r.u32(po + 8))
        if ps['skinning'] != 0:
            r.err('%s: skinning mode %d (must be 0 for the real HOME Menu)' % (what, ps['skinning']))
        bones = r.ptr(po + 4, what + '.ps.bones', optional=True)
        ps['bones'] = [r.u32(bones + 4 * k) for k in range(ps['n_bones'])] if bones else []
        npr = r.u32(po + 0xC)
        pa = r.ptr(po + 0x10, what + '.ps.prims')
        r.ranges.append((po, po + 0x14, 'PrimSet ' + what))
        ps['prims'] = []
        for k in range(npr):
            pro = r.ptr(pa + 4 * k, what + '.prim')
            pr = dict(at=pro)
            nis = r.u32(pro)
            isa = r.ptr(pro + 4, what + '.prim.idxstreams')
            nbo = r.u32(pro + 8)
            boa = r.ptr(pro + 0xC, what + '.prim.bufobjs', optional=(nbo == 0))
            pr['bufobjs'] = [r.u32(boa + 4 * j) for j in range(nbo)]
            pr['flags'], pr['cmd_alloc'] = r.u32(pro + 0x10), r.u32(pro + 0x14)
            r.ranges.append((pro, pro + 0x18, 'Primitive ' + what))
            pr['streams'] = []
            for j in range(nis):
                io = r.ptr(isa + 4 * j, what + '.idx')
                st = dict(at=io, fmt=r.u32(io), mode=r.u8(io + 4), visible=r.u8(io + 5))
                st['length'] = r.u32(io + 8)
                dp = r.ptr(io + 0xC, what + '.idx.data')
                st['data_at'] = dp
                st['rt'] = list(struct.unpack_from('<6I', r.d, io + 0x10))   # buffer obj, loc flag, cmd cache, size, locaddr, memarea
                st['bbox_ptr'] = r.u32(io + 0x28)
                r.ranges.append((io, io + 0x2C, 'IndexStream ' + what))
                if st['fmt'] == 0x1401:
                    n = st['length']
                    st['indices'] = list(r.d[dp:dp + n])
                elif st['fmt'] == 0x1403:
                    if st['length'] & 1:
                        r.err('%s: u16 index stream with odd length' % what)
                    n = st['length'] // 2
                    st['indices'] = list(struct.unpack_from('<%dH' % n, r.d, dp))
                else:
                    r.err('%s: index format 0x%x' % (what, st['fmt']))
                    st['indices'] = []
                if st['mode'] != 0:
                    r.err('%s: primitive mode %d (only 0 = triangles checked)' % (what, st['mode']))
                if len(st['indices']) % 3:
                    r.err('%s: index count %d not a multiple of 3' % (what, len(st['indices'])))
                if imag and not (imag[0] <= dp and dp + st['length'] <= imag[1]):
                    r.err('%s: index data 0x%x not inside IMAG' % (what, dp))
                if any(st['rt']):
                    r.err('%s: runtime fields of index stream not zero: %s' % (what, st['rt']))
                pr['streams'].append(st)
            ps['prims'].append(pr)
        s['primsets'].append(ps)
    s['attrs'] = []
    for i in range(nva):
        ao = r.ptr(vaa + 4 * i, what + '.attr%d' % i)
        a = dict(at=ao, type=r.u32(ao), usage=r.u32(ao + 4), flags=r.u32(ao + 8))
        if a['type'] == 0x40000002:          # interleaved vertex stream
            a['bufobj'], a['locflag'], a['length'] = r.u32(ao + 0xC), r.u32(ao + 0x10), r.u32(ao + 0x14)
            dp = r.ptr(ao + 0x18, what + '.vtx.data')
            a['data_at'] = dp
            a['locaddr'], a['memarea'] = r.u32(ao + 0x1C), r.u32(ao + 0x20)
            a['stride'] = r.u32(ao + 0x24)
            ns = r.u32(ao + 0x28)
            sa = r.ptr(ao + 0x2C, what + '.vtx.streams')
            r.ranges.append((ao, ao + 0x30, 'VtxStream ' + what))
            if imag and not (imag[0] <= dp and dp + a['length'] <= imag[1]):
                r.err('%s: vertex data 0x%x not inside IMAG' % (what, dp))
            a['streams'] = []
            for k in range(ns):
                so = r.ptr(sa + 4 * k, what + '.vtx.stream')
                st = dict(at=so, type=r.u32(so), usage=r.u32(so + 4), flags=r.u32(so + 8),
                          rt=list(struct.unpack_from('<6I', r.d, so + 0xC)),
                          fmt=r.u32(so + 0x24), ncomp=r.u32(so + 0x28), scale=r.f32(so + 0x2C), offset=r.u32(so + 0x30))
                r.ranges.append((so, so + 0x34, 'VtxAttr ' + what))
                if st['fmt'] not in GLTYPE:
                    r.err('%s: vertex attr format 0x%x' % (what, st['fmt']))
                a['streams'].append(st)
            # decode vertices
            n = a['length'] // a['stride'] if a['stride'] else 0
            if a['stride'] and a['length'] % a['stride']:
                r.err('%s: vertex buffer length %d not a multiple of stride %d' % (what, a['length'], a['stride']))
            verts = {}
            for st in a['streams']:
                code, sz = GLTYPE.get(st['fmt'], ('B', 1))
                if st['offset'] + sz * st['ncomp'] > a['stride']:
                    r.err('%s: attribute exceeds stride' % what)
                if st['fmt'] != 0x1406 and (st['offset'] % sz):
                    r.err('%s: attribute misaligned' % what)
                if st['fmt'] == 0x1406 and st['offset'] % 4:
                    r.err('%s: float attribute misaligned' % what)
                vals = []
                for v in range(n):
                    vals.append([x * (st['scale'] if st['fmt'] != 0x1406 else 1.0) for x in
                                 struct.unpack_from('<%d%s' % (st['ncomp'], code), r.d, dp + v * a['stride'] + st['offset'])])
                verts[USAGE[st['usage']] if st['usage'] < len(USAGE) else st['usage']] = vals
            a['n_vertices'] = n
            a['vertices'] = verts
        elif a['type'] == 0x40000001:        # stand-alone (non-interleaved) vertex stream, as pycgfx writes
            a['length'] = r.u32(ao + 0x14)
            dp = r.ptr(ao + 0x18, what + '.vtx.data')
            a['data_at'] = dp
            a['fmt'], a['ncomp'], a['scale'], a['offset'] = r.u32(ao + 0x24), r.u32(ao + 0x28), r.f32(ao + 0x2C), r.u32(ao + 0x30)
            r.ranges.append((ao, ao + 0x34, 'VtxStream1 ' + what))
            if a['fmt'] not in GLTYPE:
                r.err('%s: vertex stream format 0x%x' % (what, a['fmt']))
            else:
                code, sz = GLTYPE[a['fmt']]
                stride = sz * a['ncomp']
                if a['length'] % stride:
                    r.err('%s: stream length %d not a multiple of %d' % (what, a['length'], stride))
                n = a['length'] // stride
                sc = a['scale'] if a['fmt'] != 0x1406 else 1.0
                vals = [[x * sc for x in struct.unpack_from('<%d%s' % (a['ncomp'], code), r.d, dp + v * stride)]
                        for v in range(n)]
                a['n_vertices'] = n
                a['vertices'] = {USAGE[a['usage']] if a['usage'] < len(USAGE) else a['usage']: vals}
                if imag and not (imag[0] <= dp and dp + a['length'] <= imag[1]):
                    r.notes.append('%s: vertex data 0x%x not inside IMAG' % (what, dp))
            if a['usage'] in (7, 8):
                r.err('%s: bone index/weight stream present (soft skinning freezes the real HOME Menu)' % what)
        elif a['type'] == 0x80000000:        # constant vertex parameter
            a['fmt'], a['ncomp'], a['scale'], na = r.u32(ao + 0xC), r.u32(ao + 0x10), r.f32(ao + 0x14), r.u32(ao + 0x18)
            ap = r.ptr(ao + 0x1C, what + '.param', optional=(na == 0))
            a['values'] = r.fv(ap, na) if na else []
            r.ranges.append((ao, ao + 0x20, 'VtxParam ' + what))
        else:
            r.err('%s: vertex attribute type 0x%x' % (what, a['type']))
        s['attrs'].append(a)
    return s


# ----------------------------------------------------------------------------------------------- CMDL
def read_skeleton(r, o, what):
    k = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'SOBJ':
        r.err('%s: skeleton not SOBJ' % what)
        raise CgfxError('skel')
    k['name'] = r.strp(o + 0xC, what, optional=True)
    nb = r.u32(o + 0x18)
    bd = r.ptr(o + 0x1C, what + '.bones')
    root = r.ptr(o + 0x20, what + '.root')
    k['scaling_rule'], k['flags'] = r.u32(o + 0x24), r.u32(o + 0x28)
    r.ranges.append((o, o + 0x2C, 'Skeleton ' + what))
    ents, _ = read_dict(r, bd, what + '.bones')
    if len(ents) != nb:
        r.err('%s: bone count mismatch' % what)
    k['bones'] = []
    for name, bo in ents:
        b = dict(at=bo, name=r.strp(bo, what + '.bone'), flags=r.u32(bo + 4), joint=r.u32(bo + 8), parent=r.s32(bo + 0xC))
        b['links'] = [r.u32(bo + 0x10 + 4 * i) for i in range(4)]
        b['srt'] = r.fv(bo + 0x20, 9)
        b['local'], b['world'], b['invbase'] = r.fv(bo + 0x44, 12), r.fv(bo + 0x74, 12), r.fv(bo + 0xA4, 12)
        b['billboard'] = r.u32(bo + 0xD4)
        r.ranges.append((bo, bo + 0xE0, 'Bone ' + what))
        k['bones'].append(b)
    if k['bones'] and root != k['bones'][0]['at']:
        r.err('%s: root bone pointer 0x%x != first bone 0x%x' % (what, root, k['bones'][0]['at']))
    return k


def read_animgroup(r, o, what):
    g = dict(at=o, type=r.u32(o), flags=r.u32(o + 4))
    g['name'] = r.strp(o + 8, what)
    g['member_type'] = r.u32(o + 0xC)
    nm = r.u32(o + 0x10)
    md = r.ptr(o + 0x14, what + '.members')
    nb = r.u32(o + 0x18)
    bo = r.ptr(o + 0x1C, what + '.blendops', optional=(nb == 0))
    g['blend_ops'] = [r.u32(bo + 4 * i) for i in range(nb)]
    g['timing'] = r.u32(o + 0x20)
    r.ranges.append((o, o + 0x24, 'AnimGroup ' + what))
    ents, _ = read_dict(r, md, what + '.members')
    if len(ents) != nm:
        r.err('%s: member count %d != dict %d' % (what, nm, len(ents)))
    g['members'] = []
    for name, mo in ents:
        mem = dict(name=name, type=r.u32(mo))
        mem['path'] = r.strp(mo + 4, what + '.member')
        mem['raw'] = list(struct.unpack_from('<5I', r.d, mo + 8))
        if name != mem['path']:
            r.err('%s: member dict name %r != path %r' % (what, name, mem['path']))
        g['members'].append(mem)
    return g


def read_cmdl(r, o, what, imag):
    m = dict(at=o, type=r.u32(o))
    if r.magic(o + 4) != 'CMDL':
        r.err('%s: not CMDL' % what)
        raise CgfxError('CMDL')
    m['revision'] = r.u32(o + 8)
    m['name'] = r.strp(o + 0xC, what)
    m['userdata'] = (r.u32(o + 0x10), r.u32(o + 0x14))
    m['flags'], m['branch_visible'], m['n_children'], m['children'] = struct.unpack_from('<4I', r.d, o + 0x18)
    nag = r.u32(o + 0x28)
    agd = r.ptr(o + 0x2C, what + '.animgroups', optional=(nag == 0))
    m['scale'], m['rotation'], m['translation'] = r.fv(o + 0x30, 3), r.fv(o + 0x3C, 3), r.fv(o + 0x48, 3)
    m['local'] = r.fv(o + 0x54, 12)
    m['world'] = r.fv(o + 0x84, 12)
    p = o + 0xB4
    nmesh = r.u32(p)
    mo = r.ptr(p + 4, what + '.meshes')
    nmat = r.u32(p + 8)
    md = r.ptr(p + 0xC, what + '.materials')
    nshp = r.u32(p + 0x10)
    so = r.ptr(p + 0x14, what + '.shapes')
    nnode = r.u32(p + 0x18)
    nd = r.ptr(p + 0x1C, what + '.meshnodes', optional=(nnode == 0))
    m['tail'] = list(struct.unpack_from('<3I', r.d, p + 0x20))   # is_non_uniform_scalable, layer id ...
    r.ranges.append((o, p + 0x2C, 'CMDL ' + what))
    if m['type'] & 0x80:
        m['skeleton'] = read_skeleton(r, r.ptr(p + 0x2C, what + '.skeleton'), what + '.skel')
    m['animgroups'] = []
    if agd:
        ents, _ = read_dict(r, agd, what + '.animgroups')
        if len(ents) != nag:
            r.err('%s: animgroup count mismatch' % what)
        for name, go in ents:
            g = read_animgroup(r, go, '%s.ag[%s]' % (what, name))
            if g['name'] != name:
                r.err('%s: animgroup name mismatch' % what)
            m['animgroups'].append(g)
    ents, _ = read_dict(r, md, what + '.materials')
    if len(ents) != nmat:
        r.err('%s: material count mismatch' % what)
    m['materials'] = []
    for name, mto in ents:
        mt = read_mtob(r, mto, '%s.mat[%s]' % (what, name))
        if mt['name'] != name:
            r.err('%s: material name mismatch %r %r' % (what, mt['name'], name))
        m['materials'].append(mt)
    m['meshes'] = [read_mesh(r, r.ptr(mo + 4 * i, what + '.mesh%d' % i), '%s.mesh%d' % (what, i), o)
                   for i in range(nmesh)]
    m['shapes'] = [read_shape(r, r.ptr(so + 4 * i, what + '.shape%d' % i), '%s.shape%d' % (what, i), imag)
                   for i in range(nshp)]
    m['meshnodes'] = []
    if nd:
        ents, _ = read_dict(r, nd, what + '.meshnodes')
        for name, no in ents:
            m['meshnodes'].append(dict(name=r.strp(no, what + '.node'), visible=r.u32(no + 4)))
    for i, me in enumerate(m['meshes']):
        if me['shape_idx'] >= nshp:
            r.err('%s: mesh %d shape index %d out of range' % (what, i, me['shape_idx']))
        if me['material_idx'] >= nmat:
            r.err('%s: mesh %d material index %d out of range' % (what, i, me['material_idx']))
    return m


# ----------------------------------------------------------------------------------------------- file
def parse(data):
    r = Reader(data)
    if data[:4] != b'CGFX':
        raise CgfxError('not a CGFX file')
    bom, hsize, rev, fsize, nblocks = struct.unpack_from('<HHIII', data, 4)
    f = dict(revision=rev, nblocks=nblocks, size=fsize)
    if bom != 0xFEFF:
        r.err('byte order mark 0x%x' % bom)
    if hsize != 0x14:
        r.err('header size 0x%x' % hsize)
    if fsize != len(data):
        r.err('header file size 0x%x != actual 0x%x' % (fsize, len(data)))
    o = hsize
    if data[o:o + 4] != b'DATA':
        raise CgfxError('no DATA block')
    dsize = r.u32(o + 4)
    data_end = o + dsize
    imag = None
    if nblocks >= 2:
        if data[data_end:data_end + 4] != b'IMAG':
            r.err('IMAG block not at end of DATA (0x%x)' % data_end)
        else:
            isz = r.u32(data_end + 4)
            imag = (data_end + 8, data_end + isz)
            f['imag'] = (data_end, isz)
            if data_end + isz != len(data):
                r.err('IMAG end 0x%x != file size 0x%x' % (data_end + isz, len(data)))
    elif data_end != len(data):
        r.err('DATA size does not cover file and no IMAG block')
    f['data'] = (o, dsize)
    slots = []
    # NW4C DATA has 15 (count, offset) slots; the 16th position is the first DICT's magic (EFE quirk)
    nslots = 15 if r.u32(o + 8 + 8 * 15) == 0x54434944 else 16
    for i in range(nslots):
        n = r.u32(o + 8 + 8 * i)
        p = r.ptr(o + 12 + 8 * i, 'DATA.slot%d' % i, optional=True) if n else 0
        if n == 0 and r.u32(o + 12 + 8 * i):
            r.err('DATA slot %d has offset but count 0' % i)
        slots.append((n, p))
    f['slots'] = slots
    f['models'] = []
    f['textures'] = []
    if slots[0][0]:
        ents, _ = read_dict(r, slots[0][1], 'models')
        for name, mo in ents:
            f['models'].append(read_cmdl(r, mo, 'model[%s]' % name, imag))
    if slots[1][0]:
        ents, _ = read_dict(r, slots[1][1], 'textures')
        for name, to in ents:
            t = read_txob(r, to, 'tex[%s]' % name, imag)
            f['textures'].append(t)
    for i in range(2, nslots):
        if slots[i][0]:
            r.notes.append('DATA slot %d (%s, %d entries) not decoded by this parser' % (
                i, SLOT_NAMES[i] if i < len(SLOT_NAMES) else '?', slots[i][0]))
    f['reader'] = r
    validate(f, r, data_end, imag)
    return f


def validate(f, r, data_end, imag):
    # all pointers into the file; structure pointers (not pixel/vertex/index data) must stay inside DATA
    for at, t, what in r.pointers:
        is_blob = what.endswith('.pixels') or what.endswith('.vtx.data') or what.endswith('.idx.data')
        if not is_blob and not (0x14 <= t < data_end):
            r.err('pointer %s @0x%x -> 0x%x outside DATA' % (what, at, t))
    # object overlap
    rg = sorted(set(r.ranges))
    for (a0, a1, an), (b0, b1, bn) in zip(rg, rg[1:]):
        if b0 < a1 and not (a0 == b0 and a1 == b1):
            r.err('overlap: %s 0x%x-0x%x and %s 0x%x-0x%x' % (an, a0, a1, bn, b0, b1))
    # word alignment of all objects
    for a0, a1, an in rg:
        if a0 % 4:
            r.err('%s at 0x%x not 4-byte aligned' % (an, a0))
    texnames = {t['name']: t for t in f['textures']}
    for m in f['models']:
        for mt in m['materials']:
            for i, tm in enumerate(mt['texmappers']):
                if not tm:
                    continue
                ref = tm['txob']
                if ref['kind'] == 'reference' and ref['linked'] not in texnames:
                    r.err('material %s tm%d references unknown texture %r' % (mt['name'], i, ref['linked']))
                elif ref['kind'] == 'reference':
                    tex = texnames[ref['linked']]
                    regs = tm['regs']
                    size_reg = regs.get(0x82)
                    if size_reg and size_reg != (tex['width'] << 16 | tex['height']):
                        r.err('material %s tm%d: PICA reg 0x82 size 0x%08x != texture %dx%d' %
                              (mt['name'], i, size_reg, tex['width'], tex['height']))
                    fmt_reg = regs.get(0x8E)
                    if fmt_reg and fmt_reg != tex['hwformat']:
                        r.err('material %s tm%d: PICA reg 0x8E format %d != texture format %d' %
                              (mt['name'], i, fmt_reg, tex['hwformat']))
        for si, s in enumerate(m['shapes']):
            nv = None
            for a in s['attrs']:
                if a['type'] in (0x40000001, 0x40000002) and 'n_vertices' in a:
                    nv = a['n_vertices'] if nv is None else min(nv, a['n_vertices'])
                    pos = a['vertices'].get('Position')
                    if pos:
                        import math
                        bb = s['bbox']
                        if bb.get('type') == 0x80000000:
                            for v in pos:
                                for k in range(3):
                                    if abs(v[k] - bb['center'][k]) > bb['size'][k] / 2 + 1e-3:
                                        r.err('shape %d: vertex %s outside bounding box' % (si, v))
                                        break
                                else:
                                    continue
                                break
            for ps in s['primsets']:
                for pr in ps['prims']:
                    for st in pr['streams']:
                        if nv is not None and st['indices'] and max(st['indices']) >= nv:
                            r.err('shape %d: index %d >= vertex count %d' % (si, max(st['indices']), nv))
                        if st['fmt'] == 0x1401 and nv is not None and nv > 256:
                            r.err('shape %d: u8 indices with %d vertices' % (si, nv))


def dump(f, out=sys.stdout):
    p = lambda *a: print(*a, file=out)
    p('CGFX rev 0x%x size 0x%x blocks %d DATA@0x%x+0x%x IMAG %s' % (
        f['revision'], f['size'], f['nblocks'], f['data'][0], f['data'][1],
        ('@0x%x+0x%x' % f['imag']) if 'imag' in f else '-'))
    p('slots', [(i, n) for i, (n, _) in enumerate(f['slots']) if n])
    for m in f['models']:
        p('CMDL %r type 0x%x rev 0x%x flags %d branchvis %d scale %s trans %s tail %s' % (
            m['name'], m['type'], m['revision'], m['flags'], m['branch_visible'], m['scale'], m['translation'], m['tail']))
        for g in m['animgroups']:
            p('  animgroup %r type 0x%x flags %d member_type %d timing %d blendops %s' % (
                g['name'], g['type'], g['flags'], g['member_type'], g['timing'], g['blend_ops']))
            for mem in g['members']:
                p('     0x%08x %-70s %s' % (mem['type'], mem['path'], ['0x%x' % x for x in mem['raw']]))
        for mt in m['materials']:
            p('  MTOB %r type 0x%x rev 0x%x flags 0x%x texcfg %d translucency %d id %d' % (
                mt['name'], mt['type'], mt['revision'], mt['flags'], mt['texcoord_cfg'], mt['translucency'], mt['material_id']))
            names = ['Emission', 'Ambient', 'Diffuse', 'Spec0', 'Spec1', 'Const0', 'Const1', 'Const2', 'Const3', 'Const4', 'Const5']
            for n, c, u in zip(names, mt['colors_f'], mt['colors_u8']):
                p('     %-8s %s %08x' % (n, ['%.3f' % x for x in c], u))
            p('     raster', mt['raster'])
            p('     depth', ['0x%x' % x for x in mt['depth']], 'blend mode', mt['blend_mode'], mt['blend_color'],
              ['0x%x' % x for x in mt['blend_cmds']])
            p('     stencil', ['0x%x' % x for x in mt['stencil']])
            p('     n_texcoords', mt['n_texcoords'], mt['texcoords'][0])
            for i, tm in enumerate(mt['texmappers']):
                if tm:
                    p('     texmapper%d type 0x%x txob %s/%r -> %r  minfilter %d border %s cmdsize 0x%x' % (
                        i, tm['type'], tm['txob']['kind'], tm['txob']['name'], tm['txob'].get('linked'),
                        tm['minfilter'], tm['border'], tm['cmd_size']))
                    p('        cmd words', ['%08x' % x for x in tm['cmd']])
                    p('        regs', {hex(k): hex(v) for k, v in tm['regs'].items()})
            p('     shader', mt['shader']['name'], '->', mt['shader']['linked'], 'prog idx', mt['shader_prog_idx'],
              'params', mt['n_shader_params'], 'lightset', mt['lightset'], 'fog', mt['fog'])
            fr = mt['frag']
            p('     frag buffer', fr['buffer_color'], 'lighting', fr['lighting'], 'luts', fr['lut_ptrs'])
            for i, c in enumerate(fr['combiners']):
                p('        comb%d src %03x/%03x ops %08x comb %d/%d scale %d/%d const %08x addr %08x cst %d' % (
                    i, c['src_rgb'], c['src_a'], c['operands'], c['comb_rgb'], c['comb_a'], c['scale_rgb'], c['scale_a'],
                    c['const_rgba'], c['addr'], c['constant']))
            p('        alphatest %08x %08x  buffercmds %s' % (fr['alpha_test'][0], fr['alpha_test'][1],
                                                            ['%08x' % x for x in fr['buffer_cmds']]))
            p('     hashes', ['%08x' % x for x in mt['hashes']])
        for i, me in enumerate(m['meshes']):
            p('  Mesh%d %r type 0x%x shape %d mat %d vis %d prio %d nodeidx %d node %r raw %s tail %s' % (
                i, me['name'], me['type'], me['shape_idx'], me['material_idx'], me['visible'], me['priority'],
                me['node_vis_idx'], me['node_name'], ['%x' % x for x in me['raw']], me['tail']))
        for i, s in enumerate(m['shapes']):
            p('  Shape%d type 0x%x flags %d bbox c=%s size=%s posoff %s' % (
                i, s['type'], s['flags'], s['bbox'].get('center'), s['bbox'].get('size'), s['pos_offset']))
            for ps in s['primsets']:
                for pr in ps['prims']:
                    for st in pr['streams']:
                        p('     index stream @0x%x fmt 0x%x mode %d vis %d len %d n_idx %d max %s first %s' % (
                            st['data_at'], st['fmt'], st['mode'], st['visible'], st['length'], len(st['indices']),
                            max(st['indices']) if st['indices'] else None, st['indices'][:12]))
                    p('     bufobjs', pr['bufobjs'], 'flags', pr['flags'], 'skinning', ps['skinning'], 'bones', ps['bones'])
            for a in s['attrs']:
                if a['type'] == 0x40000002:
                    p('     interleaved @0x%x stride %d n %d len %d' % (a['data_at'], a['stride'], a['n_vertices'], a['length']))
                    for st in a['streams']:
                        p('       %-10s fmt 0x%x n %d scale %g off %d flags %d' % (
                            USAGE[st['usage']], st['fmt'], st['ncomp'], st['scale'], st['offset'], st['flags']))
                    for k, v in a['vertices'].items():
                        p('         ', k, [[round(x, 3) for x in vv] for vv in v[:6]])
                else:
                    p('     param %s fmt 0x%x ncomp %d scale %g values %s' % (
                        USAGE[a['usage']], a.get('fmt', 0), a.get('ncomp', 0), a.get('scale', 0), a.get('values')))
        if 'skeleton' in m:
            k = m['skeleton']
            p('  Skeleton %r type 0x%x rule %d flags %d' % (k['name'], k['type'], k['scaling_rule'], k['flags']))
            for b in k['bones']:
                p('     bone %r flags 0x%x joint %d parent %d srt %s billboard %d world %s' % (
                    b['name'], b['flags'], b['joint'], b['parent'], b['srt'], b['billboard'], b['world']))
        for n in m['meshnodes']:
            p('  meshnode', n)
    for t in f['textures']:
        p('TXOB %r %s %dx%d hw %s gl %x/%x levels %d data@0x%x size 0x%x' % (
            t['name'], t['kind'], t.get('width', 0), t.get('height', 0), FMT_NAMES.get(t.get('hwformat')),
            t.get('glformat', 0), t.get('gltype', 0), t.get('levels', 0), t.get('data_at', 0), t.get('data_size', 0)))
    r = f['reader']
    p('pointers followed: %d' % len(r.pointers))
    if r.problems:
        p('PROBLEMS (%d):' % len(r.problems))
        for x in r.problems:
            p('  -', x)
    else:
        p('NO PROBLEMS FOUND')


def banner_checks(f, size):
    r = f['reader']
    if size >= 0x80000:
        r.err('file is %d bytes; HOME Menu banner CGFX limit is 0x80000 (512 KiB)' % size)
    names = [m['name'] for m in f['models']]
    if names != ['COMMON']:
        r.err('models DICT is %s; the HOME Menu banner needs exactly one CMDL named COMMON' % names)


def summary(f, size, out=sys.stdout):
    p = lambda *a: print(*a, file=out)
    p('cgfx_parse: %d bytes (%.1f KiB of 512), models %s, textures %s' % (
        size, size / 1024, [m['name'] for m in f['models']],
        ['%s %dx%d %s' % (t['name'], t.get('width', 0), t.get('height', 0), FMT_NAMES.get(t.get('hwformat'), '?'))
         for t in f['textures']]))
    for m in f['models']:
        nv = sum(a.get('n_vertices', 0) for s in m['shapes'] for a in s['attrs'] if a['type'] == 0x40000002)
        nv += sum(a.get('n_vertices', 0) for s in m['shapes'] for a in s['attrs'] if a['type'] == 0x40000001 and a['usage'] == 0)
        nt = sum(len(st['indices']) // 3 for s in m['shapes'] for ps in s['primsets'] for pr in ps['prims'] for st in pr['streams'])
        bones = ['%s(bb%d)' % (b['name'], b['billboard']) for b in m.get('skeleton', {}).get('bones', [])]
        p('  %s: %d meshes, %d materials, %d vertices, %d triangles, bones %s, mesh nodes %s' % (
            m['name'], len(m['meshes']), len(m['materials']), nv, nt, bones, [me['node_name'] for me in m['meshes']]))
    r = f['reader']
    for n in r.notes:
        p('  note:', n)
    if r.problems:
        p('  PROBLEMS (%d):' % len(r.problems))
        for x in r.problems:
            p('   -', x)
    else:
        p('  structure OK (%d pointers checked)' % len(r.pointers))


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    d = open(args[0], 'rb').read()
    try:
        f = parse(d)
    except CgfxError as e:
        print('cgfx_parse: FATAL', e)
        sys.exit(2)
    banner_checks(f, len(d))
    if '--summary' in sys.argv:
        summary(f, len(d))
    else:
        dump(f)
        for n in f['reader'].notes:
            print('note:', n)
    sys.exit(1 if f['reader'].problems else 0)
