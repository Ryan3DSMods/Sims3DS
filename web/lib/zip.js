// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A streaming ZIP writer (files stored, not compressed - the game data is compressed already) for browsers that
// cannot write to a folder. out: { write(bytes, at) } with random access (an OPFS sync access handle).
// ZIP64 records are added when the archive passes 4 GB.

const CRC_TABLE = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1); t[n] = c; }
  return t;
})();
export function crc32Update(crc, b) {
  let c = ~crc;
  for (let i = 0; i < b.length; i++) c = CRC_TABLE[(c ^ b[i]) & 0xFF] ^ (c >>> 8);
  return ~c >>> 0;
}

const enc = new TextEncoder();
const DOS_TIME = 0, DOS_DATE = (46 << 9) | (1 << 5) | 1;     // 2026-01-01

export class ZipWriter {
  constructor(out) { this.out = out; this.pos = 0; this.entries = []; }
  _w(bytes) { this.out.write(bytes, this.pos); this.pos += bytes.length; }
  addFile(path) {
    const name = enc.encode(path);
    const offset = this.pos;
    const lh = new Uint8Array(30 + name.length + 20);           // + ZIP64 extra field room (patched or left as padding)
    const v = new DataView(lh.buffer);
    v.setUint32(0, 0x04034b50, true); v.setUint16(4, 45, true); v.setUint16(6, 0x0800, true); v.setUint16(8, 0, true);
    v.setUint16(10, DOS_TIME, true); v.setUint16(12, DOS_DATE, true);
    v.setUint16(26, name.length, true); v.setUint16(28, 20, true);
    lh.set(name, 30);
    v.setUint16(30 + name.length, 0x0001, true); v.setUint16(32 + name.length, 16, true);   // ZIP64 extra (sizes)
    this._w(lh);
    const entry = { path, name, offset, crc: 0, size: 0 };
    this.entries.push(entry);
    const self = this;
    return {
      write(chunk) { entry.crc = crc32Update(entry.crc, chunk); entry.size += chunk.length; self._w(chunk); },
      close() {
        const p = new Uint8Array(12); const pv = new DataView(p.buffer);
        pv.setUint32(0, entry.crc, true);
        const s32 = entry.size >= 0xFFFFFFFF ? 0xFFFFFFFF : entry.size;
        pv.setUint32(4, s32, true); pv.setUint32(8, s32, true);
        self.out.write(p, entry.offset + 14);
        const z = new Uint8Array(16); const zv = new DataView(z.buffer);
        zv.setBigUint64(0, BigInt(entry.size), true); zv.setBigUint64(8, BigInt(entry.size), true);
        self.out.write(z, entry.offset + 30 + name.length + 4);
      },
    };
  }
  // Another entry with the same data as an earlier, closed one. read(at, n) reads back from the output.
  addCopy(path, fromPath, read) {
    const src = this.entries.find((e) => e.path === fromPath);
    if (!src) throw new Error('zip: ' + fromPath + ' is not in the archive');
    const w = this.addFile(path);
    const dataStart = src.offset + 30 + src.name.length + 20;
    for (let p = 0; p < src.size; p += 4 << 20) w.write(read(dataStart + p, Math.min(4 << 20, src.size - p)));
    w.close();
  }
  finish() {
    const cdStart = this.pos;
    for (const e of this.entries) {
      const big = e.offset >= 0xFFFFFFFF || e.size >= 0xFFFFFFFF;
      const extra = big ? 28 : 0;
      const h = new Uint8Array(46 + e.name.length + extra); const v = new DataView(h.buffer);
      v.setUint32(0, 0x02014b50, true); v.setUint16(4, 45, true); v.setUint16(6, 45, true); v.setUint16(8, 0x0800, true);
      v.setUint16(12, DOS_TIME, true); v.setUint16(14, DOS_DATE, true);
      v.setUint32(16, e.crc, true);
      v.setUint32(20, big ? 0xFFFFFFFF : e.size, true); v.setUint32(24, big ? 0xFFFFFFFF : e.size, true);
      v.setUint16(28, e.name.length, true); v.setUint16(30, extra, true);
      v.setUint32(42, big ? 0xFFFFFFFF : e.offset, true);
      h.set(e.name, 46);
      if (big) {
        const x = 46 + e.name.length;
        v.setUint16(x, 1, true); v.setUint16(x + 2, 24, true);
        v.setBigUint64(x + 4, BigInt(e.size), true); v.setBigUint64(x + 12, BigInt(e.size), true); v.setBigUint64(x + 20, BigInt(e.offset), true);
      }
      this._w(h);
    }
    const cdSize = this.pos - cdStart;
    const n = this.entries.length;
    if (cdStart >= 0xFFFFFFFF || n >= 0xFFFF) {
      const z = new Uint8Array(56 + 20); const v = new DataView(z.buffer);
      v.setUint32(0, 0x06064b50, true); v.setBigUint64(4, 44n, true); v.setUint16(12, 45, true); v.setUint16(14, 45, true);
      v.setBigUint64(24, BigInt(n), true); v.setBigUint64(32, BigInt(n), true);
      v.setBigUint64(40, BigInt(cdSize), true); v.setBigUint64(48, BigInt(cdStart), true);
      v.setUint32(56, 0x07064b50, true); v.setBigUint64(64, BigInt(this.pos), true); v.setUint32(72, 1, true);
      this._w(z);
    }
    const e = new Uint8Array(22); const v = new DataView(e.buffer);
    v.setUint32(0, 0x06054b50, true);
    v.setUint16(8, Math.min(n, 0xFFFF), true); v.setUint16(10, Math.min(n, 0xFFFF), true);
    v.setUint32(12, Math.min(cdSize, 0xFFFFFFFF), true); v.setUint32(16, Math.min(cdStart, 0xFFFFFFFF), true);
    this._w(e);
    return this.pos;
  }
}
