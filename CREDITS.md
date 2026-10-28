# Credits and third-party notices

## Engine (MPL-2.0)

Sims3DS's engine is a C++ translation (a "Larger Work"/derivative under MPL-2.0 §3) of:

- **FreeSims** — <https://github.com/francot514/FreeSims> — by francot514 and contributors. Mozilla Public License 2.0.
  The Sims 1 reimplementation the port follows: file formats (`sims.files`), SimAntics (`SimsVille`), content, world.
- **FreeSO** — <https://github.com/riperiperi/FreeSO> — by riperiperi (Rhys Simpson) and the FreeSO contributors.
  Mozilla Public License 2.0. FreeSims is built on FreeSO's code; parts that FreeSims does not carry (Vitaboy
  avatars, HIT audio, TS1 providers, routing fixes, ...) were translated from FreeSO.

All translated files and this project's own code are distributed under the **Mozilla Public License 2.0**
([LICENSE](LICENSE)); each source file carries the MPL-2.0 notice. Files translated from a specific C# file say so in
their first comment lines ("Translated from FreeSims ...", "FreeSO ..."). Changes for the 3DS and for The Sims 1 are
marked in the code (`TS1(sims)`, `FIX(sims)`, `STUB(3ds)`) and listed in `src/*/STUBS*.md`.

## Third-party code included

| Component | Where | Licence |
|---|---|---|
| **minimp3** by lieff — <https://github.com/lieff/minimp3> | `src/third_party/minimp3/` | CC0 1.0 (public domain dedication), see `src/third_party/minimp3/LICENSE` |
| **xxHash32** — xxHashSharp by Seok-Ju Yun (<https://github.com/noricube/xxHashSharp>), after the C original by Yann Collet; came in via FreeSims `sims.files/Hash.cs` | `src/files/Hash.h`, `src/files/Hash.cpp` | BSD 2-Clause (notice kept in both files) |
| **Inter** typeface by The Inter Project Authors — <https://github.com/rsms/inter> | glyph bitmaps generated into `src/app/FontData.h` by `src/app/tools/genfont.py` | SIL Open Font License 1.1, see [LICENSES/OFL-1.1-Inter.txt](LICENSES/OFL-1.1-Inter.txt) |

## Assets included

| Asset | Licence |
|---|---|
| "Sims game logo 3D" by its creator on Printables — <https://www.printables.com/model/1630221-sims-game-logo-3d> (`assets/logo/*.stl`) and this project's adaptations of it (`src/platform/3ds/banner3d/banner_scene*`, `banner_palette.png`, `src/platform/3ds/banner.png`, `icon.png`) | CC BY-SA 4.0, see [assets/logo/ATTRIBUTION.md](assets/logo/ATTRIBUTION.md). "The Sims" logo design is a trademark of Electronic Arts. |
| `src/platform/3ds/neutral/banner.png`, `icon.png` (original artwork, `tools/banner/make_flat_banner.py`) | MPL-2.0 |
| `src/platform/3ds/banner3d/banner_silence.wav` (silence) | MPL-2.0 |

## Tools and references used (not included)

- **devkitPro** (devkitARM, libctru, 3dstools) — <https://devkitpro.org>; libctru is zlib-licensed. Required to build.
- **bannertool**, **makerom** — CIA packaging.
- **pycgfx** by skyfloogle — <https://github.com/skyfloogle/pycgfx> — glTF → CGFX for the 3D banner (cloned at build time
  by `make_banner3d.sh`).
- GBAtemp guide *"How to Make a 3D Banner Like an Official Nintendo 3DS Game"* by **Epic0522** — the hardware-tested 3D
  banner structure; `convert_banner3d.py`'s post-patches follow its approach.
- **unshield** by David Eriksson and contributors — <https://github.com/twogood/unshield> (MIT) — InstallShield cabinet
  extraction; **py7zr**, **pycdlib** — disc image handling.
- 3dbrew (<https://www.3dbrew.org>) for the CBMD/CGFX/SMDH formats.
- Luma3DS, FBI, the Homebrew Launcher — to run it.

## Game

The Sims, its expansion packs and all their content are © Electronic Arts Inc. Nothing of the game is included; the
engine reads the data from the player's own copy at runtime.
