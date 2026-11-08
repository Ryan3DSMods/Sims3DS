// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web builder's page: picks the input and the output, starts worker.js and shows its progress.

import { discNumber, isDiscImage } from './lib/builder.js';
import { VIDEO_ARGS, AUDIO_ARGS, makeStv, makeWav } from './lib/intro.js';

const $ = (id) => document.getElementById(id);
const FFMPEG_CORE = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core@0.12.6/dist/esm/';
const ZIP_NAME = 'Sims3DS-SD.zip';
const canPickDir = typeof window.showDirectoryPicker === 'function';

let input = null;          // { mode: 'discs' | 'install', files: [{file, path}] }
let sdHandle = null;
let busy = false;

// ---------------------------------------------------------------------------------------------------- input
function gb(n) { return (n / 1e9).toFixed(2) + ' GB'; }

function showPicked() {
  const box = $('picked');
  if (!input) { box.hidden = true; return; }
  box.hidden = false;
  if (input.mode === 'discs') {
    const discs = input.files.map((f, i) => ({ f, n: discNumber(f.file.name, i + 1) })).sort((a, b) => a.n - b.n);
    const nums = new Set(discs.map((d) => d.n));
    const missing = [1, 2, 3, 4].filter((n) => !nums.has(n));
    box.innerHTML = '<strong>' + discs.length + ' disc image' + (discs.length === 1 ? '' : 's') + '</strong><ul>' +
      discs.map((d) => '<li>Disc ' + d.n + ': ' + esc(d.f.file.name) + ' <small>(' + gb(d.f.file.size) + ')</small></li>').join('') + '</ul>' +
      (missing.length ? '<p class="warn">Missing: Disc ' + missing.join(', ') + '. The Complete Collection has 4 discs; all are needed.</p>'
        : '<p class="ok">All 4 discs are here.</p>');
  } else {
    const total = input.files.reduce((s, f) => s + f.file.size, 0);
    const hasGame = input.files.some((f) => /(^|\/)gamedata\//i.test(f.path));
    const hasCab = input.files.some((f) => /(^|\/)data1\.hdr$/i.test(f.path));
    box.innerHTML = '<strong>Folder: ' + esc(input.files[0].path.split('/')[0]) + '</strong> <small>(' + input.files.length + ' files, ' + gb(total) + ')</small>' +
      (hasGame ? '<p class="ok">Found an installed game (GameData).</p>' : hasCab ? '<p class="ok">Found the cabinet files of a copied disc.</p>'
        : '<p class="warn">No GameData folder found here. Pick the folder The Sims is installed in.</p>');
  }
  update();
}

function esc(s) { return String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c])); }

$('discs').addEventListener('change', (e) => {
  const files = [...e.target.files].filter((f) => isDiscImage(f.name));
  input = files.length ? { mode: 'discs', files: files.map((f) => ({ file: f, path: f.name })) } : null;
  document.querySelectorAll('.choice').forEach((c, i) => c.classList.toggle('on', i === 0 && !!input));
  showPicked();
});
$('folder').addEventListener('change', (e) => {
  const files = [...e.target.files];
  input = files.length ? { mode: 'install', files: files.map((f) => ({ file: f, path: f.webkitRelativePath || f.name })) } : null;
  document.querySelectorAll('.choice').forEach((c, i) => c.classList.toggle('on', i === 1 && !!input));
  showPicked();
});

// ---------------------------------------------------------------------------------------------------- output
if (!canPickDir) {
  $('outDir').disabled = true;
  $('outDirLabel').classList.add('off');
  $('outDirLabel').querySelector('small').textContent = 'Not available in this browser. Use Chrome or Edge on a computer, or download the zip.';
  $('outZip').checked = true;
} else {
  $('outDir').checked = true;
}
document.querySelectorAll('input[name="out"]').forEach((r) => r.addEventListener('change', update));

function outKind() { return $('outDir').checked ? 'dir' : 'zip'; }

function update() {
  let why = '';
  if (!input) why = 'Choose your game first.';
  $('start').disabled = busy || !!why;
  $('why').textContent = busy ? '' : (why || (outKind() === 'dir' ? 'You will be asked to pick your SD card.' : 'The zip is made in your browser, then saved to your Downloads.'));
}
update();

