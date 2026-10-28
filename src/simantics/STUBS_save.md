# Saving a lot like The Sims 1 (HouseNN.iff + Neighborhood.iff)

The running lot is written back into its house file in the original game's format, and the lot's Sims and money
into the neighbourhood file. New code is marked `// TS1(sims)`, fixes to translated code `// FIX(sims)`.

FreeSO has no TS1 house *writer* (Simitone stores its own VM snapshot, an `FSOV` chunk, which the original game
cannot read), but it has the exact *reader* of the original state: `VMTS1ActivatorNew.LoadFromIff` turns every OBJM
record into a VM entity, thread, stack, queue, multitile group and avatar. The saver is that conversion run backwards
(`ConvertThread`, `ConvertAction`, `ConvertAvatar`, `ConvertArchitecture`), on FreeSO's OBJM record layout.

## Files

| File | What |
|---|---|
| `files/formats/iff/IffFieldEncodeWriter.{h,cpp}` | Writing side of `IffFieldEncode` (the bit-packed "field encoding" of OBJM/TATT): 0 = one bit, else 1 + 2-bit width code + value, smallest width; strings byte aligned, NUL, padded to an even offset. |
| `files/formats/iff/chunks/OBJMWriter.{h,cpp}` | `Write*` mirror of every FreeSO OBJM reader (`OBJMInstance`, person, stack frame, interaction ...), `OBJMRawChunk` (table / records / tail split without decoding), `WriteOBJMChunk`. |
| `files/formats/iff/chunks/ARRYWriter.{h,cpp}` | ARRY run-length encoder (runs of >= 8 bytes, literal blocks, as the shipped files) and `TransposeARRY`. |
| `files/formats/iff/IffRawFile.{h,cpp}` | An IFF as its raw chunk list: unknown chunks byte for byte, original order, `rsmp` resource map regenerated (original type/entry order, 0xA3 label pads), atomic save (tmp + rename). |
| `simantics/utils/VMTS1LotSaver.{h,cpp}` | VM -> house file; `UpdateNeighbourhood` (Sims' persistent data, family budget into the provider). |
| `content/TS1NeighbourhoodWriter.{h,cpp}` | The single writer of `Neighborhood.iff` (NBRS, NGBH, TATT, FAMI); `NhoodModel::Save` now calls it. |
| `app/front/LotSave.{h,cpp}` | App: thumbnail render + `VMTS1LotSaver` for the current `LotSession`. |
| `tools/simsave.cpp` (`build-port/simsave`) | Checks and round trips (below). |

Minimal hooks in other agents' files: `VMEntity::AttributeData()` and `VMAvatar::GetMotiveChange()` (read-only
accessors), `VMWorldActivator::RestoreSavedState` (FIX: a Sim saved sitting/lying stands up next to that object instead
of at the mailbox), `NhoodModel::Save` (delegates to the writer), `front/AppFlow.cpp` (Save / save-before-leaving),
`Makefile.port` (`simsave` target).

## What is written

**House file** (rebuilt from the file the lot was loaded from - the user file, or the expansion template for a lot
never saved - and written to `<UserData>/Houses/HouseNN.iff`; the install and templates are never written):

