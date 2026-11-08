# Sims3DS web builder

The page at **https://ryan3dsmods.github.io/Sims3DS/**. It does what `tools/extract/extract_complete_collection.py`
does, in the browser: it turns the player's own discs (or installed game) into the SD-card layout and adds the app.
Nothing is uploaded; there is no server code.

| File | What it does |
|---|---|
| `index.html`, `style.css`, `app.js` | the page: input, output (SD card folder or zip), progress, intro conversion |
| `worker.js` | runs the pipeline in a Web Worker; sinks: a folder (File System Access API) or a ZIP in OPFS |
| `lib/lzma.js`, `lib/sevenzip.js` | LZMA/LZMA2 decoder and a minimal `.7z` reader (streams the disc image out of the archive) |
| `lib/cdimage.js`, `lib/iso.js` | raw CD sectors (2352/2448) -> 2048-byte sectors; ISO 9660 / Joliet read on a stream |
| `lib/installshield.js` | InstallShield 6+ cabinets (`data1.hdr` + `data*.cab`), after unshield, unpacked as the bytes go by |
| `lib/builder.js` | the pipeline and the group -> folder map (same as the Python extractor) |
| `lib/intro.js` | `cache/intro.stv` + `intro.wav` from ffmpeg.wasm's raw output (same as `src/app/tools/convert_intro.py`) |
| `lib/md5.js`, `lib/zip.js` | MD5 (the cabinet's checksums) and a streaming ZIP writer (ZIP64) |
| `vendor/` | pako 2.1.0 (raw inflate, MIT/Zlib) and the @ffmpeg/ffmpeg 0.12.10 wrapper (MIT) |
| `test/node_build.mjs` | runs the same pipeline in Node on disc images (developer test) |

Each disc is read once from start to end (a `.7z` is decoded on the fly), so memory use stays small. Files are
checked against the MD5 sums stored in the cabinet. `dist/` (sims.cia, sims.3dsx, README-on-SD.txt, version.txt) is
filled from the latest release by `.github/workflows/pages.yml` at deploy time.

Tested against the Python extractor on the Complete Collection (USA): all 4435 files byte-identical, in Chromium
(both outputs) and in Node. Typical time: 3-4 minutes.

Local try-out: `cd web && python3 -m http.server`, then open http://localhost:8000/ (copy the release's
`sims.cia`, `sims.3dsx` and `README-on-SD.txt` into `web/dist/` first, or the page skips them).
