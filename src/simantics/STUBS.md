# SimAntics core — stubs and deliberate deviations

Everything below is marked in code with `// STUB(3ds): <what the original did>`.
Game logic (VM, threads, memory scopes, routing, rooms, architecture, motives,
entities, lot loading) is translated, not stubbed.

## Rendering / MonoGame (no 3DS renderer yet)

| Where | Original | Now |
|---|---|---|
| `world/components/WorldComponents.h` | `FSO.LotView.Components.*` (ObjectComponent, AvatarComponent, TerrainComponent, RoofComponent) | Plain state holders: position, level, room, direction, dynamic sprite flags, container, DGRP pointer. No drawing. |
| `world/model/Blueprint.h` | `FSO.LotView.Model.Blueprint` | Data only. Walls/Floors/Supported **alias** the VMArchitecture vectors (pointers), RoomMap points at `VMRoomMap::Map`. |
| `world/World.h` | `FSO.LotView.World` | `InvalidateZoom`/`InitBlueprint` no-ops; `WorldState` keeps `WorldSize`, `Level`, `Zoom`. |
| `VMEntity::GetIcon`, `VMGameObject::GetIcon`, `VMAvatar::GetIcon` | `Texture2D` thumbnails | return `nullptr`. |
| `VMHeadlineRenderer::DrawFrame` | `Texture2D` balloon | returns `nullptr` (headline timing/`Update()` kept). |
| `VMArchitecture::SimulateCommands(visualChange)` | hands the renderer the simulated `VisWalls/VisFloors` | the simulated arrays are left in `VisWalls/VisFloors`; the blueprint keeps aliasing the real ones. |
| `VMArchitecture::SetTimeOfDay` | sets `Blueprint.OutsideColor` | kept (just a colour value). |
| `VMWorldActivator::InitWorldComponent` | `component.Initialize(GraphicsDevice, WorldState)` | no device. |

## Vitaboy (avatar meshes/animation)

