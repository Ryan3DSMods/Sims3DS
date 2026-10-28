# src/app — the playable sims app (shared by the 3DS build and the host simulator)

*Developer notes. Paths under `qa/` mentioned below refer to the developers' local test workspace (scripts, logs,
screenshots); it is not part of this repository because it is full of output made from the game data.*

## 3DS readiness — status (memory / load time / robustness)

Measured with the **32-bit host build** (same App code; pointers/size_t as on the ARM11) and a counting
`operator new` (`mem LABEL` script command): boot -> neighbourhood -> UserData2 house 9 (2 Sims, ~100 objects)
-> buy -> build -> Downtown (house 50) -> back to the neighbourhood -> house 9 again. Heap = live C++ heap.
SD time = estimate for a 3DS SD card through stdio (4 MB/s, 1 ms per seek, 5 ms per open; `FilePool::Stats`).

| | before | after |
|---|---|---|
| neighbourhood (boot done) | 28.5 MB | **20.1 MB** |
| full house loaded / playing | 99.0 / 99.8 MB | **43.3 / 44.0 MB** |
| peak through buy, build, Downtown, back | 104.7 MB, growing per lot | **46.5 MB**, flat (house 9 again: 43.8 MB) |
| near zoom on that house (worst case) | +17 MB, kept forever | **53.6 MB**, back to 46.5 at medium |
| first boot, SD I/O (no cache) | 685 MB read, **~182 s** | 38.6 MB, **~14 s** (+ the one-time save-area copy, 66 MB) |
| first Buy mode (catalogue scan) | ~680 MB read again | OBJD chunks only (~20 MB; 0 with a prebuilt cache) |
| boot with caches | ~3 s | ~3.4 s (more small files: Downloads walls/floors) |
| loading that house | 54 MB read, 588 opens, ~17 s | 13.3 MB read, ~20 opens, **~6 s** (+ ~1-2 s CPU) |
| files open at once | 83+ (crash: "Could not open file .../walls.iff") | <= 6 pooled + streams |

What changed (all TS1(sims)-marked):
- `files/FilePool`: at most 6 archive handles (LRU, reopen on demand, 32 KB buffers), I/O counters, `ReadWholeFile` with
  `strerror` messages and a retry after releasing handles; FAR1 manifests read in one block; `FAR1Archive::DropManifest`.
