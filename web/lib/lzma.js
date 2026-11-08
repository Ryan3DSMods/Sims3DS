// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LZMA / LZMA2 decoder (from the LZMA specification, lzma-specification.txt / LzmaSpec.cpp by Igor Pavlov, public
// domain). Used by the web builder to read .7z disc archives in the browser without a server.
//
// LZMA2 is decoded chunk by chunk: every compressed chunk is at most 64 KiB and carries its own range coder, so a
// whole chunk is read into memory first and then decoded synchronously (fast inner loop, no partial-input states).

const kNumStates = 12;
const kMatchMinLen = 2;
const kEndPosModelIndex = 14;
const kNumFullDistances = 1 << (kEndPosModelIndex >>> 1);
const kNumAlignBits = 4;
const kNumLenToPosStates = 4;
const kProbInit = 1024;

export class LzmaDecoder {
  constructor(dictSize) {
    this.dictSize = Math.max(dictSize >>> 0, 4096);
    this.win = new Uint8Array(this.dictSize);
    this.pos = 0;          // write position in win
    this.isFull = false;   // win wrapped at least once
    this.total = 0;        // bytes decoded since the last dictionary reset
    this.lc = 3; this.lp = 0; this.pb = 2;
    this.probs = null;
    // range coder
    this.inBuf = null; this.inPos = 0; this.range = 0; this.code = 0;
    this.state = 0; this.rep0 = 0; this.rep1 = 0; this.rep2 = 0; this.rep3 = 0;
    this.remainLen = 0;
  }

  resetDict() { this.pos = 0; this.isFull = false; this.total = 0; }

  setProps(lc, lp, pb) {
    if (lc > 8 || lp > 4 || pb > 4) throw new Error('LZMA: bad properties');
    this.lc = lc; this.lp = lp; this.pb = pb;
  }

  setPropsByte(d) {
    if (d >= 9 * 5 * 5) throw new Error('LZMA: bad properties byte');
    const lc = d % 9; d = (d / 9) | 0;
    this.setProps(lc, d % 5, (d / 5) | 0);
  }

  resetState() {
    // layout of this.probs (all Uint16): see offsets below
    const litSize = 0x300 << (this.lc + this.lp);
    this.oIsMatch = 0;
    this.oIsRep = this.oIsMatch + (kNumStates << 4);
    this.oIsRepG0 = this.oIsRep + kNumStates;
    this.oIsRepG1 = this.oIsRepG0 + kNumStates;
    this.oIsRepG2 = this.oIsRepG1 + kNumStates;
    this.oIsRep0Long = this.oIsRepG2 + kNumStates;
    this.oPosSlot = this.oIsRep0Long + (kNumStates << 4);
    this.oPosDecoders = this.oPosSlot + (kNumLenToPosStates << 6);
    this.oAlign = this.oPosDecoders + (1 + kNumFullDistances - kEndPosModelIndex);
    this.oLen = this.oAlign + (1 << kNumAlignBits);
    this.oRepLen = this.oLen + 514;            // len coder: choice, choice2, low[16][8], mid[16][8], high[256]
    this.oLiteral = this.oRepLen + 514;
    const n = this.oLiteral + litSize;
    if (!this.probs || this.probs.length !== n) this.probs = new Uint16Array(n);
    this.probs.fill(kProbInit);
    this.state = 0; this.rep0 = this.rep1 = this.rep2 = this.rep3 = 0;
    this.remainLen = 0;
  }

  initRange(buf, pos) {
    this.inBuf = buf; this.inPos = pos;
    if (buf[pos] !== 0) throw new Error('LZMA: bad range coder start');
    this.code = ((buf[pos + 1] << 24) | (buf[pos + 2] << 16) | (buf[pos + 3] << 8) | buf[pos + 4]) >>> 0;
    this.range = 0xFFFFFFFF;
    this.inPos = pos + 5;
  }

