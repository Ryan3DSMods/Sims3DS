# Primitive stubs — batch 3

| File | Stub | What the original did |
|---|---|---|
| VMInvokePlugin.cpp | `context.VM.EODHost.ForceDisconnectObj(caller)` (event local < 0) | Server-side EOD host forcibly disconnected the caller's plugin session. `VM.EODHost` only exists on a TSO server; on 3DS it is always null so the call is dropped. The thread state is still cleared. |
| VMInvokePlugin.cpp | `context.VM.EODHost.SimanticsDeliver(localEvt, caller)` | Delivered the SimAntics-side event to the server plugin. Dropped (no EODHost). |
| VMInvokePlugin.cpp | `context.VM.EODHost.Connect(PluginID, serverObj, serverObj, null, true, VM)` and `Connect(PluginID, caller, obj, avatar, Joinable, VM)` (inside `if (VM.IsServer)`) | Started the server-side EOD plugin for the joinable server object and for the caller. `VM::IsServer()` is always false on 3DS, so the branches are empty. `VMEODPluginThreadState` creation, event popping into Locals/TempRegisters, `Ended` handling and the idle-event shortcut are kept. |

Core additions made for this batch (marked `// ADDED(primitives)`):
- `engine/VMThread.h`: `VMEODEvent` class (Code, Data) and the C# fields of `VMEODPluginThreadState` (AvatarID, ObjectID, Joinable, Ended, Events); serialization dropped.
- `VM.h`: `void SignalDialog(std::nullptr_t)` + `std::function<void()> OnDialogClosed` for the C# `SignalDialog(null)` (dialog dismissed) call in `VMDialogPrivateStrings::ExecuteGeneric`.
