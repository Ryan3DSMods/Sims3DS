/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TTAB.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <memory>
#include <cstring>
#include <unordered_map>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

enum class TTABFlags : int32_t
{
    AllowVisitors = 1, //COVERED, TODO for no TSOFlags? (default to only roomies, unless this flag set)
    Joinable = 1 << 1, //TODO
    RunImmediately = 1 << 2, //COVERED
    AllowConsecutive = 1 << 3, //TODO

    Debug = 1 << 7, //COVERED: only available to roomies for now
    AutoFirstSelect = 1 << 8, //TODO (autonomus first select?)
    Leapfrog = 1 << 9, //COVERED
    MustRun = 1 << 10, //TODO (where would this NOT run?)
    AllowDogs = 1 << 11, //COVERED
    AllowCats = 1 << 12, //COVERED

    TS1AllowCats = 1 << 9,
    TS1AllowDogs = 1 << 10,

    // TS1(sims): FreeSO TTAB.cs TS1 flags (FreeSims did not have them).
    TS1NoChild = 1 << 4,
    TS1NoDemoChild = 1 << 5,
    TS1NoAdult = 1 << 6,


    TSOAvailableCarrying = 1 << 16, //COVERED
    TSOIsRepair = 1 << 17, //TODO (only available when wear = 0)
    TSORunCheckAlways = 1 << 18, //TODO
    TSOAvailableWhenDead = 1<<19, //COVERED
    FSOPushTail = 1 << 30,
    FSOPushHead = 1 << 29,
    // TS1(sims): FreeSO. Pushed interactions skip the permission/check tests (FreeSims abused MustRun, which is
    // TS1AllowDogs in The Sims 1 data).
    FSOSkipPermissions = 1 << 28,
    FSODirectControl = 1 << 27,
};
FSO_FLAGS_ENUM(TTABFlags)

enum class TSOFlags : int32_t
{
    NonEmpty = 1, //if this is the only flag set, flags aren't empty intentionally. force Owner, Roommates, Friends to on
    AllowObjectOwner = 1 << 1, //COVERED
    AllowRoommates = 1 << 2, //COVERED
    AllowFriends = 1 << 3, //TODO
    AllowVisitors = 1 << 4, //COVERED
    AllowGhost = 1 << 5, //COVERED
    UnderParentalControl = 1 << 6, //TODO: interactions always available
    AllowCSRs = 1 << 7 //COVERED: only available to admins
};
FSO_FLAGS_ENUM(TSOFlags)

enum class InteractionMaskFlags : int32_t
{
    AvailableWhenCarrying = 1,
    IsRepair = 1<<1,
    RunCheckAlways = 1 << 2,
    AvailableWhenDead = 1 << 3,
};
FSO_FLAGS_ENUM(InteractionMaskFlags)

/// <summary>
/// Represents a motive entry in a TTAB chunk.
/// </summary>
struct TTABMotiveEntry
{
    int32_t MotiveIndex = 0; // TS1(sims): FreeSO — index of this entry (= VMMotive), not saved
    int16_t EffectRangeMinimum = 0;
    // NOTE: FreeSO calls this EffectRangeDelta: it is the ad range added to the minimum, not a maximum.
    int16_t EffectRangeMaximum = 0;
    uint16_t PersonalityModifier = 0;
};

/// <summary>
/// Represents an interaction in a TTAB chunk.
/// </summary>
class TTABInteraction
{
public:
    uint16_t ActionFunction = 0;
    uint16_t TestFunction = 0;
    std::vector<TTABMotiveEntry> MotiveEntries;
    TTABFlags Flags = (TTABFlags)0;
    uint32_t TTAIndex = 0;
    uint32_t AttenuationCode = 0;
    float AttenuationValue = 0;
    uint32_t AutonomyThreshold = 0;
    int32_t JoiningIndex = 0;
    TSOFlags Flags2 = (TSOFlags)0x1f; //allow a lot of things

    // TS1(sims): FreeSO — the motive entries with a non-zero ad range; populated by TTAB::InitAutoInteractions.
    std::vector<TTABMotiveEntry> ActiveMotiveEntries;