| Chunk | Content |
|---|---|
| `objt` | Object types: saved entries byte for byte, new types appended (their unknown fields copied from another house file of the neighbourhood that has the GUID, else init/main tree versions from the BHAVs). |
| `ObjM` | One record per VM entity (ghosts excluded). Position (1/16 tiles, exact sub-tile position kept while the tile is the same), footprint (kept while unmoved, else the VM obstacle), attributes, the 8 temps, object data (68), stack (every BHAV frame: tree, node, params/locals sized by the BHAV, stack object, code owner type; routing frames are not TS1 frames, the routing primitive below reruns as in FreeSO), relationships (saved order, new ones appended), the tile linked list (`LinkedSlot`) and `ParentId` (previous object on the tile, or the container), container slots, dynamic sprite flags, multitile data (lead, world offset, group offset), portal data, and for persons: motives (the saved float kept when the VM value is its rounding), person data (80 / 256), active interaction + queue (UID, target, icon, TTA index, args, priority, action tree, flags), object uses (stack depth where the callee changes), motive deltas (per hour change / 1800 per tick, stop value). Records keep the saved order; new objects first (newest first), as TS1 does. |
| `Arry` 3 / 103 | Head object of every tile's list. |
| `Arry` 1/101/2/102 (or 11/111/12/112) | Floors and walls: the saved tiles are decoded as the loader does; tiles the VM changed are encoded, all others keep their bytes. Custom floors/walls (content id >= 256) get `FLRm`/`WALm` entries (file name -> local id, ids from 255 down, both map versions updated). Pool / water grids follow pool tiles. |
| `SIMI` | The simulation globals patched in place: time of day, minutes/seconds, day, month, year, selected Sim, family, house, budget ... (17, 20, 25, 32, 33 keep the saved values: the loader overrides them for this engine). |
| `BMP_` 512 | The neighbourhood picture: the lot drawn at far zoom (walls up, centred on the house) at twice the size of the saved picture and averaged down. |

| `Arry` 0 / 6 / 8 | TS1(sims): terrain altitude (4 corner bytes per tile, only tiles whose corners moved), lawn state (127 - grass), flag 1 "level" of the changed tiles. |
| `Arry` 9 / 10 | Pool / pond grids regenerated when a pool changed: per tile the mask of the 4 same-kind neighbours (1 = (x,y-1), 2 = (x-1,y), 4 = (x,y+1), 8 = (x+1,y); 0xFF none), as every shipped pool. |
| `HOUS` 0 | Roof style name (GameData/Roofs file name) and pitch, when the roof tool changed them. |
| `SIMI` lot values | ObjectsValue / ArchitectureValue + the session's buy / build spending (the house price); bulldoze sets them. |

Everything else (the second picture, `THMB`, unknown chunks) is byte for byte; unchanged terrain/HOUS/pools stay byte for byte
(roundtrip House02/House07 checked).

