# Troubleshooting

- **Log file:** copy the empty file `extras/debug` from the release zip to `sdmc:/3ds/sims/debug` (or create an empty
  file with that name). The app then writes `sdmc:/3ds/sims/log.txt`: one line per loading step with the time and
  free memory. The last `step` line shows where a hang or crash happened; `FATAL:` lines carry the error shown on
  screen; `app free` below ~10 MB means memory trouble; `WARNING: could not write` means the SD card is full. Delete
  `debug` again for normal play (logging costs a little SD time).
- **"Could not open file …" / "No objects found" / "No neighborhood found":** the data is not where the app looks.
  Check that `sdmc:/3ds/sims/GameData/Objects/Objects.far` and `sdmc:/3ds/sims/UserData/Neighborhood.iff` exist (not
  `sdmc:/3ds/sims/sims/...` or `sdmc:/3ds/sims/3ds/...`).
- **HOME Menu crashes after pressing HOME in-game:** known issue, being investigated. Quit with **START → Quit**.
- **Crashes:** Luma3DS writes crash dumps to `sdmc:/luma/dumps/arm11/` (`crash_dump_*.dmp`). Please attach the dump
  and `log.txt` to a [GitHub issue](https://github.com/Ryan3DSMods/Sims3DS/issues).
- **No sound:** `sdmc:/3ds/dspfirm.cdc` is missing (made by the standard custom-firmware setup; otherwise run
  [DSP1](https://github.com/zoogie/DSP1) once), or `SoundData/` / `Music/` were not copied.
- **Save recovery:** saves are written as `<file>.new`, then the old file is renamed to `.bak`, then `.new` replaces it.
  If a crash or power cut interrupts this, the next start repairs it automatically (log line `[save] RECOVERED`). If a
  neighbourhood cannot be loaded at all, the app offers to restore that neighbourhood from your original game data
  (the damaged folder is kept as `UserDataN.damaged`).
- **Start over:** delete `sdmc:/3ds/sims/Saves/` (your game!) and `sdmc:/3ds/sims/cache/` (then run the extractor
  again if you want the intro back, or copy `cache/intro.*` aside first).
- **Slow first start:** prebuild the caches on a PC ([BUILDING.md](BUILDING.md#faster-first-start-prebuild-the-caches-on-a-pc)).
- **No intro:** install ffmpeg and run the extractor again, or convert it by hand
  ([BUILDING.md](BUILDING.md#the-intro-video)).
- **HOME Menu banner did not change** after reinstalling: the HOME Menu caches banners — uninstall the CIA, reboot,
  install again.
- **"Out of memory" or a crash right after starting the .3dsx:** use the CIA, or title takeover (hold R while starting
  a game) for the .3dsx.

## The extractor

- **macOS: "cannot be opened because it is from an unidentified developer":** right-click `extract.command` → *Open* →
  *Open*. Only needed the first time.
- **"Python 3.8 or newer is needed":** install Python from <https://www.python.org/downloads/> (Windows: tick *Add
  python.exe to PATH*), then run the extractor again.
- **"unshield … was not found"** (disc images only): macOS `brew install unshield` (the extractor offers it when
  Homebrew is installed); Linux `sudo apt install unshield` / `sudo dnf install unshield` / `sudo pacman -S unshield`.
  On Windows the easiest way is to install the game from your discs and pick the installed folder instead.
- **"MISSING: …" at the end:** not all discs were in the folder, or the installed game is incomplete.
- Run `python3 extract_complete_collection.py <folder> --dry-run` to see what it would do without copying anything.
