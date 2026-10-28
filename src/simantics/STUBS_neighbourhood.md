# The Sims 1 neighbourhood, families and Sims on the lot

This pass ports the TS1 neighbourhood (`TS1NeighbourProvider`), makes the VM's neighbour scopes and
Set To Next neighbour searches work, implements the TS1 family Generic Sims Calls, puts a family on its lot,
and adds `simlife`, a tool that runs the family's Sims. Code that is new (FreeSims has no code for it) is marked
`// TS1(sims)`. Fixes to translated FreeSims bugs are marked `// FIX(sims)`.

Where FreeSims has no code, the semantics come from the game's own data:
- the Edith label tables in `GameData/Behavior.iff`: STR# 129 'global labels', 132 'data owners'
  (scopes), 139 'Primitives', 200 'Person Data Labels', 220 'generic sim calls', 221 'neighbor data labels',
  243 'job data', and 249 'neighborhood data labels';
- how the shipped BHAVs use them, found with a BHAV scanner/disassembler over Global.far, Objects.far and
  the character IFFs (for example VisitGenerator 'do generate', PhoneGlobals 'can call neighbor',
  SocialInteractions 'do move in', and Global 'count my (non-pet) family members').

Some decisions also rely on my knowledge of FreeSO/Simitone, which is FreeSims' successor with real TS1
support: `InheritNeighbor`, opcode 3 = Find Best Interaction, and `ScaleRate` applied for TS1. Each of these is
noted where it applies below.

## Files

| File | What it is |
|---|---|
| `content/TS1NeighbourProvider.{h,cpp}` | `FSO::Content::TS1::TS1NeighborhoodProvider` + `TS1GameState`. Translated: every C# member. New: FamilyForHouse fill, TATT/zoning/LotLocations/desc loading, `GetFamily/GetFamilyName/ListFamilies/GetHouseOfNeighbor/GetZoning/SetToNextClosestHouse/FindCharacterFile/LoadCharacters`. |
| `content/TS1JobProvider.{h,cpp}` | New. CARR careers from `Work.iff` (GameData/ExpansionPack*). **The test install has no Work.iff (no CARR anywhere)**, so it is empty. |
| `content/ContentManager.{h,cpp}` | `Content::Neighborhood`, `Content::Jobs` (forward-declared). Created in `Content::Init` after WorldObjects. `LoadCharacters()` registers `UserData/Characters/User*.iff`. |
| `files/formats/iff/chunks/NGBH.cpp` | Fixes reading of pre-Hot Date NGBH (version 0x3E). It has no inventory section, and the C# read past its end. |
| `simantics/engine/VMTS1Neighbourhood.{h,cpp}` | New. Neighbour, job, neighbourhood, type-attribute and relationship access for the VM, plus family moves (GSC 4/5/6/7/33), `InheritNeighbor`, and `SavePersistentData`. |
| `simantics/engine/VMStackFrame.{h,cpp}`, `VMThread.cpp`, `VMMemory.cpp` | `StackObjectRawID` / `StackObjectID()`; scopes 10, 12, 24, 28, 29, 32–34, 36–38. |
| `simantics/primitives/VMSetToNext.cpp` | Search types 5, 7, 10, 11 and 12 for TS1. |
| `simantics/primitives/VMRelationship.cpp` | Neighbour relationships (NBRS). |
| `simantics/primitives/VMGenericTSOCall.cpp` | TS1 modes 4, 5, 6, 7, 25, 28 and 33. |
| `simantics/primitives/VMGosubFoundAction.{h,cpp}`, `VMPrimitiveTable.cpp` | TS1 opcode 3 (Find Best Interaction) and opcode 30 (Gosub Found Action). |
| `simantics/model/VMTS1Appearance.{h,cpp}`, `entities/VMAvatar.{h,cpp}` | TS1 appearance from the body strings (`VMAvatar::TS1Appearance`). TS1 `Init` order. Name from CTSS. Child walk tables. ScaleRate. |
| `simantics/utils/VMTS1FamilyLoader.{h,cpp}` | New. Puts a family on its lot. |
| `tools/simlife.cpp` (+ `build-port/simlife` in Makefile.port) | Runs a house with its family and logs interactions, motives and resets. |
| `simantics/VM.h`, `engine/VMThread.cpp` | `VM::OnSimanticsException` diagnostic hook. Counts every reset, including those whose dialog is suppressed by `DialogCooldown`. |

