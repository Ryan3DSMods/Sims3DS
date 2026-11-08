// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// InstallShield cabinet extraction (data1.hdr + data1.cab ... dataN.cab), following unshield
// (https://github.com/twogood/unshield, MIT licence) - written for streaming: every cabinet volume is read once,
// front to back, and the files inside it are unpacked as their bytes go by. Files that are split over two volumes
// keep their state until the next volume arrives.

const FILE_SPLIT = 1, FILE_OBFUSCATED = 2, FILE_COMPRESSED = 4, FILE_INVALID = 8;
const LINK_PREV = 1;

function u16(b, p) { return b[p] | (b[p + 1] << 8); }
function u32(b, p) { return (b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (b[p + 3] << 24)) >>> 0; }
function u64(b, p) { return u32(b, p + 4) * 4294967296 + u32(b, p); }
function cstr(b, p) { let e = p; while (e < b.length && b[e]) e++; let s = ''; for (let i = p; i < e; i++) s += String.fromCharCode(b[i]); return s; }

export function parseHeader(h) {
  if (u32(h, 0) !== 0x28635349) throw new Error('data1.hdr: not an InstallShield cabinet header');
  const version = u32(h, 4);
  let major;
  if (version >>> 24 === 1) major = (version >>> 12) & 0xF;
  else if (version >>> 24 === 2 || version >>> 24 === 4) { major = version & 0xFFFF; if (major) major = Math.floor(major / 100); }
  else major = 0;
  if (major < 6) throw new Error('InstallShield cabinet version ' + major + ' is not supported (only 6 and later)');
  if (major >= 17) throw new Error('InstallShield cabinet version ' + major + ' (UTF-16 names) is not supported');
  const d = u32(h, 12);                         // cab descriptor offset
  const fto = u32(h, d + 0x0C), dirCount = u32(h, d + 0x1C), fileCount = u32(h, d + 0x28), fto2 = u32(h, d + 0x2C);
  const tbl = (i) => u32(h, d + fto + i * 4);
  const dirs = [];
  for (let i = 0; i < dirCount; i++) dirs.push(cstr(h, d + fto + tbl(i)));
  const files = [];
  for (let i = 0; i < fileCount; i++) {
    const p = d + fto + fto2 + i * 0x57;
    files.push({
      index: i, flags: u16(h, p), expanded: u64(h, p + 2), compressed: u64(h, p + 10), offset: u64(h, p + 18),
      md5: h.slice(p + 26, p + 42), name: cstr(h, d + fto + u32(h, p + 0x3A)), nameOffset: u32(h, p + 0x3A),
      dir: u16(h, p + 0x3E), linkPrev: u32(h, p + 0x4C), linkNext: u32(h, p + 0x50), linkFlags: h[p + 0x54],
      volume: u16(h, p + 0x55),
    });
  }
  const groups = [];
  for (let k = 0; k < 71; k++) {
    let o = u32(h, d + 0x3E + k * 4);
    while (o) {
      const p = d + o;
      const desc = u32(h, p + 4), next = u32(h, p + 8);
      const q = d + desc;
      groups.push({ name: cstr(h, d + u32(h, q)), first: u32(h, q + 4 + 0x12) | 0, last: u32(h, q + 8 + 0x12) | 0 });
      o = next;
    }
  }
  return { major, dirs, files, groups };
}

export function fileIsValid(f) { return !(f.flags & FILE_INVALID) && f.nameOffset !== 0 && f.offset !== 0; }

// unshield's output names: directory "\" -> "/", ' ' < > [ ] -> '_', unprintable -> '_'
function cleanDir(s) {
  let r = '';
  for (const ch of s) {
    const c = ch.charCodeAt(0);
    if (ch === '\\') r += '/';
    else if (ch === ' ' || ch === '<' || ch === '>' || ch === '[' || ch === ']') r += '_';
    else if (c < 0x20 || c > 0x7E) r += '_';
    else r += ch;
  }
  return r;
}
function cleanName(s) {
  let r = '';
  for (const ch of s) { const c = ch.charCodeAt(0); r += (c < 0x20 || c > 0x7E) ? '_' : ch; }
  return r;
}

