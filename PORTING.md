# Porting conventions: FreeSims (C#) -> sims (C++17)

Goal: a faithful, wholesale translation of the FreeSims engine (MPL-2.0) so it
reads the original Sims 1 files unchanged and runs on the New 3DS. The
reference is a local clone of https://github.com/francot514/FreeSims (plus FreeSO,
https://github.com/riperiperi/FreeSO, for the parts FreeSims shares with it). Translate, don't
redesign: keep the logic, structure, constants and comments.

## Naming (critical — parallel translators must line up)
- Keep C# class, method, field, enum and constant names EXACTLY (PascalCase).
  `VMStackFrame.InstructionPointer`, `OBJD.BaseGraphicID`, `VMMemory::GetBigVariable`.
- One C# file -> one `.h` + `.cpp` of the same base name (small POD/enum files may be header-only).
- Namespaces: C# `FSO.SimAntics.Engine` -> C++ `FSO::SimAntics::Engine`; same for the others.
- C# properties -> public fields (or getter+setter only when the property has logic).
- C# `static` members -> C++ `static`. C# `const`/`readonly` -> `static constexpr` / `const`.

## Types
- `int`->`int32_t`, `uint`->`uint32_t`, `short`->`int16_t`, `ushort`->`uint16_t`,
  `byte`->`uint8_t`, `sbyte`->`int8_t`, `long`->`int64_t`, `bool`, `float`, `double`.
- `string` -> `std::string`.  `List<T>` -> `std::vector<T>`.
  `Dictionary<K,V>` -> `std::unordered_map<K,V>` (`std::map` if ordered iteration matters).
  `HashSet<T>` -> `std::unordered_set<T>`. `T[]` -> `std::vector<T>` (or `std::array<T,N>` if fixed).
- C# reference semantics: class instances passed/held by pointer (`T*`), owned by
  the container that created them (`std::unique_ptr` in the owning collection).
  Null -> `nullptr`.
- `Vector2/Vector3/Point/Rectangle` (XNA) -> small structs in `core/math.h`.
- Exceptions: keep `throw` where C# throws (use `std::runtime_error` or a
  translated exception class). try/catch maps directly.
- `lock(x)` -> drop (single-threaded on 3DS). `Thread`, `Task`, async -> drop/stub.
- `Random` -> the VM's own `NextRandom`. `Math.*` -> `<cmath>`/`std::`.

## IO
- `IoBuffer`/`BinaryReader` -> `st::Reader` in `src/core/io.h` (little/big endian
  u8/u16/u32/s16/s32/cstr/bytes). `IoWriter` -> `st::Writer` (add if needed).
- File loading via `fopen` (works on 3DS sdmc).

## What to stub (3DS has no MonoGame / TSO backend)
- Anything touching `Microsoft.Xna.*`, `Texture2D`, `GraphicsDevice`, sound
  playback, networking/TSO (`TSOGlobalLink`, `NetPlay`, `VMTSO*`), file dialogs.
  Replace with the smallest interface that keeps callers compiling, mark with
  `// STUB(3ds): <what the original did>` and list it in STUBS.md next to the code.
- Do NOT stub game logic. If a primitive or chunk reader is logic, translate it.

## Build
- Headers under `src/<area>/`, `#pragma once`, include what you use.
- Must compile with: `g++ -std=c++17 -Wall -Isrc -c <file>` (host) and
  `arm-none-eabi-g++ -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp
  -mtp=soft -std=gnu++17 -fexceptions -D__3DS__ -Isrc -c <file>` (3DS).
- No host-only headers in engine code (no zlib, no X11, no SDL).

## Areas
- `src/files/`     <- sims.files (IFF container, every chunk class, FAR1/FAR3, utils)
- `src/simantics/` <- SimsVille/SimsAntics (engine/, entities/, model/, primitives/, VM, VMContext)
- `src/world/`     <- SimsVille/World model (architecture, blueprint) — later
- `src/platform/3ds/` <- our 3DS entry point (exists)
