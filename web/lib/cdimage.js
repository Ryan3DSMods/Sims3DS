// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Disc images -> a stream of 2048-byte ISO sectors. Raw CD images (.bin / .mdf / .img: MODE1/2352, MODE2/2352,
// 2448-byte sectors with subchannel) keep only the 2048 user-data bytes of every sector; .iso passes through.

const SYNC = [0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00];

export function detectSectorSize(head, totalSize) {
  for (let i = 0; i < 12; i++) if (head[i] !== SYNC[i]) return 2048;
  if (totalSize % 2448 === 0 && totalSize % 2352 !== 0) return 2448;
  return 2352;
}

// Returns async feed(chunk) that calls emit(isoChunk) with user data only. sectorSize from detectSectorSize.
export function makeSectorFilter(sectorSize, emit) {
  if (sectorSize === 2048) return emit;
  let partial = new Uint8Array(0);
  return async (chunk) => {
    let data = chunk;
    if (partial.length) { const m = new Uint8Array(partial.length + chunk.length); m.set(partial); m.set(chunk, partial.length); data = m; }
    const n = Math.floor(data.length / sectorSize);
    if (n > 0) {
      const out = new Uint8Array(n * 2048);
      for (let s = 0; s < n; s++) {
        const base = s * sectorSize;
        const off = data[base + 15] === 2 ? 24 : 16;   // MODE2 form 1: + 8-byte subheader
        out.set(data.subarray(base + off, base + off + 2048), s * 2048);
      }
      partial = data.slice(n * sectorSize);
      await emit(out);
    } else partial = data.slice();
  };
}
