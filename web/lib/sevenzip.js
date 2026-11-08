// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Minimal .7z reader for the web builder: lists the files of an archive and streams one file out of it.
// Supports what disc-image archives use: LZMA2 and LZMA coders, one coder per folder, plain or LZMA-encoded headers.

import { lzmaDecodeBuffer, lzma2Decode } from './lzma.js';

const SIG = [0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C];
const K = { End: 0, Header: 1, ArchiveProperties: 2, AdditionalStreamsInfo: 3, MainStreamsInfo: 4, FilesInfo: 5,
  PackInfo: 6, UnpackInfo: 7, SubStreamsInfo: 8, Size: 9, CRC: 10, Folder: 11, CodersUnpackSize: 12,
  NumUnpackStream: 13, EmptyStream: 14, EmptyFile: 15, Name: 17, EncodedHeader: 23, Dummy: 25 };

class Reader {
  constructor(buf) { this.b = buf; this.p = 0; }
  byte() { if (this.p >= this.b.length) throw new Error('7z: header truncated'); return this.b[this.p++]; }
  bytes(n) { const r = this.b.subarray(this.p, this.p + n); this.p += n; return r; }
  u32() { const b = this.b, p = this.p; this.p += 4; return (b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (b[p + 3] << 24)) >>> 0; }
  u64() { const lo = this.u32(), hi = this.u32(); return hi * 4294967296 + lo; }
  num() {
    const first = this.byte();
    let mask = 0x80, value = 0;
    for (let i = 0; i < 8; i++) {
      if ((first & mask) === 0) {
        const high = first & (mask - 1);
        return value + high * Math.pow(2, 8 * i);
      }
      value += this.byte() * Math.pow(2, 8 * i);
      mask >>>= 1;
    }
    return value;
  }
  bits(n) {   // bit vector, MSB first
    const v = new Array(n); let b = 0, mask = 0;
    for (let i = 0; i < n; i++) { if (mask === 0) { b = this.byte(); mask = 0x80; } v[i] = (b & mask) !== 0; mask >>>= 1; }
    return v;
  }
  digests(n) {
    const all = this.byte();
    const defined = all ? new Array(n).fill(true) : this.bits(n);
    const crcs = new Array(n).fill(null);
    for (let i = 0; i < n; i++) if (defined[i]) crcs[i] = this.u32();
    return crcs;
  }
}

function readPackInfo(r) {
  const info = { packPos: r.num(), sizes: [] };
  const n = r.num();
  for (;;) {
    const id = r.num();
    if (id === K.End) break;
    if (id === K.Size) for (let i = 0; i < n; i++) info.sizes.push(r.num());
    else if (id === K.CRC) r.digests(n);
    else throw new Error('7z: unexpected property in PackInfo: ' + id);
  }
  return info;
}

function readFolder(r) {
  const nCoders = r.num();
  const coders = [];
  let totalIn = 0, totalOut = 0;
  for (let i = 0; i < nCoders; i++) {
    const flags = r.byte();
    const id = Array.from(r.bytes(flags & 0x0F)).map((x) => x.toString(16).padStart(2, '0')).join('');
    let nIn = 1, nOut = 1;
    if (flags & 0x10) { nIn = r.num(); nOut = r.num(); }
    let props = new Uint8Array(0);
    if (flags & 0x20) props = r.bytes(r.num()).slice();
    if (flags & 0x80) throw new Error('7z: alternative coders are not supported');
    coders.push({ id, nIn, nOut, props });
    totalIn += nIn; totalOut += nOut;
  }
  const bindPairs = [];
  for (let i = 0; i < totalOut - 1; i++) bindPairs.push([r.num(), r.num()]);
  const nPacked = totalIn - bindPairs.length;
  const packed = [];
  if (nPacked > 1) for (let i = 0; i < nPacked; i++) packed.push(r.num());
  return { coders, bindPairs, nPacked, totalOut, unpackSizes: [] };
}

function readUnpackInfo(r) {
  if (r.num() !== K.Folder) throw new Error('7z: expected Folder');
  const n = r.num();
  if (r.byte() !== 0) throw new Error('7z: external folders are not supported');
  const folders = [];
  for (let i = 0; i < n; i++) folders.push(readFolder(r));
  if (r.num() !== K.CodersUnpackSize) throw new Error('7z: expected CodersUnpackSize');
  for (const f of folders) for (let i = 0; i < f.totalOut; i++) f.unpackSizes.push(r.num());
  for (;;) {
    const id = r.num();
    if (id === K.End) break;
    if (id === K.CRC) r.digests(n);
    else throw new Error('7z: unexpected property in UnpackInfo: ' + id);
  }
  return folders;
}

