# sims.files → src/files: stubs and deviations

Everything not listed here is a full translation of the C# logic. Search the code for
`STUB(3ds)` to find each one.

## Stubbed (no MonoGame / audio on 3DS)

| What | Where | Original behaviour | 3DS behaviour |
|---|---|---|---|
| `Texture2D`, `GraphicsDevice` | `utils/ITextureProvider.h` | XNA GPU types | Empty placeholder structs |
| `SPRFrame::GetTexture` | `formats/iff/chunks/SPR.cpp` | Uploads `Data` to a Texture2D, then frees `Data` | Decodes the frame and keeps `Data` (read it with `GetData()`). Returns `nullptr` |
| `SPR2Frame::GetTexture` / `GetZTexture` | `formats/iff/chunks/SPR2.cpp` | Uploads `PixelData` / `ZBufferData` (Alpha8) and frees them | Decodes the frame. `PixelData`, `ZBufferData` and `PalData` stay valid. Returns `nullptr` |
| `SPR2Frame::GetWorldTexture`, `DGRPSprite::GetTexture/GetWorldTexture` | SPR2.cpp, DGRP.cpp | Wrap the above | Same structure, but the texture pointers are `nullptr` |
| `BMP::GetBitmap` / `BMP::GetTexture` | `formats/iff/chunks/BMP.h` | System.Drawing / Texture2D.FromStream | Return `nullptr`. The raw BMP bytes are available via `BMP::GetData()` |
| `SPR2FrameEncoder::QuantizeFrame` | SPR2FrameEncoder.h | Delegate installed by the host app (image quantizer) | Empty `std::function` by default. `SPR2Frame::SetData` throws `bad_function_call` unless the caller sets it |
| PIFF directory scan on bare arm-none-eabi | `formats/iff/PIFFRegistry.cpp` | `Directory.GetFiles` | Uses POSIX `dirent` on host and under devkitPro (detected via `__has_include(<3ds.h>)`). Compiled out (finds no PIFFs) on bare newlib, which has no `<dirent.h>` |

## Audio formats (translated)

