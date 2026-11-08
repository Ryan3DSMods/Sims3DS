// Developer test: runs the web builder pipeline in Node on disc images and writes the SD folder layout.
//   node web/test/node_build.mjs <out dir> <disc1.7z> <disc2.7z> <disc3.7z> <disc4.7z>
// Every file is checked against the MD5 stored in the cabinet. (Node 18+, no npm packages needed.)
import fs from 'fs'; import path from 'path'; import crypto from 'crypto';
import { inflateRaw } from '../vendor/pako.esm.js';
import { buildFromDiscs, REQUIRED } from '../lib/builder.js';
const out = process.argv[2]; const discs = process.argv.slice(3);
const inputs = discs.map((p) => { const fd = fs.openSync(p, 'r'); const size = fs.fstatSync(fd).size;
  return { name: path.basename(p), size, read: async (off, len) => { const b = Buffer.allocUnsafe(len); const n = fs.readSync(fd, b, 0, len, off); return new Uint8Array(b.buffer, b.byteOffset, n); } }; });
const sink = { async openFile(rel, size) { const full = path.join(out, rel); fs.mkdirSync(path.dirname(full), { recursive: true }); const fd = fs.openSync(full, 'w');
  return { async write(c) { fs.writeSync(fd, c); }, async close() { fs.closeSync(fd); } }; },
  async duplicate(a, b) { const t = path.join(out, b); fs.mkdirSync(path.dirname(t), { recursive: true }); fs.copyFileSync(path.join(out, a), t); } };
const t0 = Date.now(); let last = 0;
const r = await buildFromDiscs(inputs, sink, { inflateRaw: (d) => inflateRaw(d),
  makeHash: () => { const h = crypto.createHash('md5'); return { update: (b) => h.update(b), digest: () => new Uint8Array(h.digest()) }; },
  log: (m) => console.log(((Date.now() - t0) / 1000).toFixed(1) + 's', m),
  progress: (p) => { if (Date.now() - last > 5000) { last = Date.now(); console.log(((Date.now() - t0) / 1000).toFixed(1) + 's', 'progress', (100 * p.done / p.total).toFixed(1) + '%', p.files + '/' + p.filesTotal); } } });
console.log('result', r.files, r.bytes, 'skipped', r.skippedGroups.join(','), ((Date.now() - t0) / 1000).toFixed(1) + 's');
for (const q of REQUIRED) console.log(q, fs.existsSync(path.join(out, q)));
