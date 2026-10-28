/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the SimsLib.

The Initial Developer of the Original Code is
Rhys Simpson. All Rights Reserved.

Contributor(s): Mats 'Afr0' Vederhus
*/
// Translated from FreeSims SimsVille/HIT/HITInterpreter.cs (namespace TSO.HIT -> FSO::HIT)
//
// TS1(sims): FreeSims' interpreter follows The Sims Online's HIT compiler. The Sims 1 banks (compiled with
// the defaultsyms.txt shipped in ExpansionPack7/Sound/Sound.far) differ in a few places, all marked below:
//   * branch/call operands are TrackData *label ids* (resolved through the bank's .hot [TrackData]), not addresses;
//   * jump takes either a 4-byte label or a 1-byte variable holding a label;
//   * rand/waiteq operands are variables; "patch" is variable 0x32;
//   * getsrcdatafield reads the source object's data fields (field 0 = gender: 0 male, 1 female, 2 child);
//   * seqgroup_kill kills the other threads of a sequence group (an actor's id for Sim voices, 1 for music);
//   * play_trk / kill_trk start and stop tracks; loop without set_loop returns to the track start.
#pragma once
#include <cstdint>

namespace FSO { namespace HIT {

class HITThread;

enum class HITResult
{
    CONTINUE,
    HALT,
    KILL
};

typedef HITResult (*HITInstruction)(HITThread* thread);

class HITInterpreter
{
public:
    static const HITInstruction Instructions[];
    static const int InstructionCount;

    static HITResult NOP(HITThread* thread);
    static HITResult Note(HITThread* thread);
    static HITResult NoteOn(HITThread* thread);
    static HITResult NoteOff(HITThread* thread);
    static HITResult LoadB(HITThread* thread);
    static HITResult LoadL(HITThread* thread);
    static HITResult Set(HITThread* thread);
    static HITResult Call(HITThread* thread);
    static HITResult Return(HITThread* thread);
    static HITResult Wait(HITThread* thread);
    static HITResult CallEntryPoint(HITThread* thread);
    static HITResult WaitSamp(HITThread* thread);
    static HITResult End(HITThread* thread);
    static HITResult Jump(HITThread* thread);
    static HITResult Test(HITThread* thread);
    static HITResult Add(HITThread* thread);
    static HITResult Sub(HITThread* thread);
    static HITResult Div(HITThread* thread);
    static HITResult Mul(HITThread* thread);
    static HITResult Cmp(HITThread* thread);
    static HITResult Less(HITThread* thread);
    static HITResult Greater(HITThread* thread);
    static HITResult Not(HITThread* thread);
    static HITResult Rand(HITThread* thread);
    static HITResult Abs(HITThread* thread);
    static HITResult Limit(HITThread* thread);
    static HITResult Error(HITThread* thread);
    static HITResult Assert(HITThread* thread);
    static HITResult AddToGroup(HITThread* thread);
    static HITResult RemoveFromGroup(HITThread* thread);
    static HITResult GetVar(HITThread* thread);
    static HITResult Loop(HITThread* thread);
    static HITResult SetLoop(HITThread* thread);
    static HITResult Callback(HITThread* thread);
    static HITResult SmartAdd(HITThread* thread);
    static HITResult SmartRemove(HITThread* thread);
    static HITResult SmartRemoveAll(HITThread* thread);
    static HITResult SmartSetCrit(HITThread* thread);
    static HITResult SmartChoose(HITThread* thread);
    static HITResult And(HITThread* thread);
    static HITResult NAnd(HITThread* thread);
    static HITResult Or(HITThread* thread);
    static HITResult NOr(HITThread* thread);
    static HITResult XOr(HITThread* thread);
    static HITResult Max(HITThread* thread);
    static HITResult Min(HITThread* thread);
    static HITResult Inc(HITThread* thread);
    static HITResult Dec(HITThread* thread);
    static HITResult PrintReg(HITThread* thread);
    static HITResult PlayTrack(HITThread* thread);
    static HITResult KillTrack(HITThread* thread);
    static HITResult Push(HITThread* thread);
    static HITResult PushMask(HITThread* thread);
    static HITResult PushVars(HITThread* thread);
    static HITResult CallMask(HITThread* thread);
    static HITResult CallPush(HITThread* thread);
    static HITResult Pop(HITThread* thread);
    static HITResult Test1(HITThread* thread);
    static HITResult Test2(HITThread* thread);
    static HITResult Test3(HITThread* thread);
    static HITResult Test4(HITThread* thread);
    static HITResult IfEqual(HITThread* thread);
    static HITResult IfNotEqual(HITThread* thread);
    static HITResult IfGreater(HITThread* thread);
    static HITResult IfLess(HITThread* thread);
    static HITResult IfGreatOrEq(HITThread* thread);
    static HITResult IfLessOrEq(HITThread* thread);
    static HITResult SmartSetList(HITThread* thread);
    static HITResult SeqGroupKill(HITThread* thread);
    static HITResult SeqGroupWait(HITThread* thread);
    static HITResult SeqGroupReturn(HITThread* thread);
    static HITResult GetSrcDataField(HITThread* thread);
    static HITResult SeqGroupTrackID(HITThread* thread);
    static HITResult SetLL(HITThread* thread);
    static HITResult SetLT(HITThread* thread);
    static HITResult SetTL(HITThread* thread);
    static HITResult WaitEqual(HITThread* thread);
    static HITResult WaitNotEqual(HITThread* thread);
    static HITResult WaitGreater(HITThread* thread);
    static HITResult WaitLess(HITThread* thread);
    static HITResult WaitGreatOrEq(HITThread* thread);
    static HITResult WaitLessOrEq(HITThread* thread);
    static HITResult Duck(HITThread* thread);
    static HITResult Unduck(HITThread* thread);
    static HITResult TestX(HITThread* thread);
    static HITResult SetLG(HITThread* thread);
    static HITResult SetGL(HITThread* thread);
    static HITResult Throw(HITThread* thread);
    static HITResult SetSrcDataField(HITThread* thread);
    static HITResult StopTrack(HITThread* thread);
    static HITResult SetChanReg(HITThread* thread);
    static HITResult PlayNote(HITThread* thread);
    static HITResult StopNote(HITThread* thread);
    static HITResult KillNote(HITThread* thread);
    static HITResult SmartIndex(HITThread* thread);
    static HITResult NoteOnLoop(HITThread* thread);
};

}} // namespace FSO::HIT