function folderUnpackSize(f) {
  // the final output: the out stream not bound to any coder input
  for (let i = f.totalOut - 1; i >= 0; i--) if (!f.bindPairs.some((bp) => bp[1] === i)) return f.unpackSizes[i];
  return f.unpackSizes[f.unpackSizes.length - 1];
}

function readSubStreams(r, folders) {
  const counts = folders.map(() => 1);
  let id = r.num();
  if (id === K.NumUnpackStream) { for (let i = 0; i < folders.length; i++) counts[i] = r.num(); id = r.num(); }
  const sizes = [];
  const hasSize = id === K.Size;
  for (let i = 0; i < folders.length; i++) {
    if (counts[i] === 0) continue;
    let sum = 0;
    if (hasSize) for (let j = 1; j < counts[i]; j++) { const s = r.num(); sizes.push(s); sum += s; }
    sizes.push(folderUnpackSize(folders[i]) - sum);
  }
  if (hasSize) id = r.num();
  while (id !== K.End) {
    if (id === K.CRC) {
      let n = 0;
      for (let i = 0; i < folders.length; i++) n += counts[i];   // (approximation: CRCs are not checked)
      r.digests(n);
    } else throw new Error('7z: unexpected property in SubStreamsInfo: ' + id);
    id = r.num();
  }
  return { counts, sizes };
}

function readStreamsInfo(r) {
  const si = { pack: null, folders: [], sub: null };
  for (;;) {
    const id = r.num();
    if (id === K.End) break;
    if (id === K.PackInfo) si.pack = readPackInfo(r);
    else if (id === K.UnpackInfo) si.folders = readUnpackInfo(r);
    else if (id === K.SubStreamsInfo) si.sub = readSubStreams(r, si.folders);
    else throw new Error('7z: unexpected property in StreamsInfo: ' + id);
  }
  if (!si.sub) si.sub = { counts: si.folders.map(() => 1), sizes: si.folders.map(folderUnpackSize) };
  return si;
}

function readFilesInfo(r) {
  const n = r.num();
  const files = Array.from({ length: n }, () => ({ name: '', emptyStream: false, emptyFile: false }));
  let emptyStreams = 0;
  for (;;) {
    const type = r.num();
    if (type === K.End) break;
    const size = r.num();
    const end = r.p + size;
    if (type === K.Name) {
      if (r.byte() !== 0) throw new Error('7z: external names are not supported');
      for (const f of files) {
        let s = '';
        for (;;) { const c = r.b[r.p] | (r.b[r.p + 1] << 8); r.p += 2; if (c === 0) break; s += String.fromCharCode(c); }
        f.name = s;
      }
    } else if (type === K.EmptyStream) {
      const v = r.bits(n); v.forEach((e, i) => { files[i].emptyStream = e; }); emptyStreams = v.filter(Boolean).length;
    } else if (type === K.EmptyFile) {
      const v = r.bits(emptyStreams); let k = 0;
      for (const f of files) if (f.emptyStream) f.emptyFile = v[k++];
    }
    r.p = end;
  }
  return files;
}

async function decodeFolderToBuffer(src, base, si, fi) {
  const f = si.folders[fi];
  if (f.coders.length !== 1) throw new Error('7z: multi-coder folders are not supported in headers');
  let packOff = base + si.pack.packPos;
  for (let i = 0; i < fi; i++) packOff += si.pack.sizes[i];
  const packed = await src.read(packOff, si.pack.sizes[fi]);
  const c = f.coders[0];
  const size = folderUnpackSize(f);
  if (c.id === '030101') return lzmaDecodeBuffer(c.props, packed, size);
  if (c.id === '21') {
    const out = new Uint8Array(size); let o = 0, p = 0;
    await lzma2Decode(c.props[0], async (n) => { const r = packed.subarray(p, p + n); p += n; return r; },
      async (chunk) => { out.set(chunk, o); o += chunk.length; }, size);
    return out;
  }
  if (c.id === '00') return packed;
  throw new Error('7z: header coder ' + c.id + ' is not supported');
}

