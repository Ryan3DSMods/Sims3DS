# Sims3DS — The Sims (2000) on the New Nintendo 3DS

Play **The Sims Complete Collection** — the base game and all 7 expansion packs — on your New 3DS. It runs right on
the console: no streaming, no PC needed while you play.

> **No game files are included.** You need **your own copy** of The Sims Complete Collection (your discs or your
> installed PC game). A small tool in the download copies the game files from your copy for you.
>
> **Unofficial fan project.** Not affiliated with, endorsed by or sponsored by Electronic Arts Inc. or Maxis.
> "The Sims" is a trademark of Electronic Arts Inc. See [Legal](#legal).

## What you need

- A **New 3DS, New 3DS XL or New 2DS XL** with **custom firmware** (Luma3DS + FBI). Not set up yet? Follow
  <https://3ds.hacks.guide>.
- **Your own The Sims Complete Collection**: your discs (as disc images you made from them), or the game installed on
  your PC.
- A **computer** (Windows, macOS or Linux) with [Python 3](https://www.python.org/downloads/), and an SD card with
  **~3.2 GB free**.

## Install in 4 steps

1. **Download** the latest `Sims3DS-<version>.zip` from the
   [Releases page](https://github.com/Ryan3DSMods/Sims3DS/releases/latest) and unzip it.
2. **Get your game files:** open the `extractor` folder and double-click **`extract.command`** (Mac) or
   **`extract.bat`** (Windows) — on Linux run `extract.sh`. When it asks, drag in the folder with **your own** game:
   either your installed The Sims folder (e.g. `C:\Program Files (x86)\Maxis\The Sims`) or a folder with images of
   your own four discs. It fills the `3ds/sims` folder of the unzipped download with the game data (10–30 minutes).
3. **Copy to the SD card:** copy the `3ds` and `cia` folders from the unzipped download to the root of your SD card.
4. **Install and play:** on the 3DS open **FBI** → *SD* → `cia` → `sims.cia` → *Install CIA*. Start **sims3ds** from
   the HOME Menu. The very first start takes 1–2 minutes (it prepares your save files); after that it starts in seconds.

Disc images only: the extractor also needs **unshield** — on a Mac it offers to install it for you (with
[Homebrew](https://brew.sh)); on Windows it is easiest to install the game from your discs and pick the installed
folder. If ffmpeg is installed, the EA intro video is converted too; otherwise the game simply skips it.

## Controls

| Button | What it does |
|---|---|
| **Stylus** (bottom screen) | tap a Sim or object for its menu, drag to scroll, use the panels |
| **Circle pad** + **A** | move the pointer on the top screen and click |
| **B** | close a menu / cancel |
| **L / R** | rotate the view |
| **D-pad up / down** | zoom in / out |
| **D-pad left / right** | previous / next Sim |
| **X** | pause / resume |
| **ZL / ZR** | floor down / up |
| **SELECT** | walls cut-away / down / up |
| **START** | menu (neighbourhood, quit, ...) |
| **Hold L + R, press SELECT** | cheat console ([cheats](docs/CHEATS.md)) |

All controls: [src/app/README.md](src/app/README.md#controls).

## Supported consoles

| Console | |
|---|---|
| **New 3DS, New 3DS XL, New 2DS XL** | **Yes** — tested on real hardware |
| Original 3DS, 3DS XL, 2DS | **No** — too slow and too little memory for the game ([why](docs/FEATURES.md#original-3ds--2ds)) |

## Known issues

- **Don't press HOME while playing:** it can crash the HOME Menu. Close the game with **START → Quit** instead. Being
  investigated.
- The first start takes 1–2 minutes; entering a house takes a few seconds and saving ~5–7 s (SD card speed).
- Some expansion-town features and very long play sessions have so far only been tested on the PC build.
- Small glitches: a Sim saved while sitting stands up after loading; a few icons or texts are simplified.

Full list: [docs/FEATURES.md](docs/FEATURES.md#known-limitations-full-list).

## FAQ / troubleshooting

- **"Could not open file" / "No neighborhood found"** — the game files are in the wrong place. They must be in
  `sdmc:/3ds/sims/` (e.g. `sdmc:/3ds/sims/GameData/`), next to `sims.3dsx`.
- **No sound** — `sdmc:/3ds/dspfirm.cdc` is missing. It is made during the normal custom-firmware setup; otherwise run
  [DSP1](https://github.com/zoogie/DSP1) once.
- **Mac says the extractor "cannot be opened"** — right-click `extract.command` → *Open* → *Open* (first time only).
- **Can I use the .3dsx instead of the CIA?** Yes: start it from the Homebrew Launcher via *title takeover* (hold R
  while starting a game). The CIA is easier.
- **It crashed** — copy `extras/debug` from the download to `sdmc:/3ds/sims/`, play until it happens again, then open
  a [GitHub issue](https://github.com/Ryan3DSMods/Sims3DS/issues) with `sdmc:/3ds/sims/log.txt` and the crash dump
  from `sdmc:/luma/dumps/arm11/`. Please never attach game files.
- **Where are my saves?** `sdmc:/3ds/sims/Saves/` — back it up now and then.

More help: [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## More documentation

- [Features and full list of known limitations](docs/FEATURES.md)
- [Building from source, preparing the SD card by hand, development](docs/BUILDING.md)
- [The 3D HOME Menu banner and banner sound](docs/BANNER.md)
- [Cheats](docs/CHEATS.md)
- [How it is tested](docs/TESTING.md)
- [Making a release](docs/RELEASING.md) (maintainers)
- [Development log](docs/DEVELOPMENT_LOG.md) · [Porting notes](PORTING.md)

## Credits and licence

- Engine code: **MPL-2.0** ([LICENSE](LICENSE)) — a C++ translation of [FreeSims](https://github.com/francot514/FreeSims)
  (francot514 and contributors) and [FreeSO](https://github.com/riperiperi/FreeSO) (riperiperi and contributors).
- 3D logo banner: made from **"Sims game logo 3D" by [3DTwinkie](https://www.printables.com/@3DTwinkie)** on
  Printables, licensed **CC BY-SA 4.0** ([assets/logo/](assets/logo/ATTRIBUTION.md)).
- Everything else (fonts, libraries, tools): [CREDITS.md](CREDITS.md).

## Legal

Sims3DS is an unofficial, non-commercial fan project. It is not affiliated with, endorsed, sponsored or approved by
Electronic Arts Inc. or Maxis. "The Sims" and its logo are trademarks of Electronic Arts Inc.; all other trademarks
belong to their owners. This repository and its releases contain **no** game data. To play you must own The Sims
Complete Collection (or the base game and expansions) and use the files from your own copy. Do not ask for, or
share, game files in this repository's issues.
