# 3D HOME Menu banner ("extended banner") for the CIA

`make cia` builds a 3D HOME Menu banner by default (`BANNER3D ?= 1`): the swaying 3D logo, hardware-tested on a New 3DS.
If any step fails (no Python 3.12, no network on the first run, ...) the build falls back to the flat
`src/platform/3ds/banner.png`; `make cia BANNER3D=0` forces the flat banner.

The logo model and everything rendered from it are **CC BY-SA 4.0** — "Sims game logo 3D" by [3DTwinkie](https://www.printables.com/@3DTwinkie) on Printables
(<https://www.printables.com/model/1630221-sims-game-logo-3d>); see [`assets/logo/ATTRIBUTION.md`](../../assets/logo/ATTRIBUTION.md).
"The Sims" logo design is a trademark of Electronic Arts; build it for personal use on your own console.

## Pipeline

    assets/logo/thesimslogo_red_backer.stl
        --make_scene.py--> src/platform/3ds/banner3d/banner_scene.gltf (+ .bin, banner_scene_static.gltf, banner_palette.png)
        --pycgfx (skyfloogle, pinned commit 1f78850)--> CGFX --convert_banner3d.py patches--> build/banner3d.cgfx
        --bannertool makebanner -ci (+ banner audio)--> build/banner.bnr
    icon.png --bannertool makesmdh -f visible,allow3d,recordusage,extendedbanner--> build/sims_cia.smdh
    makerom ... -icon build/sims_cia.smdh -banner build/banner.bnr

`src/platform/3ds/banner3d/make_banner3d.sh <build dir>` runs the CGFX + banner steps (called by `make cia`). The first
run clones pycgfx into `src/platform/3ds/.tools/`, makes a venv and `pip install`s gltflib + pillow (needs git,
Python >= 3.12 and network).

The generated scene is committed, so you only need `make_scene.py` to change it:

    python3 tools/banner3d/make_scene.py [--stl file.stl] [--static] [--no-outline] [--backdrop]   # writes src/platform/3ds/banner3d/
    python3 tools/banner3d/preview.py [scene.gltf] [out dir]       # software preview -> build/banner3d_preview/*.png
    python3 tools/banner3d/flat_banner.py                          # re-render the flat banner.png + icon.png
    python3 tools/banner3d/cgfx_parse.py [--summary] file.cgfx     # independent CGFX structure check
    (make_scene.py / preview.py / flat_banner.py need numpy, Pillow and scipy)

## Settings that work on hardware (v16)

- Scene: `COMMON` -> `world` (empty: the HOME Menu's own rotation applies to it) + `logo` (sibling of `world`) with a
  rigid node animation: yaw = 25° * sin(2πt / 5 s), 30 fps, 151 LINEAR keys (last == first), T/R/S channels ->
  pycgfx CANM "COMMON", member "logo" = ordinary Transform (5). `banner_scene_static.gltf` = the same without
  animation (automatic fallback in make_banner3d.sh, or force it with `BANNER3D_STATIC=1`).
- Colours: a 16×16 palette texture (left half red, right half white, nearest filtering). The thin red outline
  (1.9 mm, the "M" 2.8 mm, "The" 0.75 mm) is built in **geometry**: a signed distance field per letter, faces refined
  only where the outline crosses them, each vertex gets u = 0.5 + 0.045·F on the palette. A baked 256×128 texture
  variant **crashed the HOME Menu** — keep the palette approach.
- Matte material (convert_banner3d.py removes pycgfx's specular stage), one CMDL named COMMON, no skinning or bone
  streams, CGFX < 512 KiB.
- Knobs in make_scene.py: `SWAY_DEG`, `SWAY_PERIOD`, `SWAY_FPS`, `LINE_*`, `REFINE_MM`, `ISO_TOL_MM`, `TARGET_WIDTH`
  (27.2 units), `WORLD_Y` (1.4), `LINE_OVERRIDES`, `NO_LINE_MAX_H`, `CREASE_DEG`, colours. HOME Menu camera (pycgfx
  banner-camera.gltf): (0, 1, 44.786) looking -Z, yfov 30°, 5:3, znear 26.5 -> at z = 0 the top screen spans
  x -20..20, y -11..13.
- The HOME Menu caches banners: uninstall/reinstall the CIA or reboot after changing it.

## Banner sound

No game audio is included. The banner is silent unless you export a short sound from **your own** game data:

    python3 tools/banner3d/export_game_sound.py <your extracted game folder> --event ui_object_place
    python3 tools/banner3d/export_game_sound.py <your extracted game folder> --music 3          # neighbourhood music

This decodes the sound with the port's own engine (`build-port/simaudio`, built on demand) and writes
`src/platform/3ds/banner3d/banner_sound.wav` (44.1 kHz, 16-bit stereo, <= 3 s, faded out), which `make cia` uses
automatically. The file is git-ignored — keep it for yourself. Ideas (names only): `ui_object_place` /
`ui_buy_place` (purchase "ka-ching"), `ui_moneyback`, `--music 3` (neighbourhood), `--music 5` (loading screen);
`build-port/simaudio <game folder> info` lists everything your data has.

## References

- GBAtemp guide "How to Make a 3D Banner Like an Official Nintendo 3DS Game" by Epic0522 (hardware-tested scene
  structure; convert_banner3d.py's post-patches follow its `convert_banner_cgfx.py`).
- pycgfx by skyfloogle: <https://github.com/skyfloogle/pycgfx> (glTF -> CGFX).
- bannertool / makerom (3DS homebrew tools), 3dbrew's CBMD/CGFX pages.