// Which files go where. groupTarget(name) -> output folder or null (skip). Mirrors the Python extractor:
// unshield extracts every group into <group>/<dir>/<file>, then the groups are merged in sorted name order into
// their target folders (later groups overwrite, folder names match case-insensitively).
export function planExtraction(model, groupTarget) {
  const order = model.groups.map((g, i) => ({ g, i })).filter(({ g }) => groupTarget(g.name))
    .sort((a, b) => (a.g.name < b.g.name ? -1 : a.g.name > b.g.name ? 1 : 0));
  const dirCase = new Map();                 // lower-case dir path -> first spelling
  const byPath = new Map();                  // lower-case file path -> { path, index }
  const skipped = model.groups.filter((g) => !groupTarget(g.name)).map((g) => g.name);
  for (const { g } of order) {
    const target = groupTarget(g.name);
    for (let i = g.first; i <= g.last; i++) {
      const f = model.files[i];
      if (!f || !fileIsValid(f)) continue;
      const dirName = model.dirs[f.dir] ? cleanDir(model.dirs[f.dir]) : '';
      const parts = (target + '/' + dirName).split('/').filter(Boolean);
      // fix up directory spelling: first spelling seen wins (like merging into an existing folder)
      let acc = '', accLow = '';
      const fixed = [];
      for (const part of parts) {
        accLow = accLow ? accLow + '/' + part.toLowerCase() : part.toLowerCase();
        if (!dirCase.has(accLow)) dirCase.set(accLow, part);
        fixed.push(dirCase.get(accLow));
        acc = fixed.join('/');
      }
      const path = acc + '/' + cleanName(f.name);
      byPath.set(path.toLowerCase(), { path, index: i });     // later group / later file wins
    }
  }
  // data source: follow LINK_PREV to the file that holds the data
  const outputs = new Map();                 // data index -> [paths]
  let totalBytes = 0;
  for (const { path, index } of byPath.values()) {
    let k = index, guard = 0;
    while (model.files[k].linkFlags & LINK_PREV) { k = model.files[k].linkPrev; if (++guard > 100000) throw new Error('link loop'); }
    if (!outputs.has(k)) outputs.set(k, []);
    outputs.get(k).push({ path, expected: model.files[index].expanded, md5: model.files[index].md5 });
    totalBytes += model.files[index].expanded;
  }
  return { outputs, fileCount: byPath.size, totalBytes, skipped };
}

function deobfuscate(buf, seedObj) {
  let seed = seedObj.v;
  for (let i = 0; i < buf.length; i++, seed++) {
    let x = buf[i] ^ 0xD5;
    x = ((x >>> 2) | (x << 6)) & 0xFF;
    buf[i] = (x - (seed % 0x47)) & 0xFF;
  }
  seedObj.v = seed;
}

// Per data file: undoes obfuscation, splits the compressed stream into blocks (u16 size + raw deflate) and inflates.
class FileUnpacker {
  constructor(f, targets, sink, inflateRaw, makeHash) {
    this.f = f; this.targets = targets; this.sink = sink; this.inflateRaw = inflateRaw; this.makeHash = makeHash;
    this.compressed = (f.flags & FILE_COMPRESSED) !== 0;
    this.need = this.compressed ? f.compressed : f.expanded;
    this.got = 0; this.written = 0;
    this.pend = new Uint8Array(0);
    this.seed = { v: 0 };
    this.writers = null;
    this.hash = makeHash ? makeHash() : null;
  }
  async open() {
    // the data goes to the first target; identical copies (linked files) are duplicated when it is complete
    this.writers = [await this.sink.openFile(this.targets[0].path, this.targets[0].expected)];
  }
  async out(bytes) {
    if (!bytes.length) return;
    this.written += bytes.length;
    if (this.hash) this.hash.update(bytes);
    for (const w of this.writers) await w.write(bytes);
  }
  async feed(data) {
    if (!this.writers) await this.open();
    let d = data;
    if (this.got + d.length > this.need) d = d.subarray(0, this.need - this.got);
    this.got += d.length;
    if (this.f.flags & FILE_OBFUSCATED) { d = d.slice(); deobfuscate(d, this.seed); }
    if (!this.compressed) { await this.out(d); }
    else {
      let buf = d;
      if (this.pend.length) { buf = new Uint8Array(this.pend.length + d.length); buf.set(this.pend); buf.set(d, this.pend.length); }
      let p = 0;
      while (p + 2 <= buf.length) {
        const n = buf[p] | (buf[p + 1] << 8);
        if (n === 0) throw new Error(this.f.name + ': empty compressed block');
        if (p + 2 + n > buf.length) break;
        await this.out(this.inflateRaw(buf.subarray(p + 2, p + 2 + n)));
        p += 2 + n;
      }
      this.pend = buf.slice(p);
    }
    return this.got >= this.need;
  }
  async finish() {
    if (!this.writers) await this.open();      // empty file
    if (this.pend.length) throw new Error(this.f.name + ': compressed data ends inside a block');
    if (this.written !== this.f.expanded) throw new Error(this.f.name + ': unpacked ' + this.written + ' bytes, expected ' + this.f.expanded);
    let md5ok = null;
    if (this.hash) {
      const got = this.hash.digest();
      const want = this.f.md5;
      md5ok = want.every((x) => x === 0) || got.every((x, i) => x === want[i]);
      if (!md5ok) throw new Error(this.f.name + ': MD5 mismatch');
    }
    for (const w of this.writers) await w.close();
    for (let i = 1; i < this.targets.length; i++) await this.sink.duplicate(this.targets[0].path, this.targets[i].path);
    return md5ok;
  }
}

