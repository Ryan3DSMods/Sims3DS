# Features and known limitations

## Features

- **The full engine**, translated from FreeSims/FreeSO: IFF/FAR file formats, the SimAntics virtual machine with all
  primitives, routing, autonomy, the world renderer (walls, floors, roofs, terrain, water, shadows, three zooms, four
  rotations, floors/walls cut-away), Vitaboy 3D Sims with their animations and outfits.
- **All 7 expansion packs + Deluxe**: Livin' Large, House Party, Hot Date (Downtown), Vacation, Unleashed (pets, Old
  Town), Superstar (Studio Town), Makin' Magic (Magic Town).
- **The original front end**: intro, loading screen, every neighbourhood and town map, family bin, move in / evict,
  options.
- **Create-A-Sim / Create-A-Family** with heads, bodies, personality, children.
- **Live, Buy and Build mode** with the game's own catalogues, pie menus, action queue, motives, jobs, skills,
  relationships, inventory, the clothing dialog, walls/floors/wallpaper/doors/windows/terrain/pools/roofs.
- **Audio**: the HIT sound engine (XA/UTK/WAV samples), mode music and radio stations (MP3) — read from your own
  `SoundData/` and `Music/` folders.
- **Travel** by phone/cab to Downtown, Vacation Island, Old Town, Studio Town and Magic Town and back.
- **Saving** in the original file formats (houses, neighbourhoods, families), written so that a power cut or crash
  never leaves you with a broken save (atomic replace + automatic recovery).
- **Cheats**: the PC cheat console (`rosebud`, `klapaucius`, `move_objects on`, `set_hour`, ...) — see
  [CHEATS.md](CHEATS.md).
- **Dual-screen UI**: the lot on the top screen with a pointer (circle pad), a touch view of the lot plus the panels
  (pie menus, motives, queue, catalogue) on the bottom screen.

## Original 3DS / 2DS

Not supported. The original models have a 268 MHz CPU without L2 cache (≈ 3–4× slower: an estimated ~8–12 fps for
drawing the world, Sim behaviour slower so the higher game speeds stutter, and the audio mixer competes with the game
on the shared core) and 64 MB application memory (96 MB at best as a CIA) against the game's measured ~55–60 MB peak
plus code and buffers, so bigger lots and the expansion towns are likely to run out of memory. It may start, but it is
not expected to be playable. There is no old-3DS mode yet (possible future work: base game only, smaller caches,
lower Sim speed).

## Known limitations (full list)

- **Pressing HOME while the game is running can crash the HOME Menu** (seen with the 3D banner, in both the 124 MB and
  the 96 MB memory mode). Close the game with **START → Quit** instead. Being investigated.
- **First start takes 1–2 minutes**: the app copies the neighbourhoods into its own save area (~80 MB) and indexes
  every object archive once. Later starts take a few seconds. The caches can be prebuilt on a PC
  ([BUILDING.md](BUILDING.md#faster-first-start-prebuild-the-caches-on-a-pc)).
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
- **The intro video** needs a one-time conversion on a PC (the extractor does it when ffmpeg is installed); without it
  the game starts at the loading screen.
- **No online features or multiplayer**; no mouse/keyboard; the Sims Creator tools are not part of the port.
- **3D banner**: the HOME Menu caches banners (reinstall and reboot after changing it).
- Saves are written in the original formats; keep backups of `sdmc:/3ds/sims/Saves/` anyway.

## Screenshots

See the [README](../README.md#screenshots) (pictures in `docs/screenshots/`, taken on a New 3DS). You can make your own
on the console with Luma3DS's screenshot feature: L + Down + SELECT, then *Take screenshot*.
