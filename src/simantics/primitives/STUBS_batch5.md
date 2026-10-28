# STUBS — primitives batch 5

| File | Stub | Original behaviour |
|---|---|---|
| VMInventoryOperations.cpp | `STUB(3ds)` `ApplyAsyncResponse` | Replaces `vm.SendCommand(new VMNetAsyncResponseCmd(id, new VMInventoryOpState{...}))`: the command's Execute is applied directly (sets the waiting thread's BlockingState when it is exactly the same type). |
| VMInventoryOperations.cpp | `STUB(3ds)` `vm.ForwardCommand(VMNetUpdatePersistStateCmd)` dropped | Registered the new persist ID of an object moved to inventory (NetPlay). |
| VMInventoryOperations.cpp | `STUB(3ds)` `vm.ForwardCommand(VMNetSendToInventoryCmd)` dropped | Removed the object from the lot / added it to `vm.MyInventory` after a successful inventory move (NetPlay). |
| VMInventoryOperations.h | VMInventoryOpState serialization | `Deserialize`/`SerializeInto` dropped, matching the core `VMAsyncState` (serialization dropped); the C# logic is described in a comment. |

Notes (not stubs):
- The callbacks above only run when `VM::GlobalLink()` is non-null, which is never the case on 3DS (it is kept faithful,
  NOT redirected to `CheckGlobalLink`). So inventory modes 1–3 (AddStackObjToInventory, RemoveTemp0ObjOfTypeFromInventory,
  CountObjectsOfType) set a blocking state and return CONTINUE_NEXT_TICK forever. This is a TSO-only primitive (opcode 67);
  TS1 content does not use it. If needed, route to `vm->CheckGlobalLink` like batch 1's VMTransferFunds (the stub's
  ConsumeInventory never calls back, so modes 2/3 would still block).
- C# NullReferenceException / InvalidCastException / IndexOutOfRange / KeyNotFound sites (null StackObject/Callee/SemiGlobal,
  null slot in VMSnap, Caller not an avatar in VMReach, `Slots.Slots[0][i]` lookups, `SLOT.HeightOffsets[Height-1]`,
  empty EventQueue, null `GetObjectById` result in VMRelationship, null tree name in VMRunTreeByName) throw
  `VMSimanticsException` (or `std::out_of_range` via `.at()` for Args/Locals) so the thread's catch handles them instead of
  segfaulting.
- `VMRunTreeByName` and `VMRemoveObjectInstance` are in C# namespace `FSO.SimAntics.Engine.Primitives`; ported into
  `FSO::SimAntics::Primitives` like all other primitives.
- `VMSubRoutine.cs` contains only `VMSubRoutineOperand`, which the core owns (`engine/VMSubRoutineOperand.h`); no
  primitives/VMSubRoutine.* files were produced.
- `Math.Round` (banker's rounding) is ported as `std::nearbyint` (default round-to-nearest-even) in VMReach and VMSnap.
