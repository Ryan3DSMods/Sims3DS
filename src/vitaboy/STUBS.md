# src/vitaboy — Vitaboy (Sim body system) port notes

Translated from FreeSims `sims.common/rendering/vitaboy/*.cs`, `SimsVille/Vitaboy/*.cs` and the TS1
content providers in `SimsVille/ContentManager/other/TS1*.cs`. Namespaces `FSO::Vitaboy`,
`FSO::Content::TS1`, `FSO::Common::Content` (ContentID).

## Files

| C++ | C# source | status |
|---|---|---|
| VitaboyMath.h | XNA Quaternion/Matrix/Vector3.Transform/MathHelper | faithful XNA math (row vectors) |
| Animation.h/.cpp | Animation.cs (+PropertyList/TimeProperty*, PropertyListItem from Bone.cs) | faithful TSO path; **TS1 BCF/CMX path fixed** (see below) |
| Bone.h, Skeleton.h/.cpp | Bone.cs, Skeleton.cs | faithful; BCF bone layout fixed |
| Appearance.h/.cpp | Appearance.cs (+Binding.Read from Binding.cs) | faithful; Appearance.h also includes SimAvatar/Animator/Outfit (old-stub compatibility) |
| Binding.h, AppearanceType.h, BlendData.h (BlendData, BoneBinding, VitaboyVertex, MeshVertex) | same | faithful |
| Mesh.h/.cpp | Mesh.cs | faithful reader; GPU parts stubbed |
| CFP.h/.cpp | CFP.cs | faithful |
| BCF.h/.cpp | BCF.cs | faithful structure; reads binary .bcf and text .cmx |
| Outfit.h/.cpp, HandGroup.h/.cpp | Outfit.cs, HandGroup.cs | faithful (+string overloads) |
| ContentID.h | sims.common/content/ContentID.cs | faithful |
| Avatar.h/.cpp, SimAvatar.h/.cpp (+AdultVitaboyModel) | Avatar.cs, SimAvatar.cs, AdultVitaboyModel.cs | faithful logic, drawing replaced |
| Animator.h/.cpp | Animator.cs | faithful (+lazy keyframes, bounds checks) |
| BCFReadProxy.h/.cpp | (later FreeSO BCFReadProxy/CMXReadProxy) | **NEW**: binary + text readers |
| Texture.h/.cpp | ITextureRef/Texture2D | **NEW**: BMP decoder (8-bit, RLE8/RLE4, 24/32-bit), palettised storage |
| AvatarRenderer.h/.cpp | WorldCamera.cs (view/proj), _3DWorldBatch, Vitaboy.fx | **NEW**: software rasterizer + depth test |
| content/TS1AvatarProvider.h/.cpp | TS1Provider.cs, TS1BCFProvider.cs, TS1BMFProvider.cs, TS1AvatarTextureProvider.cs | translated + 3DS changes (below) |

## TS1 formats (verified against the shipped GameData, every BCF in Animation.far parses to the last byte)

The C# `bcf` branches were unfinished (FreeSims mostly ran on TSO data). Fixed layouts:

* **BCF** (little endian, pascal strings = 1 length byte): `int32 nSkel × Skeleton, int32 nSuit × Suit, int32 nSkill × Skill`.
* **Skeleton**: `pstr name, int32 boneCount` (TSO: version + int16). **Bone**: `pstr name, pstr parent,
  int32 propsCount × (int32 pairs × (pstr key, pstr value))` (TSO: int32 unknown first, then a byte
  HasProps flag), `float3 translation, float4 rotation, int32 canTranslate, canRotate, canBlend, float wiggleValue, wigglePower`.
