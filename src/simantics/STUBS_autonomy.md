# The Sims 1 autonomy, queue and saved-state pass

This pass replaces FreeSims' TSO-era autonomy with The Sims 1 behaviour. Wherever C# exists, it is translated from
**FreeSO** (riperiperi/FreeSO, MPL-2.0, FreeSims' successor with real TS1 support via Simitone). New code is marked
`// TS1(sims): ...`, and fixes to translated code are marked `// FIX(sims): ...`. Where FreeSO also has
no answer, the behaviour is inferred from the game's own data: Behavior.iff label tables and the Global.far/Objects.far
BHAVs, read with a disassembler. Each inference below says so.

## 1. Why Sims retried failing interactions (and what changed)

Tracing (`simlife --why/--trace/--ads`, see the end of this file) found these causes. They are listed in order of
impact.

| Cause | Where | Fix |
|---|---|---|
| **Wrong tick rate.** FreeSims' `VMClock::Tick` forced `TicksPerMinute = 150` (TSO) on every tick. TS1 runs 30 ticks per game minute: global 'idle minutes' multiplies by 30 and 'idle hours' idles 1800. Animations, routes and idle loops ran 5× too fast in game time. This is why the old logs showed e.g. 19 meals in 2 days. | `VMClock.h`, `VMMotiveChange.cpp` | Uses the context's 30 (FreeSO). Motive change rate is per 30-tick minute for TS1. |
| **FreeSims queue model.** Only `Queue[0]` could run. An action tree could not run a nested interaction: TS1 social responses, "Be Talked To", and parent idle/exit were affected. `Priority` person data was aliased to `Queue[0]`. Pushed interactions were tagged `MustRun`, which in TS1 data is bit 10 = *allow dogs*, so permission checks were applied wrongly. | `VMThread`, `VMIdleForInput`, `VMPushInteraction`, `VMAvatar`, `VMEntity` | Translated FreeSO's TS1 queue: `ActiveQueueBlock` (-1 = main) and `ActiveAction()`. Also translated: `AttemptPush` (priority compare against person data 33), `EndCurrentInteraction`, `AbortCurrentInteraction`, `EvaluateQueuePriorities` (implied "Cancel Interaction" → notify idle), `EnqueueAction` (TS1 queue skip, no leapfrog), `CancelAction` (TS1 entry point 4 "queue skipped"), `CheckTS1Action` (TS1 TTAB flags: pets, `TS1NoChild/NoAdult`, visitors, debug), the check tree's `auto` parameter, `SpecialResult` frames, `FSOSkipPermissions`, and `UseTemp0`. Idle For Input with Allow Push now also works inside action trees (TS1). |
| **No failed-action memory.** TS1 data uses the object's **lockout count** (object data 25). Global 'Standard Exit' := param, global 'fail food chain object' := 200, and food init := 50, ... The engine counts it down every tick, which FreeSims already did. Nothing read it in autonomy (FreeSO: "TODO: Lockout interactions"). | `VMFindBestAction` | Find Best Interaction skips objects with lockout > 0. **Inference:** the field's name, its tick-scale values, and FreeSO's Find Best Object For Function using it the same way. |
| **Failed autonomous routes were re-picked every tick.** The TS1 route-failure tree (global 398) returns at once for priority ≤ 25 and marks nothing. | `VMRoutingFrame::HardFail` | **Inference:** the target of the failed *autonomous* interaction gets lockout 200, the game's own failure value. This cut the stuck-in-doorway retries from "every tick" to "once per 6.7 game minutes per object". |
| **Goals in doorways / on top of other Sims.** Two Sims danced in the front door of house 7. Each one was then the only route of the other: a deadlock that lasted the whole run. | `VMSlotParser`, `VMRoutingFrame` | FreeSO: goals on `ArchitectualDoor` tiles, under an avatar, or in another Sim's route destination get score/100000 (`IntersectsOurDestination`). |
| **Shoo tree ran on the wrong object.** 'Move out of way' ran with the blocked route's callee as stack object. The result was a null ref or missing key, and the blocker was reset. | `VMRoutingFrame` (FIX) | FreeSO: the shoo frame's callee and stack object are the Destination object. |
| **Missing animation returned true immediately.** Child Toy Box 'Play' then spun 500k primitives in one tick. This caused 46 Cassandra resets per 2 days. | `VMAnimateSim` | FreeSO: `GOTO_TRUE_NEXT_TICK`. Also from FreeSO: TS1 animation speed 30/25, `Hurryable` ×2, `IDFromParam`, mode 1 (loop, don't wait), and the carry reset. |
| **Mourn everywhere (house 5).** Tombstone attribute 4 (hours since death) was not loaded, so every grave was "fresh". | see §3 | Saved state restored. |
| **Passed-out Sims never woke up.** 'failure sleep' sets sleep state -50 and loops until energy > 0. FreeSO only restores energy for sleep state -1. | `VMTS1MotiveDecay` | FreeSO's TS1 decay was translated: wake at the wake hour when rested, and negative fractions. **Inference:** any negative sleep state restores energy at FreeSO's rate (-1286/1000 per 2 min). |
| Check-tree exceptions reset the Sim and the object. | `VMThread::Tick` | FreeSO: a check thread just clears its stack. |
| TS1 interactions with a missing check tree were offered anyway. | `VMEntity::GetAction` | FreeSO, "VERIFIED: TS1 ignores interactions with non-zero missing check trees". |
| "Use count" (object data 62) is engine-computed in TS1. No tree writes it, but SocialInteractions, Beds 'Make Bed', Food, Toilets ... read it. | `VMEntity::GetValue/UseCount` | FreeSO `GetUsers`: avatars whose queue up to the active block targets the object. |
| Error branch 253. | `VMThread::MoveToInstruction` | FreeSO: follow the only other branch, and pop with an error only if both branches are 253. |
| Gosub Found Action. | `VMGosubFoundAction` | FreeSO: when the found interaction ends, the primitive runs again next tick. FreeSO's notes on TS1 saves say "Gosub found action and idle for input will ignore results from interactions". An earlier experiment in this pass that passed the result through was reverted. |

**Names.** `VMQueuedAction::Cancelled` keeps its FreeSims name, as PORTING.md requires. It briefly became FreeSO's
`NotifyIdle` and was renamed back. It now has FreeSO's semantics: it is set by `EvaluateQueuePriorities` and
`CancelAction`, and read by Idle For Input. `VMStackFrame::DiscardResult` (bool) was replaced by FreeSO's
`VMSpecialResult SpecialResult` {Normal, Interaction, Retry}, because the queue model needs all three values.
`VMThread::ActiveQueueBlock` is now `int8_t` = -1 (FreeSO `sbyte`).

**Delay before a queued (user) action starts.** `simlife --push <min> <obj> <interaction>` measures it:

| Situation | Delay |
|---|---|
| Idle | 1–48 ticks (0.0–1.6 s at 30 ticks/s) |
| Inside an autonomous interaction | 76–324 ticks (2.5–10.8 s) |

Inside an interaction, the pushed action raises the queue above the active interaction's priority, and the active
one gets the implied cancel (`Cancelled`). The Sim then finishes its current animation step, such as getting food
out of the fridge, until the tree reaches its next Idle For Input / Wait For Notify. That is TS1 behaviour (the X on
the running interaction). The old 150-ticks-per-minute clock made in-game waits look shorter.

## 2. TS1 autonomy scoring (`VMFindBestAction::ExecuteTS1`)

This is a translation of FreeSO's `VMFindBestAction` (its own header comment is copied into the source). It
replaces FreeSims' placeholder for TS1 (`VM::TS1`). The TSO path is unchanged.

- **Curves:** Global.iff STR#501 InteractionScore, #502 HappyWeight, #503/#504 child versions, and #505 HouseScore.
  `TS1Curve` (`src/common/TS1/`) holds the piecewise-linear "(x;y) (x;y)" lists, sorted by x and with a cached
  -100..100 range. They are loaded by `WorldGlobalProvider::InitCurves()`.
- **Motive mapping** is `WeightMotives` = Energy, Comfort, Hunger, Hygiene, Bladder, Mood, Room, Social, Fun, after
  FreeSO. It is consistent with the data: curve 0 is flat at 90 above -72, which is FreeSO's "prevents people from
  considering sleeping at 50% energy". With 180 points of energy over 16 waking hours, that gives a ~22:00 bedtime.
- **Happy:** happy = Σ curve(motive) · w, with all weights equal. FreeSO's "new style" HappyWeight weighting is left as
  a TODO there too, because the mapping of the 7 curves to motives is undetermined. For each auto interaction (TTAB
  entries with a non-zero ad range):
  - the score replaces each advertised motive's contribution by curve(motive + range·personality/1000);
  - an ad with a non-zero minimum only applies while the motive is ≤ that minimum;
  - a range of 0 with a minimum is read as 0..min;
  - personality is person data `VaryByTypes[mod]`/1000; even modifiers below 13 are inverted, and skills are ×2;
  - attenuation is score/(1 + atten·distance), using TS1 attenuation {0, 0, .1, .3, .6} or the visitor table
    {0, 0, .01, .02, .03};
  - the minimum score is 1e-7, or 1e-6 while sitting (FCNS 2).
- **Check-tree ad changes** (scopes 12/28/29) are FreeSO `MotiveAdChanges`, keyed (type<<16)|motive. `GetTreeAd` falls
  back to the active interaction's TTAB entry. `EvaluateCheck` hands the changes to Find Best through the action
  strings.
  - **FIX:** C# `TryGetValue(key, out min)` zeroed every ad the tree had not changed. Only changed keys are applied
    now.
  - **FIX:** C# added the first action string N times. Each string is added once now, so a social's sub-options keep
    their own name/param.
- **Selection:** the top 4 objects not in use (or joinable), then a weighted random pick by score, unless the entry
  is `AutoFirst`. The pick is queued at Autonomous priority (2). The action string's name becomes the queued name.
- **Skipped:** out-of-world or disabled objects, objects with lockout > 0 (§1), occupied non-joinable objects, and,
  for stray pets, indoor targets.
- **Not ported (FreeSO TODOs too):** HappyWeight weighting, and special social logic.

## 3. Saved house state (OBJM) — decoded

FreeSO's `OBJM` reader is translated (`files/formats/iff/chunks/OBJM.*`). Its `Read` keeps each object's compressed
record (u32 skip offsets after the id→type table). `Prepare(typeId→OBJD/OBJT)` then decodes an `OBJMInstance` per
object:

- footprint, X/Y in 1/16-tile units, and level;
- attributes, 8 temps, object data (68 + 5 extra), and the saved stack frames;
- relationships, slots (the first slot is the tile's linked list), and dynamic sprite flags;
- the multitile block, and portal data;
- for persons: animation strings, motives (float ×16, old and current), 80 person data (256 for versions > 0x45), the
  active/last/queued interactions, object uses, and motive deltas.

`IffFieldEncode` also got FreeSO's float bit-cast (FreeSims converted the integer, "incredibly wrong") and the
odd-offset string padding. `TTABFieldEncode::ReadFloat` got the same float fix, which corrects custom attenuations.

- **Results:** all 10 houses decode (`WA_DEBUG=1`). Saved positions agree with the ARRY#3/#103 grid for 52/56,
  356/359, 247/247, 19/20, 89/90, 123/123, 53/53, 22/22 and 135/135 objects. The disagreements are grid entries in
  rows 61/62 (persons, portals), which hold the tile linked list, not positions. The previous "281/281 disagree"
  came from the FreeSims heuristic.
- **Restore** (`VMWorldActivator::RestoreSavedState`, run after FreeSims' create/place/user-placement flow):
  - every object takes its **saved object id**. `VM::ReservedObjectIDs` and `PendingIDsByGUID` are used by
    `AddEntity`/`NextObjID`, because attributes, relationships and containers refer to saved ids;
  - attributes are restored;
  - object data is restored except engine/transient fields: direction, container, slot, flags, ids, room, flag
    field 2, placement/movement flags, hide interaction, primitive result, use count, wall adjacency, slot count;
  - dynamic sprite flags are restored, then the graphic is refreshed;
  - relationships (MeToObject) are restored;
  - persons get their person data (except posture, outfit, gestures, tree-use/tick, priority, routing/head-seek,
    route result, swimming, non-interruptible, render/autofollow), their motives (sleep state cleared) and their exact
    saved position;
  - containment is put back into slots if user placement did not already do it.
- **Not restored:**
  - **Threads/stacks/queues.** Every object and Sim restarts its main tree. FreeSO restores the stacks through its
    marshal loader, which this port does not have.
  - **Flags (object data 8).** Restoring them detached doors and windows from cut-away walls in the renderer: the
    regression the 3DS-app agent reported, now fixed and checked with `simrender` House07.
  - **Interaction-only objects.** OBJD type 11 "Internal" (Destination), "*Social Interactions" and "*comeandsee"
    keep their fresh init state. A restored Destination kept Mortimer failing one door route every tick, because its
    saved attributes name a step that no longer runs.
- **Visible effects:**
  - lamps keep their saved on/off state, so House07 renders darker at night;
  - graves keep their age, so the Goths no longer mourn all day;
  - Sims start where they were saved, with their saved motives (Bob/Betty hunger 77, Mortimer comfort 53, ...).

`WA_NO_RESTORE=1` turns the restore off, for comparison.

## 4. Primitive 19 "Make New Character" — semantics, not implemented

FreeSO `VMTS1MakeNewCharacter` gives the semantics:

- **Operand:** bytes 0..3 = local holding skin colour (0 lgt / 1 med / 2 drk), local holding age, local holding
  gender, and avatar type (0 person, 1 dog, 2 cat).
- **Person:** picks a random head and body from the BCF collections for `m/f` + `c/m` (child bodies are `*chd`), with
  textures matching the skin. It fills a `SimTemplateCreateInfo` with personality 500 ×5 and the stack object's name.
- **Pet:** the outfit index comes from stack object attribute 8, with template GUIDs 0x4A70DF92 / 0x7BEA0977.
- It writes a new character IFF and NBRS record (Simitone's neighbour generator) and returns the new neighbour id in
  the stack object id.

It needs a character-file writer and GUID allocator that this port does not have. It stays unregistered, so it acts
as true, as before. Callers are Baby 'create child' and the MailBox debug pet.

## 5. Validation (simlife, 2 game days, seed 12345)

Logs and the analyser are in the scratchpad (`auto/logs`, `auto/tools/analyze.py`, `auto/tools/run.sh`).

**Before** (`before_h*.txt`). The clock ran 150 ticks/min.

| House | Starts | Resets | Notes |
|---|---|---|---|
| 7 | Bob 94, Betty 79 | — | Meals ~19 each, fun 12/16 snapshots < -50 |
| 5 | Mortimer 573 (Sleep ×477, Mourn ×38), Bella 507 (Sleep ×397), Cassandra 287 | 46 (Toy Box) | Social 14/16 snapshots < -50 |

**After** (`after_h*.txt`):

- **House 7:** Bob 144 and Betty 107 starts, 16–23 distinct interactions, 0 resets. About 9–10 meals each in 2 days,
  1–2 showers, 1 night of sleep. Hunger, bladder and hygiene never fell below -50 at the 3-hourly snapshots. The
  longest gap between interactions is 148–174 min. Comfort stays low (the cheap chairs cap at +20).
- **House 5:** 0 resets (was 46). No Mourn loops. Mortimer goes to work, Cassandra goes to school, and there are
  baths, cooking, painting and fish. In this seed the family ends day 2 in comfort/energy failure animations
  (comfort -100). See the remaining problems below.
- **Houses 2, 6, 3 (move-in):** 0 resets. House 2 has no kitchen or bath, so the Pleasants cannot eat. Houses 6 and 3
  are sparse (Michael eats 16× and reads).

## 6. Still missing / known problems

- **Comfort is chronically low.** Sims leave chairs as soon as another ad wins, and cheap chairs stop at +20. This
  follows from the faithful scoring, but TS1 Sims sit through more of their day (meals, TV, reading while seated).
  Not resolved.
- **Late in day 2, house 5 decays** into motive-failure animations ('check comfort'). FBI picks made during those
  states are queued and dropped. With up to 660 picks per 2 days for Bella, this is still pick spam, though no
  longer the per-tick re-pick of one failed route.
- **No socials chosen** by the married couples. 'Allow Autonomous?' requires the Hot Date preference person data
  75/76 to be > 0, and that only grows through user-directed romance. This is faithful to the data, but means the
  social need is met only slowly.
- **Child animations are missing** from Animation.far (vitaboy). The Toy Box no longer resets, but Cassandra's fun
  stays at -100.
- **Not restored from saves:** thread/queue state. Opcode 19 is not implemented.
- **Not ported:** FreeSO's HappyWeight "new style" weighting, and FreeSO's newer routing frame (AvatarsToConsider
  timeouts, portal re-evaluation).

## 7. Second pass (food, comfort, TV, socials, child animations)

Fixes in this pass:

- **Animation events >= 100 counted towards the expected event count** (`VMAvatar::HandleTimePropsEvent`). FreeSO
  counts only events below 100. Events 100 and above (101 = sound cue) are extra. When an animation ends short of its
  count, Animate Sim queues the missing events 0, 1 and so on.
  - 'A2O-tvsmall-turnon' carries only `xevt=101`, and 'animate - turn on' switches the TV on when that forced event 0
    arrives. Before the fix, small TVs never turned on, 'Watch TV' (pushed by 'Turn On') failed its check, and Sims
    re-picked 'Turn On' (Bob ×10, Watch TV ×0).
  - Now 'Turn On' is followed by 'Watch TV', other Sims join, and Sims watch from seats. The same rule applies to every
    animation that carries 1xx events.
- **Child animation tables** (`VMMemory::GetAnimation`, FreeSO):
  - A Sim younger than 18 uses the object's animation table + 1 (STR#130 c2o after STR#129 a2o) and MiscChild
    (Global STR#157).
  - Global and PersonStock scopes keep the "c" + name fallback, which is an inference.
  - Previously, child names that do not follow the plain a→c rename were never found.
- **TTAB version <= 3 is skipped** (FreeSO: "DO NOT LOAD THIS TTAB TYPE"). Sofas.iff TTAB 130 used to throw "read past
  end".
- **TS1 neighbour relationships are clamped** to -100..100 on set and add, as in FreeSO.

Findings that are not code bugs:

- **House 2 (Pleasant) has no fridge, food or reachable phone.** The OBJM has 388 records, and all are placed. The only
  phone and range are the out-of-world controller copies that every lot has (also present in houses 5 and 7). House02
  is a shell: pool, fireplaces and a range shell with no counter or fridge. Hunger -88 is what TS1 would do; the player
  has to buy food.
- **Comfort.** TS1 chairs give comfort from the chair's own 'main' loop: +1, +2, +3 or +4 per 60 ticks (2 game
  minutes), up to 20, 30, 40 or 60 (BCON 'Tuning - Comfort'). Decay is 0.5 per 2 minutes. 'Sit' ends as soon as the
  Sim is seated, and the Sim stays seated until autonomy picks something else.
  - The chair check 'Sit in Chair test' rejects autonomous Sit while already seated. Sofa and bench Sit has no such
    check, so seat-hopping from chair to sofa is in the data.
  - House 5 has no dining chairs and no TV, so its Sims eat standing.
  - Most of the low comfort came from the TV bug: no 'Watch TV', so no sitting.
- **Socials need ExpansionShared.far.**
  - The `gamedata` tree has the Complete Collection Global.far: PersonGlobals 'Interaction - Tickle Extreme', 'Talk'
    and so on create the Hot Date social objects (e.g. GUID 36EF8A46, `HDSI_Tickle.iff`).
  - Those objects exist only in `ExpansionShared/ExpansionShared.far`, which `gamedata` lacks. Every social was queued
    and failed at Create Object Instance (Bob: 119 picks, 0 socials).
  - With the full data (`gamedata_full` GameData, ExpansionShared and EPs, plus the base UserData), Talk, Tickle, Tease,
    Nag, Joke and Cheer Up run (20 social starts in 2 days across houses 7, 5 and 2).
  - Romance stays off: Hot Date 'Allow Autonomous?' needs preference person data 75/76 > 0. The neighbourhood
    NBRS has no values there, and only 'increment my * gender preference' raises them, after a directed romantic
    social. This is faithful.
- **Relationships load correctly** from NBRS: Mortimer→Bella [100 1], Bob↔Betty [55 0], Cassandra→parents 50.
- **Child animations that really do not exist.**
  - 731 distinct c2*-names are referenced by object STR#130 and the Global child tables.
  - 73 are missing with `gamedata` and 50 with the full data. The 23 found only in the full data are the EP
    `c2c-soc-deviant-*` socials.
  - The 50 still missing are absent from every archive: Animation.far is byte-identical in both trees, and no EP .far
    has them. Examples: c2o-walking-turn-*, c2o-lonely, c2o-stress, C2O-pooltable-*, C2O-piano-sit-*.
  - The lookup is case-insensitive, so this is not a loader or naming problem. As in FreeSO, a missing animation
    returns true on the next tick.
- **Piano/easel for hours.** The skill objects leave via global 'Exit Skill Object': a queued action, or fun/hunger/
  bladder/energy/comfort failure, or mood <= the object's constant. A rested Sim keeps practising, which is faithful.

Results (seed 12345, 2 days; `auto/logs/p2a_*` for `gamedata`, `p2f_*` for the full data):

| | House 7 | House 5 | House 2 |
|---|---|---|---|
| before (pass 1) | Bob 144 / Betty 107 starts, TV 0, comfort<-50 in 10–12/16 snapshots | Bella 660 autopicks, comfort -100 in day 2 | no food |
| after, `gamedata` | 110 / 110 starts, Watch TV 3+3, comfort<-50 in 5–6/16 | Bella 32 autopicks, Mortimer goes to work, 0 resets, comfort<-50 2–6/16 | no food (no fridge) |
| after, full data | 77 / 69 starts, 2–3 socials each, comfort<-50 1–2/16 | Mortimer Talk/Tease/Cheer Up; **Cassandra never returns from school** (EP car/school path) | Jeff 6 Talk, Nag, Joke; still no food |

All runs: 0 SimAntics resets.

Still open from this pass:

- With the full data, Cassandra stays hidden after 'Go to School': the bus is queued again at 15:01 and she never
  reappears. The base data works. This is in the car-portal/travel path that the EP agent is changing.
- Opcode 19 remains deferred.

## 8. Third pass (school bus, car pools, date, slots)

- **Find Location For 'along object vector'.** The DirectionVectors table is now FreeSO's. FreeSims' table started at
  (16,0), two notches off, so mode 3 stepped *across* the reference object's facing.
  - The car pool created beside the school bus parked in the next lane, directly on the bus door's routing slot.
  - Children who were not on the bus before the car pool arrived failed 'send person to work' with route result 8
    (Dest Tile Occupied, blocker 'Car - Junky') and missed school.
  - Now the second vehicle parks in line along the street.
- **The date never advanced** (`VMClock`, `VM::InternalTick`). Day of Month, Month and Year now roll over at midnight
  (FreeSO, 30-day months), and globals 1, 7 and 8 mirror the clock as FreeSO's GetGlobalValue does.
  - The car portal's 'check for school bus schedule' runs once per Day of Month, so there was never a school bus after
    the first day, with either data set.
- **'At School' priority 1 is FreeSO behaviour.** 'Go to School' on the bus is a run-immediately interaction. FreeSO's
  TryRunImmediately does not set the person's Priority, so the inherited push gives max(1, 0) = 1. It was 1 in the
  working base-data runs too. Day 2 shows 50 because the person data still holds an earlier value.
- **The 08:01 → 08:07 start is FreeSO behaviour.** The queued 'At School' waits for the person main loop's
  'check queue'. In FreeSO, idle animations ('do idle core') are interrupted only by Notify Out Of Idle, so the
  current idle chain finishes first. The bus waits, so with the parking fixed she still makes it.
- **Snap and slot lookups.** A slot index past the object's table is "no slot", and Snap returns false (FreeSO
  GetSlot/VMSnap). FreeSims threw, and Hot Date 'Flies' (house 9) reset twice.
- **Route failures do not yield in FreeSO either.** Goto Routing Slot pushes the frame and returns CONTINUE; a failed
  frame returns false in the same tick, and only the optional failure tree (global 398) takes time. Nothing was ported
  for the Vacation clerks. Diagnostics: `ROUTE_DEBUG=1` prints every hard route failure with target and blocker.

## 9. Fourth pass (Room motive)

- **The Room motive was always 0.** FreeSims never computed `RoomLighting.RoomScore`, and never set `LightBaseRoom`,
  so `GetRoomScore` always read room 0. FreeSO's `RefreshRoomScore` is now ported (`VMContext`):
  - The score is the sum of the Room Impact values of the objects in the room, divided by area/12 (outside: 30),
    minus 10 indoors or 15 outdoors, clamped to -100..100.
  - It is recomputed on lighting refresh, on placing or removing an object with a Room Impact, and on Refresh type 2
    ("room score contribution").
  - Inference: rooms are not merged across open divisions. Each room is its own light base room; FreeSO groups
    "support rooms".
- **Effect.** Room ads now count: Clean Up, Water, Recycle, dirty dishes and trash. The mood includes the room again.
  - House 9: the stale saved meals on the counters are cleaned up. The 'Prepare' → 'drop food' loop is gone: the only
    counter free for food prep was the corner piece, so 'prepare food' found no surface and dropped the food.
  - House 9: Frankie bathes; hygiene minimum -43, was -100.
  - House 7: comfort below -50 in 1–5 of 16 snapshots. The pass-3 figure of 9–10 was not a bisected regression; at
    two Sims and 16 snapshots the measure is RNG-noisy, and the missing room motive skewed the picks.

## Tools added to `tools/simlife.cpp`

| Option | What it does |
|---|---|
| `--why` | Prints the last action-tree frames when an interaction ends, and every autonomy pick (top 4 with scores). |
| `--trace <sim\|object> <fromMin> <toMin>` (+`--trace-checks`) | Prints every primitive that entity's thread runs. |
| `--ads <sim> <min>` | Prints every candidate interaction scored or rejected in that minute. |
| `--push <min> <object> <interaction>` | Measures the user-queue start delay. |
| `--seed N` | Deterministic runs (default 12345). |
| `--snap-minutes N` | Prints motives every N minutes. |
| `SIMLIFE_DUMP=<name>`, `SIMLIFE_DUMPAT=x,y` | Dump object state at the end of the run. |
| `SIMLIFE_TESTGUID=<hex>` | Tries to create that object out of world at start (is the definition loaded?). |
| (startup) | Prints NBRS person data, preferences 75/76 and neighbour relationships per family member. |

`--ads <sim> <min>` now prints the first minute at or after `<min>` in which that Sim evaluates candidates.