    InteractionMaskFlags MaskFlags() const
    {
        return (InteractionMaskFlags)(((int32_t)Flags >> 16) & 0xF);
    }
    void MaskFlags(InteractionMaskFlags value)
    {
        Flags = (TTABFlags)(((int32_t)Flags & 0xFFFF) | ((int32_t)value << 16));
    }

private:
    bool FlagGet(TTABFlags f) const { return (Flags & f) > 0; }
    void FlagSet(TTABFlags f, bool value) { Flags = (TTABFlags)(Flags & ~f); if (value) Flags = (TTABFlags)(Flags | f); }
    bool Flag2Get(TSOFlags f) const { return (Flags2 & f) > 0; }
    void Flag2Set(TSOFlags f, bool value) { Flags2 = (TSOFlags)(Flags2 & ~f); if (value) Flags2 = (TSOFlags)(Flags2 | f); }
    bool MaskGet(InteractionMaskFlags f) const { return (MaskFlags() & f) > 0; }
    void MaskSet(InteractionMaskFlags f, bool value) { auto m = (InteractionMaskFlags)(MaskFlags() & ~f); if (value) m = (InteractionMaskFlags)(m | f); MaskFlags(m); }

public:
    //ALLOW
    bool AllowVisitors() const { return FlagGet(TTABFlags::AllowVisitors); }
    void AllowVisitors(bool value) { FlagSet(TTABFlags::AllowVisitors, value); }
    bool AllowFriends() const { return Flag2Get(TSOFlags::AllowFriends); }
    void AllowFriends(bool value) { Flag2Set(TSOFlags::AllowFriends, value); }
    bool AllowRoommates() const { return Flag2Get(TSOFlags::AllowRoommates); }
    void AllowRoommates(bool value) { Flag2Set(TSOFlags::AllowRoommates, value); }
    bool AllowObjectOwner() const { return Flag2Get(TSOFlags::AllowObjectOwner); }
    void AllowObjectOwner(bool value) { Flag2Set(TSOFlags::AllowObjectOwner, value); }
    bool UnderParentalControl() const { return Flag2Get(TSOFlags::UnderParentalControl); }
    void UnderParentalControl(bool value) { Flag2Set(TSOFlags::UnderParentalControl, value); }
    bool AllowCSRs() const { return Flag2Get(TSOFlags::AllowCSRs); }
    void AllowCSRs(bool value) { Flag2Set(TSOFlags::AllowCSRs, value); }
    bool AllowGhosts() const { return Flag2Get(TSOFlags::AllowGhost); }
    void AllowGhosts(bool value) { Flag2Set(TSOFlags::AllowGhost, value); }
    bool AllowCats() const { return FlagGet(TTABFlags::AllowCats); }
    void AllowCats(bool value) { FlagSet(TTABFlags::AllowCats, value); }
    bool AllowDogs() const { return FlagGet(TTABFlags::AllowDogs); }
    void AllowDogs(bool value) { FlagSet(TTABFlags::AllowDogs, value); }

    //FLAGS
    bool Debug() const { return FlagGet(TTABFlags::Debug); }
    void Debug(bool value) { FlagSet(TTABFlags::Debug, value); }
    bool Leapfrog() const { return FlagGet(TTABFlags::Leapfrog); }
    void Leapfrog(bool value) { FlagSet(TTABFlags::Leapfrog, value); }
    bool MustRun() const { return FlagGet(TTABFlags::MustRun); }
    void MustRun(bool value) { FlagSet(TTABFlags::MustRun, value); }
    bool AutoFirst() const { return FlagGet(TTABFlags::AutoFirstSelect); }
    void AutoFirst(bool value) { FlagSet(TTABFlags::AutoFirstSelect, value); }
    bool RunImmediately() const { return FlagGet(TTABFlags::RunImmediately); }
    void RunImmediately(bool value) { FlagSet(TTABFlags::RunImmediately, value); }
    bool AllowConsecutive() const { return FlagGet(TTABFlags::AllowConsecutive); }
    void AllowConsecutive(bool value) { FlagSet(TTABFlags::AllowConsecutive, value); }

    bool Carrying() const { return MaskGet(InteractionMaskFlags::AvailableWhenCarrying); }
    void Carrying(bool value) { MaskSet(InteractionMaskFlags::AvailableWhenCarrying, value); }
    bool Repair() const { return MaskGet(InteractionMaskFlags::IsRepair); }
    void Repair(bool value) { MaskSet(InteractionMaskFlags::IsRepair, value); }
    bool AlwaysCheck() const { return MaskGet(InteractionMaskFlags::RunCheckAlways); }
    void AlwaysCheck(bool value) { MaskSet(InteractionMaskFlags::RunCheckAlways, value); }
    bool WhenDead() const { return MaskGet(InteractionMaskFlags::AvailableWhenDead); }
    void WhenDead(bool value) { MaskSet(InteractionMaskFlags::AvailableWhenDead, value); }
};