| Where | Original | Now |
|---|---|---|
| `vitaboy/Animation.h` | `FSO.Vitaboy.Animation` + .anim reader | Name, motions (frame count, time property lists). No bone data. |
| `vitaboy/Appearance.h` | `Appearance`, `Outfit`, `SimAvatar`, `Skeleton`, `Animator` | `SimAvatar` holds outfit pointers + hand gestures; `ReloadSkeleton`/`Add/RemoveAccessory` no-ops; `Animator::RenderFrame` only computes the completion status (`frame < 0 || frame > NumFrames`) — which is what drives `EndReached`/looping in the VM. |
| `content::AvatarAnimationProvider` | loads `.anim` from Animations.far | returns a placeholder animation named after the request with **30 frames**. Animation-length dependent behaviour (turn/walk/animate-sim timing, xevt events) will be approximate until a real .anim reader exists. |
| `content::AvatarAppearanceProvider/AvatarOutfitProvider/AvatarSkeletonProvider/AvatarPurchasableProvider` | content lookups | cached placeholders keyed by name/id; purchasables always null. |
| `VMAvatar::SetAvatarType` | `VMAvatarType.Child` left `Avatar` null (crash later in C#) | child uses the adult skeleton. |
| `VMAvatar::SetAvatarRandomOutfit` | `new Random()` | `rand()` (cosmetic only, not simulation state). |

## Audio (HIT / XA / UTK)

Sound is real now (src/hit, src/audio; see src/audio/README.md). Deviations from the C#:

| Where | Original | Now |
|---|---|---|
| `model/Sound/VMSoundEntry.h` | `HITSound Sound` (GC reference) | `std::shared_ptr<FSO::HIT::HITSound>` shared with the HIT VM. |
| `VMPlaySound` | starts the FWAV event | same; TS1(sims): `kKillInstance`/`kTurnOffTV` events (tv_turn_off, stereo_turnoff) release the owner's TV/radio station, `kSndobCancelNote` (radio_next_song) skips to the next song; every source passes arg1 = 0x1000 + ObjectID. |
| `VMEntity::TickSounds` | `WorldUI.GetScreenPos(worldState)` | same formula, screen position computed from `WorldSpace.GetScreenFromTile(DrawPosition) + GetScreenOffset()` (the VM-facing GetScreenPos is a stub); off-world owners play unattenuated. |
| `VMAvatar::SubmitHITVars` | `ObjectVar[12] = Gender` (TSO) | TS1: field 0 = 0 male / 1 female / 2 child (what `generic_fm_smartlists` reads), also [12]; arg2 = that value. |
| `VMAvatar::HandleTimePropsEvent` | `sound` events, entry without a Name | same, with the Name set so an event does not stack. |
| `VM::Update` | pause/resume the sounds of `SoundEntities` (never filled in FreeSims) | every entity's sounds. |
| `engine/VMAmbientSound` | `AmbiencePlayer` playing TSO .fsc/.xa | tables translated, still its own inert player: TS1 has no such files (`FSO::HIT::AmbiencePlayer` exists for completeness). |

## TSO / networking (FreeSims is a TSO-engine fork; The Sims 1 single player needs none of it)

| Where | Original | Now |
|---|---|---|
| `VM::Tick` | `Driver.Tick(this)` (VMNetDriver: local/client/server) which applied net commands then called `InternalTick` | calls `InternalTick()` directly. `SendCommand`, `ForwardCommand`, `OnPacket`, `CloseNet`, `ReplaceNet`, `GetUserIP`, chat/EOD events dropped. |
| `VM::GlobalLink()` | `Driver.GlobalLink` | always `nullptr` (`IsServer() == false`). `CheckGlobalLink` (VMTSOGlobalLinkStub) is present. |
| `engine/TSOGlobalLink/IVMTSOGlobalLink.h` | interface + delegates | translated as interface + `std::function` callbacks; `QueueArchitecture(VMNetArchitectureCmd)` removed with NetPlay. |
| `engine/TSOGlobalLink/VMTSOGlobalLinkStub` | local server stub | `PerformTransaction` budget logic translated (callback invoked synchronously instead of on a thread); `Tick` (architecture command queue) stubbed; roommate requests set permissions directly instead of forwarding `VMChangePermissionsCmd`; `LeaveLot` deletes the avatar (no CloseNet/chat). |
| `engine/TSOGlobalLink/VMTSOStandaloneDatabase.h` | persisted with BinaryWriter | in-memory only. |
| `VMThread.h` `VMEODPluginThreadState` | NetPlay EOD plugin state | empty `VMAsyncState`. |
| EODHost calls in `VMAvatar::Reset/UserLeaveLot`, `VMContext::RemoveObjectInstance`, `VM::Init` | force-disconnect EOD plugins | removed. |
| `VMArchitecture::RunCommands` | `VMChatEvent` "placed N walls" messages | not emitted (costs computed as before). |
| Marshalling (`Save`/`Load`/`LoadCrossRef`, every `*Marshal` class, `VMSerializable`) | network/state serialization | not ported (out of scope for this pass). |

## Content (`src/content/ContentManager.*`)

| Where | Original | Now |
|---|---|---|
| `WorldObjectProvider::Init` | reads `Content/objects.xml`, `npc.xml`, `ep*.xml` packing slips | scans every `.iff` in Objects.far / ExpansionPack*.far / Deluxe.far / Downloads/*.far for OBJD chunks and writes `<CacheDir>/objects.cache` (GUID → far index + entry). Loose `.iff` objects (`LooseIffPaths`) are indexed directly. First archive wins on duplicate GUIDs. |
| `WorldObjectProvider` PIFF handling | `PIFFRegistry` rewrites | not applied (PIFF patches are TSO/FreeSO-only). |
| Downloads scanning on 3DS | `Directory.GetDirectories` | needs `<dirent.h>`; enabled on 3DS with `-DST_HAVE_DIRENT` (devkitARM+libctru). Plain arm-none-eabi newlib skips it. |
| `WorldWallProvider/WorldFloorProvider/WorldRoofProvider` | walls.iff / floors.iff / roof catalog | empty maps (WALm/FLRm remapping, wall prices and roof names resolve to nothing until ported). |
| `GlobalTuningProvider` (`tuning.dat` lotmotives/simmotives) | TSO tuning | empty; `GetNum` returns 0. Only TSO-path code uses it (`VMAvatarMotiveDecay`, `VMMotiveChange::ScaleRate` for `!TS1`, legacy `VMMotiveDecay`); divide-by-zero guards added. The Sims 1 avatars use `VMTS1MotiveDecay` (constants built in). |
| `WorldObjectCatalog` | TSO catalog | `GetItemByGUID` returns null. |

## UI / misc

| Where | Original | Now |
|---|---|---|
| `VMDialogHandler::ShowDialog` / exception dialogs | UI subscribes to `VM.OnDialog` | `VM::OnDialog` is a `std::function`; nothing is subscribed by default. |
| `world/model/XmlHouse.h` | `XmlHouseData.Parse/Save` (System.Xml) | data structs only. |
| `VMAvatar` `XmlCharacter` | FreeSims character xml | data struct only. |
| `VMAvatar::Tick` → `VMFindLocationFor.FindLocationFor` | primitive helper | called when `simantics/primitives/VMFindLocationFor.h` exists (`__has_include`), otherwise skipped. |
| `VMContext` primitive table | registered inline in the C# constructor | `RegisterDefaultPrimitives(VMContext*)` — weak no-op in the core, defined by `primitives/VMPrimitiveTable.cpp`. `VMChangeActionString` (opcode 50, translated here) is registered by the core if the table leaves slot 50 empty. |

## Ownership (C# had a GC)

* `VM` owns every entity and multitile group ever created (`VM::OwnEntity/OwnGroup`); they are freed when the VM is destroyed. Removed entities stay valid (marked `Dead`), exactly like dangling C# references.
* Assembled routines are cached per `BHAV*` for the program lifetime (`VM::Assemble`); `BHAVChanged` moves the old routine to a graveyard instead of freeing it.
* Stack frames and queued actions are `std::shared_ptr` (frames are referenced while being popped).
* `VMContext` owns the `VMArchitecture` (`Architecture(std::unique_ptr)`), `VMClock`, caches and (via `OwnedBlueprint`) the blueprint created by the activator. The caller owns the `VMContext` and the `VM`.

## Behavioural safety deviations (C# would have thrown; C++ would be UB)

* `VMMemory` bounds-checks temps/args/locals/TempXL/entry points/slots and converts C# `InvalidCastException` / `NullReferenceException` / `IndexOutOfRangeException` into `VMSimanticsException`, so the thread's catch handler resets the object as in FreeSims.
* `VMArchitecture::GetWall/GetFloor` return an empty tile for out-of-range offsets.
* `VMSuitProvider` job outfit level is clamped to 0..2 (C# indexed `[3]` for grade ≥ 13 and threw).
* `Math.Round` → `std::nearbyint` (banker's rounding, same as C#).

## sims adjustments after integration (coordinator)
- `VM::TS1` defaults to **true** (this port only runs The Sims 1 content). FreeSims defaulted to false,
  which sent TS1 code down TSO paths. Every other core use of the flag selects the TS1-correct branch.
- `VMGenericTSOCall` gains a TS1 dispatch (marked `ADDED(sims)`): modes 1/2/13/14 share the TSO code;
  4–7 (family management) are TODO no-ops until the neighbourhood/family model exists; 12 returns camera
  distance 0; **28 returns lot zoning (0 = residential)** — this fixed "Is lot a Residential Lot?"
  (global tree 321), which had reset every plant, fireplace and mailbox on every lot; other TS1 expansion
  modes (17+) return true.
- Known remaining gap: "set random job" (Newspaper, Doll House) iterates the neighbourhood via SetToNext,
  which needs the TS1 neighbourhood model (also commented out in FreeSims) → 1–2 resets per lot.
  → RESOLVED by the neighbourhood pass (see STUBS_neighbourhood.md): Career/neighbour SetToNext, neighbour scopes,
  TS1 family calls, families on lots (VMTS1FamilyLoader), TS1 autonomy opcodes 3/30, tools/simlife.
- No Sims appear yet: TS1 houses don't contain Sims; families live in Neighborhood.iff / UserData and are
  placed on the lot by the neighbourhood code (next subsystem).
- **FIX: FreeSims OBJM/OBJT object mapping bug.** `VMWorldActivator` read `IDToOBJT` as pairs but used the
  object ID (`IDToOBJT[k]-1`) as the OBJT index instead of the paired TypeID (`IDToOBJT[k+1]`). Most TS1
  objects got GUID 0 and were dropped (House02: 57 of 388 objects loaded). Now mapping by TypeID:
  388/388 load, positions come from the ARRY#3/#103 object grid (OBJM's own heuristic positions disagree
  with the grid 281/281 times). Only 1 placement failure on House02 (a pedestrian portal on the lot edge).
  `WA_DEBUG=1` prints loader statistics.
