# STUBS — primitives batch 2

| File | Stub | Original behaviour |
|---|---|---|
| VMChangeSuitOrAccessory.cpp | `STUB(3ds)` notes on `avatar->Avatar->AddAccessory/RemoveAccessory` | Bound/unbound the accessory `.apr` on the Vitaboy SimAvatar mesh. The calls are kept, but `SimAvatar::AddAccessory/RemoveAccessory` are core no-op stubs (vitaboy/Appearance.h); `BoundAppearances` is still updated. Guarded against a null `Avatar`. |

Core addition: `VMRoutingFrame::OwnedSlot` (`std::unique_ptr<SLOTItem>`, marked `// ADDED(primitives)` in
engine/VMRoutingFrame.h). VMGotoRelativePosition builds a `SLOTItem` on the fly and `InitRoutes` keeps the raw pointer in
`Slot` for later re-routing; in C# the GC kept it alive, here the routing frame owns it.

Other deviations (not stubs):
- C# NullReferenceException / InvalidCastException / IndexOutOfRange sites throw (`VMSimanticsException`, or
  `std::out_of_range` via `.at()`) so the thread's catch handles them: null ref object in VMFindLocationFor, Caller not an
  avatar in VMGotoRelativePosition / VMChangeSuitOrAccessory, missing STR#304, null target in VMRefresh lighting refresh,
  out-of-range Locals/Args/FunctionToEntryPoint/direction-table indices.
- VMExpression: C# unchecked int semantics emulated (wrapping +, -, *, ++, --; `1 << n` masks n to 5 bits;
  `int.MinValue / -1` throws like C#; `% -1` returns 0 instead of trapping; `(short)Math.Sqrt(negative)` gives 0).
- VMExpression / VMRunFunctionalTree were in C# namespace `FSO.SimAntics.Engine.Primitives`; placed in
  `FSO::SimAntics::Primitives` like the rest.
- VMRunFunctionalTreeOperand::ChangeIcon setter keeps the C# bug (it reads the getter, not `value`).
- `Direction` log2 (VMFindLocationFor modes 3/4, VMDrop) uses `nearbyint(log(x)/log(2))` = C# `Math.Round(Math.Log(x, 2))`.
