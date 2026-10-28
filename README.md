# Sims3DS — The Sims (2000) on the New Nintendo 3DS

**Sims3DS** (app name *sims3ds*, "Sims 1 3DS (unofficial)") is a native New 3DS port of the engine of
**The Sims (2000) Complete Collection**: a C++ translation of the open-source reimplementations
[FreeSims](https://github.com/francot514/FreeSims) and [FreeSO](https://github.com/riperiperi/FreeSO) (both MPL-2.0),
built for the 3DS's ARM11 CPU with devkitPro/libctru. No streaming, no PC in the loop.

> **No game files are included.** Like OpenRCT2 or OpenTTD, this repository contains only engine code. Every object,
> house, Sim, sprite, sound and text comes at runtime from **your own copy** of The Sims Complete Collection, which you
> extract to your SD card (step 1 below).

> **Unofficial fan project.** Not affiliated with, endorsed by or sponsored by Electronic Arts Inc. or Maxis.
> "The Sims" is a trademark of Electronic Arts Inc. You must own the game. See [Legal](#legal).

Releases: <https://github.com/Ryan3DSMods/Sims3DS/releases>

---

## Contents

- [Features](#features)
- [Supported consoles](#supported-consoles)
- [Status and known limitations](#status-and-known-limitations)
- [Screenshots](#screenshots)
- [What you need](#what-you-need)
- [Guide](#guide): [1 game data](#1-get-the-game-data-from-your-own-copy) · [2 build](#2-build) · [3 SD card](#3-copy-everything-to-the-sd-card) · [4 install/run](#4-install-and-run) · [5 first start and controls](#5-first-start-and-controls) · [6 troubleshooting](#6-troubleshooting)
- [Optional: the 3D logo banner and banner sound](#optional-the-3d-logo-banner-and-banner-sound)
- [For developers](#for-developers)
- [Credits and licences](#credits-and-licences) · [Legal](#legal)

## Features

- **The full engine**, translated from FreeSims/FreeSO: IFF/FAR file formats, the SimAntics virtual machine with all
  primitives, routing, autonomy, the world renderer (walls, floors, roofs, terrain, water, shadows, three zooms, four
  rotations, floors/walls cut-away), Vitaboy 3D Sims with their animations and outfits.
- **All 7 expansion packs + Deluxe**: Livin' Large, House Party, Hot Date (Downtown), Vacation, Unleashed (pets, Old
  Town), Superstar (Studio Town), Makin' Magic (Magic Town).
- **The original front end**: intro (optional, see troubleshooting), loading screen, every neighbourhood and town map,
  family bin, move in / evict, options.
- **Create-A-Sim / Create-A-Family** with heads, bodies, personality, children.
- **Live, Buy and Build mode** with the game's own catalogues, pie menus, action queue, motives, jobs, skills,
  relationships, inventory, the clothing dialog, walls/floors/wallpaper/doors/windows/terrain/pools/roofs.
- **Audio**: the HIT sound engine (XA/UTK/WAV samples), mode music and radio stations (MP3) — read from your own
  `SoundData/` and `Music/` folders.
- **Travel** by phone/cab to Downtown, Vacation Island, Old Town, Studio Town and Magic Town and back.
- **Saving** in the original file formats (houses, neighbourhoods, families), written so that a power cut or crash
  never leaves you with a broken save (atomic replace + automatic recovery).
- **Cheats**: the PC cheat console (`rosebud`, `klapaucius`, `move_objects on`, `set_hour`, ...) — see
  [docs/CHEATS.md](docs/CHEATS.md).
- **Dual-screen UI**: the lot on the top screen with a pointer (circle pad), a touch view of the lot plus the panels
  (pie menus, motives, queue, catalogue) on the bottom screen.

## Supported consoles

| Console | Status |
|---|---|
| **New 3DS, New 3DS XL, New 2DS XL** | **Supported — tested on real hardware by the author** (New 3DS, CIA build). Boot, every neighbourhood, Create-A-Family, moving in, entering houses and playing, Buy/Build mode, saving and reloading, the travel screens, the HOME button and the 3D HOME Menu banner all ran on the console. Install the **CIA** (runs in the 124 MB New 3DS memory mode set in `sims.rsf`; the 178 MB mode was tried and crashed the HOME Menu) or start the **.3dsx** from the Homebrew Launcher with *title takeover* (hold R while starting a game). |
| Original 3DS, 3DS XL, 2DS | **Not supported / untested.** 268 MHz CPU without L2 cache (≈ 3–4× slower: an estimated ~8–12 fps for drawing the world, Sim behaviour slower so the higher game speeds stutter, and the audio mixer competes with the game on the shared core). 64 MB application memory (96 MB at best as a CIA in extended mode) against the game's measured ~55–60 MB peak plus code and buffers, so bigger lots and the expansion towns are likely to run out of memory. It may start, but it is not expected to be playable. There is no old-3DS mode yet (possible future work: base game only, smaller caches, lower Sim speed). |

Custom firmware is required (Luma3DS); see <https://3ds.hacks.guide> if your console is not set up yet.

## Status and known limitations

The port is playable from first start to saving, across the base game and every expansion. Before each release the
whole game is run through a test pass on the PC build of the same code (every neighbourhood and town, Create-A-Family,
Live/Buy/Build, travel, save/reload, audio, all 69 expansion template lots loaded with 0 SimAntics resets, two-game-day
simulations of the original families, a memory check with a 32-bit build, every source file compiled for the 3DS CPU),
and the console build has been played on a New 3DS (see above). Details: [docs/TESTING.md](docs/TESTING.md).

Known limitations (honest list):

- **First start takes 1–2 minutes**: the app copies the neighbourhoods into its own save area (~80 MB) and indexes
  every object archive once. Later starts take a few seconds. You can prebuild the caches on a PC (see
  [troubleshooting](#6-troubleshooting)).
- **SD card speed** dominates loading: entering a house takes several seconds, a save ~5–7 s (a progress bar is shown).
- **Not yet confirmed on the console** (verified only on the PC build): every expansion town feature in depth (for
  example a full Superstar career or Makin' Magic crafting), very long play sessions on one lot (the heap grows slowly
  as objects pile up — watch `app free` in `log.txt`), and the Old 3DS.
- **Gameplay is faithful, sometimes harshly so**: kitchen fires at cooking skill 0, unhappy Sims refusing the car pool
  or school bus, the Help System question and the expansion intro letters coming back on each lot load — these
  follow the game's own scripts.
- **Small open issues**: a Sim saved while seated stands next to the seat after reloading (running actions are not
  saved); a few community-only objects show in the residential buy catalogue; 2 of 887 residential catalogue items
  cannot be picked up (they delete themselves off community lots); some queue icons fall back to a plain circle; wall
  sconces can float above the floor with walls cut away; text is shortened with "..." in a few tight panels.
- **Generated townies** (Downtown, Vacation, Magic Town, Studio Town) are created by the port: their number, names and
  looks are our own choice because the original rules live in the game's executable, which is not used.
- **The intro video** (with its sound) needs a one-time conversion on a PC (the original video codec cannot be
  decoded on the 3DS); without it the game starts at the loading screen.
- **No online features or multiplayer**; no mouse/keyboard; the Sims Creator tools are not part of the port.
- **3D banner**: the HOME Menu caches banners (reinstall or reboot after changing it); the 3D banner path needs
  Python 3.12+ and network on the first build — otherwise the flat banner is used automatically.
- Saves are written in the original formats; keep backups of `sdmc:/3ds/sims/Saves/` anyway.

## Screenshots

No screenshots are included in this repository, because they would show the game's copyrighted art. You can make your
own on the console (e.g. with Luma3DS's screenshot feature: L + Down + SELECT, then *Take screenshot*) and add them to
your fork or release notes if you like.

## What you need

- A **New 3DS / New 3DS XL / New 2DS XL** with **custom firmware** (Luma3DS) and the Homebrew Launcher or FBI.
- **~3.2 GB free** on the SD card (game data ~3.0 GB + the save area ~80 MB + caches).
- **Your own The Sims Complete Collection (USA, 4 discs)** as disc images (e.g. `.bin/.cue`, `.iso`, or `.7z` dumps of
  your discs) — **or** a full PC installation of The Sims with all 7 expansion packs (see [Option B](#option-b-copy-from-a-pc-installation)).
  The base game alone works too (without the expansion content).
- The **DSP firmware** dump `sdmc:/3ds/dspfirm.cdc` for sound (made by the standard CFW setup; otherwise run
  [DSP1](https://github.com/zoogie/DSP1) once).
- A computer (macOS, Linux or Windows) with:
  - **devkitPro** with the `3ds-dev` group (devkitARM, libctru, 3dstools) — to build;
  - **bannertool** and **makerom** in your `PATH` — only for the `.cia`;
  - **Python 3** with `py7zr` and `pycdlib` (`python3 -m pip install py7zr pycdlib`) and **unshield** — for the disc
    extraction.

Prebuilt `.3dsx` / `.cia` files may be attached to the [releases](https://github.com/Ryan3DSMods/Sims3DS/releases);
then you can skip step 2.

## Guide

### 1. Get the game data from your own copy

#### Option A: from your Complete Collection discs (recommended)

The four discs hold one InstallShield cabinet set split across them (Disc 1: `Setup/data1.hdr`, `data1.cab`,
`data2.cab`; Disc 2: `data3.cab`; Disc 3: `data4.cab`; Disc 4: `data5.cab`). The script
[`tools/extract/extract_complete_collection.py`](tools/extract/extract_complete_collection.py) does everything:

1. `.7z` → `.bin` (py7zr), if your images are packed;
2. raw `.bin` (MODE1/2352, also 2448-byte sectors) → `.iso` by keeping the 2048 data bytes of every sector;
3. `.iso` → the cabinet files, read with pycdlib (Joliet names). Copy-protected discs can have a primary volume
   descriptor or directory records pycdlib refuses; the script then falls back to its own tolerant ISO 9660/Joliet
   reader that only follows directory records;
4. `unshield -d <work>/groups x data1.hdr` extracts all 55 file groups;
5. the groups are laid out like a PC install: `GameData_ALL_Recursive` + `GameData_ALL_Ranger` → `GameData/`,
   `ExpansionPack1` → `ExpansionPack/`, `ExpansionPack2..7` → same names, `ExpansionPackGOLD` → `Deluxe/`,
   `Music_ALL_Recursive` → `Music/`, `SoundData_SimsSounds` → `SoundData/`; `ExpansionShared`, `Downloads`,
   `UIGraphics`, `UserData`, `UserData2` and all `Template*` groups keep their names. Executables, the installer,
   copy protection and localisation templates are skipped.

Install the helpers:

```sh
python3 -m pip install py7zr pycdlib
# unshield:  macOS: brew install unshield   Debian/Ubuntu: sudo apt install unshield   Arch: sudo pacman -S unshield
# Windows: run everything inside WSL (Ubuntu), or build unshield from https://github.com/twogood/unshield (cmake + zlib)
```

Put your four disc images (any of `.7z`, `.bin`, `.iso`, `.mdf`, or copied disc folders, named with "Disc 1" ...
"Disc 4") into one folder and run:

```sh
python3 tools/extract/extract_complete_collection.py --discs ~/SimsDiscs --out ~/sims_sd
```

It needs ~3 GB for the output plus ~1.6 GB of temporary space (removed at the end) and takes 10–30 minutes. At the end
it checks the key files and prints the size (~3.06 GB). The contents of `~/sims_sd` are what goes to `sdmc:/3ds/sims/`.

#### Option B: copy from a PC installation

If The Sims and its expansions are installed on a PC (e.g. `C:\Program Files (x86)\Maxis\The Sims`), copy these
folders from the install folder: `GameData`, `UserData`, `UserData2`, `ExpansionPack`, `ExpansionPack2` …
`ExpansionPack7`, `ExpansionShared`, `Deluxe`, `Downloads`, `UIGraphics`, `Music`, `SoundData`, and every
`Template*` folder (`TemplateCommunity`, `TemplateDowntown`, `TemplateFamilyUnleashed`, `TemplateMagicTown`,
`TemplateNPCs`, `TemplateStudiotown`, `TemplateUserData`, `TemplateVacation`). For the base game only, `GameData` and
`UserData` are enough (~150 MB). Do **not** copy a `Saves` folder. Use an unplayed install if you can — the
neighbourhoods start as they are in `UserData*`.

### 2. Build

Install devkitPro (<https://devkitpro.org/wiki/Getting_Started>) and the 3DS packages, then:

```sh
sudo dkp-pacman -S 3ds-dev            # macOS/Linux; on Windows use the devkitPro installer + its MSYS2 shell
source /etc/profile.d/devkit-env.sh    # sets DEVKITPRO / DEVKITARM (new shells do this automatically)
cd src/platform/3ds
make -j4                              # -> sims.3dsx (+ sims.smdh)
make cia                              # -> sims.cia (needs bannertool + makerom in PATH)
```

The Makefile already passes libctru's include path itself (`-I$(CTRULIB)/include`), so no extra flags are needed.
`make cia` builds the 3D HOME Menu banner by default and falls back to the flat banner automatically
(`make cia BANNER3D=0` forces the flat one; see [the banner section](#optional-the-3d-logo-banner-and-banner-sound)).

### 3. Copy everything to the SD card

```
sdmc:/3ds/sims/
  sims.3dsx                 the app (for the Homebrew Launcher; not needed if you install the CIA)
  GameData/  UserData/  UserData2/  ExpansionPack/ … ExpansionPack7/  ExpansionShared/  Deluxe/  Downloads/
  UIGraphics/  Music/  SoundData/  Template*/            <- the contents of your extracted folder (step 1)
  debug                     optional EMPTY file: the app then writes a log to log.txt
  cache/                    created by the app (indexes; can be prebuilt on a PC, see troubleshooting)
  Saves/                    created by the app (your game: a copy of the neighbourhoods on first start)
sdmc:/3ds/dspfirm.cdc       DSP firmware, needed for sound
```

The folder must be exactly `sdmc:/3ds/sims/`. Copying ~3 GB of small files to an SD card takes a while; a card reader
on a computer is much faster than FTP.

### 4. Install and run

- **CIA (recommended):** copy `sims.cia` to the SD card, then on the 3DS: FBI → SD → `sims.cia` → *Install CIA*.
  Start **sims3ds** from the HOME Menu.
- **.3dsx:** start a game you own while holding **R** (title takeover) to open the Homebrew Launcher with the full
  application memory, then pick sims3ds. Starting it from the Homebrew Launcher *app* or the album applet gives too
  little memory.

### 5. First start and controls

On the very first start the loading screen shows "copying" and "indexing objects (first run only)": that takes
**1–2 minutes** (more on slow cards). Later starts reach the neighbourhood in a few seconds. Leave the console on
during the first start.

| Control | Action |
|---|---|
| **Stylus on the bottom screen** | touch view of the lot: tap an object / Sim / floor → pie menu; drag → scroll; tap panels and buttons |
| **Circle pad** | the pointer (hand cursor) on the top screen; push against the edge to scroll |
| **A** | click at the pointer (or where the stylus last was) |
| **B** | close a pie menu / cancel |
| **C-stick** (New 3DS) | scroll |
| **L / R** | rotate the view (in the neighbourhood: previous / next neighbourhood) |
| **D-pad up / down** | zoom in / out (near / medium / far) |
| **D-pad left / right** | previous / next Sim |
| **Y** | centre on the selected Sim (Buy mode: rotate the object in hand) |
| **X** | pause / resume (Buy mode: sell; neighbourhood: family bin) |
| **ZL / ZR** | floor down / up |
| **SELECT** | walls cut-away / down / up |
| **START** | menu (resume, walls, floors, neighbourhood, quit) |
| **Hold L + R, press SELECT** | the cheat console ([docs/CHEATS.md](docs/CHEATS.md)) |

Neighbourhood: tap a lot (the top screen shows it close up), tap again or press A to enter / move in; the d-pad walks
the lots. The full control reference is in [src/app/README.md](src/app/README.md#controls).

### 6. Troubleshooting

- **Log file:** create an empty file `sdmc:/3ds/sims/debug`. The app then writes `sdmc:/3ds/sims/log.txt`: one line per
  loading step with the time and free memory. The last `step` line shows where a hang or crash happened; `FATAL:`
  lines carry the error shown on screen; `app free` below ~10 MB means memory trouble; `WARNING: could not write` means
  the SD card is full.
- **"Could not open file …" / "No objects found" / "No neighborhood found":** the data is not where the app looks.
  Check that `sdmc:/3ds/sims/GameData/Objects/Objects.far` and `sdmc:/3ds/sims/UserData/Neighborhood.iff` exist.
- **Crashes:** Luma3DS writes crash dumps to `sdmc:/luma/dumps/arm11/` (`crash_dump_*.dmp`). Please attach the dump
  and `log.txt` to a [GitHub issue](https://github.com/Ryan3DSMods/Sims3DS/issues).
- **No sound:** `sdmc:/3ds/dspfirm.cdc` is missing (see [What you need](#what-you-need)), or `SoundData/` / `Music/`
  were not copied.
- **Save recovery:** saves are written as `<file>.new`, then the old file is renamed to `.bak`, then `.new` replaces it.
  If a crash or power cut interrupts this, the next start repairs it automatically (log line `[save] RECOVERED`). If a
  neighbourhood cannot be loaded at all, the app offers to restore that neighbourhood from your original game data
  (the damaged folder is kept as `UserDataN.damaged`).
- **Start over:** delete `sdmc:/3ds/sims/Saves/` (your game!) and `sdmc:/3ds/sims/cache/`.
- **Slow first start:** prebuild the caches on a PC with the same data:
  `make -f Makefile.port build-port/simcache && build-port/simcache ~/sims_sd ~/sims_cache`, then copy the files of
  `~/sims_cache` to `sdmc:/3ds/sims/cache/`. Caches that do not match the card are rebuilt silently.
- **Intro video:** the EA intro (`Music/eag_us.avi`) uses a video codec the 3DS cannot decode, so it is converted once
  on a PC (needs ffmpeg): `python3 src/app/tools/convert_intro.py ~/sims_sd/Music/eag_us.avi intro.stv` writes
  `intro.stv` (picture) and `intro.wav` (sound); copy both to `sdmc:/3ds/sims/cache/` (or give the SD path directly:
  `... <sd>/3ds/sims/cache/intro.stv`). Without them the game skips straight to the loading screen.
- **HOME Menu banner did not change** after reinstalling: the HOME Menu caches banners — uninstall the CIA, reboot,
  install again.
- **"Out of memory" or a crash right after starting the .3dsx:** use title takeover (hold R) or the CIA (step 4).

## Optional: the 3D logo banner and banner sound

`make cia` builds a 3D HOME Menu banner: a swaying 3D "The Sims" logo (hardware-tested on a New 3DS). It is made from
the model **"Sims game logo 3D" by its creator on Printables**
(<https://www.printables.com/model/1630221-sims-game-logo-3d>), licensed **CC BY-SA 4.0**. The two STL files are in
[`assets/logo/`](assets/logo/); the scene generated from them (`src/platform/3ds/banner3d/banner_scene*.gltf/.bin`,
`banner_palette.png`) and the flat `banner.png` / `icon.png` rendered from it are adaptations under CC BY-SA 4.0 too —
see [assets/logo/ATTRIBUTION.md](assets/logo/ATTRIBUTION.md). The logo design itself is a trademark of Electronic
Arts; the model licence covers only the creator's 3D work. Build it for personal use on your own console.

How it is made (all scripts included): `tools/banner3d/make_scene.py` turns the STL into a glTF scene (white letter
faces, thin red outlines built in geometry, ±25° sway, 16×16 palette texture — a baked 256×128 texture crashed the
HOME Menu), `src/platform/3ds/banner3d/make_banner3d.sh` converts it with
[pycgfx](https://github.com/skyfloogle/pycgfx) (skyfloogle) to CGFX and packs it with bannertool. This follows the
GBAtemp guide *"How to Make a 3D Banner Like an Official Nintendo 3DS Game"* by Epic0522. Details and the settings that
work on hardware: [tools/banner3d/README.md](tools/banner3d/README.md).

- `make cia BANNER3D=0` uses the flat banner instead.
- Logo-free alternative: `make cia BANNER3D=0 ICON=$PWD/neutral/icon.png BANNER_PNG=$PWD/neutral/banner.png`
  (original artwork in `src/platform/3ds/neutral/`, made by `tools/banner/make_flat_banner.py`).

**Banner sound:** no game audio is in this repository, so the banner is silent by default. To hear a game sound in the
HOME Menu, export one from **your own** game data:

```sh
python3 tools/banner3d/export_game_sound.py ~/sims_sd --event ui_object_place    # the purchase "ka-ching"
python3 tools/banner3d/export_game_sound.py ~/sims_sd --music 3                  # neighbourhood music
```

It decodes the sound with the port's sound engine and writes `src/platform/3ds/banner3d/banner_sound.wav`
(44.1 kHz 16-bit stereo, ≤ 3 s, faded out), which `make cia` then uses. The file is git-ignored; do not publish it.
All in-game audio is likewise read at runtime from your `SoundData/` and `Music/` folders.

## For developers

- **Host simulator:** the same app code runs on a PC with scripted input and writes both screens to PNGs:
  ```sh
  make -f Makefile.port -j4 build-port/sim3ds_host
  build-port/sim3ds_host <game folder> <cache dir> <out dir> src/app/tools/demo_script.txt
  ```
  Other host tools (`simlife`, `simep`, `simsave`, `simaudio`, `simsound`, `cas_host`, `simcache`, `simrun`,
  `simshot`, `simrender`, `simbody`) are targets of `Makefile.port`; see the headers of `src/tools/*.cpp`. Needs g++
  (C++17) and zlib. Set `SIM3DS_SAVEROOT=<dir>` so test runs do not write into your game folder.
- **ARM compile check without devkitPro:** `tools/arm_check/arm_check.sh <libctru>/libctru/include` (stock
  `arm-none-eabi-g++` + libctru headers).
- **Docs:** [PORTING.md](PORTING.md) (translation conventions C# → C++), [src/app/README.md](src/app/README.md) (app,
  SD layout, memory/performance, save safety), `src/*/STUBS*.md` (what is stubbed or changed versus FreeSims, per
  area), [src/audio/README.md](src/audio/README.md), [src/simantics/README.md](src/simantics/README.md),
  [docs/TESTING.md](docs/TESTING.md), [docs/DEVELOPMENT_LOG.md](docs/DEVELOPMENT_LOG.md).
- Code comments: `TS1(sims)` marks Sims-1-specific additions, `FIX(sims)` fixes, `STUB(3ds)` stubs.
- Contributions are welcome via pull requests. Never commit game files, screenshots of game art, or anything
  extracted from the game.

## Credits and licences

- Engine code: **MPL-2.0** ([LICENSE](LICENSE)) — a translation of FreeSims by francot514 and contributors and FreeSO
  by riperiperi and the FreeSO contributors, plus this project's own code under the same licence.
- 3D logo model and its derivatives: **CC BY-SA 4.0** ([assets/logo/](assets/logo/)).
- UI font bitmaps: generated from **Inter** (SIL OFL 1.1, [LICENSES/OFL-1.1-Inter.txt](LICENSES/OFL-1.1-Inter.txt)).
- Full list, including minimp3 (CC0) and xxHash (BSD-2-Clause): [CREDITS.md](CREDITS.md).

## Legal

Sims3DS is an unofficial, non-commercial fan project. It is not affiliated with, endorsed, sponsored or approved by
Electronic Arts Inc. or Maxis. "The Sims" and its logo are trademarks of Electronic Arts Inc.; all other trademarks
belong to their owners. This repository contains **no** game data: no executables, archives, sprites, sounds, music,
text tables or other assets of the game. To play you must own a legitimate copy of The Sims Complete Collection (or the
base game and expansions) and extract the data from it yourself. Do not ask for, or share, game files in this
repository's issues.
