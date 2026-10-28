# src/app/cas — Create-A-Family / Create-A-Sim (dual screen)

The Sims' "CREATE A FAMILY" and "CREATE A SIM" screens for the 3DS: the original art (UIGraphics.far), strings
(UIText.iff / Live.iff), skins (every head and body of the base game, the 7 expansion packs, Deluxe, SkinsBuy and the
bundled downloads), personality/zodiac rules, and a save that writes the same files the PC game writes, so the family
shows up in the neighbourhood's family bin and plays on a lot through `VMTS1FamilyLoader`.

```
top    400x240  live 3D Sim (Vitaboy SimAvatar + software rasterizer) in the CAS Sim panel, idle animations,
                turntable / circle pad / C-stick / L R / touch drag to turn; name, astrological sign (medallion),
                the selected trait's TS1 description (Live.iff STR#131) or the bio.
                Family view: the whole family standing in a row (idling), plumbob over the selected Sim, funds.
bottom 320x240  Create A Sim: first name bar, PERSONALITY points, 5 trait rows of 10 LEDs + points-remaining bar,
                the original quarter-circle cluster (adult/child, light/medium/dark, male/female), head/body arrows
                with catalogue position and skin name, biography, Random, Cancel/Done (original buttons).
                Create A Family: DsgnFamBkg (scaled 0.4): last name bar, 8 member portraits (live 3D heads),
                Add a New Sim / Delete Selected Sim / Edit Selected Sim, selected Sim summary, §20,000, Cancel/Done.
```

## API (for the front end)

```cpp
#include "app/cas/CASScreen.h"
using simapp::cas::CASScreen;

// when the player picks "Create New Family" in the neighbourhood:
auto cfg = CASScreen::DefaultConfig(appConfig.DataRoot);   // UserDataDir = Content::Get()->Neighborhood->UserPath
cfg.LineupFrameSkip = 2;                                   // optional, 3DS
std::unique_ptr<CASScreen> cas(new CASScreen(cfg));        // blocking ~0.3-1 s: catalogue + art
cas->GUIDInUse = [](uint32_t g) { return FSO::Content::Content::Get()->WorldObjects->Entries.count(g) != 0; };
if (!cas->Ready()) { /* cas->Error: no skins found */ }

// every frame:
cas->Update(appInput, dtMs);          // AppInput as App::Frame gets it
cas->Draw(app.Top, app.Bottom);       // both canvases
if (cas->Done()) {                    // the family was saved (character files + Neighborhood.iff)
    int16_t familyId = cas->FamilyID();          // FAMI chunk id = TS1 family number (pd 61, global 9)
    CASScreen::ReloadEngineNeighbourhood(cfg.UserDataDir); // re-reads Neighborhood.iff + registers the Sims
    // -> back to the neighbourhood; the family is in the bin (FAMI.HouseNumber 0)
} else if (cas->Cancelled()) { /* back, nothing written */ }
printf("%s", cas->Log.c_str()); cas->Log.clear();  // optional
```

* Text entry: `CASScreen::TextInput` defaults to the 3DS system keyboard (`CASSystemKeyboard`, libctru swkbd, blocking
  while shown; UTF-8 <-> Latin-1). Host drivers replace it (cas_host answers with scripted text).
