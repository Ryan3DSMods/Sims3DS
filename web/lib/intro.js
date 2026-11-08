// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The EA intro for the 3DS (same as src/app/tools/convert_intro.py): ffmpeg (here ffmpeg.wasm, loaded on demand)
// decodes Music/eag_us.avi; this file turns its raw output into cache/intro.stv (grey RLE frames) and
// cache/intro.wav (16-bit stereo at 32728 Hz, exactly as long as the video).

export const W = 400, H = 240, FPS = 30, RATE = 32728;

export const VIDEO_ARGS = ['-loglevel', 'error', '-i', 'in.avi', '-vf',
  `scale=${W}:${H}:force_original_aspect_ratio=decrease,pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2,fps=${FPS}`,
  '-f', 'rawvideo', '-pix_fmt', 'gray', 'video.raw'];
export const AUDIO_ARGS = ['-loglevel', 'error', '-i', 'in.avi', '-vn', '-ac', '2', '-ar', String(RATE),
  '-f', 's16le', '-acodec', 'pcm_s16le', 'audio.raw'];

export function makeStv(raw) {
  const n = Math.floor(raw.length / (W * H));
  const parts = [];
  const head = new Uint8Array(12); const hv = new DataView(head.buffer);
  head.set([0x53, 0x54, 0x56, 0x31]);                       // "STV1"
  hv.setUint16(4, W, true); hv.setUint16(6, H, true); hv.setUint16(8, FPS * 100, true); hv.setUint16(10, n, true);
  parts.push(head);
  const crush = (v) => (v < 8 ? v & 0xFC : v);              // crush near-black noise
  for (let f = 0; f < n; f++) {
    const px = raw.subarray(f * W * H, (f + 1) * W * H);
    const rle = new Uint8Array(px.length * 2); let o = 0;
    let i = 0;
    while (i < px.length) {
      const v = crush(px[i]);
      let j = i + 1;
      while (j < px.length && j - i < 255 && crush(px[j]) === v) j++;
      rle[o++] = j - i; rle[o++] = v;
      i = j;
    }
    const sz = new Uint8Array(4); new DataView(sz.buffer).setUint32(0, o, true);
    parts.push(sz, rle.slice(0, o));
  }
  return { frames: n, data: concat(parts) };
}

export function makeWav(pcm, frames) {
  const want = Math.round(frames * RATE / FPS);
  const have = Math.floor(pcm.length / 4);
  const src = new Int16Array(pcm.buffer.slice(pcm.byteOffset, pcm.byteOffset + have * 4));
  const out = new Int16Array(want * 2);
  out.set(src.subarray(0, Math.min(have, want) * 2));
  if (have > want) {
    const fade = Math.min(want, Math.floor(RATE / 100));    // 10 ms fade-out where the sound is cut
    for (let k = 0; k < fade; k++) {
      const g = (fade - 1 - k) / fade;
      const i = (want - fade + k) * 2;
      out[i] = Math.trunc(out[i] * g); out[i + 1] = Math.trunc(out[i + 1] * g);
    }
  }
  const data = new Uint8Array(out.buffer);
  const h = new Uint8Array(44); const v = new DataView(h.buffer);
  const str = (o, s) => { for (let i = 0; i < 4; i++) h[o + i] = s.charCodeAt(i); };
  str(0, 'RIFF'); v.setUint32(4, 36 + data.length, true); str(8, 'WAVE'); str(12, 'fmt ');
  v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 2, true); v.setUint32(24, RATE, true);
  v.setUint32(28, RATE * 4, true); v.setUint16(32, 4, true); v.setUint16(34, 16, true);
  str(36, 'data'); v.setUint32(40, data.length, true);
  return concat([h, data]);
}

function concat(parts) {
  const n = parts.reduce((s, p) => s + p.length, 0);
  const out = new Uint8Array(n); let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}
