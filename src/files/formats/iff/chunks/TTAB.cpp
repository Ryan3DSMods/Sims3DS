/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TTAB.cs
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/IffFile.h"
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

const float TTAB::AttenuationValues[5] = {
    0, //custom
    0, //none
    0.002f, //low
    0.02f, //medium
    0.1f, //high (falloff entirely in 10 tiles)
};

// TS1(sims): FreeSO
const float TTAB::TS1AttenuationValues[5] = {
    0, //custom
    0, //none
    0.1f, //low
    0.3f, //medium
    0.6f, //high
};
const float TTAB::VisitorAttenuationValues[5] = {
    0, //custom
    0, //none
    0.01f, //low
    0.02f, //medium
    0.03f, //high
};

const uint8_t TTABFieldEncode::widths[4] = { 5, 8, 13, 16 };
const uint8_t TTABFieldEncode::widths2[4] = { 6, 11, 21, 32 };

void TTAB::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        InteractionByIndex.clear();
        Interactions.clear();
        Interactions.resize(io.ReadUInt16());
        if (Interactions.size() == 0) return; //no interactions, don't bother reading remainder.
        auto version = io.ReadUInt16();
        if (version <= 3)
        {
            // TS1(sims): FreeSO — "DO NOT LOAD THIS TTAB TYPE" (e.g. Sofas.iff TTAB 130 'untitled tree table', an
            // unused early-format table whose read ran past the end of the chunk).
            Interactions.clear();
            return;
        }
        std::unique_ptr<IOProxy> iop;
        if (version != 9 && version != 10) iop.reset(new TTABNormal(&io));
        else
        {
            auto compressionCode = io.ReadByte();
            // FIX(sims): FreeSO reads compression code != 1 as a normal (uncompressed) table; FreeSims threw.
            if (compressionCode != 1) iop.reset(new TTABNormal(&io));
            else iop.reset(new TTABFieldEncode(&io)); //haven't guaranteed that this works, since none of the objects in the test lot use it.
        }
        for (size_t i = 0; i < Interactions.size(); i++)
        {
            auto result = new TTABInteraction();
            Interactions[i].reset(result);
            result->ActionFunction = iop->ReadUInt16();   
            result->TestFunction = iop->ReadUInt16();
            result->MotiveEntries = std::vector<TTABMotiveEntry>(iop->ReadUInt32());
            result->Flags = (TTABFlags)iop->ReadUInt32();
            result->TTAIndex = iop->ReadUInt32();
            if (version > 6) result->AttenuationCode = iop->ReadUInt32();
            result->AttenuationValue = iop->ReadFloat();
            result->AutonomyThreshold = iop->ReadUInt32();
            result->JoiningIndex = iop->ReadInt32();
            for (size_t j = 0; j < result->MotiveEntries.size(); j++)
            {
                TTABMotiveEntry motive;
                motive.MotiveIndex = (int32_t)j; // TS1(sims): FreeSO
                if (version > 6) motive.EffectRangeMinimum = iop->ReadInt16();
                motive.EffectRangeMaximum = iop->ReadInt16();
                if (version > 6) motive.PersonalityModifier = iop->ReadUInt16();
                result->MotiveEntries[j] = motive;
            }
            if (version > 9)
            {
                result->Flags2 = (TSOFlags)iop->ReadUInt32();
            }
            //C#: InteractionByIndex.Add(result.TTAIndex, result); (throws on duplicate key)
            if (InteractionByIndex.find(result->TTAIndex) != InteractionByIndex.end())
                throw std::runtime_error("An item with the same key has already been added.");
            InteractionByIndex[result->TTAIndex] = result;
        }
    }
    InitAutoInteractions(); // TS1(sims): FreeSO
}

// TS1(sims): FreeSO TTAB.InitAutoInteractions.
void TTAB::InitAutoInteractions()
{
    AutoInteractions.clear();
    for (auto& interaction : Interactions)
    {
        if (!interaction) continue;
        interaction->ActiveMotiveEntries.clear();
        for (size_t j = 0; j < interaction->MotiveEntries.size(); j++)
        {
            interaction->MotiveEntries[j].MotiveIndex = (int32_t)j;
            if (interaction->MotiveEntries[j].EffectRangeMaximum != 0) interaction->ActiveMotiveEntries.push_back(interaction->MotiveEntries[j]);
        }
        if (interaction->ActiveMotiveEntries.size() > 0) AutoInteractions.push_back(interaction.get());
    }
}

bool TTAB::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt16((uint16_t)Interactions.size());
        io.WriteUInt16(8); //version. don't save to high version cause we can't write out using the complex io proxy.
        for (size_t i = 0; i < Interactions.size(); i++)
        {
            auto& action = Interactions[i];
            io.WriteUInt16(action->ActionFunction);
            io.WriteUInt16(action->TestFunction);
            io.WriteUInt32((uint32_t)action->MotiveEntries.size());
            io.WriteUInt32((uint32_t)action->Flags);
            io.WriteUInt32(action->TTAIndex);
            io.WriteUInt32(action->AttenuationCode);
            io.WriteFloat(action->AttenuationValue);
            io.WriteUInt32(action->AutonomyThreshold);
            io.WriteInt32(action->JoiningIndex);
            for (size_t j=0; j < action->MotiveEntries.size(); j++)
            {
                auto& mot = action->MotiveEntries[j];
                io.WriteInt16(mot.EffectRangeMinimum);
                io.WriteInt16(mot.EffectRangeMaximum);
                io.WriteUInt16(mot.PersonalityModifier);
            }
            //TODO: write out TSOFlags
        }
    }
    return true;
}

void TTAB::InsertInteraction(TTABInteraction* action, int32_t index)
{
    if (index == -1) index = 0;
    Interactions.insert(Interactions.begin() + index, std::unique_ptr<TTABInteraction>(action));

    if (InteractionByIndex.find(action->TTAIndex) == InteractionByIndex.end()) InteractionByIndex[action->TTAIndex] = action;
    InitAutoInteractions(); // TS1(sims): FreeSO
}

void TTAB::DeleteInteraction(int32_t index)
{
    auto action = Interactions[index].get();
    if (index == -1) index = 0;
    auto it = InteractionByIndex.find(action->TTAIndex);
    if (it != InteractionByIndex.end()) InteractionByIndex.erase(it);
    Interactions.erase(Interactions.begin() + index);
    InitAutoInteractions(); // TS1(sims): FreeSO
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
