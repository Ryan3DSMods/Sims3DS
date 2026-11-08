// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// ISO 9660 / Joliet reading on a *stream*: the disc is read once from start to end (it may come out of a 7z archive),
// and parts of it are requested by position. Tolerant like the Python extractor: it only follows directory records
// and never checks the volume sizes (copy-protected discs carry odd values there).

// Delivers byte ranges of a sequential stream to whoever requested them.
export class RangeFeeder {
  constructor() { this.reqs = []; this.pos = 0; this.missed = 0; }
  request(start, length, onData, onEnd) {
    const r = { start, end: start + length, next: start, onData, onEnd };
    if (start < this.pos) { this.missed++; r.missed = true; return r; }
    if (length === 0) { r.zero = true; }
    this.reqs.push(r);
    return r;
  }
  async feed(chunk) {
    const a = this.pos, b = a + chunk.length;
    for (;;) {
      let best = null;
      for (const r of this.reqs) if (r.next < b && (best === null || r.next < best.next)) best = r;
      if (!best) break;
      if (best.next < a) { this.missed++; best.missed = true; this.reqs.splice(this.reqs.indexOf(best), 1); continue; }
      const e = Math.min(best.end, b);
      if (e > best.next) { const s = best.next; best.next = e; await best.onData(chunk.subarray(s - a, e - a), s - best.start); }
      if (best.next >= best.end) { this.reqs.splice(this.reqs.indexOf(best), 1); if (best.onEnd) await best.onEnd(); }
    }
    // zero-length requests at or before b
    for (const r of this.reqs.slice()) if (r.zero && r.start <= b) { this.reqs.splice(this.reqs.indexOf(r), 1); if (r.onEnd) await r.onEnd(); }
    this.pos = b;
  }
  get pending() { return this.reqs.length; }
}

function u32(b, p) { return (b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (b[p + 3] << 24)) >>> 0; }

// Collect a requested range into one buffer.
function collect(feeder, start, length, done) {
  const buf = new Uint8Array(length);
  return feeder.request(start, length, async (d, off) => { buf.set(d, off); }, async () => { await done(buf); });
}

// Walk the directory tree of the ISO in `feeder`; onFile({path, lba, size}) for every file. Returns a promise-free
// setup: everything happens while the stream is fed. onReady() is called once the root is known.
export function walkIso(feeder, onFile, onError) {
  let joliet = false;
  const parseDir = (path) => async (data) => {
    let i = 0;
    while (i < data.length) {
      const ln = data[i];
      if (ln === 0) { i = (Math.floor(i / 2048) + 1) * 2048; continue; }
      if (i + ln > data.length || ln < 34) break;
      const nl = data[i + 32];
      const raw = data.subarray(i + 33, i + 33 + nl);
      const lba = u32(data, i + 2), size = u32(data, i + 10), isDir = (data[i + 25] & 2) !== 0;
      i += ln;
      if (nl === 1 && (raw[0] === 0 || raw[0] === 1)) continue;
      let name = '';
      if (joliet) { for (let k = 0; k + 1 < raw.length; k += 2) name += String.fromCharCode((raw[k] << 8) | raw[k + 1]); }
      else { for (let k = 0; k < raw.length; k++) name += String.fromCharCode(raw[k]); }
      name = name.split(';')[0].replace(/\.$/, '');
      const p = path + '/' + name;
      if (isDir) collect(feeder, lba * 2048, size, parseDir(p));
      else await onFile({ path: p, lba, size });
    }
  };
  collect(feeder, 16 * 2048, 16 * 2048, async (vd) => {
    let root = null;
    for (let s = 0; s < 16; s++) {
      const d = vd.subarray(s * 2048, s * 2048 + 2048);
      if (d[1] !== 0x43 || d[2] !== 0x44 || d[3] !== 0x30 || d[4] !== 0x30 || d[5] !== 0x31) break;   // "CD001"
      const t = d[0];
      if (t === 255) break;
      if (t === 1 && root === null) root = d.slice(156, 156 + 34);
      if (t === 2 && d[88] === 0x25 && d[89] === 0x2F && (d[90] === 0x40 || d[90] === 0x43 || d[90] === 0x45)) { root = d.slice(156, 156 + 34); joliet = true; }
    }
    if (!root) { if (onError) onError(new Error('this does not look like a CD image (no ISO 9660 volume)')); return; }
    collect(feeder, u32(root, 2) * 2048, u32(root, 10), parseDir(''));
  });
}
