# STUBS — primitives batch 1

| File | Stub | Original behaviour |
|---|---|---|
| VMPlaySound.cpp | `STUB(3ds)` file-local `HITVM` whose `PlaySoundEvent` returns `nullptr` | `HITVM.Get().PlaySoundEvent(fwav.Name)` started a HIT sound thread; the FWAV lookup, owner selection and duplicate check are kept, no `VMSoundEntry` is ever added. |
| VMPlaySound.cpp | `STUB(3ds)` HITThread interruptable check dropped | `if ((lastThread?.Sound as HITThread)?.Interruptable == true) lastThread = null;` (no HITThread type on 3DS). |
| VMPlaySound.cpp | `STUB(3ds)` `SubmitHITVars` dropped | `((VMAvatar)owner).SubmitHITVars((HITThread)thread)` pushed avatar vars into the HIT thread. |
| VMStopAllSounds.cpp | `STUB(3ds)` note | `Sound.RemoveOwner` is the no-op `HITSound` shell from `model/Sound/VMSoundEntry.h`; SoundThreads are still cleared. |
| VMTransferFunds.cpp | `STUB(3ds)` GlobalLink fallback | `VM::GlobalLink()` is always null on 3DS; the async path now uses `vm->CheckGlobalLink` (the `VMTSOGlobalLinkStub`, which FreeSims single-player used as its GlobalLink) so the primitive does not block forever. |
| VMTransferFunds.cpp | `STUB(3ds)` `ApplyAsyncResponse` | Replaces `vm.SendCommand(new VMNetAsyncResponseCmd(id, state))`: sets the caller thread's BlockingState when it is a VMTransferFundsState (the command's Execute). Visual budget update skipped (only ran for non-stub links). |
| VMTransferFunds.h | VMTransferFundsState serialization | `Deserialize`/`SerializeInto` dropped, matching the core `VMAsyncState` (serialization dropped). |

Other deviations (not stubs): C# NullReferenceException/InvalidCastException sites (null StackObject/owner, Caller not an
avatar, null animation, null TSOState, out-of-range attenuation code) throw `VMSimanticsException` so the thread's catch
handles them instead of segfaulting. `VMAnimateSim` guards `avatar->Avatar` (SimAvatar stub) against null before
setting hand gestures. `VMAnimateSim` is in `FSO::SimAntics::Primitives` (C# had `FSO.SimAntics.Engine.Primitives`).