// src: { size, read(offset, length) -> Promise<Uint8Array> }
export async function open7z(src) {
  const sh = await src.read(0, 32);
  for (let i = 0; i < 6; i++) if (sh[i] !== SIG[i]) throw new Error('not a 7z archive');
  const r0 = new Reader(sh); r0.p = 12;
  const nextOff = r0.u64(), nextSize = r0.u64();
  let hdr = await src.read(32 + nextOff, nextSize);
  let r = new Reader(hdr);
  let id = r.num();
  while (id === K.EncodedHeader) {
    const si = readStreamsInfo(r);
    hdr = await decodeFolderToBuffer(src, 32, si, 0);
    r = new Reader(hdr);
    id = r.num();
  }
  if (id !== K.Header) throw new Error('7z: bad header');
  let si = null, files = [];
  for (;;) {
    const t = r.num();
    if (t === K.End) break;
    if (t === K.ArchiveProperties) { for (;;) { const p = r.num(); if (p === 0) break; r.p += r.num(); } }
    else if (t === K.AdditionalStreamsInfo) readStreamsInfo(r);
    else if (t === K.MainStreamsInfo) si = readStreamsInfo(r);
    else if (t === K.FilesInfo) files = readFilesInfo(r);
    else throw new Error('7z: unexpected property in Header: ' + t);
  }
  // map files with data to folders / substreams
  const entries = [];
  let fi = 0, inFolder = 0, sizeIdx = 0;
  for (const f of files) {
    if (f.emptyStream) { entries.push({ name: f.name, size: 0, folder: -1 }); continue; }
    while (si && fi < si.folders.length && si.sub.counts[fi] === 0) fi++;
    const e = { name: f.name, size: si.sub.sizes[sizeIdx++], folder: fi, index: inFolder };
    entries.push(e);
    if (++inFolder >= si.sub.counts[fi]) { fi++; inFolder = 0; }
  }
  return { src, si, entries };
}

// Stream one entry: emit(chunk) gets the decoded bytes in order. onProgress(packedBytesRead) optional.
export async function stream7zEntry(arc, entry, emit, onProgress) {
  const { src, si } = arc;
  const f = si.folders[entry.folder];
  if (f.coders.length !== 1) throw new Error('7z: archives with filters (e.g. BCJ) are not supported');
  const c = f.coders[0];
  let packOff = 32 + si.pack.packPos;
  for (let i = 0; i < entry.folder; i++) packOff += si.pack.sizes[i];
  const packSize = si.pack.sizes[entry.folder];
  // skip the substreams before this entry
  let skip = 0;
  { let k = 0; for (let i = 0; i < entry.folder; i++) k += si.sub.counts[i]; for (let j = 0; j < entry.index; j++) skip += si.sub.sizes[k + j]; }
  let remain = entry.size;
  const sink = async (chunk) => {
    let c2 = chunk;
    if (skip > 0) { if (skip >= c2.length) { skip -= c2.length; return; } c2 = c2.subarray(skip); skip = 0; }
    if (remain <= 0) return;
    if (c2.length > remain) c2 = c2.subarray(0, remain);
    remain -= c2.length;
    await emit(c2);
  };
  // buffered reader over the packed stream
  const BLOCK = 4 << 20;
  let buf = new Uint8Array(0), bufPos = 0, filePos = packOff;
  const end = packOff + packSize;
  const read = async (n) => {
    if (bufPos + n > buf.length) {
      const rest = buf.subarray(bufPos);
      const want = Math.max(n - rest.length, Math.min(BLOCK, end - filePos));
      const more = await src.read(filePos, Math.min(want, end - filePos));
      filePos += more.length;
      if (onProgress) onProgress(filePos - packOff, packSize);
      const nb = new Uint8Array(rest.length + more.length);
      nb.set(rest); nb.set(more, rest.length);
      buf = nb; bufPos = 0;
      if (n > buf.length) throw new Error('7z: unexpected end of data');
    }
    const r = buf.subarray(bufPos, bufPos + n); bufPos += n; return r;
  };
  const total = folderUnpackSize(f);
  if (c.id === '21') await lzma2Decode(c.props[0], read, sink, total);
  else if (c.id === '030101') {
    // LZMA1 streams are not chunked: only small ones are supported (read whole)
    if (packSize > 256 << 20) throw new Error('7z: large LZMA (not LZMA2) archives are not supported, re-pack with LZMA2');
    const all = await src.read(packOff, packSize);
    await sink(lzmaDecodeBuffer(c.props, all, total));
  } else if (c.id === '00') {
    let p = 0; while (p < packSize) { const n = Math.min(BLOCK, packSize - p); await sink(await src.read(packOff + p, n)); p += n; if (onProgress) onProgress(p, packSize); }
  } else throw new Error('7z: compression method ' + c.id + ' is not supported (LZMA2, LZMA or store)');
  if (remain !== 0) throw new Error('7z: entry ended early');
}