* **Suit** (Appearance.ReadBCF): `pstr name, int32 type, int32 zero, uint32 n × (pstr bone, pstr mesh, int32 censor, int32 unknown)`.
* **Skill** (Animation): `pstr name, pstr xskill (cfp name), float duration(ms), float distance, int32 isMoving`
  (C#: version uint + byte), `uint32 translationCount, uint32 rotationCount, uint32 motionCount`, motion:
  `pstr bone, uint32 frames, float duration, int32 hasTranslation, int32 hasRotation` (C#: bytes),
  `uint32 firstTranslation, uint32 firstRotation, uint32 propListCount × PropertyList,
  uint32 timePropListCount × (uint32 n × (uint32 time_ms, PropertyList))` with
  `PropertyList = uint32 pairs × (pstr, pstr)` (C# TSO: flag bytes + an extra item-count level).
  Time property IDs are milliseconds (VMAvatar compares them with frame × 33.33).
* **CFP**: unchanged from C# (verified: decoded quaternions are unit length).
* **BMF** = C# Mesh.Read(bmf=true).
* **Text CMX** = the BCF record sequence, one value or string per line, `// comment` lines,
  `version 300` header, vectors as `| x y z |`. Read by `CMXReadProxy` through the same readers.
* **Text SKN** = the BMF sequence as text, **except blend lines are `otherVertex weight`** (binary
  is `weight otherVertex`) — verified on the shipped skins (first column indexes real vertices,
  second is 0..0x8000). Handled in Mesh::Read via `IOProxy::IsText()`.
* **Animation.ndx** (in Animation.far): 3 sections (skeletons, suits, skills), each
  `int32 n × (pstr name, pstr file.cmx)`; the FAR stores the file as `file.cmx.bcf`.

## Deliberate deviations / 3DS changes

* `TS1BCFProvider.Init` reads Animation.ndx instead of parsing all ~1200 BCFs at startup; loose
  `Skins/*.cmx` are still parsed (tiny). Unindexed names fall back to `<name>.cmx.bcf` / `<name>.cmx`.
* Lazy loading everywhere: BCFs parsed on first request; CFP keyframes decoded on first pose
  (`Animation::EnsureKeyframes` via `Animation::KeyframeLoader`), `UnloadKeyframes()` to drop them;
  meshes/textures on first bind. Textures stay 8-bit palettised (64 KB per 256² body texture).
* `TS1Provider` indexes FAR manifests once and reads entries by offset with its own FILE* (C#
  FAR1Archive.GetEntry searches by name each time); FARs without avatar files are not kept open.
* No `<dirent.h>` on plain arm-none-eabi: with `-D__3DS__` and without `-DST_HAVE_DIRENT` the
  provider opens `GameData/Animation/Animation.far`, `GameData/Textures/Textures.far` directly and
  probes `GameData/Skins/` and `ExpansionShared/SkinsBuy/` by file name (SD card is case-insensitive).
* `Avatar.AddAccessory`: C# only added when `Accessories.Count > 0` (so never). Fixed.
* `SimAvatar.ReloadHandgroup`: uses `Outfit.LiteralHandgroup` (TS1 hands from body strings 17–22,
  as FreeSO) and always the LightSkin set for it (TS1 texture names already carry the skin tone).
* `SimHandGesture` values corrected to C# (Idle 0, Pointing 1, Fist 2, None 3); the old stub had 1/2 swapped.
* Null skeleton → empty skeleton (C# would throw); null mesh in AddBinding → binding skipped.
* C# properties with reload side effects (`SimAvatar.Body/Head/Handgroup/Appearance/Left|RightHandGesture`)
  are `AvatarProperty<>` members: `avatar->Body = oft;` reloads, reads convert to the value.
* `Avatar.Skeleton/BaseSkeleton` are public raw pointers (C# get/internal set) owned by the avatar.
* Outfits: TS1 has no .oft files. `TS1AvatarContent::RegisterOutfit*` creates Outfit objects from body
  strings under synthetic ids `0x7531xxxxxxxxxxxx` (FNV hash of the strings) so `VMAvatar.BodyOutfit`
  stays a `uint64`.

## STUB(3ds)

* `Avatar.Effect/setVitaboyEffect`, `StoreOnGPU`, `Mesh.StoreOnGPU/InvalidateMesh/DrawGeometry/Draw`,
  `_3DComponent` base: MonoGame/HLSL — replaced by `AvatarRenderer::DrawAvatar`.
* TSO id lookups: `AvatarBindings.Get(type,file)`, `AvatarMeshes/AvatarTextures.Get(type,file)`,
  `AvatarHandgroups.Get(...)`, appearances by TSO ContentID → return null (no TSO data on TS1 installs).
* `.tga` avatar textures (TS1AvatarTextureProvider fallback): no TGA decoder (none in the base game).
* Animator/AnimationHandle `GameTime` → plain millisecond values.
* Collection.cs / PurchasableOutfit.cs (TSO catalogue) not translated.

## Hooks in other areas (minimal, marked `HOOK(vitaboy)`)

* `src/content/ContentManager.cpp`: AvatarAnimation/Appearance/Outfit/Skeleton providers delegate to
  `TS1AvatarContent` when TS1 data exists (placeholders kept as fallback without data).
* `src/simantics/entities/VMAvatar.cpp`: TS1 body strings (non-hex) register TS1 outfits in the
  `SetAvatarBodyStrings` catch block; child Sims get `child.skel`.

## Performance notes (3DS)

* Skinning is folded into per-bone bone→screen affine matrices (orthographic camera), 1–2 3×4
  transforms per vertex; a Sim is ~700–900 triangles (body ~550, head ~120, hands 2×36), about half
  culled as back faces; at Near zoom a Sim covers ~3–4k pixels.
* Planned if needed: fixed-point edge stepping, 16-bit z, caching skinned vertices while the pose
  is unchanged, skipping hands/accessories at Far zoom, LRU for keyframes/textures.