- `IffFile::ReadFromFile` (rsmp-driven): reads the file header + resource map, then only the chunks needed; sprites,
  bitmaps, strings, catalogue text, pie labels, draw groups, sounds (`SPR2 SPR# BMP_ STR# CTSS TTAs DGRP FWAV TPRP
  THMB`, ~95 % of an object file) stay on disk until first used (one seek + read each). Used for object files,
  Global.far, work.iff (CARR only), walls/floors/Build.iff, *.wll/*.flr, Sprites.iff (`IffFile::OpenReadOnly`; never
  for save files). `onlyType` reads one chunk type (the object index and the catalogue scan read only OBJD).
  Render output is identical (A/B with `SIM3DS_NODEFER=1`; the only differences are the Sims' pre-existing
  dependence on heap addresses).
- Leaving a lot (travel or back to the neighbourhood) frees its object files and the assembled BHAV routines
  (`WorldObjectProvider::ReleaseLoaded`, `VM::ReleaseAssembled`); controller objects stay.
- Zoom change frees the decoded pixels of all object sprite frames (`SPR2Frame::DropDecoded`; an SPR2 holds the frames
  of all three zooms, near = 4x medium); the next frame decodes what the new zoom shows.
- The neighbourhood map art (~8 MB) is freed while a lot is played and rebuilt on return.
- 3DS: `__ctru_linear_heap_size = 8 MB` (libctru's default gives the linear heap up to 32 MB; this app draws in
  software), so malloc gets ~24 MB more.
- Caches (all text, relative paths, no endianness/pointer-size dependence; `cache_version.txt` = 3 rebuilds old ones):
  `objects.cache` (STOC3), `catalog.cache`, `wallfloorwll.cache` / `wallfloorflr.cache`, `cmxnames.cache` (names
  defined by the ~300 loose skin CMX files: saves ~300 SD opens on the first Sim of every boot); hardware pass:
  `avatars.cache`, `bcfnames.cache`, `dirs.cache` (see "Hardware pass" below). After adding loose skins to a Skins
  folder (not a new Downloads folder), delete `cache/avatars.cache`, `bcfnames.cache` and `dirs.cache`. Downloads/*.far are
  sorted so archive indexes match on every file system.
- Robustness: every loading step writes `[  12.345 s] <step> | heap used .. MB (arena ..), app free .. MB, linear free
  .. MB | io .. MB, .. opens, .. seeks` to log.txt (when `sdmc:/3ds/sims/debug` exists; flushed per line) plus
  a progress line every 2 s inside long steps; failures end on the error screen and in the log as `FATAL: ...`
  (out of memory names the step; lot-load exceptions no longer retry every frame); `init:` / `frame:` exceptions and
  out-of-memory are logged with the memory summary; save-area copy write errors (SD full) are reported and the partial
  file removed. The first log line gives the console type and application/linear memory.
- Loading screens show real progress during the blocking steps ("Indexing objects (first run only): 412 / 1192",
  "Loading objects: 37 (4.2 MB read)").

**Memory pass (26 Oct, `// FIX(sims)`)** -- `qa/scripts/long32.txt` on the 32-bit host (C++ heap, `mem LABEL`):

| | before (`qa/long32_base.log`) | after (`qa/long32_mem.log`) |
|---|---|---|
| neighbourhood at boot | 26.6 MB | **17.0 MB** |
| Goths loaded / after 1800 frames (first game day) | 46.1 / 66.5 MB | **30.9 / 42.6 MB** |
| near zoom on that lot | 83.0 MB | **50.7 MB** |
| Downtown / Vacation / Old Town 61 / Studio 83 / Magic 96 | 63.6 / 69.3 / 68.4 / 82.2 / 86.9 MB | **39.8 / 46.6 / 43.2 / 43.7 / 53.1 MB** |
| back in the neighbourhood after all that | 65.1 MB | **23.8 MB** |
| peak of the whole session / RSS high-water | 88.6 / 103 MB | **53.2 / 69.5 MB** |

Steady state (`qa/scripts/long32_leak.txt`: the neighbourhood visited after a first lot, then the whole session):
22.9 MB -> 23.7 MB, i.e. nothing accumulates; what the neighbourhood holds over the boot value (~6 MB) is built once by the
first lot (avatar file index 2.8 MB, the controller objects every lot creates 2 MB, catalogue index, lot UI art).
Found with a heap profiler in the host tool (`SIM3DS_HEAPPROF=<dir>` records the call stack of every live block;
`memprof LABEL` dumps it; `qa/tools/heapprof.py <binary> <dump> [<older dump>] [--focus=Function]` symbolises and
diffs by allocating function). What changed:
- Strings: install files read through `IffFile::ReadFromFile` keep only US English (+ the default language) of a
  format -3 STR# (`STR::Read`); the other 19 translations were 2/3 of all decoded strings (8 MB on a lot).
- Character files (`UserData/Characters`, TemplateNPCs) are opened read-only with deferred chunks (were read whole:
  their 20-language STR# 130-132 + portraits, ~6 MB for a neighbourhood). `IffFile::DetachSource(path)` (called by
  `RawIff::WriteFileAtomic`) loads what is still on disk before a character file is rewritten (outfit changes), so
  nothing reads the new file at old offsets; it also closes the pooled handle.
- Career strings (work.iff STR#, dialog tokens): `onlyType` reads keep heavy chunks deferred (4 MB of raw bytes).
- Decoded sprites are palettised: SPR2 frames 3 bytes/pixel (index + alpha + z; were RGBA + index + z + an unused
  alpha array: 7), SPR# (walls/floors) 2 bytes (index + alpha; were RGBA). `_2DWorldBatch::RenderSprite` looks the
  palette up per drawn pixel (one loop per texel format): output pixel-identical (A/B `qa/shots_ab/A` vs `B`: only
  moving Sims differ), +1.3 % instructions in RenderSprite (callgrind, `qa/scripts/bench_render.txt`).
- Wall/floor SPR# frames of read-only files are read again from the file when decoded (no compressed copy, ~3 MB) and
  `IffFile::DropDecodedFrames` frees decoded SPR#/SPR2 pixels on a zoom change and when a lot is left.
- Leaving a lot (`ReleaseLotContent`) also frees: avatar meshes, avatar textures, all animation keyframes and the BCF
  files not used by that lot (`TS1AvatarContent::ReleaseCaches`, `TS1BCFProvider::TrimCache`), the lot UI art
  (`UIGraphics::ReleaseImages`), the catalogue details (`UICatalog::TrimDetails(0)`), the headline sprites. Closing
  Create-A-Sim frees the avatar caches too. Before, all of these were kept for the whole session (~25 MB).
- The avatar file index (22k names) is frozen after Init into one name pool + sorted table (`TS1Provider::Freeze`):
  0.9 MB instead of 2.5 MB of hash nodes.
- Cost: wall frames, avatar files and BCFs are read again per lot: ~0.3 s more SD time per lot visit (strace:
  walls.iff +740 seeks, Animation.far +540 seeks over the whole session).

Still possible: the object index (`WorldObjectProvider::Init`, 4.5 MB of strings/maps for 5300 objects) and the BCF
name maps (1.4 MB) could be compacted the same way; animation keyframe budget (6 MB) could be lowered.

**Prebuilt caches** (skip the slow first boot): on a PC with the same install tree as the SD card,
```
make -f Makefile.port build-port/simcache
build-port/simcache <installRoot> <outDir>      # ~25 s of SD work done in < 1 s
```
then copy `<outDir>/*` (objects.cache, catalog.cache, wallfloorwll.cache, wallfloorflr.cache, cmxnames.cache,
avatars.cache, bcfnames.cache, dirs.cache, cache_version.txt) to `sdmc:/3ds/sims/cache/`. A cache that does not match the card's archives is rebuilt
silently.

What to look for in log.txt: the last `[ t s] step` line is where a hang/crash happened; `app free` falling under
~10 MB means memory trouble; `FATAL:` lines carry the on-screen error (with `strerror` for files); `WARNING: could
not write` = SD card full.

**Hardware pass (26 Oct, first New 3DS run; `// TS1(sims) hardware pass`, `// FIX(sims) hardware pass`)** -- CIA build,
178 MB mode: boot to the neighbourhood in 36 s, then Create-A-Family and entering a lot "did not respond". Cause: the
first Sim of a session (CAS line-up or a lot's family) built the avatar file index by walking the whole install tree
with a `stat` per entry (3,600 stats, 360 directory operations) and opening every archive again; ~25-30 s with nothing
on screen (CAS had no loading screen at all, the lot kept its static picture) and nothing in log.txt (no step there).

*Calibrated SD cost model* (`files/FilePool.h SdCostModel`): the hardware log's step times fitted against the file
system operations of the same steps counted on the host (identical I/O: the pool counters matched the log to the
byte). The SD card is not slow at reading -- directories are:

| operation (devkitARM newlib -> libctru archive device -> FS service) | cost on the New 3DS |
|---|---|
| open a directory (`opendir`), or `stat` a directory (failed `FSUSER_OpenFile` + `OpenDirectory`) | **~36 ms** |
| open / `stat` a file, failed `stat` | ~2 ms |
| a read request (stdio buffer refill or a large `fread`) | ~0.3 ms + ~80 ms per MB |
| `fseek` (only moves the offset; `SEEK_END` asks the size) | ~0 |
| CPU (804 MHz ARM11 vs. the host build) | ~22x the host's CPU time |

Fit: object archives + index 7.4 s (152 directory ops), walls 9.5 s / floors 8.1 s (224 directory ops each: the per-entry
`IsDir` stats of the Downloads scan), characters 1.4 s (280 file ops, 7.5 MB), neighbourhood 7.5 s (mostly CPU); the
residuals are within ~0.5 s. Not separable from this log: the exact per-MB and per-read cost (the next log measures
them: see below). Consequence: bigger stdio buffers do not help (random chunk reads would read more), opens are cheap,
and every directory listing / directory stat avoided saves ~36 ms.

What changed:
- `FilePool`: `ListDirectory` (types from `readdir`'s `d_type`, no stat per entry) and `ListInstallDirectory` (the
  install tree's listings remembered for the run and in `cache/dirs.cache`; a listing is reused when its parent was
  listed live and did not change; `Downloads/` itself is always listed live, so a new download is seen), `InstallFile/
  DirExists`. Used by the object archive list, the walls/floors scans (`WorldContentFiles::FindInstall`,
  `FilesWithExt(..., install)`) and the avatar index. Pool size 6 -> **16** archives (a lot load re-opened the same 10
  archives ~200 times; the walls.iff failure was at 83 open files).
- Avatar index (`TS1Provider`): saved as `cache/avatars.cache` (22k names, keyed by the install's archive list), the
  three animation/suit/skeleton name maps as `cache/bcfnames.cache` (was: every AnimationOut.ndx = ~75 archive opens);
  a rebuild walks with typed listings and skips Music/SoundData/Sound/Saves/cache/Template* (nothing for avatars there),
  with progress ("Indexing Sim files (first run only): n / 85"). `tools/simcache` prebuilds both plus dirs.cache.
- Create-A-Family has a loading screen (both screens, the Create A Family background art, the step name and a bar):
  the first frame shows it, the second builds the CAS screen while each step redraws it (`SubScreen::DrawProgress`).
- `log.txt` (when `sdmc:/3ds/sims/debug` exists), new lines:
  - `[  t s]   done <step>: 7.39 s | fs 83 opens 0.17 s, 24 stats 0.83 s, 83 dirs+86 dir reads 3.07 s, 161 reads 6.14 MB
    0.56 s, 162 seeks, 0 writes 0.00 s | pool 83 opens, 81 files` when a step ends: wall time, then every SD operation
    of the step with the time the FS service really took (3DS: `sdhook` in `platform/3ds/main.cpp` wraps libctru's
    sdmc device table and times each call with the system tick; main thread only), then the FilePool opens and the
    number of distinct files. The host adds `est. 3DS x s (sd y + cpu z)` from the model.
  - `[  t s]   ...still in <step>: 4.01 s | fs ... | heap ...` every ~2 s while a step keeps touching the SD card
    (called from the device hook itself, so even a loop without progress reports writes it), and
    `...still in <step>: <progress> n/m` from the progress reports.
  - `[  t s]   slow frame: 5.95 s in loading, last step 'lot 5: first frame'` for any frame over 1 s.
  - new steps: `cas: opening Create-A-Family`, `cas: UI art`, `cas: skin catalogue`, `avatars: file index + animation
    names` (nested; the outer step then continues as `<step> (continued)`), `avatars: indexing the Sim files (first
    run only)`, `cas: first frame`, `cas: running`, `cas: saving the family`, `lot N: first frame`, `lot N: playing`,
    `save: house N`, `save: neighbourhood`, `front: neighbourhood ready`, `front: back to the neighbourhood`.
  - memory: `heap free` is now what malloc can still hand out (heap size - arena + free blocks). The old `app free` was
    `osGetMemRegionFree(APPLICATION)`, constant because the whole heap is committed before main: its 46.3 MB is memory
    in neither heap (96 MB app heap cap + 32 MB linear cap in 178 MB mode), now printed once as `not mapped` in the
    first line. (It could become linear heap for large buffers if memory ever runs short.)

Estimates (host counts x model; `SIM3DS_SDSIM=1` reproduces them in real time), house 5 (Goths) of neighbourhood 1:

| | before | after (caches built) | first run after the update |
|---|---|---|---|
| boot: object archives + index / walls / floors | 7.4 / 9.5 / 8.1 s (measured) | 2.1 / 0.7 / 0.3 s | 6.2 / 1.2 / 0.4 s (Downloads listed once) |
| boot to the neighbourhood | 36 s (measured) | ~14 s | ~18 s |
| Create-A-Family opens | ~27 s, frozen screen | **~3.6 s**, loading screen | ~11 s with progress (avatar index) |
| enter a house (tap -> playing) | ~29 s, static picture | **~6.5 s** | ~13.5 s with progress |

Host tools for this: `SIM3DS_LOGSTEPS=1` prints the same lines as log.txt (with the 3DS estimate), `SIM3DS_SDSIM=<scale>`
sleeps the modelled cost of every operation (1 = console speed; `src/tools/host_sdsim.cpp` interposes fopen/fread/
fseek/stat/opendir/readdir and models newlib's buffers and the FS calls), `[mem]` lines add the `fs ... 3DS model` totals.
Next log to check: the `done` lines' measured `dirs` / `stats` / `reads` times against the table above, and whether
`stats` stays near 0 in the scans (if devkitARM's `readdir` ever returns `DT_UNKNOWN`, `ListDirectory` falls back to a
stat per entry and the `object archives` step shows it).

Host tools: `mem LABEL` (heap live/peak + I/O), `SIM3DS_LOGSTEPS=1` (the log.txt lines), `SIM3DS_SDSIM=<scale>` (SD card
at console speed, see above), `SIM3DS_PRESENTDIR=dir`
(screens shown during blocking loads), `SIM3DS_NODEFER=1` (A/B: whole files), `SIM3DS_MAXOPEN=n`,
`SIM3DS_SPRDEBUG=1`, `SIM3DS_HEAPPROF=dir` + `memprof LABEL` (heap by call stack, `qa/tools/heapprof.py`). 32-bit build: `apt install g++-multilib lib32z1-dev`, then a copy of Makefile.port with
`CXX := g++ -m32` and `build-port32/`.

Not done / next:
- (done in the memory pass: sprite frames are palettised, 3 / 2 bytes per pixel.)
- `UIGraphics` / front-end art canvases are 32-bit (~8-10 MB while in the front end).
- The save-area copy on first run (66 MB read + written, ~40 s on the SD) could copy lazily (only the neighbourhood
  being opened).
- `PickObjectAt` (live UI) validates the sprites of every object, including off-screen ones.

The game shell around the translated engine: the original **front end** (EA intro, loading screen, neighbourhood
screens, family bin, Create-A-Sim; `front/`, see the section below), then **live mode** with the lot
on the top screen (400x240) and a touch panel on the bottom screen (320x240): select a Sim, tap an object to
get its **pie menu**, queue the interaction exactly like FreeSims' UI does, watch the Sim's **motives** and
**action queue**, change **game speed** (pause / 1 / 2 / 3 like TS1), scroll, rotate, zoom.

```
src/app/                       platform independent (host + 3DS)
  App.h/.cpp                   screens, input handling, live-mode HUD, layout; scripting helpers for the host
  LotSession.h/.cpp            one loaded lot: World + VMContext + VM + family, select / pie / push / cancel
  WorldPicking.h/.cpp          screen -> object picking (CPU "OBJID pass", see below)
  PieMenu.h/.cpp               FreeSims UIPieMenu: '/' categories, radial layout, touch + d-pad
  UIStrings.h/.cpp             labels from GameData/UIText.iff (STR# 138/145/148/152/160/166...)
  Canvas.h/.cpp, Font.h/.cpp   32-bit software canvas + anti-aliased bitmap font
  FontData.h, tools/genfont.py the font (Inter, SIL OFL 1.1, rasterized to 4-bit glyphs; regenerate with the script)
  Theme.h, Input.h             TS1-ish colours; the AppInput snapshot the platform fills
src/platform/3ds/main.cpp      thin libctru layer: gfx (BGR8, rotated blit), hid/ir:rst input, clock, N3DS speedup
src/platform/3ds/Makefile      devkitPro template building ALL engine sources + app + platform -> sims.3dsx
src/platform/3ds/sims.rsf, icon.png   `make cia` (178 MB extended-memory mode on the New 3DS)
src/tools/sim3ds_host.cpp      host simulator: the same App with scripted input, both screens -> one PNG
```

Nothing in `src/simantics`, `src/world` or `src/vitaboy` was modified for the app (no hooks were needed).

## How it drives the engine (what FreeSims' UI does)

| FreeSims (SimsVille/UI) | here |
|---|---|
| UILotControl.OnMouse: `World.GetObjectIDAtScreenPos` -> `GetInteractionGroupLeader` -> `GetPieMenu(vm, ActiveEntity, false)` | `LotSession::PieTargetAt` (picking: `WorldPicking.cpp`) |
| empty walkable tile: GotoObject (GUID 0x7C4) moved to the tile, its pie menu ("Go Here") | same (`SolidToAvatars` test, ghost goto object created at load) |
| UIPieMenu: names split on '/', categories, Param0 | `PieMenu` |
| VMNetInteractionCmd: `callee.PushUserInteraction(id, caller, ctx, {Param0,0,0,0})`, MAX_USER_ACTIONS | `LotSession::PushInteraction` |
| VMNetGotoCmd: new goto object at the tile, `PushUserInteraction` | same |
| UIInteractionQueue click -> VMNetInteractionCancelCmd -> `Thread.CancelAction(uid)` | queue column tap -> `LotSession::CancelAction` |
| dialogs (VM.OnDialog) / VMNetDialogResponseCmd | modal panel; answer sets the caller thread's `VMDialogResult` (Responded, ResponseCode); the sim pauses while a blocking dialog is up (TS1) |
| selected Sim | family tabs / "Select <name>" pie entry; TS1 global 3 "Selected Person ID" updated |

Game speed: normal = 30 VM ticks/s (60 `VM::Update` calls: tick + fractional animation, as FreeSims at 60 Hz),
high = 3x, ultra = 10x. The 3DS build caps SimAntics work at 14 ms per frame (`Config::VMBudgetMs`); when the
CPU cannot keep up at ultra speed the game simply runs slower instead of dropping frames.

### Picking (the renderer had no `GetObjectIDAtScreenPos`)
`PickObjectAt` is the CPU equivalent of FreeSims' OBJID render, built from the world's own rasterizer without
touching `src/world`: each object component on a visible level re-runs its DGRP sprites through the same
`_2DWorldBatch` (same depth function) into a 7x7 target around the tap, avatars are re-rasterized by
`Vitaboy::DrawAvatar` into the same window (camera shifted). The nearest covered pixel wins; a candidate must not
be behind what the last frame drew there (its depth <= the frame's depth + epsilon), so walls and Sims in front
occlude. ~0.2-1 ms per tap on the host (only runs on a tap).

## SD card layout (New 3DS)

Copy the game's install tree (PC "The Sims" / Complete Collection, extracted as described in
`distribution/3ds/readme.md`) to `sdmc:/3ds/sims/`:

```
sdmc:/3ds/sims/
  sims.3dsx                   the app (Homebrew Launcher), or install sims.cia
  GameData/
    Objects/Objects.far             72 MB  all objects (required)
    Global/Global.far                2 MB  global/semi-global behaviour (required)
    Animation/Animation.far         17 MB  Sim skeletons/animations (required for Sims)
    Textures/Textures.far           24 MB  Sim body/head textures (required for Sims)
    Skins/                          15 MB  *.cmx / *.skn / *.bmp (required for Sims; copy the folder)
    walls.iff  floors.iff  Build.iff         wall / floor catalogs (required)
    UIText.iff                               UI strings (optional: English fallbacks built in)
    LotLocations.iff  MasterLotZoning.iff    neighbourhood map / zoning (required for the neighbourhood)
    Walls/*.wll  Floors/*.flr                custom walls/floors used by some houses (copy if present)
  UserData/
    Neighborhood.iff                         families, neighbours (required)
    Houses/House00.iff ... House99.iff       the lots (required)
    Characters/User*.iff                     the Sims (required)
    LotZoning.iff                            optional
  ExpansionShared/SkinsBuy/                  optional (if the install has it)
  cache/                                     created by the app (objects/catalogue/wall/floor/skin indexes); can be
                                             prebuilt on a PC with build-port/simcache (see "3DS readiness" above)
  debug                                      optional empty file: write stdout to log.txt
```
About 150 MB in total for the **base game only**. (REL check, 26 Oct:) for the **Complete Collection with every
expansion** (what the host tests use) also copy, next to GameData/ and UserData/: `ExpansionPack/` … `ExpansionPack7/`
(their `ExpansionPack*.far` hold the expansion objects: Downtown, Vacation, Old Town, Studio Town, Magic Town, pets…;
~1.6 GB with their sound files), `ExpansionShared/`, `Deluxe/`, `Downloads/`, `UIGraphics/`, `UserData2/`, all
`Template*/` folders (incl. `TemplateNPCs/`, `TemplateFamilyUnleashed/`), `SoundData/` and `Music/`. Simplest: copy the
whole extracted install tree (~2.9 GB; `Creator_Files/` is not needed; do not copy a `Saves/` folder from a PC test run:
the app creates its own save area there on first boot, ~80 MB). Sound also needs the DSP firmware
`sdmc:/3ds/dspfirm.cdc` (src/audio/README.md).
Also read when present: `UIGraphics/` (UIGraphics.far + Res_*.h/.RT: all front-end art and the original
font; without it the front end falls back to plain shapes), the expansion `Template*/` folders (copied into the save
area on first run), `Music/` + `SoundData/` (sound engine). Nothing else from the install is read (no Behavior.iff; the
folders Lights/, Roofs/, UserObjects/, ExpansionShared/Sound are only probed if present).
The **first boot** indexes the OBJD chunks of every archive and writes `cache/objects.cache` (~15 s on the SD card,
once, or prebuilt with `simcache`); later boots read the cache.

Memory: ~45 MB of heap for a full house on the 32-bit host (see "3DS readiness" above), flat across lots. That fits the New 3DS when started as
the CIA (178 MB mode, `sims.rsf`) or from the Homebrew Launcher when it runs in a large-memory
application (title takeover); the 64 MB Old 3DS application region is too tight.

## Controls

| | |
|---|---|
| **Touch view** (bottom, lot scaled 0.65) | tap object / Sim / floor -> pie menu; drag -> scroll |
| **Pie menu** | tap item; tap centre = back; tap outside / B = close; d-pad + A also work |
| **Family tabs** (bottom header) | select Sim (tap the selected one again: centre on it) |
| **Speed buttons** (header) / X | pause, normal, high, ultra / X toggles pause |
| **Queue column** (right) | tap an action to cancel it |
| Circle pad | the pointer (hand cursor) on the top screen; pushing it against the screen edge scrolls |
| C-stick | scroll |
| L / R | rotate the view |
| D-pad up / down | zoom in / out (near / medium / far) |
| D-pad left / right | previous / next Sim |
| A or Y | centre on the selected Sim |
| ZL / ZR | floor down / up |
| SELECT | walls cutaway / down / up |
| START | menu (resume, walls, floors, back to the neighbourhood, quit) |
| L + R held, then SELECT | the cheat console (also Options panel "Cheat"; in the neighbourhood too) — see docs/CHEATS.md |
| **Pointer visibility** | the top-screen pointer (cursor sprite, its hover cursor art and the touch-view crosshair) is only shown while the pad is in use: any touch on the bottom screen hides it, the circle pad, C-stick or A show it again; in live mode it fades out after 3 s without pad movement. A always clicks at the pointer position (the last stylus position after a touch). Buy/build placement feedback stays visible with the pointer hidden: the ghost object + its tile footprint (valid/invalid colour), placement errors, wall / floor / terrain / pool drag previews and their cost tooltip follow the stylus in the touch view; the place / delete / tool cursor is drawn only while the pointer is shown. Walls are cut around the pointer while it is shown, around the stylus while it touches the lot, and otherwise keep the last cut. |
| Neighbourhood | tap a lot (top screen shows it close up + its popup), tap again (or A) = enter / move in; d-pad walks the lots; circle pad looks around; X family bin; L/R previous/next neighbourhood; B cancels evict/move-in mode; START quit |

## Host simulator

```
make -f Makefile.port build-port/sim3ds_host
build-port/sim3ds_host <installRoot> <cacheDir> <outDir> <script|->
```
Same `App` code; time advances 1/30 s per frame; both screens composed into a 400x480 PNG (top above, bottom
centred, like the console). Script commands: `frames N`, `ticks N`, `tap X Y`, `drag X0 Y0 X1 Y1 N`,
`press BTN [N]`, `circle DX DY N`, `cheat TEXT` / `chord` / `budget` (cheat console), `cstick DX DY N` (C-stick), `touch X Y` / `tiletouch TX TY` + `release` (stylus held across commands, for shots mid-drag), `pointer` (pointer shown/hidden, alpha, mouse-cut position), `house N`, `tapobj NAME` (finds a pixel that *picks* that object, through the
real picking path), `pick TEXT` (pie item), `button ID` (speed0-3, sim0.., queue0.., menu ids), `shot NAME`,
`record NAME EVERY COUNT`, `stats on|off`, `log`, `list`, `bench N`, `expect TEXT`, `testdialog`.
Demo screenshots (not included in the repository) can be made with `src/app/tools/demo_script.txt`.

## Performance (New 3DS budget)

Measured on the host with callgrind (instructions per frame, Newbie house, 2 Sims, medium zoom, 400x240,
`-O1` host build; `bench` command):

| per frame | x86 instr. | share |
|---|---|---|
| `World::Draw` total | 8.5-10.1 M | 70 % |
| - static layer composite (`DrawScrollBuffer`, 3 layers x 96k px) | 3.5-3.7 M | |
| - avatars (`DrawAvatarComponents`) | 2.8-3.7 M | of which only ~1.2 M is `DrawAvatar`: the rest is the full-screen depth conversion world<->NDC done twice per frame |
| - dynamic sprites (`_2DWorldBatch::End`) | 1.2-2.5 M | |
| VM, normal / high / ultra speed (2 / 6 / 20 `VM::Update`) | 0.2 / 1.1 / 2.9 M | |
| touch panel (thumbnail 260x156 + bars + queue) | 2.2 M (1.5 M with the 3DS's half-rate thumbnail) | |
| top HUD | 0.2-0.3 M | |
| 3DS only: rotate+convert both canvases into the BGR8 framebuffers | ~1.5 M (est.) | |

Estimate for the ARM11 MPCore at 804 MHz with L2 (`osSetSpeedupEnable(true)`), taking ~1.8-2.5 cycles per x86
instruction of this code (more ARM instructions, VFPv2, in-order core): **~13 M instr -> ~30-40 ms per frame
at normal speed (~25-30 fps)**, +5-10 ms at ultra speed (capped by the 14 ms VM budget). Native host: ~1.3 ms/frame.

What is already done for the 3DS: static layer caching (World2D scroll buffers) with a small margin
(`ScrollBuffer = 64`: 3 x 464x304 x 8 B = 3.4 MB instead of 3 x 912x752 x 8 B = 16 MB at C#'s 512), the
touch-screen thumbnail refreshed every 2nd frame, no extra VBlank wait when a frame already took > 15 ms (avoids
quantizing 35 ms frames to 50 ms), the N3DS 804 MHz + L2 mode, UIText.iff strings extracted and the file dropped.

Next steps with the biggest wins (they are in `src/world` / `src/vitaboy`, so left to their owners):
1. `World::DrawAvatarComponents` converts the whole 400x240 depth buffer to NDC and back every frame
   (~2.5 M instr/frame, ~20 % of a frame): restrict it to the avatars' screen bounds or convert per pixel inside
   the avatar rasterizer.
2. Composite the three static layers into one cached colour+depth layer while there is no static damage
   (saves ~2.4 M/frame), and store depth as 16-bit.
3. Fixed-point edge stepping in the avatar rasterizer (VFPv2 has no SIMD).

**Feedback pass FB1 (27 Oct, first user report from the New 3DS; `// FIX(sims) FB1`)**
- *Move-in in neighbourhood 2 / new neighbourhoods*: the family bin of UserData2 and of every neighbourhood made from
  TemplateUserData held only the Unleashed "Strays" (family 4000 from TemplateFamilyUnleashed/Strays_4000.FAM: eight
  stray dogs and cats, the Old Town adoption NPC family). Moving it in gave a lot with no controllable Sim. The bin
  (`NhoodModel::Bin`) now lists only households with a person (STR# 200 bodystring[0] not dog/cat/kat) and `CanMoveIn`
  refuses pet-only families. Every other path was re-checked on the host in neighbourhoods 1, 2 and 3 (created from
  TemplateUserData), including switching in one session, CAS families, evict + move in, and lot saving: they all use
  `prov->UserPath` (NhoodModel::Switch, CASBridge -> CAS UserDataDir, LotSave/VMTS1LotSaver::UserHousePath,
  TS1NeighbourhoodWriter) and work. Each move-in attempt now writes `front: move in family F -> house H in <dir>:
  result R` to log.txt. Host option `SIM3DS_FATLOCK=1|2` reports (2: refuses) writes / removes / renames of files that
  are still open, as the 3DS FS service does (none found in these flows).
- *Town maps*: lots are placed like the game (FreeSO UINeighborhoodHouseButton): the LotLocations point is the lot's
  centre; the roofed picture BMP_ 513 is drawn 1:1 on the EP towns and the 1.0 neighbourhood, at half size on the
  Unleashed neighbourhood (BMP_ 1025), centred and moved by THMB 512 (base y, x offset); the selected lot shows the
  open picture BMP_ 512 (AddYOff lower). Before: the point was used as the top-left of the half-size picture, so the
  towns' lots were half size and off their plots (over the roads). `SIM3DS_MAPDUMP=dir` writes each map (800x600 PPM).
- *Saved house pictures* (front/LotSave): BMP_ 512 (no roof) and 513 (roof) are rendered like the game's: noon light,
  all stories, the lot proper only (buildable tiles; magenta around it), the lot centre at the THMB anchor, a quarter of
  far zoom (4x4 average, drawn in 2x2 passes so the world caches stay at the old size); BMP_ 1024/1025 rewritten at half
  size when present. Before: one night-time rectangle of the far view (no keying), which covered the roads on the map.
- *Buy catalogue*: a catalogue BMP_ is a strip of button frames (37x37 normal | active; wider than 2:1 = 4 frames,
  FreeSO UICatalogItem); the whole strip was drawn, so every item showed its picture twice (grid, query panel, queue
  and inventory icons). One frame is used now (UICatalog details Icon / IconActive, LotScreen DecodeBmpChunk).
- *Neighbourhood banner*: buttons at 0.5 (27x26, was 0.42 = 22 px), banner art stretched to 320x26, Previous / Next
  with a wider touch area (UIButton PadX / PadY); live panel tabs keep their 18 px art with a 24 px touch area. CAS
  controls are already >= 23 px.
- *Loading screens*: entering a lot and every trip use the classic loading screen (LoadingScreen::InitLot: the
  picture, the sliding STR# 155 labels 4/14/15/16 driven by the lot loader's steps, the copyright line; touch screen:
  destination, label list, progress). Picture: a home lot the splash (kSimsLogo), a community lot / trip the expansion
  pack's vehicle (kDTGo, kVacationGo / kVacationGo2 home, kNeighborhoodGo, kStudiotownGo, kMagicLandGo). The load
  loop (Music/Modes/Load/loadloop.wav, the only load music in the install) now plays during the blocking load:
  `App::PumpLoadAudio` ticks the HIT VM from the loader's step / progress reports (never from inside a file
  operation). Before, the HIT VM did not tick while a lot loaded, so the loop never started. Also fixed: the trip's
  progress redraws showed the home-lot screen (`_LoadTravel` was cleared before the load).