`HIT/` (HITFile, HSM, Hot, EVT, TLO, Patch, Hitlist, Track, FSC, HITConstants), `XA/XAFile`, `UTK/UTKFile2` are full
translations (used by src/hit). XA decoding is bit-exact against ffmpeg's adpcm_ea_maxis_xa (mono and stereo).
Deviations: `XAFile::DecompressedData()` is a getter (plus `Samples()/Channels()/SampleRate()`), the decoded PCM is
trimmed to the header's size; `Hot::Load` skips malformed lines instead of throwing, looks HSM labels up in lower
case (the C# missed mixed-case labels), reads `@Pch...` patch labels, drops the TSO `SoundID -> TrackData` override
(collides with TS1 label ids) and keeps the TS1-only EventMapping columns; `TLO` keeps the C#'s FileID1 typo.

## Not translated (outside scope / desktop-only)

- `formats/dbpf/*` (DBPF is a TSO/Sims 2 container and is not used by TS1).
- `formats/PiffEncoder.cs` (IDE-only PIFF generation).
- `ImageLoader.cs` (System.Drawing/Texture loading).
- `formats/iff/chunks/ISPR.cs` (an empty marker interface in a stray namespace).
- `Properties/AssemblyInfo.cs`.
- `formats/otf/OTF.cs` is a duplicate of `OTFFile.cs` under the old `TSO.Files` namespace, so only `OTFFile` is translated.

## Deviations (mechanical, behaviour-preserving unless noted)

- **Type keys replace `System.Type`.** Each chunk class declares `IFF_CHUNK_TYPE("XXXX")`, which gives it `static constexpr TYPE` and a virtual `GetTypeKey()`. `IffFile` indexes `ByChunkId`/`ByChunkType` by that string. The subclass relationships CTSS/TTAs/FAMs : STR and FLRm : WALm are kept, and each subclass has its own key, so `Get<STR>` does not return a CTSS (as in C#). `Revert<T>` compares type keys where C# used `IsAssignableFrom`.
- **`List<T>()` return value.** It returns an empty vector where C# returned `null` when the type is absent. Use `HasChunkType<T>()` when null-ness matters (PIFFRegistry does).
- **Ownership.** `IffFile` owns the chunks it reads (`unique_ptr`). Chunks added through `Patch()` remain owned by the PIFF file, and the PIFFRegistry owns every PIFF it loads. Frames (`SPR2::Frames`, `SPR::Frames`), TTAB interactions and NBRS entries are held by `unique_ptr`.
- **Null byte arrays.** "null" byte arrays are tracked with a flag: `IffChunk::HasOriginalData`, and the private `HasToDecode` in the SPR/SPR2 frames.
- **`IffFile` constructors.** `IffFile(path)` reads the whole file with `fopen`. `IffFile::Read(const std::vector<uint8_t>&)` was added so FAR members can be parsed from memory.
- **`IffFile::Write` type lookup.** It looks up the type string by instantiating each registered factory, replacing the C# reflection lookup.
- **C# properties that have logic** become getter/setter pairs: `OBJD::GUID1()/GUID2()`, `IsMaster()`, `IsMultiTile()`; `TTABInteraction::MaskFlags()` and the `Allow*`/`Debug`/... flag accessors; `SIMI::PurchaseValue()`; `STR::Length()`; `ARRY::TransposeData()`; `Decompresser::CompressedSize()/DecompressedSize()`. Plain auto-properties are public fields.
- **`OBJD::GetPropertyByName` / `SetPropertyByName`.** They use a name → member-pointer table instead of reflection, and work in `int32_t` (`Write` only ever used `<int>`).
- **`[Flags]` enums** are `enum class` plus `FSO_FLAGS_ENUM` operators (`AbstractIffChunk.h`), so `(Flags & X) > 0` reads as in C#.
- **"Read past end" handling.** `TTABFieldEncode`/`IffFieldEncode` `ReadBit` test for end of stream instead of catching the exception. The C# behaviour (read 0) is preserved.
- **SPR2 decode guards (deviation).** In SPR2 decode, pixel commands 0x01/0x02/0x06 guard the `PixelData`/`ZBufferData` writes with `hasPixels`/`hasZBuffer`. The C# wrote them unconditionally, which would have been a NullReference on frames without those channels. Behaviour is identical for every valid frame.
- **NBRS early return.** NBRS keeps the C# quirk that an early `return` (stream exhausted) skips the `OrderBy(NeighbourID)` sort.
- **Faithful "(sic)" behaviour.** Kept as in the C#: `SIMIBudgetDay::Write` writes `MiscIncome` twice, `SIMI::Write` zeroes `GlobalData`, and `NGBH::Write` writes `16` as the inventory count.
- **`OBJM` debug output.** OBJM's `Console.WriteLine` of the offset deltas is dropped.
- **`st::Reader`.** It gained the IoBuffer API: `HasMore()`, `Seek`/`Skip`, `Mark`/`SeekFromMark`, `ReadVarLen`, `ReadPascalString`, `ReadLongPascalString`, `ReadVariableLengthPascalString` (7-bit length), `ReadNullTerminatedString/UTF8`, `ReadFloat` (always little endian, as in the C#), and `ReadBytes` (short read at EOF, like BinaryReader). `st::Writer` mirrors IoWriter.
- **`core/math.h`.** `st::Color` now defaults to (0,0,0,0) like XNA `default(Color)`, and gained `==`/`!=`/`Black()`/`Transparent()`. Another translator edited this shared file concurrently; their additions were kept.

## Known source-level limitations (the C# behaves the same way)

These Objects.far members throw while decoding exactly one chunk. In each case the C# reader would throw too:

- ~~**OBJD version 136, 160 bytes** (`Cursors.iff` #128, `People\Bones.iff` #128).~~ FIXED (sims, buy/build mode needs the "Tile Cursor" GUID 0x437): `BHAV_Repair` is now read only when numFields > 78, as FreeSO does. Delete an old `cache/objects.cache` once so the cursor objects get indexed.
- **TTAB versions 2 and 3** (`HelpSystem.iff` #128, `Stoves.iff` #128, `Sofas.iff` #130). The C# layout misreads these versions and gets motive-entry counts of 0xA3A30010 and 0x10010, which ends in `bad_alloc` or "read past end".