## Neighbourhood loading (goal 1)
- `InitSpecific(id)` reads the user data folder. It uses `UserData<id+1>` if that folder exists, and otherwise
  `UserData`. FreeSims always used `UserData/Neighborhood.iff`, but its `GetHouse` path left out `UserData/`.
  `UserPath` here is the user data folder itself.
- `Neighborhood.iff` provides:
  - `NBRS`: 33 valid neighbour records.
  - `NGBH`: neighbourhood data, `[0 1 7 0 …]` = level complete, tutorial progress, tutorial house 7, …
  - `TATT`: type attributes.
  - `FAMI`/`FAMs`: six families (Default, Newbie (house 7), Bachelor, Roomies, Pleasant, Goth (house 5)).
  - `FamilyForHouse` = every FAMI with `HouseNumber > 0`.
- Zoning comes from `UserData/LotZoning.iff`, or else `GameData/MasterLotZoning.iff` (STR#1 "N, residential|community"
  → 0/1). Lot map positions come from `GameData/LotLocations.iff` (used by Closest House).
- Characters are loaded from `UserData/Characters/<NBRS.Name>.iff`. The neighbour name is the file name, so no
  directory scan is needed, which matters because bare newlib has no dirent. Each OBJD is registered with
  `WorldObjects::AddObject` as a standalone iff. There are 20 characters. NPCs (maid, etc.) are already in
  Objects.far.
- The neighbour's person data uses TS1 indices (Behavior.iff STR#200): 31 neighbour id, 56/57 job type/level,
  58 age, 60 skin, **61 family number** (= FAMI chunk id), 65 gender.

## VM scopes (goal 2)
The stack object id can hold a neighbour id. "Set to Next neighbor" writes a neighbour id to the stack object
id. FreeSims kept only the entity pointer, so the id read back as 0 and neighbour loops never ended.
`VMStackFrame::StackObjectRawID` keeps the raw value; it is copied into subroutine and routing frames, and
`StackObjectID()` returns the entity's id or the raw id. Limitation: a primitive that sets `StackObject = nullptr`
directly, instead of through the scope, leaves the old raw id in place.