**Saving overlay (27 Oct, user request "a percentage bar when saving"; `// ADDED(sims) save progress`)** -- a save (Options "Save", "Yes" in the "Save?" / "Quit?" dialogs, the save
before a trip) took ~7 s on the 3DS with a frozen screen. Now `front/SaveOverlay` shows "Saving..." over the dimmed lot
(top screen, with the lot's name) and "Saving house: 45%", the step (Drawing house picture / Writing house file /
Writing neighbourhood; when leaving also "Loading the neighbourhood" while the map loads) and the loading screens' bar
on the touch screen, drawn with the front end's dialog art and fonts. TS1 has no saving screen and no "saved" message
(no string for either in UIText.iff), so the overlay closes at 100 %; the port's old "The game has been saved." box after
Options "Save" is gone (a failure still shows STR#153 "Save House" / "An error was encountered saving the house.").
It is driven by `files/LoadProgress.h` reports ("save: house pictures" from front/LotSave, "save: house file" from
VMTS1LotSaver, "save: neighbourhood" from TS1NeighbourhoodWriter, "front: neighbourhood map" from NhoodModel::LoadArea /
NeighbourhoodScreen::BuildArt); App::SaveOverlayReport maps them onto the bar, pumps the sound (PumpLoadAudio) and
redraws at most every 50 ms. To keep every stretch without a report short, the house pictures (FB1) are drawn
differently (the same picture; the old first pass was scrolled from the sizing draw and was off by a pixel in places): the object sprites are loaded object by object first (the first far-zoom World::Draw read
~6.5 MB in one go, ~0.6 s), a 16x16 draw takes the zoom / level / light changes, and the 4x canvas is drawn in 4x4
passes instead of 2x2 (0.1-0.18 s each; World::FreeCenter lets their view centres lie off the lot); the extra full
"sizing" draw is gone. Host check (3DS estimate = CPU x 22 + SD model): the log line `save: N redraws in T s, longest
without a redraw X ms` -- Goths (House05) 4.2 s, longest 0.21 s; with the neighbourhood map when leaving 7.8 s, 0.21 s.
Host: `SIM3DS_PRESENTDIR=dir SIM3DS_PRESENTMAX=n SIM3DS_PRESENTFROM=frame` writes the screens shown during a save.
The type entries the saver copies from the other house files (for object types the lot's file did not have) are read
from their `objt` chunk only, found through the resource map (`VMTS1LotSaver.cpp ReadObjtChunk`; files without one
are read whole as before): the house file step went from ~3.5 s to ~0.9 s, the saves are byte-identical (simsave
roundtrip with `SIM3DS_SEED`, houses 1 / 5 / 7 with edits).

**Save safety pass (27 Oct, user's v6 report "Could not open sdmc:/3ds/sims/Saves/UserData/Neighborhood.iff", every
neighbourhood empty after a save + restart; `// FIX(sims) SAVE`)** -- what went wrong: the port had three "atomic"
writers and none survived an interruption at every point on the console: `RawIff::WriteFileAtomic` (CAS, family
import / Old Town templates, User*.iff) wrote `.tmp`, REMOVED the target, then renamed; `IffRawFile::Save` (houses,
Neighborhood.iff) renamed the old file to `.bak` before renaming `.tmp` in; the caches removed + renamed. libctru's
`rename()` onto an existing file is itself a delete + rename (`archive_rename`), the 3DS FS refuses to delete / rename /
open for writing a file another handle has open, and a crash (the HOME-menu ErrDisp of the 178 MB mode) or power cut
between "old file gone" and "new file in place" left only `Neighborhood.iff.tmp` / `.bak` on the card -- which nothing
read back. On the next start `Available()` skipped UserData (no Neighborhood.iff), switching to it fell into "This
neighborhood is empty. Create a new one?" whose copy did nothing (the folder's `sims_setup.txt` was there), `Switch`
failed with "Could not open ..." AFTER unloading the current neighbourhood, so every neighbourhood looked empty.
- One writer, `files/SafeFile` (`Write`, `BeginWrite`/`EndWrite` for fprintf writers): `<path>.new` written, fflush +
  fsync (FSFILE_Flush) + fclose + size check; every handle of ours on `<path>` closed (`IffFile::DetachSource`,
  FilePool); old `.bak` removed; `<path>` -> `.bak`; `.new` -> `<path>` (no rename ever targets an existing file);
  `.bak` removed. Failures are logged with errno (`[save] FAILED writing ...` in log.txt) and keep the old file. Used by
  IffRawFile::Save, RawIff::WriteFileAtomic (so CAS, family import, VMTS1LotSaver, TS1NeighbourhoodWriter, bulldoze),
  the provider's SaveNeighbourhood/SaveHouse, the save-area copy (UserDataSetup: a cut copy no longer leaves a
  truncated file that is kept for good), sims_options.txt and every cache (objects/catalog/wallfloor/avatars/bcfnames/
  dirs, cache_version.txt).
- Recovery at load: `SafeFile::Recover(path)` keeps `<path>` when it is a complete IFF (`IffComplete`: chunks tile the
  file, the rsmp lists nothing past the end; Neighborhood.iff must have NBRS; checked against all 213 install IFFs and
  3000 saved ones, and every truncation of them is caught), else puts back the first complete `.new` / `.tmp` / `.bak`
  (the damaged one is kept as `.damaged`), and logs `[save] RECOVERED ...`. Called by `UserDataSetup::Available()`
  (a set-up folder without Neighborhood.iff), `NhoodModel::Switch` (Neighborhood.iff every time; the folder's, Houses/
  and Characters/ leftovers once per run) and `LotSession::Load` (a house with leftovers). The old writers' `.tmp` /
  `.bak` are handled the same way, so the user's card repairs itself on the first start of the new build.
- Nothing usable left: `Switch` fails before unloading anything (the current neighbourhood stays), the map shows
  "Neighborhood N: could not be loaded ... Restore it from the original game?" and `UserDataSetup::RestoreFromInstall`
  renames only that folder to `UserDataN.damaged` and copies it again from the install (UserDataN, else
  TemplateUserData). At boot the next loadable neighbourhood opens with that dialog; with none loadable the error
  screen offers the restore (A). A neighbourhood created from TemplateUserData whose copy was cut is completed at boot.
- Host emulation (`tools/host_sdsim.cpp`): `SIM3DS_FATLOCK=2` refuses what the 3DS FS refuses (remove / rename / open
  for writing of an open file, opening a file open for writing, renames from / onto open files) and performs a rename
  onto an existing file like libctru (delete, then rename: two steps); `=3` refuses those renames (bare FS, EEXIST).
  `SIM3DS_POWERCUT=N` (+ `SIM3DS_POWERCUT_MATCH`, `SIM3DS_POWERCUT_TRACE=1`, `=0` counts) ends the process with
  `_exit` at the N-th write step (open for writing, each fwrite -- half written --, close with its buffer lost, remove,
  rename). `qa/save_fix/cutloop.sh <script>` cuts a scenario at every step and boots again (`qa/save_fix/scripts/
  verify.txt`: both neighbourhoods must show their families, every IFF must validate, no error).

Results (`qa/save_fix/loops_final.txt`, FATLOCK=2 unless noted; each cut followed by a fresh start): leave with save 13/13
cuts, Options Save 13/13, evict 7/7, bulldoze 7/7, CAS family 19/19, evict + move in + save 19/19, trip (auto-save,
generated townies) 46/46, neighbourhood 2 leave with save (incl. its first-open Old Town install) 81/81, neighbourhood 2
CAS 78/78 (trip / nb2: every Neighborhood / house step, every 4th character step), FATLOCK=3 leave 13/13 and CAS 19/19:
after every cut both neighbourhoods load with their families, every IFF validates, no leftovers stay. 0 `[fatlock]`
conflicts in all flows. Hand-made cards (old naming): only `Neighborhood.iff.tmp` -> restored; `.bak` + cut `.tmp` ->
`.bak` restored; cut Neighborhood.iff + `.bak` -> restored, the cut one kept as `.damaged`; missing with no backup ->
restore offer on the map (screenshots `qa/save_fix/shots/`), both missing -> the error screen's restore. simsave raw /
nhood / roundtrip (houses 1-11, 56-77) as before (`qa/save_fix/save_rt/`).

## Front end (`front/`) — intro, loading, neighbourhood, family bin, options flow

TS1(sims) unless noted: FreeSims has no TS1 neighbourhood/front end, so these screens are rebuilt from the
game's own art (UIGraphics.far via the Res_*.RT tables) and texts (UIText.iff STR#128-174, 225, 260...).

```
front/ResGraphics      UIGraphics.far + Res_*.h/.RT ("kNbhdBck_UL" -> Community/NScreen_unleashed.bmp), BMP 1/4/8/24/32 bpp
                       incl. RLE8/RLE4, TGA, magenta keying, area resampling; UIButton (FreeSims UIButton frames 0-3)
front/TS1Font          the original "variablesans" .ffn fonts (format reverse-engineered: glyph table + 4bpp atlas)
front/FrontStrings     the STR# tables used here, %s/%d formatting, Simoleons ('$' is the Simoleon glyph of the font)
front/IntroScreen      EA intro (<cache>/intro.stv made by tools/convert_intro.py from Music/eag_us.avi) and
                       LoadingScreen (translated from FreeSims LoadingScreen.cs: setup.bmp, STR#155 sliding labels)
front/UserDataSetup    the save area <DataRoot>/Saves/UserData, UserData2...: copies of the install's neighbourhoods plus
                       the EP template lots (installer behaviour); new neighbourhoods from TemplateUserData. The install
                       is never written. TS1NeighborhoodProvider.UserDataRoot points the engine there.
front/NhoodModel       areas (neighbourhood / Downtown / Vacation / Studio Town / Magic Town), lots (LotLocations.iff,
                       BMP_ 1024/512 house pictures, SIMI value, desc/street names), families, move in / evict /
                       delete, Neighborhood.iff save (content/TS1NeighbourhoodWriter: neighbours, NGBH, TATT, families; other chunks byte for byte)
front/NeighbourhoodScreen  the map (top = native 1:1 close-up with the lot popup, touch = whole 800x600 at 0.4 with the
                       original banner/buttons, waves animated), modes, dialogs; FamilyPicker = "SELECT A FAMILY"
front/CASBridge        runs src/app/cas CASScreen as a sub screen ("Create New Family"); credits (STR#163)
front/GameOptions      TS1 Options panel content (Neighborhood/Save/Quit, Graphics/Sound/Play pages), hosted by
                       ui/UIOptionsPanel; settings in <SaveRoot>/sims_options.txt
front/AppFlow.cpp      App members: boot, front <-> lot, "Save?" / "Quit?" dialogs (STR#153) when leaving a lot
front/LotSave          Options "Save" and "Yes" when leaving: the house file (simantics/utils/VMTS1LotSaver: objects, walls/
                       floors, Sims, time, picture) + the lot's Sims/money into the neighbourhood, which NhoodModel::Save
                       writes through content/TS1NeighbourhoodWriter (see src/simantics/STUBS_save.md)
front/FrontAudio       music per screen / UI sounds through src/audio (no-ops without it)
front/SaveOverlay      ADDED(sims): the "Saving..." overlay with the progress bar while a lot is saved (see above)
front/SimDialogView    the SimAntics dialogs of a lot (dialog primitive) on both screens with the original dialog art
Travel (AppFlow.cpp)   VMTS1Travel: lot selection dialogs 5/7/8/11/13 open the destination map in pick mode
                       (NeighbourhoodScreen::BeginPick, answered with the house number); VM::OnRequestLotSwitch ->
                       the trip screen (Res_Nbhd kDTGo / kVacationGo / kNeighborhoodGo / kStudiotownGo / kMagicLandGo)
                       -> LotSession::Load(house, ..., travel) -> VMTS1Travel::Arrive; EventSelectedSimChanged adds
                       the spawned traveller to the controllable Sims. Options: Free Will -> global 30 'Free Will'
                       (autonomy level 0 = off; App::OnPlaySettings for more). <cache>/cache_version.txt: a stale
                       objects.cache is deleted once when the OBJD index format changes.
```

Host simulator: `house N` taps lot N on the map (finishing the boot first, travelling to its area), `boot`
skips the loading animation, `front` prints the front end state + lots, `button ID` also reaches the front end's
buttons (movein, bulldoze, downtown, vacation, studiotown, magictown, return, prev, next, exit, credits; family bin
add/delete/movein/cancel, `pick:<family>`; dialog buttons dlg0..dlg2). Env: `SIM3DS_INTRO=1` plays the intro,
`SIM3DS_SAVEROOT=dir` uses another save area. In a lot: `travel N` (Generic Sims Call 17 as the game
makes it, -1 = home), `lotdialog T` (a lot selection dialog), `button simdlgN` (dialog buttons). 

Bulldoze (evict mode on an unoccupied house: front/LotSave BulldozeLot keeps terrain and street, removes the buildable
area's objects / walls / floors / pools, saves an empty lot, ArchitectureValue 0) and Import Family (FamilyPicker in import
mode over Export/*.FAM and TemplateFamilyUnleashed/*.FAM, content/TS1FamilyImport into the family bin) work.
STUB(3ds) / missing vs the original: importing lots (EP area Import buttons), Unleashed
rezoning, the Studio Town cars / Magic Town balloons and fog animations, MagiCoins prices, family portraits in the
the Unleashed .FAM template families.

## The lot UI (`LotScreen`, `ui/`, `live/`, `buy/`, `build/`) — live, buy and build mode

`LotScreen` is FreeSims' UILotControl + UIUCP for the two screens: the top screen is the lot with TS1's overlays
(action queue icons top-left, plumbob, balloons from `GameData/Sprites.iff` via `live/UIHeadlineRenderer`,
"+§n" money pop-ups, the original `.cur` cursors, buy/build tooltips and the query panel); the bottom screen is
the control panel built from the original art in `UIGraphics/UIGraphics.far` (`ui/UIGraphics`: FAR1 + BMP/TGA/CUR
decoding, magenta keys, 4-state button strips, box-filtered and cached once). Without UIGraphics.far everything
falls back to Theme.h shapes. Strings come from UIText.iff / Live.iff (`ui/GameText`).

| file | FreeSims / TS1 | what |
|---|---|---|
| `ui/UIUCP` | UIUCP.cs | gadget column: Live/Buy/Build/Options buttons (people/objects/arch/options.bmp), funds, clock, TS1 mood meter (Greenbars/Redbars) |
| `live/UILiveMode` | UILiveMode, UIInteractionQueue, UIPersonGrid, UIPersonPage, UIJobInfo | portraits (character BMP_ 2002), speed buttons, touch view, cancellable queue (QueueCancel.bmp), sub-panels Mood / Job+skills / Personality / Relationships (BMP_ 2003 faces, daily+lifetime) / House; tap the open tab again = enlarged panel |
| `live/UIMotiveDisplay` | UIMotiveDisplay.cs | 8 bars in TS1 order with the C# colours and rise/fall arrows |
| `live/UIHeadlineRenderer` | UIHeadlineRenderer.cs | thought/speech/motive/relationship balloons, algorithmic icons, crossed-out route failure |
| `buy/UICatalog` | UICatalog.cs (+FreeSO TS1ObjectProvider) | catalogue from OBJD Function/Room/BuildModeType/FunctionSubsort flags, cached in `cache/catalog.cache`; lazy CTSS names/descriptions + BMP thumbnails |
| `buy/UIBuyMode` | UIBuyMode.cs, UICatalogItem.cs, UIQueryPanel | function/room sort (BuyF*/BuyR* art), sub-sorts (STR# 200-207), pages, hand tool, hold toolbar |
| `buy/UIObjectHolder` | UIObjectHolder.cs + VMNetBuy/Move/DeleteObjectCmd | ghost + Tile Cursor footprint, rotate, placement errors (STR# 137), buy / move / sell back (depreciation, full refund within the session) |
| `build/UIBuildMode` | UIBuildMode.cs | TS1's twelve tools in the original order (UIText STR#139): Terrain, Water, Wall, Wallpaper, Stair, Fireplace, Plant, Floor, Door, Window, Roof, Hand (`toolBtn*.bmp`); sub-tools with the original art (`cpanel\Build\Terrain*Icon`, `PoolToolIcon`, Hot Date `TerrainGrassIcon` / `WaterToolIcon`, `roofshallow/medium/steep/flat`, roof patterns in `roofpatterntemplate`), Build.iff STR#128/#133 names, prices and texts with the `Popup*.bmp` pictures on the top screen, cursors SimsRaise/Lower/Level/water.cur |
| `build/UITerrainTools` | FreeSO UITerrainRaiser / UITerrainFlatten / UIGrassPaint | raise / lower (tap or drag a rectangle of vertices, hold to keep raising, X inverts), level (drag), grass; vertex markers on the top screen |
| Water tool | UIFloorPainter in pool mode | pools (65535) and Hot Date ponds (65534), drag to dig, Y = delete pool tiles only; ladders / diving boards are buy-mode objects |
| Roof tool | FreeSO UIRoofer | pitch (0.4 / 0.6 / 1.0 / flat, TS1 HOUS) + style (GameData/Roofs/*.bmp), switches to walls up so the roof shows; saved into HOUS |
| `build/UIWallPlacer`, `UIWallPainter`, `UIFloorPainter` | LotControls/*.cs | drag tools, simulated preview + cost tooltip, VMNetArchitectureCmd run locally (`LotSession::RunArchitecture`) |

TS1 rules kept: time stops in buy/build/options mode; money is the family budget (VM globals 13/14,
`LotSession::Budget`). Touch: tap the lot in the touch view (pie menu / pick up), drag scrolls; while holding an
object the stylus moves the ghost and a second tap on the same tile buys it; drawing tools drag. Buttons: circle
pad = cursor (edge scroll), A = click / hold-drag, Y = centre Sim / rotate held object, X = pause / sell, B = cancel.
Live mode Inventory tab (Hot Date+, `cpanel\Buttons\Gift.bmp`, Live.iff ModeTips 6): the Sim's NGBH tokens with the
Gift / Magic / Other sort buttons (InventoryGiftSort/MagicSort/OtherSort.bmp); token types 2 hidden (system), 4/5 +
MagiCoins = magic, type 1 / gift-token OBJDs = gifts (the gift/other split is an assumption). Objects depreciate daily
(OBJD DailyDepreciation down to DepreciationLimit on object data 41 CurrentValue; bought objects start at price minus
InitialDepreciation; selling pays CurrentValue, full price for what was bought this session). Build/buy spending is added
to the house's SIMI Architecture/ObjectsValue on save (the neighbourhood's house price). Clothing dialog (type 6): the
outfits of VMTS1Outfits::ValidOutfits(Sim, temp 0) as texture thumbnails, answer = index. Characters without portraits
(imported .FAM members, babies) get their face drawn by the CAS preview. Not ported: undo / redo, stairs checks beyond
the engine's own. Host commands: `center X Y`, `tiledrag X0 Y0 X1 Y1 N` (drag between tile corners in the touch view),
`invadd <name part> <type> <max>`, `clothesdialog <type>`.  
