// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web builder's pipeline (same steps as tools/extract/extract_complete_collection.py, all in the browser):
//   disc image (.7z / .bin / .iso / .mdf) -> 2048-byte sectors -> ISO 9660 -> data1.hdr + data*.cab
//   -> InstallShield file groups -> the folder layout sims3ds reads (sdmc:/3ds/sims/...).
// Every disc is read once, front to back; nothing is held in memory except the file being unpacked.

import { open7z, stream7zEntry } from './sevenzip.js';
import { detectSectorSize, makeSectorFilter } from './cdimage.js';
import { RangeFeeder, walkIso } from './iso.js';
import { parseHeader, planExtraction, CabExtractor } from './installshield.js';

// unshield file group -> folder under 3ds/sims/ (tools/extract/extract_complete_collection.py GROUP_MAP)
export const GROUP_MAP = {
  GameData_ALL_Recursive: 'GameData', GameData_ALL_Ranger: 'GameData',
  ExpansionPack1: 'ExpansionPack', ExpansionPack2: 'ExpansionPack2', ExpansionPack3: 'ExpansionPack3',
  ExpansionPack4: 'ExpansionPack4', ExpansionPack5: 'ExpansionPack5', ExpansionPack6: 'ExpansionPack6',
  ExpansionPack7: 'ExpansionPack7', ExpansionPackGOLD: 'Deluxe', ExpansionShared: 'ExpansionShared',
  Downloads: 'Downloads', Music_ALL_Recursive: 'Music', SoundData_SimsSounds: 'SoundData', UIGraphics: 'UIGraphics',
  UserData: 'UserData', UserData2: 'UserData2',
};
export function groupTarget(name) {
  if (Object.prototype.hasOwnProperty.call(GROUP_MAP, name)) return GROUP_MAP[name];
  if (name.startsWith('Template')) return name;
  return null;
}

// folders of an installed game that sims3ds reads (case-insensitive); every Template* folder too
const INSTALL_DIRS = ['GameData', 'UserData', 'UserData2', 'UserData3', 'UserData4', 'UserData5', 'UserData6',
  'UserData7', 'UserData8', 'ExpansionPack', 'ExpansionPack2', 'ExpansionPack3', 'ExpansionPack4', 'ExpansionPack5',
  'ExpansionPack6', 'ExpansionPack7', 'ExpansionShared', 'Deluxe', 'Downloads', 'UIGraphics', 'Music', 'SoundData'];

export function discNumber(name, fallback) {
  const m = /disc\s*(\d+)/i.exec(name) || /cd\s*(\d+)/i.exec(name);
  return m ? parseInt(m[1], 10) : fallback;
}

const IMAGE_RE = /\.(7z|bin|iso|mdf|img)$/i;
export function isDiscImage(name) { return IMAGE_RE.test(name); }

class Stop extends Error { constructor() { super('stop'); } }

// Read one disc image front to back; feed() gets 2048-byte-sector ISO data. src: {name, size, read(off,len)}
async function streamDisc(input, feed, onBytes) {
  const low = input.name.toLowerCase();
  let rawSize = input.size;
  let filter = null;
  const emitRaw = async (chunk) => {
    if (!filter) filter = makeSectorFilter(detectSectorSize(chunk, rawSize), feed);
    await filter(chunk);
  };
  if (low.endsWith('.7z')) {
    const arc = await open7z(input);
    const imgs = arc.entries.filter((e) => /\.(bin|iso|img|mdf)$/i.test(e.name));
    if (!imgs.length) throw new Error(input.name + ': no disc image (.bin / .iso) inside the 7z archive');
    imgs.sort((a, b) => b.size - a.size);
    rawSize = imgs[0].size;
    await stream7zEntry(arc, imgs[0], emitRaw, (done) => onBytes(done));
  } else {
    const BLOCK = 4 << 20;
    for (let p = 0; p < input.size; p += BLOCK) {
      const n = Math.min(BLOCK, input.size - p);
      await emitRaw(await input.read(p, n));
      onBytes(p + n);
    }
  }
}