| Scope | TS1 meaning | Implementation |
|---|---|---|
| 10 Stack Object ID | get returns the raw id when it names no object | TS1 only |
| 12/28/29 check tree ad range / personality var / min | check trees adjust interaction ads | stored per thread (`VMThread::MotiveAdChanges`), default 0. **Not yet used by VMFindBestAction's score.** FreeSims threw, which reset Sims on every social/dance check. |
| 24 neighbor in stack object | STR#221 | 0: object id of that neighbour on the lot (or 0). 1: lives on this lot (house == global 10). 2: age (pd 58). 3/4: relationship of the caller's neighbour to it (unused by the data). 5: friends (relationships ≥ 50). 6: house of its family. 7: has telephone (= lives in a house; *inference*). 8: has baby (0). 9: its family's FamilyFriends. Setting it still throws (no BHAV sets it). |
| 32 neighbor's person data | person data of the neighbour in the stack object | the live avatar's data if the neighbour is on the lot, otherwise the NBRS record. Set writes both. FreeSims' commented code used `Caller.ObjectID`, but the data show the stack object. |
| 33 job data[temp0,temp1] | STR#243 | `TS1JobProvider::GetJobData` (empty without Work.iff → 0) |
| 34 neighborhood data | NGBH.NeighborhoodData[0..15] | get/set |
| 36/37 my / stack obj's type attr | TATT[type attr GUID][data] | type attr GUID = OBJD fields 92/93 (RawData[90..91]). If 0, the object's own GUID is used (*inference*). Set creates a 32-entry record (C# SetTATT). |
| 38 Neighbor's Object Definition | OBJD var of the neighbour's GUID | `GetEntityDefinitionVar` |

### Set To Next (TS1, `VMSetToNext.cpp`)
These run before, and independently of, the entity loop. In the C# they sat inside it, and were never reached
when no lot object id was greater than the target.
- **5 NeighborId**: `SetToNext(current)` (C# provider code that FreeSims commented out).
- **7 NeighborOfType**: `SetToNext(current, GUID)`.
- **10 Career**: next CARR id. With no Work.iff, returns false. This alone fixes Newspaper/Doll House/JobFinder
  'set random job', which looped until the 500k-primitive guard.
- **11 ClosestHouse**: next residential house by distance from global 10 on the LotLocations map.
- **12 FamilyMember**: next avatar on the lot with the same family number (pd 61) as the target object, or the
  caller's if the target has none. Wraps around to the lowest id, because the shipped trees start at an object
  and stop when they get back to it.

### Relationships (`VMRelationship.cpp`)
- Bit 1 (0x02) of the new operand means "ids are neighbour ids". FreeSims tested 0x08, which no shipped BHAV
  sets. The shipped flags are 0x00–0x06, and 0x02/0x03/0x06 appear exactly in neighbour loops: PhoneGlobals
  'get nth friend', 'adjust rel for talk', PersonGlobals 'init relationships', and MailBox 'Make me know everyone'.
- With neighbour ids, "me" is the caller's neighbour id (pd 31) and the stack object is `StackObjectID()`.
- Two neighbours on the lot (object ids) also use their NBRS relationship, so on-lot and phone relationships are
  the same data (Bob→Betty = [55, 0]). Everything else uses `MeToObject` as before.

## Generic Sims Call, TS1 (goal 3)
Labels come from Behavior.iff STR#220. Usage was found by scanning the shipped BHAVs.

| Mode | Semantics implemented | Evidence / notes |
|---|---|---|
| 4 add to family | the person in the stack object (an avatar, or a neighbour id) joins the lot's family (`GameState.ActiveFamily`, else FAMI #global 9): FAMI GUID lists and pd 61 in the record and on the avatar | 'do move in' calls it with the stack object id = neighbour id; Baby 'create child'; Tutorial 'add Betty' |
| 5 combine assets of family in temp 0 | that family's Budget/NetWorth are added to the lot family, the old FAMI is emptied, and globals 13/14 are updated | 'do move in' after its members are added |
| 6 remove from family | the person moves to family 0 | 'kill person for good', 'do kick out', 'check for military school' (the stack object id is the leaver's neighbour id) |
| 7 make new neighbour ("depracated") | creates an NBRS record for the stack object avatar | no shipped caller |
| 25 save this Sim's persistent data | avatar person data → NBRS record | |
| 28 zoning of lot temp0 | ZoningDictionary (0 residential, 1 community) | 'Is lot a Residential Lot?' |
| 33 add to family in temp 0 | the stack object person moves to family temp 0 | Cat/Dog 'Action - Run Away' |

Not determined, so these still return true: 3 (taxi dialog), 8–11 (tutorial complete, build/buy enable),
15–16, and the 17+ expansion calls (downtown/vacation spawns, suit table calls 29–31, fame 34/36, and others).
Mode 24 in TS1 is "Hot Date Change Suits Permanently". Primitive 19 "Make New Character" (Baby, MailBox debug
pet) is not registered, so it acts as true.

## Sims on the lot (goal 4): `VMTS1FamilyLoader`
- `LoadFamily(house[, FAMI])` runs after `VMWorldActivator::LoadFromIff`:
  - A house the family lives in already lists its Sims in OBJT/OBJM. With the characters registered, the
    activator creates them, out of world, because the OBJM heuristic has no person positions. They are reused.
    Missing members are created with `CreateObjectInstance(guid)`.
  - Out-of-world members are placed with the translated `VMFindLocationFor` next to the mailbox. This follows
    FreeSims `CreateAvatar`, which used mailbox GUIDs 0xEF121974/0x1D95C9B0/0x74E8A2A9/0x865A6812. The fallbacks
    are a portal, then the lot centre.
  - Out-of-world non-family persons from the house file are deleted.
  - TS1 globals are restored: 9 Current Family, **10 Current House** (FreeSims' activator sets it to 1), 13/14
    budget mod/div 10000, and 3 selected person.
  - "Move in": passing a family-bin FAMI sets its HouseNumber.
- `VMAvatar::Init` takes a TS1 path (similar to FreeSO's `InheritNeighbor`):
  - body strings, gender/age/skin, walk tables and default motives are set **before** the person's init tree;
  - the neighbour record's persistent person data (0..79, except posture) and pd 31 are copied in;
  - the name comes from CTSS #CatalogStringsID string 0 ("Bob");
  - FreeSims' TSO test values (skills 1000, pd 61 = 100, neat 1000, …) are **not** applied.
- **Appearance for the body agent: `VMAvatar::TS1Appearance`** (`model/VMTS1Appearance.h`). It holds AgeType,
  Gender, Age, SkinTone, Body/Head/NakedBody/Underwear/FormalBody as `{Mesh, Binding, Texture}`
  (e.g. `b002mafat_01` / `BODY` / `B002MAFatlgt_slob`; head binding `HEAD-HEAD`), head/hand bone names,
  6 hands, and `Raw` (all body strings). It is filled in Init. TS1 `Change Suit (update)` from STR#304 updates
  `TS1Appearance.Body`; it used to throw on the hex parse. FreeSims' `BodyOutfit/HeadOutfit` remain random TSO ids.
- Children: walk/swim tables overlay the global `StdChild` (151) / `PanicChild` (160) entries onto the adult
  ones. `VMMemory::GetAnimation` swaps `a2…` for the `c2…` animation when the caller is a child and that
  animation exists.

## Autonomy plumbing (needed for goal 5)
- The TS1 person tree 'try autonomy' calls **primitive 3 "Find Best Interaction"**, then **primitive 30 "Gosub
  Found Action"**. FreeSims registers neither: 3 is unregistered (acts as true) and 30 is TSO syslog. Now
  3 → translated `VMFindBestAction`, and 30 → `VMGosubFoundAction`. The latter hands control to the queued
  interaction through `AttemptPush` (like Idle for Input allow-push); when that tree returns, main continues at
  the true/false branch.
- Found while running Sims:
  - **FIX** `VMMemory` "my motives" (14) was gated on the *stack object* being an avatar, which is a FreeSims
    copy-paste bug. Sims never finished showers or toilets ('tweak shower finished': my hygiene ≥ …).
  - **FIX** `VMThread::HandleResult` dereferenced a null instruction when a subroutine returned into a routing
    frame (crash).
  - **FIX** `VMRoutingFrame::PlayAnim` with a missing animation now throws (C#: NullReference → reset) instead
    of crashing.
  - **TS1** `VMMotiveChange::ScaleRate` (translated but never called in FreeSims) is applied in
    `VMAvatar::SetMotiveChange`. Without it, energy in bed rose about 2/hour.
  - **TS1** motive changes are not clamped back to their max after the step that passes it. The bed sets energy
    max 90 and wakes on energy > 90, so with the clamp Sims slept until hunger/bladder failure.

## simlife results (2 game days each, 863,700 VM updates)

| House | Family | Sims | Interactions started | Resets |
|---|---|---|---|---|
| 7 | Newbie (lives there) | Bob, Betty | 480 / 523 | 0 |
| 5 | Goth (lives there) | Mortimer, Bella, Cassandra | 758 / 776 / 644 | 15 (14× Cassandra Toy Box 'Play' → global 'Exit fun object' infinite loop, 1× Bella null reference) |
| 2 | Pleasant (moved in) | Jeff, Diane, Daniel, Jennifer | 62 / 168 / 135 / 45 | 0 |
| 6 | Bachelor (moved in) | Michael | 89 | 0 |
| 3 | Roomies (moved in) | Chris, Melissa | 9 / 40 | 0 |

`simrun` on the houses without families (1, 2, 3, 4, 6, 8, 9, 10; 20,000 updates) shows **0 resets**. Before this
pass, Newspaper and Doll House 'set random job' reset once per lot. House00 still fails in the activator
("missing ARRY#10"); that is not part of this pass.

## Known gaps
- No Work.iff in the install, so there are no careers: Career Set To Next is false and job data is 0. The
  car pool and school bus still come, driven by person data.
- VMFindBestAction is FreeSims' TSO-style scorer. It ignores the TS1 check-tree ad adjustments (scopes 12/28/29)
  and the TS1 curves in Global.iff STR#501–505. Sims retry failing interactions many times ("Sleep x331",
  "Clean Up x538"), and occasionally get stuck after a failed route. TS1 attenuation/memory of failed actions
  is not ported.
- Child animations missing from Animation.far make Animate Sim return true without waiting, as in the C#. The
  Toy Box 'Play' loop then spins without yielding, which is the Cassandra resets.
- Saved Sim state in the house file (OBJM person data, motives, positions) is not decoded, because the OBJM
  reader is a heuristic. Sims start at the mailbox with init-tree motives.
- Not runtime-tested: GSC 4/5/6/7/25/33 (no shipped tree reached them during these runs), and neighbour scope
  sets. Neighbourhood saving (`SaveNeighbourhood`/`SaveHouse`) is translated but never called; simlife does not
  write game data.

> **Update (autonomy pass):** the gaps above about VMFindBestAction scoring, retry loops and undecoded OBJM saved
> state are addressed in `STUBS_autonomy.md` (FreeSO TS1 autonomy and queue model, 30 ticks per game minute, lockout
> memory, decoded OBJM with restored object/Sim state). The Toy Box resets are gone (missing animations now wait a tick).

> Update (expansion pass): `work.iff` is read from ExpansionShared.far (21 careers), Generic Sims Calls 17+ and
> Manage Inventory are implemented - see `STUBS_expansions.md`.
