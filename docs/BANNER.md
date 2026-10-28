# The 3D HOME Menu banner and banner sound

`make cia` builds a 3D HOME Menu banner: a swaying 3D "The Sims" logo (hardware-tested on a New 3DS). The release
CIA uses it too.

## Where the logo comes from

It is made from the model **"Sims game logo 3D" by [3DTwinkie](https://www.printables.com/@3DTwinkie) on Printables**
(<https://www.printables.com/model/1630221-sims-game-logo-3d>), licensed **CC BY-SA 4.0**. The two STL files are in
[`assets/logo/`](../assets/logo/); the scene generated from them (`src/platform/3ds/banner3d/banner_scene*.gltf/.bin`,
`banner_palette.png`) and the flat `banner.png` / `icon.png` rendered from it are adaptations under CC BY-SA 4.0 too —
see [assets/logo/ATTRIBUTION.md](../assets/logo/ATTRIBUTION.md). The logo design itself is a trademark of Electronic
Arts; the model licence covers only the creator's 3D work.

## How it is made

All scripts are included: `tools/banner3d/make_scene.py` turns the STL into a glTF scene (white letter faces, thin red
outlines built in geometry, ±25° sway, 16×16 palette texture — a baked 256×128 texture crashed the HOME Menu), and
`src/platform/3ds/banner3d/make_banner3d.sh` converts it with [pycgfx](https://github.com/skyfloogle/pycgfx)
(skyfloogle) to CGFX and packs it with bannertool. This follows the GBAtemp guide *"How to Make a 3D Banner Like an
Official Nintendo 3DS Game"* by Epic0522. Details and the settings that work on hardware:
[tools/banner3d/README.md](../tools/banner3d/README.md).

The first build needs git, Python 3.12+ and network (it fetches pycgfx at a pinned commit); if anything fails the
build falls back to the flat banner automatically.

- `make cia BANNER3D=0` uses the flat banner instead.
- Logo-free alternative: `make cia BANNER3D=0 ICON=$PWD/neutral/icon.png BANNER_PNG=$PWD/neutral/banner.png`
  (original artwork in `src/platform/3ds/neutral/`, made by `tools/banner/make_flat_banner.py`).
- The HOME Menu caches banners: after changing it, uninstall the CIA, reboot, install again.

## Banner sound (personal builds only)

No game audio is in this repository, so the banner is silent by default — and `make release` always uses the silent
one, so a published CIA never contains game audio. For your own console you can export a sound from **your own** game
data:

```sh
python3 tools/banner3d/export_game_sound.py ~/sims_sd --event ui_object_place    # the purchase "ka-ching"
python3 tools/banner3d/export_game_sound.py ~/sims_sd --music 3                  # neighbourhood music
```

It decodes the sound with the port's sound engine and writes `src/platform/3ds/banner3d/banner_sound.wav`
(44.1 kHz 16-bit stereo, ≤ 3 s, faded out), which `make cia` then uses. The file is git-ignored; do not publish it.