// Extract from disc images. opts: { inflateRaw, makeHash?, log(msg), progress({phase, done, total, file}) }
export async function buildFromDiscs(inputs, sink, opts) {
  const log = opts.log || (() => {});
  const progress = opts.progress || (() => {});
  const discs = inputs.map((inp, i) => ({ inp, num: discNumber(inp.name, i + 1) })).sort((a, b) => a.num - b.num);
  const totalBytes = discs.reduce((s, d) => s + d.inp.size, 0);
  let doneBefore = 0;
  let model = null, plan = null, extractor = null;
  const SMALL = 64 << 20;
  const buffered = new Map();               // volume -> Uint8Array (small volumes seen before their turn)
  const consumedVol = new Set();
  let maxVolume = 0;
  let filesDone = 0;

  const tryBuffered = async () => {
    while (extractor && buffered.has(extractor.nextVolume)) {
      const v = extractor.nextVolume; const data = buffered.get(v); buffered.delete(v);
      const vol = extractor.volume(v);
      await vol.feed(data); await vol.end(); consumedVol.add(v);
      log('data' + v + '.cab: done');
    }
  };

  const runDisc = async (d) => {
    const feeder = new RangeFeeder();
    const wanted = [];                       // cab volumes on this disc
    let sawCab = false;
    walkIso(feeder, async (f) => {
      const base = f.path.split('/').pop().toLowerCase();
      if (base === 'data1.hdr' && !model) {
        sawCab = true;
        const buf = new Uint8Array(f.size);
        feeder.request(f.lba * 2048, f.size, async (c, off) => { buf.set(c, off); }, async () => {
          model = parseHeader(buf);
          plan = planExtraction(model, groupTarget);
          extractor = new CabExtractor(model, plan, sink, {
            inflateRaw: opts.inflateRaw, makeHash: opts.makeHash,
            onFileDone: (k, targets) => { filesDone += targets.length; },
          });
          maxVolume = Math.max(...model.files.map((x) => x.volume));
          log('cabinet header: InstallShield ' + model.major + ', ' + model.files.length + ' files in ' + model.groups.length +
              ' groups; extracting ' + plan.fileCount + ' files (' + (plan.totalBytes / 1e9).toFixed(2) + ' GB)');
          await tryBuffered();
        });
        return;
      }
      const m = /^data(\d+)\.cab$/.exec(base);
      if (!m) return;
      sawCab = true;
      const v = parseInt(m[1], 10);
      if (consumedVol.has(v)) return;
      wanted.push(v);
      let vol = null, small = null;
      feeder.request(f.lba * 2048, f.size, async (c, off) => {
        if (off === 0) {
          if (extractor && extractor.nextVolume === v) { vol = extractor.volume(v); log('data' + v + '.cab: unpacking'); }
          else if (f.size <= SMALL) small = new Uint8Array(f.size);
        }
        if (vol) await vol.feed(c);
        else if (small) small.set(c, off);
      }, async () => {
        if (vol) { await vol.end(); consumedVol.add(v); log('data' + v + '.cab: done'); await tryBuffered(); }
        else if (small) { buffered.set(v, small); await tryBuffered(); }
      });
    }, (err) => { throw err; });

    try {
      await streamDisc(d.inp, async (chunk) => {
        await feeder.feed(chunk);
        // stop early once everything this disc offered has been read
        if (feeder.pos > 64 * 2048 && feeder.pending === 0 && sawCab) throw new Stop();
      }, (done) => {
        progress({ phase: 'disc', disc: d.num, done: doneBefore + done, total: totalBytes, files: filesDone, filesTotal: plan ? plan.fileCount : 0 });
      });
    } catch (e) { if (!(e instanceof Stop)) throw e; }
    if (!sawCab) log('disc ' + d.num + ' (' + d.inp.name + '): no cabinet files found');
    return wanted.filter((v) => !consumedVol.has(v));
  };

  // first round in disc order; discs with volumes that could not be read in turn (e.g. disc 2 before disc 1) again
  let todo = discs;
  for (let round = 0; round < 3 && todo.length; round++) {
    const again = [];
    for (const d of todo) {
      log('disc ' + d.num + ': ' + d.inp.name);
      const left = await runDisc(d);
      doneBefore += d.inp.size;
      if (left.length) again.push(d);
    }
    todo = again;
    if (todo.length) log('reading ' + todo.length + ' disc(s) again (their cabinet parts came before the parts they continue)');
  }
  if (!model) throw new Error('data1.hdr was not found: is Disc 1 included?');
  for (let v = 1; v <= maxVolume; v++) if (!consumedVol.has(v)) throw new Error('data' + v + '.cab is missing: are all discs included?');
  const missing = extractor.check();
  if (missing.length) throw new Error(missing.length + ' files could not be extracted, e.g. ' + missing.slice(0, 3).join(', '));
  return { files: plan.fileCount, bytes: plan.totalBytes, skippedGroups: plan.skipped };
}