  // Copy raw bytes into the dictionary (LZMA2 uncompressed chunk) and return them.
  putRaw(src, start, len, out, outPos) {
    const win = this.win, size = this.dictSize;
    for (let i = 0; i < len; i++) {
      const b = src[start + i];
      win[this.pos++] = b;
      if (this.pos === size) { this.pos = 0; this.isFull = true; }
      out[outPos + i] = b;
    }
    this.total += len;
  }

  // Decode exactly outLen bytes into out[outPos..]. If allowEnd, an end marker may stop early; returns bytes decoded.
  decode(out, outPos, outLen, allowEnd) {
    const probs = this.probs, win = this.win, dictSize = this.dictSize, buf = this.inBuf;
    let range = this.range, code = this.code, ip = this.inPos;
    let pos = this.pos, isFull = this.isFull, total = this.total;
    let state = this.state, rep0 = this.rep0, rep1 = this.rep1, rep2 = this.rep2, rep3 = this.rep3;
    const lc = this.lc, lpMask = (1 << this.lp) - 1, pbMask = (1 << this.pb) - 1;
    const oIsMatch = this.oIsMatch, oIsRep = this.oIsRep, oIsRepG0 = this.oIsRepG0, oIsRepG1 = this.oIsRepG1,
      oIsRepG2 = this.oIsRepG2, oIsRep0Long = this.oIsRep0Long, oPosSlot = this.oPosSlot,
      oPosDecoders = this.oPosDecoders, oAlign = this.oAlign, oLen = this.oLen, oRepLen = this.oRepLen,
      oLiteral = this.oLiteral;
    let op = outPos;
    const outEnd = outPos + outLen;

    // --- range coder primitives (inlined as closures over the locals) ---
    const bit = (i) => {
      const p = probs[i];
      const bound = (range >>> 11) * p;
      let b;
      if ((code >>> 0) < (bound >>> 0)) {
        range = bound; probs[i] = p + ((2048 - p) >>> 5); b = 0;
      } else {
        range = (range - bound) >>> 0; code = (code - bound) >>> 0; probs[i] = p - (p >>> 5); b = 1;
      }
      if (range < 0x1000000) { range = (range << 8) >>> 0; code = ((code << 8) | buf[ip++]) >>> 0; }
      return b;
    };
    const bitTree = (base, numBits) => {
      let m = 1;
      for (let i = 0; i < numBits; i++) m = (m << 1) + bit(base + m);
      return m - (1 << numBits);
    };
    const bitTreeReverse = (base, numBits) => {
      let m = 1, sym = 0;
      for (let i = 0; i < numBits; i++) { const b = bit(base + m); m = (m << 1) + b; sym |= b << i; }
      return sym;
    };
    const direct = (numBits) => {
      let res = 0;
      do {
        range = range >>> 1;
        code = (code - range) >>> 0;
        const t = 0 - (code >>> 31);          // 0 or -1
        code = (code + (range & t)) >>> 0;
        if (code === range) throw new Error('LZMA: corrupted data');
        if (range < 0x1000000) { range = (range << 8) >>> 0; code = ((code << 8) | buf[ip++]) >>> 0; }
        res = ((res << 1) + (t + 1)) >>> 0;
      } while (--numBits);
      return res;
    };
    const lenDecode = (base, posState) => {
      if (bit(base) === 0) return bitTree(base + 2 + (posState << 3), 3);
      if (bit(base + 1) === 0) return 8 + bitTree(base + 2 + 128 + (posState << 3), 3);
      return 16 + bitTree(base + 2 + 256, 8);
    };
    const getByte = (dist) => {               // dist >= 1
      let i = pos - dist; if (i < 0) i += dictSize;
      return win[i];
    };

    // finish a match left over from the previous call
    while (this.remainLen > 0 && op < outEnd) {
      const b = getByte(rep0 + 1);
      win[pos] = b; out[op++] = b; if (++pos === dictSize) { pos = 0; isFull = true; }
      total++; this.remainLen--;
    }

    while (op < outEnd) {
      const posState = total & pbMask;
      if (bit(oIsMatch + (state << 4) + posState) === 0) {
        // literal
        const prev = (total === 0 && !isFull) ? 0 : getByte(1);
        const litState = ((total & lpMask) << lc) + (prev >>> (8 - lc));
        const base = oLiteral + 0x300 * litState;
        let sym = 1;
        if (state >= 7) {
          let matchByte = getByte(rep0 + 1);
          do {
            const matchBit = (matchByte >>> 7) & 1;
            matchByte <<= 1;
            const b = bit(base + ((1 + matchBit) << 8) + sym);
            sym = (sym << 1) | b;
            if (matchBit !== b) break;
          } while (sym < 0x100);
        }
        while (sym < 0x100) sym = (sym << 1) | bit(base + sym);
        const b = sym - 0x100;
        win[pos] = b; out[op++] = b; if (++pos === dictSize) { pos = 0; isFull = true; }
        total++;
        state = state < 4 ? 0 : (state < 10 ? state - 3 : state - 6);
        continue;
      }
      let len;
      if (bit(oIsRep + state) !== 0) {
        if (total === 0 && !isFull) throw new Error('LZMA: corrupted data (rep at start)');
        if (bit(oIsRepG0 + state) === 0) {
          if (bit(oIsRep0Long + (state << 4) + posState) === 0) {
            // short rep
            state = state < 7 ? 9 : 11;
            const b = getByte(rep0 + 1);
            win[pos] = b; out[op++] = b; if (++pos === dictSize) { pos = 0; isFull = true; }
            total++;
            continue;
          }
        } else {
          let dist;
          if (bit(oIsRepG1 + state) === 0) {
            dist = rep1;
          } else {
            if (bit(oIsRepG2 + state) === 0) {
              dist = rep2;
            } else {
              dist = rep3; rep3 = rep2;
            }
            rep2 = rep1;
          }
          rep1 = rep0; rep0 = dist;
        }
        len = lenDecode(oRepLen, posState);
        state = state < 7 ? 8 : 11;
      } else {
        rep3 = rep2; rep2 = rep1; rep1 = rep0;
        len = lenDecode(oLen, posState);
        state = state < 7 ? 7 : 10;
        // distance
        const lenState = len < kNumLenToPosStates - 1 ? len : kNumLenToPosStates - 1;
        const posSlot = bitTree(oPosSlot + (lenState << 6), 6);
        if (posSlot < 4) {
          rep0 = posSlot;
        } else {
          const numDirectBits = (posSlot >>> 1) - 1;
          let dist = ((2 | (posSlot & 1)) << numDirectBits) >>> 0;
          if (posSlot < kEndPosModelIndex) {
            dist = (dist + bitTreeReverse(oPosDecoders + dist - posSlot, numDirectBits)) >>> 0;
          } else {
            dist = (dist + (direct(numDirectBits - kNumAlignBits) << kNumAlignBits)) >>> 0;
            dist = (dist + bitTreeReverse(oAlign, kNumAlignBits)) >>> 0;
          }
          rep0 = dist;
        }
        if (rep0 === 0xFFFFFFFF) {             // end marker
          if (!allowEnd) throw new Error('LZMA: unexpected end marker');
          this.range = range; this.code = code; this.inPos = ip;
          this.pos = pos; this.isFull = isFull; this.total = total;
          this.state = state; this.rep0 = rep0; this.rep1 = rep1; this.rep2 = rep2; this.rep3 = rep3;
          return op - outPos;
        }
        if (rep0 >= dictSize || (!isFull && rep0 >= total)) throw new Error('LZMA: distance beyond the dictionary');
      }
      len += kMatchMinLen;
      // copy the match
      let n = outEnd - op;
      if (len < n) n = len;
      this.remainLen = len - n;
      let src = pos - rep0 - 1; if (src < 0) src += dictSize;
      for (let i = 0; i < n; i++) {
        const b = win[src];
        if (++src === dictSize) src = 0;
        win[pos] = b; out[op++] = b; if (++pos === dictSize) { pos = 0; isFull = true; }
      }
      total += n;
    }

    this.range = range; this.code = code; this.inPos = ip;
    this.pos = pos; this.isFull = isFull; this.total = total;
    this.state = state; this.rep0 = rep0; this.rep1 = rep1; this.rep2 = rep2; this.rep3 = rep3;
    return op - outPos;
  }
}