Fidelity rules learnt from re-saving unmodified houses (`simsave diffobjm`): object data the engine computes or that
this port initialises differently from TS1 keeps the saved value while the object has not moved: `Flags` bits
ShowGhost / AllowPersonIntersection / ChairFacing / HideForCutaway / NeedsMaintinance, `FlagField2`, `SlotCount`,
`WallAdjacencyFlags`, `Room`. Interaction-only objects the loader leaves fresh (`Internal` OBJD type, `*Social
Interactions`, `*comeandsee`) are written from their saved record. Objects the loader could not place (two objects
on one tile, e.g. House07's flower bed under a rose bush; objects on tile row 0) keep their saved position.

**Neighbourhood** (`TS1NeighbourhoodWriter`, called by `NhoodModel::Save`): every neighbour record (person data
0..87 from memory, the rest of the 80/256 as saved; relationships; records created at run time appended in their
slot), NGBH (16 neighbourhood shorts incl. the expansion "help shown" counters, and the Hot Date+ inventories),
TATT (field encoded), FAMI (house, number, budget, net worth, friends, members patched into the saved record, which
keeps its version; FIX: FAMI::Write writes the version it read and its tail - new families take the file's version,
7 in the shipped neighbourhoods - instead of FreeSims' 9 + 16 bytes; CAS families too), deleted families removed. Unchanged records keep their bytes: loading and writing the shipped
`Neighborhood.iff` files reproduces them exactly. Before the write, `UpdateNeighbourhood` runs TS1 "save this Sim's
persistent data" (Generic Sims Call 25) for every person on the lot and puts the lot family's money (globals 13/14)
into its FAMI. Character files (`User*.iff`) hold no run-time state in TS1 (skills, job, personality are person data
in NBRS) and are not written.

## Front end

`App::LotFlow` Options **Save**: house (`front::SaveLot`) then neighbourhood (`FrontEnd::SaveNeighbourhood`), dialog
"The game has been saved." **Neighborhood / Quit** with "Yes" in the "Save?" / "Quit?" dialog: `App::FinishLeave`
saves the house before the session is released, `FrontEnd::ReturnFromLot` writes the neighbourhood. "No" discards
both (the neighbourhood is reloaded, as before).

## Checks (`build-port/simsave`)

```
simsave raw  <file.iff>...                                  parse + write == original bytes (rsmp regenerated)
simsave objm <gameData> <cache> <House.iff>...              decode + re-encode every OBJM record == original bytes
simsave diffobjm <gameData> <cache> <a.iff> <b.iff>         field-by-field record comparison
simsave nhood <gameData> <cache> <saveRoot> [hood]          neighbourhood load + write must be "unchanged"
simsave roundtrip <gameData> <cache> <saveRoot> <house> <outDir> [updates]
        load house + family, run updates, render, save house + neighbourhood, reload, compare (objects by id+GUID,
        positions, containers, attributes, object data, relationships, sprite flags, person data, motives, floors,
        walls, clock, globals), render, save again and compare the files.
        SIMSAVE_EDIT=1: delete, move, buy (known and new types, a 6-tile object), paint floors (custom .flr), build and
        repaper walls (custom .wll), change a motive and the money before saving. SIMSAVE_HOOD=1: UserData2.
```

Results on the Complete Collection data:
- `raw`: all 24 house files of UserData/UserData2 are reproduced byte for byte. (Character files and
  Neighborhood.iff carry a stale rsmp the game evidently does not use; it is regenerated when those files are written.)
- `objm`: every record of every house with a decodable OBJM (versions 0x3E/0x3F, 3,280 records incl. all persons)
  re-encodes bit for bit; House11 (version 0x38) has data after the fields FreeSO knows: it is carried as raw bits.
- `nhood`: both shipped neighbourhoods load and write back unchanged.
- `roundtrip`, 0 updates, House07: the saved records equal the game's except thread state (stacks/temps restart in
  this engine) and person data 27/28/39/41/42/44 (tree use, routing) which the loader does not restore.
- `roundtrip` after 1 to 6 game hours, UserData houses 1-11 and UserData2 houses 1-10 with `SIMSAVE_EDIT`:
  0 object count differences, all positions/containers/attributes/relationships/motives/person data equal except the
  loader limitations below; floors and walls (incl. custom ones) equal; the second save of a reloaded lot equals the
  first except for the controllers (new ids every load). Renders before save and after reload are identical
  (`h7_t3600_edit_a_before.png` / `_c_reloaded.png`: moved fountain, bought doll house and 6-tile table present).
- App (`sim3ds_host`): Options Save, then Neighborhood -> "Save?" Yes -> re-enter the lot: house and neighbourhood
  written, the lot reloads, the neighbourhood screen shows the new house picture.

## Not saved / approximations

- **Loader limitations (not the file):** threads restart on load (FreeSO restores stacks; this engine does not), so
  saved stacks are written but not read back here; interaction-only objects restart fresh; moving vehicles (school
  bus) are placed on tile centres; objects on tile row/column 0 are not placed by the loader (they keep their saved
  position at the next save); global 36 is reset by an init tree on load.
- **PrimitiveState** (state of a waiting primitive) is written 0; FreeSO does not read it either.
- **Person animation strings**: the current animation is not mapped from Vitaboy's state; existing Sims keep their
  saved strings, new ones get the standing loop. Accessories are not written for new Sims.
- **OBJT unknown fields** of a type no house in the neighbourhood has (expansion controllers): Unknown1a/2a = 0.
- **Lot / object value fields of SIMI** (LotValue, ObjectsValue, ArchitectureValue) and the budget history keep the
  saved values.
- **Thumbnail**: drawn with the current time of day (the game's pictures are daylight) and without the magenta
  border outside the lot; THMB and BMP_ 513 keep their saved bytes.
- **TS1GameState** (travel bookkeeping) has no place in the TS1 files.
- **Not runtime tested on the original game** (no Windows/TS1 here): the files follow the shipped layout and every
  chunk the saver writes is re-read by FreeSO's readers; OBJM records are bit-exact for unchanged state.