// From loose cabinet files (a copied disc folder or the cabinet files themselves): data1.hdr + data*.cab.
export async function buildFromCabFiles(files, sink, opts) {
  const log = opts.log || (() => {});
  const progress = opts.progress || (() => {});
  const byName = new Map();
  for (const f of files) byName.set(f.path.split('/').pop().toLowerCase(), f);
  const hdrFile = byName.get('data1.hdr');
  if (!hdrFile) throw new Error('data1.hdr was not found');
  const model = parseHeader(await hdrFile.read(0, hdrFile.size));
  const plan = planExtraction(model, groupTarget);
  let filesDone = 0;
  const ex = new CabExtractor(model, plan, sink, { inflateRaw: opts.inflateRaw, makeHash: opts.makeHash,
    onFileDone: (k, t) => { filesDone += t.length; } });
  const maxVolume = Math.max(...model.files.map((x) => x.volume));
  const vols = [];
  for (let v = 1; v <= maxVolume; v++) {
    const f = byName.get('data' + v + '.cab');
    if (!f) throw new Error('data' + v + '.cab is missing');
    vols.push(f);
  }
  const total = vols.reduce((s, f) => s + f.size, 0);
  log('cabinet header: InstallShield ' + model.major + '; extracting ' + plan.fileCount + ' files (' + (plan.totalBytes / 1e9).toFixed(2) + ' GB)');
  let done = 0;
  for (let i = 0; i < vols.length; i++) {
    const f = vols[i]; const vol = ex.volume(i + 1);
    const BLOCK = 4 << 20;
    for (let p = 0; p < f.size; p += BLOCK) {
      const c = await f.read(p, Math.min(BLOCK, f.size - p));
      await vol.feed(c); done += c.length;
      progress({ phase: 'cab', done, total, files: filesDone, filesTotal: plan.fileCount });
    }
    await vol.end();
    log('data' + (i + 1) + '.cab: done');
  }
  const missing = ex.check();
  if (missing.length) throw new Error(missing.length + ' files could not be extracted, e.g. ' + missing.slice(0, 3).join(', '));
  return { files: plan.fileCount, bytes: plan.totalBytes, skippedGroups: plan.skipped };
}

// Copy from an installed game. files: [{ path: 'The Sims/GameData/...', size, read(off,len) }]
export async function buildFromInstall(files, sink, opts) {
  const log = opts.log || (() => {});
  const progress = opts.progress || (() => {});
  const want = new Map(INSTALL_DIRS.map((n) => [n.toLowerCase(), n]));
  // find the game root: the folder that holds GameData and UserData
  const roots = new Map();
  for (const f of files) {
    const parts = f.path.split('/');
    for (let i = 0; i < parts.length - 1; i++) {
      if (parts[i].toLowerCase() === 'gamedata') { const r = parts.slice(0, i).join('/'); roots.set(r, (roots.get(r) || 0) + 1); }
    }
  }
  let root = null, best = 0;
  for (const [r, n] of roots) if (n > best) { best = n; root = r; }
  if (root === null) throw new Error('this folder does not contain a GameData folder: pick the folder The Sims is installed in');
  const pre = root ? root + '/' : '';
  const plan = [];
  for (const f of files) {
    if (!f.path.startsWith(pre)) continue;
    const rel = f.path.slice(pre.length);
    const parts = rel.split('/');
    if (parts.length < 2) continue;
    const top = parts[0].toLowerCase();
    let target = want.get(top);
    if (!target && top.startsWith('template')) target = parts[0];
    if (!target) continue;
    if (parts.some((p) => p.toLowerCase() === 'saves')) continue;
    const name = parts[parts.length - 1].toLowerCase();
    if (name.endsWith('.exe') || name.endsWith('.dll') || name === 'thumbs.db' || name === '.ds_store') continue;
    plan.push({ f, path: [target, ...parts.slice(1)].join('/') });
  }
  if (!plan.some((p) => p.path.toLowerCase() === 'userdata/neighborhood.iff')) throw new Error('UserData/Neighborhood.iff is missing in this folder');
  const total = plan.reduce((s, p) => s + p.f.size, 0);
  log('installed game in "' + (root || '(the chosen folder)') + '": copying ' + plan.length + ' files (' + (total / 1e9).toFixed(2) + ' GB)');
  let done = 0, n = 0;
  for (const { f, path } of plan) {
    const w = await sink.openFile(path, f.size);
    const BLOCK = 4 << 20;
    for (let p = 0; p < f.size; p += BLOCK) { const c = await f.read(p, Math.min(BLOCK, f.size - p)); await w.write(c); done += c.length; progress({ phase: 'copy', done, total, files: n, filesTotal: plan.length }); }
    await w.close();
    n++;
  }
  return { files: plan.length, bytes: total };
}

export const REQUIRED = ['GameData/Objects/Objects.far', 'GameData/Global/Global.far', 'GameData/walls.iff',
  'GameData/floors.iff', 'UserData/Neighborhood.iff'];
