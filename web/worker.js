// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web builder's worker: runs the pipeline (lib/builder.js) off the page's thread and writes the result either
// straight into a folder (the SD card, File System Access API) or into a ZIP in the browser's private storage.

import { inflateRaw } from './vendor/pako.esm.js';
import { buildFromDiscs, buildFromInstall, buildFromCabFiles, isDiscImage, REQUIRED } from './lib/builder.js';
import { MD5 } from './lib/md5.js';
import { ZipWriter } from './lib/zip.js';

const post = (m, t) => self.postMessage(m, t || []);
const log = (msg) => post({ type: 'log', msg });

function fileSource(file, path) {
  return { name: file.name, path: path || file.webkitRelativePath || file.name, size: file.size,
    read: async (off, len) => new Uint8Array(await file.slice(off, off + len).arrayBuffer()) };
}

// ---- sinks: openFile(path) -> { write(bytes), close() }
function dirSink(root) {
  const dirs = new Map([['', root]]);
  const getDir = async (path) => {
    if (dirs.has(path)) return dirs.get(path);
    const i = path.lastIndexOf('/');
    const parent = await getDir(i < 0 ? '' : path.slice(0, i));
    const h = await parent.getDirectoryHandle(path.slice(i + 1), { create: true });
    dirs.set(path, h);
    return h;
  };
  return {
    async openFile(path) {
      const i = path.lastIndexOf('/');
      const dir = await getDir(i < 0 ? '' : path.slice(0, i));
      const fh = await dir.getFileHandle(path.slice(i + 1), { create: true });
      const w = await fh.createWritable({ keepExistingData: false });
      return { write: (b) => w.write(b), close: () => w.close() };
    },
    async duplicate(from, to) {
      const i = from.lastIndexOf('/');
      const src = await (await (await getDir(i < 0 ? '' : from.slice(0, i))).getFileHandle(from.slice(i + 1))).getFile();
      const w = await this.openFile(to);
      for (let p = 0; p < src.size; p += 4 << 20) await w.write(new Uint8Array(await src.slice(p, p + (4 << 20)).arrayBuffer()));
      await w.close();
    },
    async finish() {},
  };
}

async function zipSink(name) {
  const root = await navigator.storage.getDirectory();
  const fh = await root.getFileHandle(name, { create: true });
  const ah = await fh.createSyncAccessHandle();
  ah.truncate(0);
  const zip = new ZipWriter({ write: (b, at) => { const n = ah.write(b, { at }); if (n !== b.length) throw new Error('could not write the zip (is the disk full?)'); } });
  return {
    async openFile(path) { const e = zip.addFile(path); return { write: async (b) => e.write(b), close: async () => e.close() }; },
    async duplicate(from, to) {
      zip.addCopy(to, from, (at, n) => { const b = new Uint8Array(n); const got = ah.read(b, { at }); if (got !== n) throw new Error('zip: short read'); return b; });
    },
    async finish() { const size = zip.finish(); ah.flush(); ah.close(); return size; },
    abort() { try { ah.close(); } catch (e) { /* closed */ } },
  };
}

// prefix every path (game data lives in 3ds/sims/) and remember a few files on the way
function prefixed(sink, prefix, keep) {
  return {
    async openFile(path, size) {
      const w = await sink.openFile(prefix + path, size);
      const lower = path.toLowerCase();
      if (keep && keep.has(lower)) {
        const parts = [];
        return { write: async (b) => { parts.push(b.slice()); await w.write(b); },
          close: async () => { await w.close(); keep.set(lower, parts); } };
      }
      return w;
    },
    duplicate: (from, to) => sink.duplicate(prefix + from, prefix + to),
  };
}

let introResolve = null;
self.onmessage = async (ev) => {
  const m = ev.data;
  if (m.type === 'intro') { if (introResolve) introResolve(m); return; }
  if (m.type !== 'start') return;
  let sink = null;
  const t0 = Date.now();
  try {
    sink = m.output.kind === 'dir' ? dirSink(m.output.handle) : await zipSink(m.output.name);
    // the app itself
    for (const x of m.extras || []) {
      const w = await sink.openFile(x.path); await w.write(new Uint8Array(x.data)); await w.close();
      log('added ' + x.path);
    }
    const keep = new Map([['music/eag_us.avi', null]]);
    const data = prefixed(sink, '3ds/sims/', keep);
    const opts = {
      inflateRaw: (d) => inflateRaw(d),
      makeHash: m.verify ? () => new MD5() : null,
      log,
      progress: (p) => post({ type: 'progress', ...p }),
    };
    let result;
    const files = m.files.map((f) => fileSource(f.file, f.path));
    if (m.mode === 'discs') {
      const imgs = files.filter((f) => isDiscImage(f.name));
      if (!imgs.length) throw new Error('no disc images selected (.7z, .bin, .iso, .mdf)');
      result = await buildFromDiscs(imgs, data, opts);
    } else {
      const hasGame = files.some((f) => /(^|\/)gamedata\//i.test(f.path));
      const hasCab = files.some((f) => /(^|\/)data1\.hdr$/i.test(f.path));
      if (!hasGame && hasCab) result = await buildFromCabFiles(files, data, opts);
      else result = await buildFromInstall(files, data, opts);
    }
    // the EA intro: converted on the page (ffmpeg.wasm), written here
    const avi = keep.get('music/eag_us.avi');
    if (m.intro && avi) {
      post({ type: 'needIntro', avi: concat(avi) });
      const r = await new Promise((res) => { introResolve = res; });
      if (r.stv) {
        for (const [p, b] of [['3ds/sims/cache/intro.stv', r.stv], ['3ds/sims/cache/intro.wav', r.wav]]) {
          const w = await sink.openFile(p); await w.write(new Uint8Array(b)); await w.close();
        }
        log('EA intro converted');
      } else log('EA intro skipped: ' + (r.error || 'not converted'));
    }
    const zipSize = await sink.finish();
    post({ type: 'done', result, zipSize, seconds: (Date.now() - t0) / 1000 });
  } catch (e) {
    if (sink && sink.abort) sink.abort();
    post({ type: 'error', msg: e && e.message ? e.message : String(e) });
  }
};

function concat(parts) {
  const n = parts.reduce((s, p) => s + p.length, 0);
  const out = new Uint8Array(n); let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}

void REQUIRED;
