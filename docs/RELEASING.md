# Making a release (for the maintainer)

A release is one zip that players download from GitHub. It holds everything for the SD card **except the game data**,
plus the extractor that players run on their own copy of the game.

## What is in `Sims3DS-<version>.zip`

```
README.txt                    short instructions
3ds/sims/sims.3dsx            Homebrew Launcher version
3ds/sims/README-on-SD.txt     where the game folders go
cia/sims.cia                  install with FBI (3D banner, or the flat one if the 3D step failed; silent banner sound)
extras/debug                  optional: copy to sdmc:/3ds/sims/ to get a log.txt
extractor/extract.command     macOS (double-click)
extractor/extract.bat         Windows (double-click or drag the game folder onto it)
extractor/extract.sh          Linux
extractor/extract_complete_collection.py, convert_intro.py
licenses/                     LICENSE, CREDITS.md, logo attribution (CC BY-SA 4.0), font licence
```

The extractor writes the game data into the `3ds/sims/` folder of the unpacked zip, so players copy `3ds` and `cia` to
the SD card in one go.

Never put game data, game audio (`banner_sound.wav`), screenshots of the game or anything extracted from it into a
release. `make release` always builds the CIA with the silent banner sound, even if you exported a banner sound for
your own console.

## The easy way: GitHub Actions (no Mac needed)

GitHub builds the zip for you with [.github/workflows/release.yml](../.github/workflows/release.yml):
on GitHub open **Actions → Build release → Run workflow**, type the version (e.g. `1.1`), tick **publish** and press
**Run**. About 5 minutes later release `v1.1` is on the Releases page with `Sims3DS-1.1.zip` attached (3D banner,
124 MB memory mode, silent banner sound). Without **publish** the zip is only a download on the workflow run, which
is handy for testing it on the console first. Pushing a tag `v1.1` does the same as a publish run.

## Building it yourself: one-time setup (Mac)

1. devkitPro with `3ds-dev` (`sudo dkp-pacman -S 3ds-dev`), see [BUILDING.md](BUILDING.md).
2. `makerom` and `bannertool` in your `PATH` (e.g. copy both binaries to `/usr/local/bin`).
3. For the 3D banner: Python 3.12+ (`brew install python@3.12`) and git. The first build downloads pycgfx.
4. `zip` (built into macOS).

## Build the zip

```sh
cd ~/path/to/Sims3DS                       # your clone
git pull
source /etc/profile.d/devkit-env.sh
tools/release/make_release.sh 1.0          # -> dist/Sims3DS-1.0.zip
```

Same thing from the platform folder: `cd src/platform/3ds && make -j4 && make release VERSION=1.0`.

Options: `--cia-mem 96` (or `make release CIA_MEM=96`) for the 96 MB memory mode, `--flat-banner` (or `BANNER3D=0`)
for the flat banner. Check the output: it lists the files in the zip and prints which banner and memory mode were used
("cia: using the 3D banner" / "falling back to the flat banner.png"). Test the CIA on the console before uploading.

## Upload it on GitHub

1. Commit and push your changes as usual.
2. Open <https://github.com/Ryan3DSMods/Sims3DS/releases> and click **Draft a new release**.
3. **Choose a tag** → type `v1.0` → *Create new tag on publish* (target: `main`).
4. Title: `Sims3DS 1.0`. In the description, write what changed (known issues too).
5. Drag `dist/Sims3DS-1.0.zip` into the box *Attach binaries by dropping them here*. Wait for the upload to finish.
6. Click **Publish release**.

The README's "Download the latest release" link always points to the newest published release. For the next one,
use a higher number (`1.1`, `1.2`, …) for both the zip and the tag.