// ---------------------------------------------------------------------------------------------------- run
let worker = null, t0 = 0;

function logLine(s) { const l = $('log'); l.textContent += s + '\n'; l.scrollTop = l.scrollHeight; }

function fmtTime(s) { s = Math.max(0, Math.round(s)); const m = Math.floor(s / 60); return m ? m + ' min ' + (s % 60) + ' s' : s + ' s'; }

async function pickSd() {
  const h = await window.showDirectoryPicker({ id: 'sims3ds-sd', mode: 'readwrite' });
  let has3ds = false, hasNin = false;
  for await (const [name, entry] of h.entries()) {
    if (entry.kind !== 'directory') continue;
    if (name.toLowerCase() === '3ds') has3ds = true;
    if (name.toLowerCase() === 'nintendo 3ds') hasNin = true;
  }
  const info = $('sdInfo');
  info.hidden = false;
  if (hasNin || has3ds) { info.className = 'note'; info.textContent = 'Writing to "' + h.name + '" (looks like a 3DS SD card).'; }
  else { info.className = 'note warn'; info.textContent = '"' + h.name + '" does not look like a 3DS SD card (no "Nintendo 3DS" or "3ds" folder). The files go there anyway; copy the 3ds and cia folders to your SD card afterwards.'; }
  return h;
}

async function fetchExtras() {
  const list = [['dist/sims.cia', 'cia/sims.cia'], ['dist/sims.3dsx', '3ds/sims/sims.3dsx'], ['dist/README-on-SD.txt', '3ds/sims/README-on-SD.txt']];
  const out = [];
  for (const [url, path] of list) {
    try { const r = await fetch(url, { cache: 'no-cache' }); if (r.ok) out.push({ path, data: await r.arrayBuffer() }); else logLine('note: ' + url + ' is not on this site (' + r.status + ')'); }
    catch (e) { logLine('note: could not load ' + url); }
  }
  return out;
}

async function convertIntro(avi) {
  setPhase('Converting the EA intro (downloading the converter)...');
  const { FFmpeg } = await import('./vendor/ffmpeg/index.js');
  const toBlobURL = async (url, type) => URL.createObjectURL(new Blob([await (await fetch(url)).arrayBuffer()], { type }));
  const ff = new FFmpeg();
  await ff.load({ coreURL: await toBlobURL(FFMPEG_CORE + 'ffmpeg-core.js', 'text/javascript'),
    wasmURL: await toBlobURL(FFMPEG_CORE + 'ffmpeg-core.wasm', 'application/wasm') });
  setPhase('Converting the EA intro...');
  await ff.writeFile('in.avi', avi);
  await ff.exec(VIDEO_ARGS);
  const video = await ff.readFile('video.raw');
  await ff.exec(AUDIO_ARGS);
  let audio = null;
  try { audio = await ff.readFile('audio.raw'); } catch (e) { audio = null; }
  ff.terminate();
  const stv = makeStv(video);
  if (!stv.frames) throw new Error('the converter produced no frames');
  const wav = audio && audio.length ? makeWav(audio, stv.frames) : null;
  return { stv: stv.data.buffer, wav: wav ? wav.buffer : makeWav(new Uint8Array(0), stv.frames).buffer };
}

function setPhase(s) { $('phase').textContent = s; }