* Moving the family in: `CASFamilyWriter::MoveIn(userDataDir, familyId, houseNumber)` sets FAMI.HouseNumber in
  Neighborhood.iff (refuses an occupied house); then `ReloadEngineNeighbourhood` and load the lot with
  `VMTS1FamilyLoader::LoadFamily(house)` as usual. (Lot price / budget deduction is the neighbourhood screen's job.)
* Buttons: Create A Sim — D-pad up/down trait, left/right -/+ point, X random look, Y random personality, SELECT
  name, A/START Done, B Cancel, L/R or circle pad turn. Create A Family — D-pad left/right select, A edit (or add),
  X add, Y delete, SELECT last name, START Done, B Cancel. Dialogs: A = first button, B = last.

## Files

| file | what |
|---|---|
| `CASScreen.{h,cpp}` | the screen: pages, layout, input, drawing, dialogs (UIText STR# 129/130/152/164), saving |
| `CASSimDesc.{h,cpp}` | a Sim / family being designed: body strings, person data, zodiac, `InitTraits` (translated BHAV) |
| `CASCatalog.{h,cpp}` | heads/bodies per age, sex, skin tone, from the avatar content index (all packs) |
| `CASSimPreview.{h,cpp}` | SimAvatar + outfits + idles, drawing with `Vitaboy::DrawAvatar`, portrait rendering |
| `CASFamilyWriter.{h,cpp}` | writes User#####.iff (TemplatePerson clone) and Neighborhood.iff (NBRS/FAMI/FAMs, rsmp) |
| `CASArt.{h,cpp}` | UIGraphics.far art (decoded, resampled once) + UIText/Live strings |
| `CASTextInput.{h,cpp}` | swkbd on the 3DS |
| `demo_cas.txt` | cas_host script that builds a 3-Sim family |
| `../../tools/cas_host.cpp` | host driver (`make -f Makefile.port build-port/cas_host`) |

Everything here is marked TS1(sims): FreeSims has no TS1 CAS. Its TSO avatar creator
(SimsVille/UI/Screens/PersonSelectionEdit.cs) is the model for the preview (`UISim` with `AutoRotate = true`,
skin buttons Light/Medium/Dark, Female/Male, head/body browsers) and its names were kept where they apply.

## What the original does and how it was established

* **Catalogue** — The Sims lists skins by texture file name (House Party costume-trunk `_ct` bodies and the Vacation winter/swim head variants are left out, they are put on by the game): heads `c###<m|f><a|c><lgt|med|drk>_name.bmp` on mesh
  `c###<g><a>_*`, bodies `b###<g><a><fat|fit|skn|chd><skin>_name.bmp` on mesh `b###<g><a><type>_*` (`u` = unisex
  child). NPC uniforms are named differently (maid, policeflgt_01, ...) so they never appear. All sources the avatar
  provider indexes are used: 645 heads / 641 bodies on the Complete Collection (e.g. adult male light: 132 heads,
  100 bodies). Skin tone changes keep the same style where it exists (c001malgt_ross -> c001madrk_ross).
* **Body strings** (STR# 200, as every shipped character): 0 adult|child, 1 body, 2 head (`HEAD-HEAD`), 3..6 HEAD/Top/
  R_HAND/Palm, 12 male|female, 13 27|9, 14 lgt|med|drk, 15 naked (`n<g><type>_01`, `nuskn_01`/`nuchd_01`),
  16 underwear (`u<g><type><skin>_briefs01|undies01`), 17..22 hands (`H<m|f|u><LO..RC>,HAND=hua<o|p|c><skin>`),
  27 formal (`f<g><type>_01`, children `,BODY=`), 28/29 bride gloves for females.
* **Person data** (Behavior.iff STR#200, values as in the shipped NBRS): traits ×100 at Nice 2, Active 3, Playful 5,
  Outgoing 6, Neat 7; interests 46..55; children job type -1 / level 4; 58 age 27|9; 60 skin lgt 1, drk 2, med 3;
  61 family number (FAMI chunk id); 65 gender (female 1); 70 zodiac (1 Aries..12 Pisces — *inference*: the shipped
  Sims store 0; the label exists and the value is harmless).
* **Interests**: `CASSimDesc::InitTraits` is the translation of TemplatePerson.iff BHAV 4100 'init traits'.
* **Zodiac**: the sign whose 25-point profile (the shared Sims table) is nearest to the traits; names/tooltip from
  UIText STR#164 / 130:27. The game has no zodiac bitmaps in its data; the medallion glyph is drawn.
* **Family**: up to 8 Sims, at least one adult and a last name to finish, §20,000 (FAMI.Budget), house 0 (bin),
  family number = highest + 1, FAMI/FAMs chunk id = lowest free.
* **Files**: see `CASFamilyWriter.h` — TemplatePerson.iff clone with exactly the two OBJD fields every shipped
  character changes (GUID, OriginalGUID = 0x7FD96B54), STR#200, CTSS #2000 (name, bio), the five BMP_ portraits
  (faces 105x41 on (0,0,80), relationship images 200x40 on the five relationship colours, thumbnail 25x25 on
  (0,0,248), speech 16/32 on white — formats measured on User00000.iff), rsmp rebuilt. Neighborhood.iff: NBRS slot =
  neighbour id - 1 (verified on both shipped neighbourhoods), version-4 record with person mode 5.
* **Shared-code fix**: `files/formats/iff/chunks/NBRS.cpp` `Write` (FIX(sims)): FreeSims wrote the count of all
  slots but only the valid neighbours, in dictionary order, corrupting any neighbourhood with free slots. It now
  writes slot by slot (a free slot is an int32 0) and keeps the version read; `cas_host <root> . roundtrip` checks
  that the shipped neighbourhoods read back identically.

## Not (yet) like the original

* No music (the CAS track) or UI sounds; no on-screen tooltips; the original's "Personality for %s" pop-up is the
  bottom summary + top description instead.
* The original lets you type straight into the name/bio fields; here the system keyboard opens.
* The vitaboy renderer only has the game's 30° orthographic iso camera; the preview pitches the skeleton ~17° towards
  the camera (portraits: the full 30°, i.e. level) instead of the original CAS perspective close-up.
* `ReloadEngineNeighbourhood` is not exercised by cas_host (the test loads the saved family in fresh processes:
  simlife / sim3ds_host).
* Swimwear/PJ/winter outfits (Hot Date+, Vacation) are not chosen in CAS (the game assigns them in Live mode).
* Relationships between new family members start empty (the PC game adds family flags when they move in).

## Verification (host)

```
make -f Makefile.port build-port/cas_host
build-port/cas_host <root> out roundtrip                      # NBRS round trip on the shipped neighbourhoods
build-port/cas_host <root> out catalog                        # catalogue sizes per age/sex/skin
CAS_USERDATA=<root>/UserData build-port/cas_host <root> out script src/app/cas/demo_cas.txt   # builds + saves "Simmons"
build-port/cas_host <root> out movein 6 2                     # family #6 into house 2
build-port/simlife <root> cache 2 0.5                         # the family plays (Alex, Maria, Kim: walk, read, swim)
```
Use a copy of the install's UserData (the script writes into it). 
