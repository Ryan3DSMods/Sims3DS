// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Incremental MD5 (RFC 1321). The cabinet stores an MD5 for every file; checking it catches a bad disc dump.

const S = [7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
  5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
  6, 10, 15, 21];
const T = new Int32Array(64);
for (let i = 0; i < 64; i++) T[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) | 0;

export class MD5 {
  constructor() { this.h = new Int32Array([0x67452301, 0xefcdab89 | 0, 0x98badcfe | 0, 0x10325476]); this.buf = new Uint8Array(64); this.n = 0; this.len = 0; this.w = new Int32Array(16); }
  block(b, o) {
    const w = this.w, h = this.h;
    for (let i = 0; i < 16; i++) { const q = o + i * 4; w[i] = b[q] | (b[q + 1] << 8) | (b[q + 2] << 16) | (b[q + 3] << 24); }
    let a = h[0], bb = h[1], c = h[2], d = h[3];
    for (let i = 0; i < 16; i++) {
      const x = (a + ((bb & c) | (~bb & d)) + T[i] + w[i]) | 0;
      a = d; d = c; c = bb; bb = (bb + ((x << S[i]) | (x >>> (32 - S[i])))) | 0;
    }
    for (let i = 16; i < 32; i++) {
      const x = (a + ((d & bb) | (~d & c)) + T[i] + w[(5 * i + 1) & 15]) | 0;
      a = d; d = c; c = bb; bb = (bb + ((x << S[i]) | (x >>> (32 - S[i])))) | 0;
    }
    for (let i = 32; i < 48; i++) {
      const x = (a + (bb ^ c ^ d) + T[i] + w[(3 * i + 5) & 15]) | 0;
      a = d; d = c; c = bb; bb = (bb + ((x << S[i]) | (x >>> (32 - S[i])))) | 0;
    }
    for (let i = 48; i < 64; i++) {
      const x = (a + (c ^ (bb | ~d)) + T[i] + w[(7 * i) & 15]) | 0;
      a = d; d = c; c = bb; bb = (bb + ((x << S[i]) | (x >>> (32 - S[i])))) | 0;
    }
    h[0] = (h[0] + a) | 0; h[1] = (h[1] + bb) | 0; h[2] = (h[2] + c) | 0; h[3] = (h[3] + d) | 0;
  }
  update(data) {
    let p = 0;
    this.len += data.length;
    if (this.n) {
      const take = Math.min(64 - this.n, data.length);
      this.buf.set(data.subarray(0, take), this.n); this.n += take; p = take;
      if (this.n < 64) return;
      this.block(this.buf, 0); this.n = 0;
    }
    for (; p + 64 <= data.length; p += 64) this.block(data, p);
    if (p < data.length) { this.buf.set(data.subarray(p), 0); this.n = data.length - p; }
  }
  digest() {
    const bits = this.len * 8;
    const pad = new Uint8Array(((this.n < 56) ? 56 : 120) - this.n + 8);
    pad[0] = 0x80;
    const lo = bits >>> 0, hi = Math.floor(bits / 4294967296);
    const L = pad.length - 8;
    for (let i = 0; i < 4; i++) { pad[L + i] = (lo >>> (8 * i)) & 0xFF; pad[L + 4 + i] = (hi >>> (8 * i)) & 0xFF; }
    this.len -= pad.length;           // padding does not count
    this.update(pad);
    const out = new Uint8Array(16);
    for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) out[i * 4 + j] = (this.h[i] >>> (8 * j)) & 0xFF;
    return out;
  }
}
