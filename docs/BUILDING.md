# Building Sims3DS yourself

Most players do not need this page: the [release zip](https://github.com/Ryan3DSMods/Sims3DS/releases) already has
`sims.cia` and `sims.3dsx`, and the extractor in it prepares the game data (see the [README](../README.md)). This page
is for building from source, preparing the SD card by hand, and development.

## 1. What you need

- **devkitPro** with the `3ds-dev` group (devkitARM, libctru, 3dstools) — <https://devkitpro.org/wiki/Getting_Started>.
- **bannertool** and **makerom** in your `PATH` — only for the `.cia`.
- For the 3D HOME Menu banner: git, Python 3.12+ and network on the first build (see [BANNER.md](BANNER.md)).
  Without them the flat banner is used automatically.
- To prepare the game data: Python 3.8+; for disc images also `py7zr`, `pycdlib` and **unshield**.

## 2. Build

```sh
sudo dkp-pacman -S 3ds-dev            # macOS/Linux; on Windows use the devkitPro installer + its MSYS2 shell
source /etc/profile.d/devkit-env.sh    # sets DEVKITPRO / DEVKITARM (new shells do this automatically)
cd src/platform/3ds
make -j4                              # -> sims.3dsx (+ sims.smdh)
make cia                              # -> sims.cia (needs bannertool + makerom in PATH)
```

The Makefile passes libctru's include path itself (`-I$(CTRULIB)/include`), so no extra flags are needed.

Options of `make cia`:

| Variable | Default | Meaning |
|---|---|---|
| `BANNER3D` | `1` | 3D HOME Menu banner; `0` = the flat `banner.png` ([BANNER.md](BANNER.md)) |
| `CIA_MEM` | `124` | New 3DS memory mode of the CIA: `124` (SystemMode 64MB + SystemModeExt 124MB) or `96` (SystemMode 96MB + SystemModeExt Legacy). `sims.rsf` is patched into `build/sims_cia.rsf`. 178 MB was tried earlier and crashed the HOME Menu. |
| `BANNER_AUDIO` | — | a `.wav` for the banner sound (default: your own `banner3d/banner_sound.wav`, else silence) |
| `ICON`, `BANNER_PNG` | the logo art | e.g. the neutral art in `neutral/` |

`make release VERSION=1.0` builds both files and packs the player zip — see [RELEASING.md](RELEASING.md).

## 3. Get the game data from your own copy

The one-step way is the extractor: `tools/extract/extract.command` (macOS), `extract.bat` (Windows) or `extract.sh`
(Linux). It takes either images of **your own** discs or **your own** installed game, and writes a ready
`3ds/sims/` folder (plus the converted intro when ffmpeg is installed). It runs
[`tools/extract/extract_complete_collection.py`](../tools/extract/extract_complete_collection.py), which you can also
call directly:

```sh
python3 tools/extract/extract_complete_collection.py <your discs folder or installed game folder> --out ~/sims_sd
python3 tools/extract/extract_complete_collection.py <folder> --dry-run      # only show what it would do
```

### Option A: from images of your own Complete Collection discs

Make an image of each of your four discs (any of `.iso`, `.bin`, `.mdf`, `.7z` of those, or a copied disc folder,
with "Disc 1" … "Disc 4" in the names) and put them in one folder. The discs hold one InstallShield cabinet set split
across them (Disc 1: `Setup/data1.hdr`, `data1.cab`, `data2.cab`; Disc 2: `data3.cab`; Disc 3: `data4.cab`; Disc 4:
`data5.cab`). The script:

1. `.7z` → `.bin` (py7zr), if your images are packed;
2. raw `.bin` (MODE1/2352, also 2448-byte sectors) → `.iso` by keeping the 2048 data bytes of every sector;
3. `.iso` → the cabinet files, read with pycdlib (Joliet names). Copy-protected discs can have a primary volume
   descriptor or directory records pycdlib refuses; the script then falls back to its own tolerant ISO 9660/Joliet
   reader;
4. `unshield -d <work>/groups x data1.hdr` extracts all 55 file groups;
5. lays the groups out like a PC install: `GameData_ALL_Recursive` + `GameData_ALL_Ranger` → `GameData/`,
   `ExpansionPack1` → `ExpansionPack/`, `ExpansionPack2..7` → same names, `ExpansionPackGOLD` → `Deluxe/`,
   `Music_ALL_Recursive` → `Music/`, `SoundData_SimsSounds` → `SoundData/`; `ExpansionShared`, `Downloads`,
   `UIGraphics`, `UserData`, `UserData2` and all `Template*` groups keep their names. Executables, the installer,
   copy protection and localisation templates are skipped.

Helpers:

```sh
python3 -m pip install --user py7zr pycdlib
# unshield:  macOS: brew install unshield   Debian/Ubuntu: sudo apt install unshield
#            Fedora: sudo dnf install unshield   Arch: sudo pacman -S unshield
# Windows: install the game from your discs and use Option B, or use WSL (Ubuntu) + extract.sh
```

It needs ~3 GB for the output plus ~1.6 GB of temporary space (removed at the end) and takes 10–30 minutes. At the end
it checks the key files and prints the size (~3.06 GB).

### Option B: from your own installed game

Point the extractor at the install folder (e.g. `C:\Program Files (x86)\Maxis\The Sims`; picking the `Maxis` folder
works too). It copies `GameData`, `UserData*`, `ExpansionPack`, `ExpansionPack2` … `ExpansionPack7`,
`ExpansionShared`, `Deluxe`, `Downloads`, `UIGraphics`, `Music`, `SoundData` and every `Template*` folder — never
`Saves` or executables. No extra tools are needed. For the base game only, `GameData` and `UserData` are enough
(~150 MB). Use an unplayed install if you can — the neighbourhoods start as they are in `UserData*`.

### The intro video

The EA intro (`Music/eag_us.avi`) uses a video codec the 3DS cannot decode, so it is converted once on a PC. The
extractor does this automatically when **ffmpeg** is installed. By hand:
`python3 src/app/tools/convert_intro.py ~/sims_sd/Music/eag_us.avi ~/sims_sd/cache/intro.stv` writes `intro.stv`
(picture) and `intro.wav` (sound) into `cache/`. Without them the game starts at the loading screen.

## 4. The SD card

```
sdmc:/3ds/sims/
  sims.3dsx                 the app (for the Homebrew Launcher; not needed if you install the CIA)
  GameData/  UserData/  UserData2/  ExpansionPack/ … ExpansionPack7/  ExpansionShared/  Deluxe/  Downloads/
  UIGraphics/  Music/  SoundData/  Template*/            <- your extracted game data
  debug                     optional EMPTY file: the app then writes a log to log.txt
  cache/                    created by the app (indexes; intro.stv/.wav from the extractor)
  Saves/                    created by the app (your game: a copy of the neighbourhoods on first start)
sdmc:/3ds/dspfirm.cdc       DSP firmware, needed for sound
```

The folder must be exactly `sdmc:/3ds/sims/`. You need ~3.2 GB free. Copying ~3 GB of small files takes a while; a
card reader on a computer is much faster than FTP. `dspfirm.cdc` is made by the standard custom-firmware setup;
otherwise run [DSP1](https://github.com/zoogie/DSP1) once.

## 5. Install and run

- **CIA (recommended):** FBI → SD → `sims.cia` → *Install CIA*. Start **sims3ds** from the HOME Menu.
- **.3dsx:** start a game you own while holding **R** (title takeover) to open the Homebrew Launcher with the full
  application memory, then pick sims3ds. Starting it from the Homebrew Launcher *app* or the album applet gives too
  little memory.

### Faster first start: prebuild the caches on a PC

```sh
make -f Makefile.port build-port/simcache && build-port/simcache ~/sims_sd ~/sims_cache
```

Copy the files of `~/sims_cache` to `sdmc:/3ds/sims/cache/`. Caches that do not match the card are rebuilt silently.

## 6. For developers

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
- **Docs:** [PORTING.md](../PORTING.md) (translation conventions C# → C++), [src/app/README.md](../src/app/README.md)
  (app, SD layout, controls, memory/performance, save safety), `src/*/STUBS*.md` (what is stubbed or changed versus
  FreeSims, per area), [src/audio/README.md](../src/audio/README.md),
  [src/simantics/README.md](../src/simantics/README.md), [TESTING.md](TESTING.md),
  [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md).
- Code comments: `TS1(sims)` marks Sims-1-specific additions, `FIX(sims)` fixes, `STUB(3ds)` stubs.
- Contributions are welcome via pull requests. Never commit game files, screenshots of game art, or anything
  extracted from the game.
