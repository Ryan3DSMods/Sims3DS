# The Sims expansion packs (Livin' Large .. Makin' Magic, Deluxe, ExpansionShared, Downloads)

This pass makes the Complete Collection data (`gamedata_full`: GameData + ExpansionPack..7 + ExpansionShared +
Deluxe + Downloads + Template*) run in the engine. New code is marked `// TS1(sims)`. Code translated from
FreeSO (FreeSims' MPL-2.0 successor, which has the TS1 parts FreeSims lacks) is said so at the top of each file.
Semantics without a FreeSO reference come from the shipped BHAVs and the Edith label tables in `GameData/Behavior.iff`
(STR#220 'generic sim calls', #217 'dialog types', #252 'Game Edition', #129 'global labels', #200 'Person Data').

Test tool: `build-port/simep` (`make -f Makefile.port build-port/simep`):
```
simep <dataRoot> <cacheDir> [--ticks N] [--family] [--quiet] House.iff ...   # per lot: missing objects, unresolved
                                                                             # walls/floors, resets with BHAV stack,
                                                                             # unimplemented primitives, GSC/inventory modes run
simep <dataRoot> <cacheDir> --travel <homeHouse> [--ticks N] Lot.iff ...     # home -> lot -> home through VMTS1Travel
```
Debug env: `SIMEP_TRACE=<routine hex|FFFF> SIMEP_TRACENAME=<entity> SIMEP_TRACEOP=<opcode> SIMEP_CREATE=<guid>`.

## Content (src/content)
| What | Where |
|---|---|
| `ExpansionShared.far` loaded with the object archives (plumb bobs, Hot Date socials HDSI_*, pet/magic/fame socials, drinks, help systems, **work.iff**). Order: Objects.far, ExpansionPack..7, ExpansionShared, Deluxe, Downloads/**.far. No GUID is defined twice in the install, so the order only matters for file names. | ContentManager.cpp |
| GUID index cache is now `STOC2` (extra flags column: global sim object / person). Old caches are rebuilt once. | ContentManager.cpp |
| `WorldObjectProvider::ControllerObjects` / `PersonGUIDs` (FreeSO TS1ObjectProvider): every OBJD with Global == 1 that is not a gift token (71 objects). | ContentManager.{h,cpp} |
| Careers: `work.iff` read from the archives (ExpansionShared.far: 21 careers incl. the expansion ones). | TS1JobProvider.cpp |
| Expansion walls/floors inside the archives (`WallsAndFloors\*.wll/.flr`, 457 files, 29 MB): indexed, catalog strings cached in `<cache>/wallfloorwll.cache` / `wallfloorflr.cache`, each iff read only when first drawn. 272 custom walls / 208 floors; every wall/floor referenced by the 69 template lots resolves. | TS1ExpansionContent.{h,cpp}, WorldWallProvider.cpp, WorldFloorProvider.cpp |
| Templates: a house / LotLocations / StreetNames / *Desc file missing from the user folder is read from the newest `Template*` folder (MagicTown, Studiotown, Community, Vacation, Downtown, UserData) - what the installers copy into each neighbourhood. Zoning = MasterLotZoning + templates + user (later wins); unknown lots are community (Studio Town 2) like FreeSO. Lot names: DTDesc (21-39), VIDesc (40-49). `LotMaps` = every LotLocations map (1 original, 2 downtown, 3 vacation, 4 Old Town, 5 Studio Town, 7 Magic Town). | TS1NeighbourProvider.{h,cpp} |
| Hot Date townies `TemplateNPCs/*.iff` registered as objects. | TS1NeighbourProvider.cpp |
| Neighbour inventories (NGBH, Hot Date+): `GetInventoryByNID / InitInventoryForNID / GetMagicoinsForNeighbor`; an empty NGBH is created for pre-Hot Date neighbourhoods. | TS1NeighbourProvider.{h,cpp} |
| Avatar animations: every `AnimationOut.ndx` (one per EP folder: Animations\, Skins\, Accessories\ ...) is read; before only the first one was. No-dirent builds also probe the EP fars. | vitaboy/content/TS1AvatarProvider.{h,cpp} |
| `OBJDType::GiftToken = 12` (FreeSO). | files/.../OBJD.h |

The GameData `Global.far` of the Complete Collection already contains the expansion globals/semi-globals (Cat/Dog/
Dragon/Fame/Vacation*... 24 iffs) - the installers replaced it - so nothing is merged there.

## Engine (src/simantics)
| What | Where |
|---|---|
| Primitive 51 **Manage Inventory** (FreeSO VMTS1InventoryOperations; FreeSims left 51 unregistered = silently true, 3917 uses in the data). FIX: the autofollow/follow-home slots (10/11) are looked up and replaced correctly. | primitives/VMTS1InventoryOperations.{h,cpp} |
| Primitive 25 **Alter Budget** (FreeSO VMTS1Budget) on globals 13/14 + FAMI.Budget; refuses overdrafts. TSO transfer funds kept for non-TS1 VMs. | primitives/VMTS1Budget.{h,cpp} |
| 10 Tutorial / 34 UI Effect: no-ops answering true. Registration: `RegisterTS1ExpansionPrimitives` (VMContext ctor). | primitives/VMTS1Primitives.cpp, VMContext.{h,cpp} |
| **Generic Sims Call** TS1 modes (all of STR#220 except 4-7/25/28/33, which stay in VMGenericTSOCall): 3 pull down taxi dialog, 10/11 build/buy off/on (`VM::TS1BuildBuyEnabled` + event), 17 change to lot, 18 build the downtown Sim, 19/20 spawn date / take-back-home date, 21 sim data effects, 22, 23 downtown time (implemented; FreeSO left it empty), 26 vacation family, 27, 32 purchased pets, 34/36 **fame promote/demote** (new, from GameData/fame.iff STR#4/5/6: score pd80, star level pd81, high watermark pd82), 35, 37 cancel pie menu (event), 38 **get tokens from string** (new: Makin' Magic recipes), 41 head flush (event), 42 make temp0 selected Sim. Stubs: 24/29/30/31 suits (index 0 / count 1), 39/40 child/pet to adult, 43 family spells into controller (true). | primitives/VMGenericTS1Call.{h,cpp}, VMGenericTSOCall.cpp |
| **Travel** between lots (see `engine/VMTS1Travel.h` for the front-end API). | engine/VMTS1Travel.{h,cpp}, VM.h (`OnRequestLotSwitch`, `OnTS1Event`) |
| TS1 dialog types 5-15 (downtown/vacation/neighbourhood/studio/magic lot selection, phone book, pet choice, clothes): numeric answer in temp 0 (FreeSO). `VMTS1Travel::LotMapForDialog(type)` says which map to show. | engine/VMDialogOperand.h, primitives/VMDialogPrivateStrings.cpp |
| Global sim objects created on every lot (FreeSO VMTS1Activator), except Unleashed's community-only controllers on residential lots (Village, NPCs, Pet Show, Neighborhood ped marker: never present in any saved residential lot; the trolley they bring idles forever on a residential lot and blocked the school bus). | utils/VMWorldActivator.{h,cpp} |
| FIX globals: 38 TS1 globals (33 machine level ... 37 transit filter), bounds by array (FreeSims capped at 32). Global 10 keeps the house number saved in the lot; 3/9 reset to 0 (FreeSO); 33 = `VMWorldActivator::MachineLevel` (2, FreeSO). | VM.cpp, VMWorldActivator.cpp |
| FIX Remove Object Instance on itself yields (FreeSO); Sleep: dead caller yields (FreeSO), counts below -1000 are long unsigned waits (expansion controllers sleep "-29536" = 36000 ticks); Set To Next "object on same tile" of nothing = false (FreeSO "verified in ts1"); FreeSims' "bad object" list that deleted the Vacation wagon every tick removed. | VMRemoveObjectInstance.cpp, VMSleep.cpp, VMSetToNext.cpp, VMEntity.cpp |

## Results (simep, 3000 ticks per lot)
* 69 template lots (Downtown 21-30, Vacation 40-48, Old Town 50-80, Studio Town 81-89, Magic Town 90-99): 0 objects
  missing except the 5 Old Town homes that the Unleashed installer fills from `TemplateFamilyUnleashed/*.FAM` (65 refs:
  those families' Sims/pets and their painted paintings/seeds; .FAM import is not implemented). Unimplemented primitive
  uses fell from 1300-1500 per lot to 11-22 (9 Burn, 19 Make New Character, 21 Show String, 33 Find 5 Worst Motives).
* Resets with nobody visiting: Downtown/Studio/Magic/Vacation 0-14 per lot (was 2-113); some Old Town lots spin their
  sax player / organ grinder (they look for a non-NPC Sim that only exists when a family visits).
* `--travel 9` (UserData2's Mashuga family) to lots of all five expansion towns and back: the Sim arrives through the
  lot's own ped markers (GSC 18/26), keeps its motives (sim data tokens), comes home with them; 0-2 resets.
* Base lots (UserData2 houses 1-10, 6000 ticks): 0 resets, 0 missing.

## Known gaps
* Suit tables (GSC 24/29/30/31: Hot Date clothing racks, pets' coats at birth) need the TS1 clothing collections
  (FreeSO TS1BCFProvider.CollectionsByName) - stubbed.
* `.FAM` family import (Unleashed template families), MakeNewCharacter (19), Burn (9), Child/Pet to adult (39/40),
  family spells into controller (43).
* Saving lots/neighbourhood: the original resumes the home lot's saved VM after a trip; this port reloads the home
  lot from its objects, so `Arrive` replays the tail of the trip (motives, clock, follow-home date).
* Renderer: lot terrain types by house number (snow 40-42, sand 28/29/46-48, dark/autumn grass and clouds in Magic
  Town - FreeSO VMTS1Activator.HouseNumToType) are not applied; water on the snow lots draws as the missing pattern.
* Vacation desk clerks spin when their route fails instantly (routing never yields on failure).

## Second pass (TS1(sims))
| Item | Status | Files |
|---|---|---|
| Person suits (Change Suit, person scope): daywear, naked, swimwear, uniform (CARR job level meshes), formal, pyjamas, skeleton, bought formal/swim/pyjamas/winter/high-fashion (body strings 30..34), with the Sim's hands. Verified: Bella → pyjamas at bed time, Mortimer → science uniform at work, Bob → pyjamas. "Use temp" flag. Update flag registers the new daywear outfit. | done | content/TS1Clothing.{h,cpp}, engine/VMSuitProvider.cpp, primitives/VMChangeSuitOrAccessory.cpp |
| Clothing collections (FreeSO TS1BCFProvider) + GSC 24/29/30/31 (racks, dressers, pet coats); wardrobe writes body strings to the character file. API: VMTS1Outfits::ValidOutfits/SetSuit for the Clothing dialog (type 6). | done | engine/VMTS1Outfits.{h,cpp}, VMGenericTS1Call.cpp |
| Shared character writer: raw IFF + TemplatePerson/Dog/Cat clone + neighbour record; CASSimDesc/CASCatalog moved to content (app/cas headers forward). | done | content/TS1CharacterFiles.*, TS1SimDesc.*, TS1SkinCatalog.* |
| Make New Character (19): babies, clones, pets (coat = SO attr 8), toad prince. Create Object Instance "use neighbor"(4)/"no duplicate"(1). | done | primitives/VMTS1MakeNewCharacter.*, VMCreateObjectInstance.cpp |
| Burn (9), Show String (21), Find 5 Worst Motives (33); Clock.FirePercent. | done | primitives/VMTS1MiscPrimitives.*, VMClock.h |
| GSC 39 child→adult (in place, adult skeleton/outfit, character file + NBRS), 40 pet→adult, 43 family spells (Magic Controller attrs 0..5, kept per family in TS1GameState::FamilySpells; FIX(sims): saved in Neighborhood.iff as the private chunk FSPL by TS1NeighbourhoodWriter and applied once the family is active - the original's storage is unknown: Sims.exe is SafeDisc-encrypted, no Makin' Magic save with spells in the data, FreeSO leaves GSC 43 empty; FAMI v9's 4-dword tail is a candidate), 0 house tutorial complete (global 26 = 0). | done | VMGenericTS1Call.cpp, VMTS1Outfits.cpp, VMTS1Travel.cpp |
| .FAM import (TemplateFamilyUnleashed): TS1FamilyImport::Read / Import / InstallTemplates. Old Town 56/63/69/76/77 now load with 0 missing objects. | done | content/TS1FamilyImport.* |
| Fixes: family budget set on arrival (ActivateFamily: globals 13/14); phone plugins found (Set To Next by category, Run Tree By Name in the running (check) thread) → "Go Downtown/Vacation/Studio/Magic/Old Town" on the phone; kat skeleton for Unleashed cats; check trees stop after 20000 primitives (false) instead of resetting the Sim; a route never ends in the tick it started (desk clerks). | done | VMTS1Travel.cpp, VMSetToNext.cpp, VMRunTreeByName.cpp, VMAvatar.cpp, VMThread.cpp |
| Remaining: Old Town sax player / organ grinder loops when no visitor (house 61/71-75). FAMI version: fixed (written as read, see STUBS_save.md). | parked | |

## EP verification pass (26 Oct, `// FIX(sims)`, see TEST_REPORT.md 5.5-5.9)
- Global controllers are created after the lot's objects (VMWorldActivator): Vacation Director placement.
- Travellers wear person data 8 on arrival (VMTS1Travel::WearCurrentOutfit): winter/swim outfits from 'Dress Appropriately'.
- "Spawn Vacation Purchases" pushed when the family is back home (VMTS1Travel::Arrive / PushTreeByLabel).
- `$TokenNameLocal:N` dialog token; spellbook dialog (type 12) UI; Superstar star meter in the job panel.
- Trips from home save house + neighbourhood first (app/front/AppFlow.cpp), as TS1 does.

## QA3 (26 Oct, `// FIX(sims) QA3`)
- Superstar townies: Sims.exe generates the fans (family 5500) and celebrities (5000) that Studio Town's controllers and
  cars bring; the port clones TemplateNPCs/SSCelebGeneric.iff (OBJD type 2048 = generator marker -> Person) on arrival
  on lots 81-89 up to 16 fans / 8 celebrities (VMTS1Travel::EnsureStudioTownies). Hot Date's HDT_Generic (512) and
  Makin' Magic's MMClownieGeneric (4096) are the same kind of template (generated since QA4, see below).
- Find Best Action (TS1): TTAB autonomy threshold >= 100 is never chosen by free will (CatGlobals' disabled dog entries).

## QA4 (26 Oct, `// FIX(sims) QA4`) — generated townies of every town
What the data says (TemplateNPCs, GameData/Live.iff, the controllers' visitor trees); the generator itself is in the
SafeDisc-encrypted Sims.exe, so the counts and the moment of creation are this port's choice.

| Town | Who brings them (BHAV) | Family number | Template (OBJD type) | Names (Live.iff STR#) | Port creates (once per neighbourhood, topped up on arrival, saved with it) |
|---|---|---|---|---|---|
| Downtown 21-39 | PedMarkers 'Can this person visit?': adults living in a house **or** family 2000; Unleashed CarVisitorTrolley 'can visit?' also takes 2000 | 2000 (Global.iff BCON 0x10F 'Special Family Numbers' #0) | HDT_<Bombshell/DonJuan/Fatale/LoungeL/Party/WallFlower>_<F/M><D/L/M> (1024: own outfit, fixed personality in 'Main', interests + job 15/3 in 'init tree' when family == 2000); HDT_Generic (512: random personality/job for any family >= 2000) | 150/151 'Datable NPC Name - Male/Female' | 20: one archetype of each kind per gender (12, random skin tone, the archetype's own body strings) + 8 HDT_Generic with a random CAS look |
| Vacation 40-49 | VacationPedMarker 'Create a Tourist Family' brings a whole family 3000 + 1..5 - 1; mailbox/phone/shuttle van accept 3000..3009 | 3000..3004 | HDT_Generic (no tourist template exists; its 'init tree' and PersonGlobals 'Init NPC Jobs' handle 3000..3010) | 160 family names, 161/162 adults, 163/164 children | 5 families of 1-2 adults + 0-2 children (max 4) |
| Old Town 50-80 | CarVisitorTrolley 'can visit?': 2000 and 3000..3009 | — | — | — | the Downtown and Vacation sets above |
| Studio Town 81-89 | see QA3 | 5500 fans / 5000 celebrities | SSCelebGeneric (2048) | 167/168 fans, 165/166 celebrities (QA4: were invented names) | 16 fans + 8 celebrities |
| Magic Town 90-99 | PedMarkersMagic 'Can this person visit?': adults in a house **or** family 6000; ClownPortal 'Only Townies' | 6000 | MMClownieGeneric (4096: MagiCoins/ingredients, job, skills in 'init tree' for 6000) | 181/182 'Magic Town Male/Female First Names' | 16 |

Code: `VMTS1Travel::EnsureTownies(vm, house)` (called from Arrive for every expansion lot) -> EnsureDowntownies /
EnsureTourists / EnsureStudioTownies / EnsureMagicTownies, sharing one helper (TownieMaker in VMTS1Travel.cpp: CAS look,
personality, Hot Date wardrobe, character file via TS1CharacterFiles::WriteCharacter with ForcePerson, NBRS record via
AddNeighbour). The install already copies TemplateNPCs/*.iff into each UserData*/Characters (as the installers do); the
generated townies are separate User#####.iff clones.

Engine fixes found while running the busier towns (all `// FIX(sims) QA4`):
- SetToNextCache::RemoveObject also leaves the category list (FreeSO). Deleted meals stayed in 'category = SP0' searches
  (CounterGlobals 'food slot test': 36 busboy resets per Downtown 24 run; bar 'Is counter attached to bar?' loops).
- VMEntity::SetValue(Flags) stores the flags before rebuilding the footprint (FreeSO order); was inverted for 'has zero extent'.
- VMEntity::Delete: contained/held objects go out of world, unhide, then Find Location For next to us (FreeSO);
  a held meal kept a deleted Sim as container ('held by person' reset the janitor).
- VMArchitecture::RaycastWall: a line exactly through a tile corner is blocked only when both detours are (the
  Unleashed rabbit's diagonal snap back to its own tile failed one way and hit Maxis' breakpoint).
- Relationship with a missing object answers true (FreeSO; Hot Date 'Gossip A' / 'Brag' after the partner left).
- Reach / Find Location For with a deleted stack object answer false in TS1 (port policy; FreeSO would reset).
- Notify Out Of Idle on a missing object (segfault -> FreeSO null check); Drop Onto, Look Towards, Run Tree By Name and
  Burn throw a SimAntics error instead of dereferencing a null stack object.

## Release check H1 (26 Oct, `// FIX(sims) H1`)
- Unleashed community-only controllers (Village, NPCs, Pet Show, Neighborhood ped marker) only on community lots (zoning
  1), per their load tree 'Allow On Community Only'; Studio Town (zoning 2) no longer gets idle-forever visitor trolleys.
- Snap: primitive result 2 on LocationOutOfBounds / no valid goals (FreeSO) - cars' 'drive away' ends at the lot edge.
- `VMTS1Travel::KeepSelectedSim` (every tick): global 3 names a living Sim of the family, or 0.
