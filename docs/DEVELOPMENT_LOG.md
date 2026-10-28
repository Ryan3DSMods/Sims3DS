# Development log (early milestones)

*Historical notes from the first weeks of the port, kept for developers. The current state is in the top-level
README.md and src/app/README.md. Tool names below still work (`make` builds build/simtool; `make -f Makefile.port ...`
builds the others).*


A from-scratch C++ port of **The Sims** (2000) engine for the New Nintendo 3DS,
using [FreeSims](https://github.com/francot514/FreeSims) (MPL-2.0) as the
format/behaviour reference and the owner's own game CD for data. Like OpenRCT2,
this ships **no game assets** — it reads the files from a real installation.

## Status

**Screenshots from the translated engine.** `simshot` renders what the engine loaded —
real floor textures from floors.iff, every object at its saved tile/rotation with its real
DGRP sprites, FreeSims' exact Medium-zoom placement math — after fixing a FreeSims bug that
dropped most Sims 1 furniture on load (see STUBS.md). Walls are drawn "walls down" and depth
is painter's order (textured walls + the z-buffer renderer are the next renderer step).

```
make -f Makefile.port build-port/simshot
build-port/simshot <gameDataDir> <cacheDir> House05.iff floors.iff shot.png
```

**Wholesale port of FreeSims (the engine) — running.** The engine is now a faithful
C++ translation of FreeSims rather than a hand-written subset: `src/files/` (every
IFF chunk class, FAR1/FAR3, ~8.6k lines), `src/simantics/` (VM core: threads, memory
scopes, entities, routing, architecture, world loader, ~15k lines) and all **47
SimAntics primitives**, plus a GUID-indexed content manager. All 122 source files
compile for host and the 3DS (~1 MB of ARM code). `simrun` loads a real house through
the translated world loader — with its actual furniture (OBJM placement) — and ticks
the VM: every neighbourhood house (19–104 objects) runs with only 1–2 object resets,
both from the not-yet-ported neighbourhood model. See PORTING.md / STUBS.md.

```
make -f Makefile.port            # -> build-port/simrun
build-port/simrun <gameDataDir> <cacheDir> House02.iff [ticks]
```

**The living lot — simulation behind the renderer.** `simtool living` draws a real
house, places real composited objects (fridge, toilet, shower, bed, stereo) in its
interior, and runs the autonomy loop on it: each hour the Sim picks an interaction
by the game's scoring, walks to that object, performs it, and its motives update
on a HUD — rendered frame by frame to an animation. (The Sim is a placeholder
figure: Sims are skinned 3-D meshes in the original, a subsystem not built yet;
object positions are chosen, since OBJM placement isn't decoded.)

```
simtool living House02.iff Objects.far frames/    # one PNG per step + status text
```

**3DS target — the engine cross-compiles for the console.** The portable core
(far1, iff, vm) and the 3DS app (`src/platform/3ds/main.cpp`, a libctru
framebuffer lot renderer) both compile cleanly for ARM11/armv6k/VFPv2 — the exact
3DS CPU — and against real libctru headers. The crt0/linker script/specs and
`3dsxtool` are in hand; the final link needs devkitPro's libctru runtime, so the
`.3dsx` builds with one `make` on a devkitPro setup. See `distribution/3ds/`.

**Needs-driven autonomy — the Sims AI, on real advertisements.** Every
interaction advertises per-motive effect ranges in its TTAB (fridge "Have A Meal"
Hunger +65, toilet "Use" Bladder +70, shower Hygiene +25 ...). `src/sim/autonomy`
ports the game's `FindBestAction` scoring — linear interp against the Sim's
current need, the autonomy threshold, the ±15 jitter — and a Sim with decaying
motives picks what to do. A simulated day reads like gameplay: eats when hungry,
uses the toilet, showers, each at the right time. Interactions are filtered
through their VM guard trees first (so a TV's "Turn Off" only appears when it's
on). The VM now carries a **Sim entity** (motives + person data as live scopes:
MyMotives / StackObjectMotives / MyPersonData), and `SetToNext` (iterate lot
objects) ends correctly on an empty lot — so Sim-state guards like the bed's
"Sleep" (`MyMotives[Energy] < 50`) evaluate properly and Bob sleeps when tired.
The lot's available interactions are re-evaluated against the Sim each hour.
Remaining gap: small Fun ads sit under the 50 threshold (the game layers mood /
personality on top).

```
simtool autonomy Objects.far day.csv Fridges.iff Toilets.iff ShowerC.iff TVs.iff ...
```

**Interactivity — object pie menus, evaluated by the VM.** Objects' interaction
tables (TTAB, both plain and the field-encoded v9/10 form) and names (TTAs) parse,
so any object's pie menu reads out — the BBQ's "Grill"/"Force Fire", the bed's
"Sleep"/"Make Bed"/"Wake Up", the bookshelf's "Study Cooking"/"Read a Book" — and
each interaction's guard tree runs through the VM to decide whether it's currently
available. That's the actual click-an-object mechanic of The Sims.

```
simtool interactions Objects.far Bookcases.iff   # pie menu + VM-evaluated guards
simtool simulate     Objects.far AlarmClock.iff  # click an action -> state changes -> menu updates
```

**First interactive loop.** `simulate` runs the whole click mechanic on real
bytecode: the alarm clock shows "Set Alarm" available; running its action tree
sets attribute[5]=1; the menu re-evaluates and now "Set Alarm" is gone and
"Unset Alarm" is available — exactly as the game behaves. (Complex actions that
need routing/animation primitives the minimal VM lacks report an honest error
instead of faking success.)

**Milestone 3 — SimAntics VM: interpreter core runs real bytecode.** BHAV trees
parse and disassemble (primitive names, scopes, operators, branch targets), and
the VM executes them — the Expression primitive (all arithmetic/compare/flag
operators), control flow via true/false pointers (255=ret false, 254=ret true,
253=error), variable scopes (literal/temp/local/param/attributes/global), and
private sub-routine calls. Validated two ways: a hand-assembled tree computes
`5 + 7 = 12 > 10 → TRUE`, and the AlarmClock object's real "Set Alarm" / "main"
trees disassemble and execute. Still ahead: the other ~60 primitives (animate,
routing, interactions, motives), real object instances + the scheduler/tick loop
and interaction queues.

```
simtool bhavlist Objects.far AlarmClock.iff     # list behaviour trees
simtool bhav     Objects.far AlarmClock.iff 4096  # disassemble "main"
simtool vmtest                                   # interpreter self-test
simtool vmrun    Objects.far AlarmClock.iff 4098  # execute a real tree
```

**Milestone 2 — lot viewer: host-rendering works.** DGRP sprite references
composite real objects (`simtool object`), ARRY lot grids decode (floors/walls/
altitude/objects), and `simtool lot` renders a house in isometric — real floor
plan with per-room colour, raised 3-D walls, altitude, the street, and a
composited object placed in the interior. Validated on House01–11 from the disc.
Still ahead in M2: real furniture placement from the object-ID grid + GUID
lookup, textured floors/walls from the catalogs, and the devkitARM/citro3d
cross-compile to run it on hardware.

**Milestone 1 — asset loaders: DONE and validated on real game data.**

Portable core (`src/core`, `src/formats`), no MonoGame/C#/host-only deps:

| Loader | File | Reference | Validated against |
|--------|------|-----------|-------------------|
| FAR1 archive | `formats/far1.*` | `sims.files/FAR1` | `Global.far` (24 members) |
| IFF container | `formats/iff.*` | `IffFile.cs` | RANGER8, Sprites, floors, walls, Build |
| PALT palette | `formats/iff.*` | `PALT.cs` | 66 palettes in floors.iff |
| SPR2 sprite (RLE) | `formats/iff.*` | `SPR2.cs` | pool tiles decoded pixel-correct |
| STR# strings | `formats/iff.*` | `STR.cs` | localized Build tooltips |
| OBJD / DGRP | `formats/iff.*` | `OBJD.cs`,`DGRP.cs` | objects composite (clock, bookcases) |
| ARRY lot grid | `formats/iff.*` | `ARRY.cs` | House01–11 floor/wall/alt plans |

Proof: `simtool sheet floors.iff out/` produces correct isometric pool-tile
sprites at all three zoom levels with clean transparency.

## Host build & verification tool

```
make                       # builds build/simtool (needs g++, zlib)
build/simtool far   Global.far
build/simtool iff   floors.iff
build/simtool strs  Build.iff
build/simtool sprite floors.iff <sprId> <frame> out.png
build/simtool sheet floors.iff out/ [maxN]
build/simtool object Objects.far Bookcases.iff out/   # composite object DGRPs
build/simtool arry  House02.iff                       # dump lot grids
build/simtool lot   House02.iff Objects.far lot.png   # render the lot in iso
```

## Getting the data off the disc (done once, documented in distribution/3ds)

The Complete Collection discs are InstallShield installers. `unshield` extracts
the cabinet (`SETUP/DATA1.HDR` + `DATA2.CAB`) to the real game tree under
`GameData_ALL_Recursive/` — `Objects.far` (75 MB), `Global.far`, `Textures.far`,
`walls.iff`, `floors.iff`, `Sprites.iff`, and loose `Floors/*.flr`, `Walls/*.wll`.

## Roadmap

- **M2 — Lot viewer (host rendering DONE):** remaining — place the real furniture
  from the object-ID ARRY (id 3) + `objt`/`ObjM` GUID table, texture floors/walls
  from their catalogs, then cross-compile to a `.3dsx` and draw on the bottom
  screen (touch to pan, circle pad to rotate).
- **M3 — SimAntics VM (core DONE):** remaining — the rest of the primitive set,
  real object/sim entities, the tick scheduler and interaction queue.
- **M4 — Sims, pathfinding, needs, UI.**

Memory budget is survivable: the original ran in 32 MB; a C++ build stays well
under the New 3DS ~178 MB extended-memory ceiling.
