# Primitive stubs — batch 4

| File | Location | What the original did | 3DS behaviour |
|---|---|---|---|
| VMGenericTSOCall.cpp | `AskStackObjectToBeRoommate` (24) | `context.VM.GlobalLink.RequestRoommate(...)` when Temp0 == 1 and a TSO server link exists | Code kept verbatim behind `GlobalLink() != nullptr`; `VM::GlobalLink()` is always null on 3DS, so it never fires. |
| VMGenericTSOCall.cpp | `KickoutRoommate` (27) | `context.VM.GlobalLink.RemoveRoommate(...)` | Same: kept behind the always-null `GlobalLink()` check. |
| VMGenericTSOCall.cpp | `KickoutVisitor` (28) | `((VMServerDriver)context.VM.Driver).KickUser(vm, StackObject.Name)` when a server link exists | `// STUB(3ds)`: NetPlay `VMServerDriver` not ported; branch is empty (dead, GlobalLink is null). `UserLeaveLot()` still runs. |
| VMGenericTSOCall.cpp | `LeaveLot` (25) | `GlobalLink.LeaveLot` or else `CheckGlobalLink.LeaveLot` | Not a stub: falls to the local `CheckGlobalLink` stub exactly like the C# no-server path. |

No other stubs: VMCreateObjectInstance, VMFindBestObjectForFunction, VMPushInteraction, VMGetTerrainInfo,
VMGotoRoutingSlot, VMGetDistanceTo, VMSpecialEffect, VMBreakPoint are pure logic and fully translated
(VMSpecialEffect / VMBreakPoint / most of VMGetTerrainInfo are TODO no-ops already in FreeSims, not 3DS stubs).