$('start').addEventListener('click', async () => {
  if (busy || !input) return;
  $('err').hidden = true; $('done').hidden = true;
  let output;
  try {
    if (outKind() === 'dir') { sdHandle = await pickSd(); output = { kind: 'dir', handle: sdHandle }; }
    else {
      const est = navigator.storage && navigator.storage.estimate ? await navigator.storage.estimate() : null;
      if (est && est.quota && est.quota - est.usage < 3.6e9) throw new Error('Your browser only allows ' + gb(est.quota - est.usage) + ' of storage for this page, and the zip needs about 3.2 GB. Free up disk space, or use Chrome / Edge and write straight to the SD card.');
      if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {});
      output = { kind: 'zip', name: ZIP_NAME };
    }
  } catch (e) {
    if (e && e.name === 'AbortError') return;
    showError(e.message || String(e)); return;
  }
  busy = true; update();
  $('run').hidden = false; $('log').textContent = ''; $('fill').style.width = '0%'; $('eta').textContent = '';
  setPhase('Loading the app files...');
  const extras = await fetchExtras();
  t0 = Date.now();
  worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
  worker.onmessage = async (ev) => {
    const m = ev.data;
    if (m.type === 'log') logLine(m.msg);
    else if (m.type === 'progress') {
      const f = m.total ? m.done / m.total : 0;
      $('fill').style.width = (100 * f).toFixed(1) + '%';
      const what = m.phase === 'disc' ? 'Reading disc ' + m.disc : m.phase === 'copy' ? 'Copying' : 'Unpacking';
      setPhase(what + (m.filesTotal ? ' · ' + m.files + ' of ' + m.filesTotal + ' files' : '') + ' · ' + (100 * f).toFixed(0) + '%');
      const el = (Date.now() - t0) / 1000;
      if (f > 0.03) $('eta').textContent = 'about ' + fmtTime(el / f - el) + ' left';
    } else if (m.type === 'needIntro') {
      let r;
      try { r = await convertIntro(m.avi); } catch (e) { logLine('intro: ' + (e.message || e)); r = { error: e.message || String(e) }; }
      worker.postMessage({ type: 'intro', ...r }, r.stv ? [r.stv, r.wav] : []);
      setPhase('Finishing...');
    } else if (m.type === 'done') { finish(m); }
    else if (m.type === 'error') { busy = false; update(); worker.terminate(); showError(m.msg); }
  };
  worker.onerror = (e) => { busy = false; update(); showError(e.message || 'the builder stopped unexpectedly'); };
  worker.postMessage({ type: 'start', mode: input.mode, files: input.files, output, extras, verify: $('optVerify').checked, intro: $('optIntro').checked });
});

function showError(msg) {
  const el = $('err'); el.hidden = false;
  el.innerHTML = '<strong>Something went wrong:</strong> ' + esc(msg) + '<br><small>Check that all discs are selected and readable. If it keeps happening, please open an issue on <a href="https://github.com/Ryan3DSMods/Sims3DS/issues">GitHub</a> with the text under "Details".</small>';
}

async function finish(m) {
  busy = false; update(); worker.terminate();
  $('fill').style.width = '100%';
  setPhase('Done in ' + fmtTime(m.seconds) + ' · ' + m.result.files + ' game files (' + gb(m.result.bytes) + ')');
  $('eta').textContent = '';
  const d = $('done'); d.hidden = false;
  const steps = '<ol><li>Put the SD card back in your 3DS.</li><li>Open <strong>FBI</strong> → <em>SD</em> → <code>cia</code> → <code>sims.cia</code> → <em>Install CIA</em>.</li>' +
    '<li>Start <strong>sims3ds</strong> from the HOME Menu. The first start takes 1–2 minutes.</li></ol>' +
    '<p class="hint">Quit the game with START → Quit: pressing HOME while playing can crash the HOME Menu (known issue).</p>';
  if (outKind() === 'dir') {
    d.innerHTML = '<h3>Your SD card is ready.</h3>' + steps;
  } else {
    const root = await navigator.storage.getDirectory();
    const file = await (await root.getFileHandle(ZIP_NAME)).getFile();
    const url = URL.createObjectURL(file);
    d.innerHTML = '<h3>Your zip is ready (' + gb(file.size) + ').</h3><a class="dl" href="' + url + '" download="' + ZIP_NAME + '">Save ' + ZIP_NAME + '</a>' +
      '<p>Unzip it and copy the <code>3ds</code> and <code>cia</code> folders to the root of your SD card. Then:</p>' + steps +
      '<p><button id="clean" class="linkbtn">Delete the zip from the browser\'s storage</button> (after you have saved it).</p>';
    $('clean').addEventListener('click', async () => { URL.revokeObjectURL(url); await root.removeEntry(ZIP_NAME); $('clean').textContent = 'Deleted.'; $('clean').disabled = true; });
  }
}

fetch('dist/version.txt', { cache: 'no-cache' }).then((r) => (r.ok ? r.text() : '')).then((v) => { if (v) $('ver').textContent = 'v' + v.trim(); }).catch(() => {});