/// <summary>
/// This chunk type defines a list of interactions for an object and assigns a BHAV subroutine
/// for each interaction. The pie menu labels shown to the user are stored in a TTAs chunk with
/// the same ID.
/// </summary>
class TTAB : public IffChunk
{
public:
    IFF_CHUNK_TYPE("TTAB")

    std::vector<std::unique_ptr<TTABInteraction>> Interactions;
    std::unordered_map<uint32_t, TTABInteraction*> InteractionByIndex;
    // TS1(sims): FreeSO — interactions that advertise at least one motive (the only ones autonomy considers).
    std::vector<TTABInteraction*> AutoInteractions;
    void InitAutoInteractions();

    static const float AttenuationValues[5];
    // TS1(sims): FreeSO TTAB.AttenuationValues / VisitorAttenuationValues (The Sims 1 autonomy: score / (1 + atten * distance)).
    // FreeSims' AttenuationValues above are its TSO-style linear falloff constants.
    static const float TS1AttenuationValues[5];
    static const float VisitorAttenuationValues[5];

    /// <summary>
    /// Reads a TTAB chunk from a stream.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream object holding a TTAB chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;

    /// Takes ownership of `action`.
    void InsertInteraction(TTABInteraction* action, int32_t index);

    void DeleteInteraction(int32_t index);
};

class IOProxy
{
public:
    virtual ~IOProxy() {}
    virtual uint16_t ReadUInt16() = 0;
    virtual int16_t ReadInt16() = 0;
    virtual int32_t ReadInt32() = 0;
    virtual uint32_t ReadUInt32() = 0;
    virtual float ReadFloat() = 0;

    st::Reader* io;
    explicit IOProxy(st::Reader* io)
    {
        this->io = io;
    }
};

class TTABNormal : public IOProxy
{
public:
    uint16_t ReadUInt16() override { return io->ReadUInt16(); }
    int16_t ReadInt16() override { return io->ReadInt16(); }
    int32_t ReadInt32() override { return io->ReadInt32(); }
    uint32_t ReadUInt32() override { return io->ReadUInt32(); }
    float ReadFloat() override { return io->ReadFloat(); }

    explicit TTABNormal(st::Reader* io) : IOProxy(io) { }
};

/// <summary>
/// Used to read values from field encoded stream.
/// </summary>
class TTABFieldEncode : public IOProxy
{
private:
    uint8_t bitPos = 0;
    uint8_t curByte = 0;
    static const uint8_t widths[4];
    static const uint8_t widths2[4];

public:
    void setBytePos(int32_t n)
    {
        io->Seek(st::SeekOrigin::Begin, n);
        curByte = io->ReadByte();
        bitPos = 0;
    }

    uint16_t ReadUInt16() override
    {
        return (uint16_t)ReadField(false);
    }

    int16_t ReadInt16() override
    {
        return (int16_t)ReadField(false);
    }

    int32_t ReadInt32() override
    {
        return (int32_t)ReadField(true);
    }

    uint32_t ReadUInt32() override
    {
        return (uint32_t)ReadField(true);
    }

    float ReadFloat() override
    {
        // TS1(sims): FreeSO (IffFieldEncode) — the field holds the float's bits. FreeSims converted the
        // integer ("this is incredibly wrong"), which gave custom attenuations like -8.4e8.
        uint32_t data = (uint32_t)ReadField(true);
        float f;
        std::memcpy(&f, &data, sizeof(f));
        return f;
    }

private:
    int64_t ReadField(bool big)
    {
        if (ReadBit() == 0) return 0;

        uint32_t code = ReadBits(2);
        uint8_t width = (big)?widths2[code]:widths[code];
        int64_t value = ReadBits(width);
        value |= -(value & ((int64_t)1 << (width-1)));

        return value;
    }

    uint32_t ReadBits(int n)
    {
        uint32_t total = 0;
        for (int i = 0; i < n; i++)
        {
            total += (uint32_t)((uint32_t)ReadBit() << ((n - i)-1));
        }
        return total;
    }

    uint8_t ReadBit()
    {
        uint8_t result = (uint8_t)((curByte & (1 << (7 - bitPos))) >> (7 - bitPos));
        if (++bitPos > 7)
        {
            bitPos = 0;
            //C#: try { curByte = io.ReadByte(); } catch { curByte = 0; } (no more data, read 0)
            if (io->Position() < io->Length()) curByte = io->ReadByte();
            else curByte = 0; //no more data, read 0
        }
        return result;
    }

public:
    explicit TTABFieldEncode(st::Reader* io) : IOProxy(io)
    {
        curByte = io->ReadByte();
        bitPos = 0;
    }
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