export function lzma2DictSize(propByte) {
  const bits = propByte & 0x3F;
  if (bits > 40) throw new Error('LZMA2: bad dictionary size');
  if (bits === 40) return 0xFFFFFFFF;
  return (2 | (bits & 1)) * Math.pow(2, (bits >>> 1) + 11);
}

// Decode a whole LZMA (LZMA1, 7z "030101") stream held in memory. props = 5 bytes (props byte + dict size LE).
export function lzmaDecodeBuffer(props, input, outSize) {
  const dict = (props[1] | (props[2] << 8) | (props[3] << 16) | (props[4] << 24)) >>> 0;
  const d = new LzmaDecoder(Math.min(Math.max(dict, 4096), Math.max(outSize, 4096)));
  d.setPropsByte(props[0]);
  d.resetState();
  d.initRange(input, 0);
  const out = new Uint8Array(outSize);
  d.decode(out, 0, outSize, true);
  return out;
}

// Streaming LZMA2: `read(n)` is an async function returning the next n bytes of the packed stream (Uint8Array);
// `emit(chunk)` receives decoded bytes (a fresh Uint8Array each time, may be awaited for back-pressure).
export async function lzma2Decode(propByte, read, emit, expectedSize) {
  const dictSize = lzma2DictSize(propByte);
  // the dictionary never needs to be larger than the whole output
  const d = new LzmaDecoder(expectedSize ? Math.min(dictSize, Math.max(expectedSize, 4096)) : dictSize);
  let needProps = true, needDictReset = true, produced = 0;
  for (;;) {
    const c = (await read(1))[0];
    if (c === 0x00) break;
    if (c === 0x01 || c === 0x02) {
      const h = await read(2);
      const size = ((h[0] << 8) | h[1]) + 1;
      if (c === 0x01) { d.resetDict(); needDictReset = false; } else if (needDictReset) throw new Error('LZMA2: missing dictionary reset');
      const data = await read(size);
      const out = new Uint8Array(size);
      d.putRaw(data, 0, size, out, 0);
      produced += size;
      await emit(out);
      continue;
    }
    if (c < 0x80) throw new Error('LZMA2: bad control byte ' + c);
    const h = await read(4);
    const unpacked = ((c & 0x1F) << 16) + ((h[0] << 8) | h[1]) + 1;
    const packed = ((h[2] << 8) | h[3]) + 1;
    const mode = (c >>> 5) & 3;
    if (mode === 3) { d.resetDict(); needDictReset = false; } else if (needDictReset) throw new Error('LZMA2: missing dictionary reset');
    if (mode >= 2) {
      const p = (await read(1))[0];
      d.setPropsByte(p);
      if (d.lc + d.lp > 4) throw new Error('LZMA2: lc + lp > 4');
      needProps = false;
    } else if (needProps) throw new Error('LZMA2: missing properties');
    if (mode >= 1) d.resetState();
    const input = await read(packed);
    const padded = new Uint8Array(packed + 8);   // a few spare bytes: normalisation may peek past the end
    padded.set(input);
    d.initRange(padded, 0);
    const out = new Uint8Array(unpacked);
    d.decode(out, 0, unpacked, false);
    if (d.remainLen !== 0) throw new Error('LZMA2: match crosses a chunk boundary');
    produced += unpacked;
    await emit(out);
  }
  if (expectedSize && produced !== expectedSize) throw new Error('LZMA2: size mismatch ' + produced + ' != ' + expectedSize);
  return produced;
}
