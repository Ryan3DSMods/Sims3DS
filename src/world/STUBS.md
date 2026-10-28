# src/world — FreeSims `SimsVille/World` (FSO.LotView) as a C++ software renderer

Entry point: `FSO::LotView::World::Draw(Framebuffer&)` paints the lot into an RGBA8888 colour buffer with a
float depth buffer (`world/Framebuffer.h`, `ToRGB565()` for the 3DS). No GPU, no MonoGame.
Host test tool: `src/tools/simrender.cpp` (`make -f Makefile.port build-port/simrender`).

## File map (C# -> C++)

| C# | C++ | status |
|---|---|---|
| World.cs | World.h/.cpp | translated; 3D parts stubbed (see below) |
| World2D.cs | World2D.h/.cpp | translated (static scroll-buffer layers + dynamic layer) |
| WorldState.cs (WorldState, WorldSpace) | WorldState.h/.cpp | translated (projection, 3 zooms, 4 rotations, scroll) |
| model/WorldZoom.cs, WorldRotation.cs | WorldZoom.h, model/WorldRotation.h | translated |
| utils/WorldCamera.cs | utils/WorldCamera.h | depth function only (see "depth") |
| utils/_2DWorldBatch.cs + Content/Effects/2DWorldBatch.fx | utils/_2DWorldBatch.h/.cpp | CPU rasterizer of the shader techniques |
| utils/_2DSprite.cs, _2DSpriteVertex.cs, _2DDrawGroup.cs | utils/_2DSprite.h | sprite translated; vertex/draw-group types not needed |
| utils/DGRPRenderer.cs | utils/DGRPRenderer.h/.cpp | translated |
| sims.common TextureGenerator (GetWallZBuffer, GetAirTiles) | utils/TextureGenerator.h/.cpp | translated |
| components/FloorComponent.cs | components/FloorComponent.h/.cpp | translated incl. pools/water edges + corners |
| components/WallComponent.cs | components/WallComponent.h/.cpp | translated: patterns, styles/masks, up/down/cutaway (DownLeftUpRight/DownRightUpLeft + overlays 252/253), thick-wall end caps, diagonal walls + half floors, junctions |
| components/ObjectComponent.cs, EntityComponent.cs, WorldComponent.cs | components/*.h/.cpp | translated (see "VM-written fields") |
| components/TerrainComponent.cs | components/TerrainComponent.h/.cpp | software mesh: TS1 lawn state (ARRY 6), FreeSO lot types by house number (grass/sand/snow/Magic Town dark+autumn/cloud), blade texture, outside light |
| components/RoofComponent.cs | components/RoofComponent.h/.cpp | translated (RegenRoof/RoofSpread/IsRoofable/MeshRects), drawn by utils/TriangleRaster into the wall layer; HOUS roof name + pitch; content/WorldRoofProvider.cpp |
| components/AvatarComponent.cs | components/AvatarComponent.h | STUB (state only) |
| model/Blueprint.cs | model/Blueprint.h/.cpp | translated (+GenerateRoomLights, Cutaway, WallComp/FloorComp) |
| ContentManager/WorldWallProvider.cs, WorldFloorProvider.cs | src/content/WorldWallProvider.*, WorldFloorProvider.*, model/WallFloor.h | translated for the TS1 file layout |

## Depth (the GPU z-buffer on the CPU)
`vsZSprite` projects `worldPos + offToBack` and `+ dirToFront` with the orthographic world camera and the
pixel shader writes `back + ((1 - z/255)/0.4) * (front - back)`, z = the sprite's z-buffer texel (SPR2
ZBufferData, or the generated wall/floor/junction buffers). Orthographic depth is affine in view-space z, so
`WorldCamera::Depth(p) = -viewZ(p)` with `view = RotY(315/225/135/45) * RotX(30)` gives the same ordering;
the projection's scale/offset and the scroll-centre translation cancel out. Depth test is LessEqual with
depth write, alpha blending as `BlendState.AlphaBlend` over premultiplied output. All four rotations use the
C# `FrontDirForRot`/`BackOffForRot` vectors. Because the depth is absolute, cached layers are restored by a
plain depth-tested copy (`drawSimpleRestoreDepth` without the 1.23 fudge factor of `DrawScrollBuffer`).

## Approximations / differences (all marked in code)
- Shader technique: the main FreeSims draw runs with `OutputDepth = true`, so the `*DepthChannel` pixel
  shaders are what is translated: ambient light is always multiplied (room colour from `Blueprint.RoomColors`),
  alpha <= 0.01 is discarded. Room 65533 (disabled object) is drawn greyscale (the FreeSims shader would
  multiply it by an unset, transparent ambient entry).
- Sprites are rasterized in submission order per render mode instead of grouped by texture (only matters for
  exactly equal depths).
- Sub-pixel projection offsets (-1.5/-0.5 GL, -2 DX, DrawScrollBuffer's X-2) are ignored: sprites land on
  `AbsoluteDestRect - scroll` exactly.
- `World2D`: one off-screen colour+depth buffer per static layer (floor+terrain, walls, static objects) like
  the C# scroll buffers; a layer redraw re-runs its component instead of replaying a cached vertex list.
  Lighting ticks (every 240 frames) only re-render the layers when a room colour actually changed.
  `World2D::SCROLL_BUFFER` (default 512 as in C#) can be lowered on the 3DS (`World::SetScrollBuffer`):
  memory is 3 x (view + SCROLL_BUFFER)^2 x 8 bytes.
- TerrainComponent (SIMPLIFIED): flat per-tile grass diamonds (floor z-buffer, so it sorts like a floor) tinted
  LightGreen->LightBrown by the C# `GenerateGrassStates` noise (fixed seed) and lit by the tile's room colour.
  No 3D grass mesh/blades, no lot-type textures.
- `DGRPRenderer` keeps FreeSims' quirk of using `centerRelative * 3` (tile x,y,z) directly as the sprite world
  offset (y lands on the height axis). Only SPR2 sprites are drawn (C# also skips SPR# sprites: no z-buffer).
- Texture cache: `Texture2D` is a non-owning view of the decoded SPR/SPR2 frame data (decoded lazily on first
  draw, never copied); `ClearTextureCache` only drops the views. Decoded frames are not freed on zoom change.

## VM-written fields (C# property setters)
The VM assigns `ObjectComponent::DGRP`, `DynamicSpriteFlags(2)`, `Direction`, `Room`, `Visible`, `Position`
directly. `ObjectComponent::SyncRenderer()` (called from `Update`/`ValidateSprite`/`Draw`) compares them with
what the DGRPRenderer last used and raises the same `OBJECT_GRAPHIC_CHANGE` damage + renderer invalidation the
C# setters did. `EntityComponent::Position` is the raw `_Position`; the C# `Position` getter (container slot
following) is `DrawPosition()`. `WorldState` properties are getter/setter pairs (`Zoom()`/`Zoom(z)`);
`WorldSize` stays a field (VM writes it).
Damage entries may reference components the VM has already deleted; World2D only dereferences components
still in `Blueprint::Objects`.

## STUB(3ds)
- World3D / `_3DWorldBatch`: replaced. `World::DrawAvatarComponents` draws `Blueprint.Avatars` (AvatarComponent.Avatar =
  the VM's SimAvatar, `DrawPosition()`, `RadianDirection`, room ambient tint) with `src/vitaboy/AvatarRenderer` after
  the 2D pass. The avatar renderer works in XNA NDC depth (`FSO::Vitaboy::WorldCamera`); our buffer holds -viewZ, so
  the buffer is converted with the exact affine map (two sample points) before and back after the avatars - Sims are
  hidden by walls/counters/objects in front of them and hide what is behind them. `cam.PixelOffset` = the rounding
  residual of `GetScreenOffset()` + (1,0) (PosCenterOffsets). `World::AvatarDrawHook` remains as an optional extra.
  `AvatarComponent::GetSLOTPosition` returns the avatar position (C#: R_FINGER0 bone), so carried objects sit at the feet.
- RoofComponent: roof geometry not generated/drawn (FreeSims draws roofs in 3D).
- SubWorlds, object/lot thumbnails, `GetObjectIDAtScreenPos` (OBJID render), headline balloons, edge/mouse
  scrolling (`TestScroll`), ScrollAnchor, smooth zoom (PreciseZoom != 1 is treated as 1 by the rasterizer).

## Content (src/content/WorldWallProvider, WorldFloorProvider)
TS1 paths instead of FreeSims' TSO ones: `GameData/walls.iff`, `GameData/floors.iff`, `GameData/Build.iff`
(case-insensitive lookup), custom `GameData/Walls/*.wll`, `GameData/Floors/*.flr` and `Downloads/**` (one
level). Custom ids start at 256 in sorted file-name order; `DynamicWallFromID`/`DynamicFloorFromID` map the
lowercase file name without extension to the id, which is exactly what `VMWorldActivator::BuildWallDict` /
`BuildFloorDict` look up for the house WALm/FLRm entries. TS1 Build.iff string tables: walls = STR# 0x83
("Wallpaper"), wall styles = 0x81, floors = 0x82 ("Floor", FreeSims/TSO used 0x83), pool = 0x85. All global
floors present in floors.iff (ids 1..255) are registered. Custom IFFs are opened once at init (catalog
strings) and their sprites decode lazily. Without `dirent` (plain newlib 3DS build without ST_HAVE_DIRENT)
custom walls/floors are not scanned.

## Renderer pass (roofs, terrain, water, light, performance)
- Roofs: shown when `WorldState::DrawRoofs` (app: walls up + top story in view, LotSession::UpdateCutaway; simrender
  `SIMRENDER_ROOF=1`). HOUS "GUID" field is the TS1 roof pitch (float). Face shading relative to the view; fascia rim.
- Water: TS1 water tiles (floors.iff 0x800..) key their palette-transparent colour (drawn with the "solid" pixel
  command) -> FloorComponent::GetWaterTexture. Fixes the magenta ice rink on Vacation House42 / ponds.
- Terrain depth sits 0.4 units behind the floor plane (quantized floor z-buffers).
- Sim shadows: World::DrawAvatarShadow (TS1 soft ellipse under each Sim, depth-aware), `World::DrawAvatarShadows`.
- Deferred room light: the static layers store unlit colour + per-pixel room id (Framebuffer::Room); the light is
  applied in World2D::MergeLayer, so time-of-day / lamp changes rebuild only the composite.
- World2D: layers shift on scroll and render only the strip that scrolled in (clip rect); one composite buffer is
  row-copied into the frame; static objects changing layer / removed redraw only their bounds.
- Avatars: depth written directly in world depth space (no 2 full-buffer conversions per frame); plane-stepped
  rasterizer with tight bounds.
- Not done: terrain altitude (ARRY 0) is not applied to floors/objects (only terrain mesh API: SetHeights),
  lamp glow sprites beyond what objects draw (street/lot edge and effects: see "Live-mode visual details").

## Live-mode visual details (TS1(sims) unless noted)
- Outside the lot: the road, sidewalk and kerb are floor tiles inside the house's own lot data (FreeSO VMTS1Activator
  FlipRoad); FreeSO has no TS1 out-of-lot terrain, so beyond the lot stays black (lot07_far.png). `World::PreDraw`
  `TS1ScrollBounds`: the tile under the middle of the screen stays 6/4/3 tiles (far/medium/near) inside the lot (FreeSims'
  bounds let the view go 1.05-1.2 lot widths past the edge: all black). edge_clamp_compare.png.
- Cursor cutaway (FreeSims UILotControl, faithful): `LotSession::UpdateMouseCut` (called by App before each draw) adds
  `VMArchitectureTools::ApplyCutRectangle` 5x5 around the cursor (shifted by CutCheckDir*2) to the all-rooms cut; damage
  only when notable. On top of the room cut it only lowers walls that are not around an indoor room (back walls,
  free-standing walls; fences never cut). `LotSession::CursorCutaway = false` turns it off — unverified whether TS1 itself
  does this (FreeSims/TSO behaviour). cutaway_cursor_compare.png, cutaway_partial_verify.png.
- `World2D::CutDirtyRect` (FSO_CPP): a WALL_CUT_CHANGED re-renders only the walls whose cut changed (tiles diffed against
  the cut the wall layer was drawn with; full redraw on level change or when the area exceeds the buffer). Checked
  pixel-identical against a full redraw. Cost of one cursor-cut change: 26.7 M -> 8.7 M instructions (medium zoom).
- Effects: TS1 fire/smoke/steam/sparkles are object sprites (FreeSO ParticleComponent/PART is TSO-only). The fire Burn
  creates (Fire.iff 0x24C95F99) draws and animates (f01_fire.png, f03_fire_near.png).
- Censor mosaic (new): person data CensorshipFlags (FreeSO VMCensorshipFlags) -> AvatarComponent::CensorFlags (VMAvatar;
  "spine if female" cleared for men) -> `World::DrawCensor` pixelates squares over PELVIS / SPINE2 / HEAD / hands / feet
  (posed bones through the avatar world matrix) or the whole avatar (128). Cells 3/6/12 px. Verified with the shower:
  128 while undressing, 1 while showering (censor_shower_seq.png, censor_zoom.png). Cell size/box size are estimates.
- Money over head (FreeSO VMAvatar.ShowMoneyHeadline): person data MoneyAmmountOverHead and expression scope
  MoneyOverHead32Bit now reach `VMAvatar::ShowMoneyHeadline`; LotScreen polls `MoneyHeadlineSerial` and floats the
  "+§n"/"-§n" over that Sim's head (a funds popup of the same amount merges into it). Colours: green / red (unverified
  against the original; FreeSO/TSO uses a grey box). money_compare.png. Two popups at once overlap.
- Plumbob: no sprite or mesh in the data (drawn by the exe). LotScreen::DrawPlumbob draws a shaded, spinning octahedron
  (one turn per 2.4 s), always green (as far as known, TS1 has no mood colours). plumbob_zoom.png.
- Idle thought balloons: VM-driven (BHAVs); UIHeadlineRenderer animation timing (Anim/15) matches FreeSims.
- Lighting: GameData/Lights + lightMap.dat look like Sim shading data (unverified), not room light. Outdoor lamps do not
  light the outside room (VMContext::RefreshLighting, as FreeSims). Not changed.
- Not done: hover outlines (as far as known TS1 has none; the cursor art changes), stacking of simultaneous money popups,
  lamp glow.
- Host test commands (sim3ds_host): `spawn GUIDHEX DX DY`, `use NAME TEXT`, `moneyhead N`, `cursor X Y`.
  Known issue (also in the Oct 23 build): `tapobj` no longer finds objects on house 7 ("not pickable").
- Perf (callgrind, House 5, 3 Sims, medium, 30 frames, speed 1): 10.35 M -> 10.46 M instr/frame (+1 %); World::Draw
  6.00 -> 6.04 M, top HUD 0.42 -> 0.42 M.

## Terrain altitude + build-mode pass (TS1(sims))
- `simantics/model/VMArchitectureTerrain.*` (FreeSO): heights per vertex, FreeSO units = TS1 altitude x 10. TS1 house
  ARRY 0 has 4 bytes per tile = its 4 corners; in loader order byte 1 = V(x,y), byte 2 = V(x+1,y), byte 0 = V(x,y+1),
  byte 3 = V(x+1,y+1) (verified on the shipped houses). Centres, sloped flags, preview copies (EnterVis/ExitVis).
- `VMArchitecture`: Terrain, TerrainLimit, OutsideClip (FreeSO; TS1 = the ARRY 8 flag-32 buildable area),
  Get/SetTerrainHeight/Grass, SyncTerrain (Blueprint::Altitude/AltitudeCenters/FineArea + TerrainComponent::SetHeights,
  preview heights to the mesh only), TERRAIN_RAISE / TERRAIN_FLATTEN / GRASS_DOT. `VMArchitectureTools::RaiseTerrain`
  (FreeSO flood fill) with TS1 rules: max 20 altitude units between neighbouring vertices (the shipped lots have 10-20;
  FreeSO 5), only vertices whose 4 tiles are buildable, and `VMContext::SlopeVertexCheck` (walls, floors, objects that
  need level ground) is enforced (FreeSO skips both in TS1 mode). Disturbed lawn turns brown (FreeSO).
- Renderer: `Blueprint::GetAltitude/InterpAltitude` (FreeSO); floors, walls, wall junctions, roofs, objects and Sims
  (`EntityComponent::DrawPosition`, AltitudeSource) are lifted by the terrain (House07's lot sits 20 units above the
  street); terrain mesh shaded by slope; `WorldSpace::GetTileAtPosWithScroll` follows the altitude (iterated) so the
  build tools / picking hit the raised ground.
- Buildable area: walls (Verify*), floors (dot/rect/fill and its spread), terrain, buy-mode placement
  (UIObjectHolder: LocationOutOfBounds) stop at the lot edge; BuildableFloors = 2. Objects that need level ground refuse
  sloped tiles (FreeSO SlopeValid), walls/floors/pools refuse sloped ground.
- FIX ARRY decoder: a run reaching the end of the grid now fills it with its pad value (FreeSims left zeros: the
  street rows of ARRY 8 read as buildable, the pool grids as 0).
- FIX `VMEntity::FloorChangeValid`: water is 65534 (was 63354).
Not done / approximations: smooth terrain mode (Ctrl) has no UI toggle; lone 1x1 pool tiles are saved with mask 0, which the
loader (FreeSO rule `!= 0`) ignores; terrain heights are not clipped against objects lying across a vertex on level 2;
the terrain marker is drawn by the UI (no TSO cursor object in TS1).