// Streams cabinet volumes in order (1, 2, 3, ...). Call volume(v).feed(chunk) with the bytes of dataV.cab from the
// start, then volume(v).end().
export class CabExtractor {
  constructor(model, plan, sink, opts) {
    this.model = model; this.plan = plan; this.sink = sink;
    this.inflateRaw = opts.inflateRaw; this.makeHash = opts.makeHash || null;
    this.onFileDone = opts.onFileDone || (() => {});
    this.active = new Map();                 // data index -> FileUnpacker (split files survive a volume change)
    this.done = new Set();
    this.nextVolume = 1;
  }

  volume(v) {
    if (v !== this.nextVolume) throw new Error('cabinet volume ' + v + ' before ' + this.nextVolume);
    const self = this;
    let head = new Uint8Array(0), segs = null, pos = 0;
    const setup = () => {
      // common header 20 bytes + v6 volume header 64 bytes
      const vh = {
        firstIndex: u32(head, 28), lastIndex: u32(head, 32),
        firstOffset: u64(head, 36), firstExpanded: u64(head, 44), firstCompressed: u64(head, 52),
        lastOffset: u64(head, 60), lastExpanded: u64(head, 68), lastCompressed: u64(head, 76),
      };
      segs = [];
      for (const [k, targets] of self.plan.outputs) {
        const f = self.model.files[k];
        const csize = (f.flags & FILE_COMPRESSED) ? f.compressed : f.expanded;
        if (f.flags & FILE_SPLIT) {
          if (k === vh.lastIndex && f.volume === v) segs.push({ k, start: vh.lastOffset, len: (f.flags & FILE_COMPRESSED) ? vh.lastCompressed : vh.lastExpanded, targets });
          else if (k === vh.firstIndex && f.volume < v) segs.push({ k, start: vh.firstOffset, len: (f.flags & FILE_COMPRESSED) ? vh.firstCompressed : vh.firstExpanded, targets });
          else if (f.volume === v) segs.push({ k, start: f.offset, len: csize, targets });
        } else if (f.volume === v) {
          segs.push({ k, start: f.offset, len: csize, targets });
        }
      }
      segs.sort((a, b) => a.start - b.start);
      self.segIdx = 0;
    };
    return {
      async feed(chunk) {
        let c = chunk;
        if (!segs) {
          const take = Math.min(84 - head.length, c.length);
          const nh = new Uint8Array(head.length + take); nh.set(head); nh.set(c.subarray(0, take), head.length); head = nh;
          if (head.length < 84) { pos += c.length; return; }
          if (u32(head, 0) !== 0x28635349) throw new Error('data' + v + '.cab: not an InstallShield cabinet');
          setup();
        }
        const a = pos, b = pos + c.length;
        pos = b;
        // deliver to every segment overlapping [a, b)
        for (let i = self.segIdx; i < segs.length; i++) {
          const s = segs[i];
          if (s.start >= b) break;
          const e = s.start + s.len;
          if (e <= a || s.finished) continue;
          const from = Math.max(s.start, a), to = Math.min(e, b);
          let u = self.active.get(s.k);
          if (!u) { u = new FileUnpacker(self.model.files[s.k], s.targets, self.sink, self.inflateRaw, self.makeHash); self.active.set(s.k, u); }
          const complete = await u.feed(c.subarray(from - a, to - a));
          if (to >= e) s.finished = true;
          if (complete) {
            await u.finish();
            self.active.delete(s.k); self.done.add(s.k);
            self.onFileDone(s.k, s.targets);
          }
        }
        while (self.segIdx < segs.length && segs[self.segIdx].finished) self.segIdx++;
      },
      async end() {
        if (!segs) throw new Error('data' + v + '.cab is too short');
        const unfinished = segs.filter((s) => !s.finished);
        if (unfinished.length) throw new Error('data' + v + '.cab ended before ' + unfinished.length + ' files were complete');
        self.nextVolume = v + 1;
      },
    };
  }

  check() {
    const missing = [...this.plan.outputs.keys()].filter((k) => !this.done.has(k));
    return missing.map((k) => this.model.files[k].name);
  }
}
