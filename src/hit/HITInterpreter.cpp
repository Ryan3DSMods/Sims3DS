/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the SimsLib.

The Initial Developer of the Original Code is
Rhys Simpson. All Rights Reserved.

Contributor(s): Mats 'Afr0' Vederhus
*/
// Translated from FreeSims SimsVille/HIT/HITInterpreter.cs
#include "hit/HITInterpreter.h"
#include <algorithm>
#include "hit/HITThread.h"
#include "hit/HITVM.h"

namespace FSO { namespace HIT {

using namespace FSO::Files::HIT;

const HITInstruction HITInterpreter::Instructions[] = {
    NOP,            Note,           NoteOn,         NoteOff,        LoadB,          LoadL,          Set,            Call,
    Return,         Wait,           CallEntryPoint, WaitSamp,       End,            Jump,           Test,           NOP,
    Add,            Sub,            Div,            Mul,            Cmp,            Less,           Greater,        Not,
    Rand,           Abs,            Limit,          Error,          Assert,         AddToGroup,     RemoveFromGroup, GetVar,
    Loop,           SetLoop,        Callback,       SmartAdd,       SmartRemove,    SmartRemoveAll, SmartSetCrit,   SmartChoose,
    And,            NAnd,           Or,             NOr,            XOr,            Max,            Min,            Inc,
    Dec,            PrintReg,       PlayTrack,      KillTrack,      Push,           PushMask,       PushVars,       CallMask,
    CallPush,       Pop,            Test1,          Test2,          Test3,          Test4,          IfEqual,        IfNotEqual,
    IfGreater,      IfLess,         IfGreatOrEq,    IfLessOrEq,     SmartSetList,   SeqGroupKill,   SeqGroupWait,   SeqGroupReturn,
    GetSrcDataField, SeqGroupTrackID, SetLL,        SetLT,          SetTL,          WaitEqual,      WaitNotEqual,   WaitGreater,
    WaitLess,       WaitGreatOrEq,  WaitLessOrEq,   Duck,           Unduck,         TestX,          SetLG,          SetGL,
    Throw,          SetSrcDataField, StopTrack,     SetChanReg,     PlayNote,       StopNote,       KillNote,       SmartIndex,
    NoteOnLoop
};
const int HITInterpreter::InstructionCount = (int)(sizeof(Instructions) / sizeof(Instructions[0]));

// TS1(sims): a code target operand is a TrackData label of the thread's bank
static uint32_t Target(HITThread* thread, uint32_t label)
{
    return thread->ResGroup != nullptr ? thread->ResGroup->ResolveLabel(label) : label;
}

/// <summary>
/// Does nothing.
/// </summary>
HITResult HITInterpreter::NOP(HITThread* thread)
{
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Note(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Play a note, whose ID resides in the specified variable.
/// </summary>
HITResult HITInterpreter::NoteOn(HITThread* thread)
{
    auto dest = thread->ReadByte();

    thread->WriteVar(dest, thread->NoteOn());

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::NoteOff(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Sign-extend a 1-byte constant to 4 bytes and write to a variable.
/// </summary>
HITResult HITInterpreter::LoadB(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto value = (int8_t)thread->ReadByte();

    thread->WriteVar(dest, value);

    thread->SetFlags(value);

    return HITResult::CONTINUE;
}

/// <summary>
/// Write a 4-byte constant to a variable.
/// </summary>
HITResult HITInterpreter::LoadL(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto value = thread->ReadInt32();

    thread->WriteVar(dest, value);
    thread->SetFlags(value);

    return HITResult::CONTINUE;
}

/// <summary>
/// Copy the contents of one variable into another.
/// </summary>
HITResult HITInterpreter::Set(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadVar(thread->ReadByte());

    thread->WriteVar(dest, src);

    thread->SetFlags(src);

    return HITResult::CONTINUE;
}

/// <summary>
/// Call a function; push the instruction pointer and jump to the given address.
/// </summary>
HITResult HITInterpreter::Call(HITThread* thread)
{
    uint32_t targ = Target(thread, thread->ReadUInt32());
    thread->Stack.push_back((int32_t)thread->PC);
    thread->PC = targ;

    return HITResult::CONTINUE;
}

/// <summary>
/// Kill this thread.
/// </summary>
HITResult HITInterpreter::Return(HITThread* thread)
{
    return HITResult::KILL;
}

/// <summary>
/// Wait for a length of time in milliseconds, specified by a variable.
/// </summary>
HITResult HITInterpreter::Wait(HITThread* thread)
{
    auto src = thread->ReadByte();
    if (thread->WaitRemain == -1) thread->WaitRemain = thread->ReadVar(src);
    thread->WaitRemain -= 16; //assuming tick rate is 60 times a second
    if (thread->WaitRemain > 0)
    {
        thread->PC -= 2;
        return HITResult::HALT;
    }
    else
    {
        thread->WaitRemain = -1;
        return HITResult::CONTINUE;
    }
}

HITResult HITInterpreter::CallEntryPoint(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Wait for the previously selected note to finish playing.
/// </summary>
HITResult HITInterpreter::WaitSamp(HITThread* thread)
{
    if (thread->NoteActive(thread->LastNote()))
    {
        thread->PC--;
        return HITResult::HALT;
    }
    else
        // TS1(sims): carries on in the same tick (the C# halted once more), so looped sounds
        // (GenericLooped: note_on / wait_samp / loop) restart without an extra 16 ms gap
        return HITResult::CONTINUE;
}

/// <summary>
/// Return from this function; pop the instruction pointer from the stack and jump.
/// </summary>
HITResult HITInterpreter::End(HITThread* thread)
{
    if (thread->Stack.size() > 0)
    {
        thread->PC = (uint32_t)thread->Stack.back();
        thread->Stack.pop_back();
        return HITResult::CONTINUE;
    }
    else
    {
        return HITResult::KILL;
    }
}

/// <summary>
/// Jump to a given address.
/// </summary>
HITResult HITInterpreter::Jump(HITThread* thread)
{
    // TS1(sims): the operand is a 4-byte label, or one byte naming a variable that holds a label. A label is
    // recognised by being in the bank's TrackData table (the C# guessed from the first byte being > 15, which fails
    // for The Sims' small label ids).
    auto literal = thread->PeekUInt32();
    if (thread->ResGroup != nullptr && thread->ResGroup->IsLabel(literal))
    {
        thread->PC = Target(thread, thread->ReadUInt32());
        return HITResult::CONTINUE;
    }

    auto read = thread->ReadByte();

    if (read > 15) //literal
    {
        thread->PC--; //backtraaackkk
        thread->PC = Target(thread, thread->ReadUInt32());
    }
    else //no idea if there are collisions. if there are i'm blaming fatbag. >:)
    {
        thread->PC = Target(thread, (uint32_t)thread->ReadVar(read));
    }

    return HITResult::CONTINUE;
}

/// <summary>
/// Examine a variable and set the flags.
/// </summary>
HITResult HITInterpreter::Test(HITThread* thread)
{
    auto value = thread->ReadVar(thread->ReadByte());

    thread->SetFlags(value);

    return HITResult::CONTINUE;
}

/// <summary>
/// Increment a "dest" variable by a "src" variable.
/// </summary>
HITResult HITInterpreter::Add(HITThread* thread) //0x10
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    auto result = thread->ReadVar(dest) + thread->ReadVar(src);
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

/// <summary>
/// Decrement a "dest" variable by a "src" variable.
/// </summary>
HITResult HITInterpreter::Sub(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    auto result = thread->ReadVar(dest) - thread->ReadVar(src);
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

/// <summary>
/// Divide a "dest" variable by a "src" variable, and round the result towards zero (truncate).
/// </summary>
HITResult HITInterpreter::Div(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    auto d = thread->ReadVar(src);
    auto result = (d == 0) ? 0 : thread->ReadVar(dest) / d; // C#: DivideByZeroException killed the thread
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

/// <summary>
/// Multiply a "dest" variable by a "src" variable.
/// </summary>
HITResult HITInterpreter::Mul(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    auto result = thread->ReadVar(dest) * thread->ReadVar(src);
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

/// <summary>
/// Compare two variables and set the flags.
/// </summary>
HITResult HITInterpreter::Cmp(HITThread* thread) //same as sub, but does not set afterwards.
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    auto result = thread->ReadVar(dest) - thread->ReadVar(src);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Less(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Greater(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Not(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Generate a random number between "low" and "high" variables, inclusive,
/// and store the result in the "dest" variable.
/// </summary>
HITResult HITInterpreter::Rand(HITThread* thread)
{
    auto dest = thread->ReadByte();
    // TS1(sims): low/high name variables (the C# used the operand bytes themselves as the bounds)
    auto low = thread->ReadVar(thread->ReadByte());
    auto high = thread->ReadVar(thread->ReadByte());
    if (high < low) std::swap(high, low);

    auto result = HITVM::NextRandom(high + 1 - low) + low;
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Abs(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Limit(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Error(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Assert(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::AddToGroup(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::RemoveFromGroup(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::GetVar(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Jump back to the loop point (start of track subroutine by default).
/// </summary>
HITResult HITInterpreter::Loop(HITThread* thread) //0x20
{
    // TS1(sims): "start of track subroutine by default" (defaultsyms.txt); the C# jumped to -1 and died
    // TS1(sims): a loop whose sample is missing would spin forever; such a thread ends
    if (thread->NoteFailed) return HITResult::KILL;
    thread->PC = (thread->LoopPointer >= 0) ? (uint32_t)thread->LoopPointer : thread->EntryPC;
    return HITResult::CONTINUE;
}

/// <summary>
/// Set the loop point to the current position.
/// </summary>
HITResult HITInterpreter::SetLoop(HITThread* thread)
{
    thread->LoopPointer = (int32_t)thread->PC;
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Callback(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::SmartAdd(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::SmartRemove(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::SmartRemoveAll(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::SmartSetCrit(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Set the specified variable to a random entry from the selected hitlist.
/// </summary>
HITResult HITInterpreter::SmartChoose(HITThread* thread)
{
    auto dest = thread->ReadByte();
    thread->WriteVar(dest, (int)thread->HitlistChoose());
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::And(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::NAnd(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Or(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::NOr(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::XOr(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Find the higher of a "dest" variable and a "src" constant and store the result in the variable.
/// </summary>
HITResult HITInterpreter::Max(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadInt32();

    auto result = std::max(thread->ReadVar(dest), (int)src);
    thread->WriteVar(dest, result);

    thread->SetFlags(result);

    return HITResult::CONTINUE;
}

/// <summary>
/// Find the lower of a "dest" variable and a "src" constant and store the result in the variable.
/// </summary>
HITResult HITInterpreter::Min(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadInt32();

    auto result = std::min(thread->ReadVar(dest), (int)src);
    thread->WriteVar(dest, result);

    thread->SetFlags(result);
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Inc(HITThread* thread)
{
    // TS1(sims): inc var (one HouseParty track uses it; the C# treated it as a no-op without operand)
    auto dest = thread->ReadByte();
    auto result = thread->ReadVar(dest) + 1;
    thread->WriteVar(dest, result);
    thread->SetFlags(result);
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Dec(HITThread* thread) //0x30
{
    auto dest = thread->ReadByte(); // TS1(sims): see Inc
    auto result = thread->ReadVar(dest) - 1;
    thread->WriteVar(dest, result);
    thread->SetFlags(result);
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::PrintReg(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Play a track, whose ID resides in the specified variable.
/// </summary>
HITResult HITInterpreter::PlayTrack(HITThread* thread)
{
    auto dest = thread->ReadByte();
    // TS1(sims): starts the track (SimsSound's music helpers use it); the C# ignored it ("Not used in TSO.")
    if (thread->VM != nullptr) thread->VM->PlayTrack((uint32_t)thread->ReadVar(dest), thread->ResGroup);
    return HITResult::CONTINUE;
}

/// <summary>
/// Kill a track, whose ID resides in the specified variable.
/// </summary>
HITResult HITInterpreter::KillTrack(HITThread* thread)
{
    auto src = thread->ReadByte();
    if (thread->VM != nullptr) thread->VM->KillTrack((uint32_t)thread->ReadVar(src), thread); // TS1(sims)
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Push(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::PushMask(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::PushVars(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::CallMask(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::CallPush(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Pop(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::Test1(HITThread* thread)
{
    //no idea what these do. examples?
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Test2(HITThread* thread)
{
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Test3(HITThread* thread)
{
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Test4(HITThread* thread)
{
    return HITResult::CONTINUE;
}

/// <summary>
/// If the zero flag is set, jump to the given address.
/// </summary>
HITResult HITInterpreter::IfEqual(HITThread* thread)
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (thread->ZeroFlag) thread->PC = loc;

    return HITResult::CONTINUE;
}

/// <summary>
/// If the zero flag is not set, jump to the given address.
/// </summary>
HITResult HITInterpreter::IfNotEqual(HITThread* thread)
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (!thread->ZeroFlag) thread->PC = loc;

    return HITResult::CONTINUE;
}

/// <summary>
/// If the sign flag is not set and the zero flag is not set, jump to the given address
/// </summary>
HITResult HITInterpreter::IfGreater(HITThread* thread) //0x40
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (!thread->SignFlag && !thread->ZeroFlag) thread->PC = loc; //last set/compare result was > 0

    return HITResult::CONTINUE;
}

/// <summary>
/// If the sign flag is set, jump to the given address.
/// </summary>
HITResult HITInterpreter::IfLess(HITThread* thread)
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (thread->SignFlag) thread->PC = loc; //last set/compare result was < 0

    return HITResult::CONTINUE;
}

/// <summary>
/// If the sign flag is not set, jump to the given address.
/// </summary>
HITResult HITInterpreter::IfGreatOrEq(HITThread* thread)
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (!thread->SignFlag) thread->PC = loc; //last set/compare result was >= 0

    return HITResult::CONTINUE;
}

/// <summary>
/// If the sign flag is set or the zero flag is set, jump to the given address.
/// </summary>
HITResult HITInterpreter::IfLessOrEq(HITThread* thread)
{
    auto loc = Target(thread, thread->ReadUInt32());

    if (thread->SignFlag || thread->ZeroFlag) thread->PC = loc; //last set/compare result was <= 0

    return HITResult::CONTINUE;
}

/// <summary>
/// Choose a global hitlist, or 0 for the one local to the track (source: defaultsyms.txt).
/// </summary>
HITResult HITInterpreter::SmartSetList(HITThread* thread)
{ //sets the hitlist
    auto src = thread->ReadByte();
    thread->LoadHitlist((uint32_t)thread->ReadVar(src));

    return HITResult::CONTINUE;
}

/// <summary>
/// Kill an actor's vocals, given a constant ID.
/// </summary>
HITResult HITInterpreter::SeqGroupKill(HITThread* thread)
{
    auto src = thread->ReadByte();

    // TS1(sims): the operand is a variable holding a sequence-group id: the speaking actor (arg1 = Instance,
    // see VMAvatar::SubmitHITVars) or GroupMusic (1). The other threads of that group are killed and this thread
    // joins it, so a Sim saying something new cuts off what he was saying, and a new station replaces the old.
    // (C#: src == HITPerson.Instance -> KillVocals() of this thread only; other actors were a TODO.)
    auto group = thread->ReadVar(src);
    if (thread->VM != nullptr) thread->VM->KillSeqGroup(group, thread);
    thread->SeqGroup = group;

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::SeqGroupWait(HITThread* thread) //unused in the sims
{
    return HITResult::CONTINUE;
}

/// <summary>
/// Kill a sequence group with the return value specified by a constant.
/// </summary>
HITResult HITInterpreter::SeqGroupReturn(HITThread* thread)
{
    auto src = thread->ReadByte();
    (void)src;
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::GetSrcDataField(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();
    auto field = thread->ReadByte();

    // TS1(sims): getsrcdatafield dest, srcVar, fieldVar: data field <fieldVar> of the source object
    // <srcVar> (always this thread's own source: arg1). The fields are submitted by the owner when the event
    // starts (VMAvatar::SubmitHITVars): 0 = gender (0 male, 1 female, 2 child).
    // (C#: int ObjectVar = ReadVar(src); ObjectVar = ReadVar(10010 + ObjectVar);)
    (void)src;
    int fieldIndex = thread->ReadVar(field);
    int ObjectVar = (fieldIndex >= 0 && fieldIndex < (int)thread->ObjectVar.size()) ? thread->ObjectVar[(size_t)fieldIndex] : 0;
    thread->WriteVar(dest, ObjectVar);
    thread->SetFlags(ObjectVar);

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::SeqGroupTrackID(HITThread* thread)
{
    auto dest = thread->ReadByte(); //uhhhh
    auto src = thread->ReadByte();
    (void)dest; (void)src;
    return HITResult::CONTINUE;
}

/// <summary>
/// Copy the contents of one variable into another (equivalent to set and settt;
/// defaultsyms.txt says "ISN'T THIS THE SAME AS SET TOO?")
/// </summary>
HITResult HITInterpreter::SetLL(HITThread* thread)
{
    Set(thread);
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::SetLT(HITThread* thread)
{
    //set local... to... t... yeah i don't know either
    //might be object vars

    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();
    (void)dest; (void)src;

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::SetTL(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();
    (void)dest; (void)src;

    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Wait until two variables are equal.
/// </summary>
HITResult HITInterpreter::WaitEqual(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadByte();

    // TS1(sims): compares dest with src (the C# compared dest with itself and never waited)
    if (thread->ReadVar(dest) != thread->ReadVar(src))
    {
        thread->PC -= 3;
        return HITResult::HALT;
    }
    else
        return HITResult::CONTINUE;
}

HITResult HITInterpreter::WaitNotEqual(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::WaitGreater(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::WaitLess(HITThread* thread) //0x50
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::WaitGreatOrEq(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::WaitLessOrEq(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Ducks all audio with priority lower than this.
/// Imagine all the other sounds getting quieter when the fire music plays.
/// </summary>
HITResult HITInterpreter::Duck(HITThread* thread)
{
    thread->Duck();
    return HITResult::CONTINUE;
}

/// <summary>
/// Unducks all audio back to the volume before Duck() was called.
/// </summary>
HITResult HITInterpreter::Unduck(HITThread* thread)
{
    thread->Unduck();
    return HITResult::CONTINUE; //quack
}

HITResult HITInterpreter::TestX(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Set global = local (source: defaultsyms.txt).
/// </summary>
HITResult HITInterpreter::SetLG(HITThread* thread)
{
    auto local = thread->ReadByte();
    auto global = thread->ReadInt32();

    thread->VM->WriteGlobal(global, local);

    return HITResult::CONTINUE;
}

/// <summary>
/// Read globally, set locally (source: defaultsyms.txt).
/// </summary>
HITResult HITInterpreter::SetGL(HITThread* thread)
{
    auto dest = thread->ReadByte();
    auto src = thread->ReadInt32();

    int Global = thread->VM->ReadGlobal(src);
    thread->WriteVar(dest, Global);

    return HITResult::CONTINUE;
}

HITResult HITInterpreter::Throw(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::SetSrcDataField(HITThread* thread)
{
    auto value = thread->ReadByte();
    auto src = thread->ReadByte();
    auto field = thread->ReadByte();
    (void)value; (void)src; (void)field;

    //TODO: System for keeping track of which objects correspond to ObjectID.

    return HITResult::CONTINUE; //you can set these??? what
}

/// <summary>
/// Stop playing a track, whose ID resides in the specified variable.
/// </summary>
HITResult HITInterpreter::StopTrack(HITThread* thread)
{
    auto src = thread->ReadByte();
    (void)src;
    return HITResult::CONTINUE;
}

HITResult HITInterpreter::SetChanReg(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::PlayNote(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::StopNote(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

HITResult HITInterpreter::KillNote(HITThread* thread)
{
    return HITResult::CONTINUE; //unused in the sims
}

/// <summary>
/// Load a track ("index" variable) from a hitlist ("table" variable).
/// </summary>
HITResult HITInterpreter::SmartIndex(HITThread* thread)
{
    auto dest = thread->ReadVar(thread->ReadByte());
    auto index = thread->ReadVar(thread->ReadByte());

    thread->LoadHitlist((uint8_t)index);
    //Converting this to an int is a hack because WriteVar only takes an int... o_O
    int TrackID = (int)thread->LoadTrack(index);

    thread->WriteVar(dest, TrackID);

    return HITResult::CONTINUE; //Appears to be unused.
}

HITResult HITInterpreter::NoteOnLoop(HITThread* thread) //0x60
{
    auto dest = thread->ReadByte();
    thread->WriteVar(dest, thread->NoteLoop());

    return HITResult::CONTINUE;
}

}} // namespace FSO::HIT
