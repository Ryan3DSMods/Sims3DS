/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMChangeSuitOrAccessory.cs
#include "simantics/primitives/VMChangeSuitOrAccessory.h"
#include <cstring>
#include <string>
#include <algorithm>
#include "core/io.h"
#include "content/ContentManager.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSuitProvider.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/VM.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMChangeSuitOrAccessory::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMChangeSuitOrAccessoryOperand*>(args);

    // C#: (VMAvatar)context.Caller — throws InvalidCastException for non-avatars.
    auto avatar = dynamic_cast<VMAvatar*>(context->Caller);
    if (avatar == nullptr)
        throw VMSimanticsException("Specified cast is not valid. (VMAvatar)", context);

    if ((operand->Flags & VMChangeSuitOrAccessoryFlags::Update) == (int64_t)VMChangeSuitOrAccessoryFlags::Update)
    { //update default outfit with outfit in stringset 304 with index in temp 0
        auto str = context->Callee->Object->Resource->Get<STR>(304);
        if (str == nullptr)
            throw VMSimanticsException("Object reference not set to an instance of an object. (STR#304)", context);
        if (context->VM()->TS1) {
            // TS1(sims): The Sims 1 suit tables hold body strings ("mesh,BODY=texture") or suit names, not TSO
            // hex outfit ids (Convert.ToUInt64 threw and reset the Sim). Record the new normal body appearance.
            auto suit = str->GetString((context->Thread->TempRegisters[0]));
            auto part = Model::VMTS1BodyPart::Parse(suit);
            if (suit.find(',') != std::string::npos) avatar->TS1Appearance.Body = part;
            else avatar->TS1Appearance.Body.Mesh = suit;
            // TS1(sims): FreeSO "avatar.DefaultSuits.Daywear = VMOutfitReference.Parse(...); BodyOutfit = Daywear".
            if (suit.find(',') != std::string::npos) {
                auto oft = VMSuitProvider::RegisterTS1Outfit(suit, VMSuitProvider::BodyStringsOf(avatar));
                if (oft != 0) { avatar->DefaultSuits.Daywear = oft; avatar->BodyOutfit(oft); }
            }
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        // C#: Convert.ToUInt64(string, 16) — throws on bad input (std::stoull throws too; the thread catches both).
        avatar->DefaultSuits.Daywear = std::stoull(str->GetString((context->Thread->TempRegisters[0])), nullptr, 16);
        avatar->BodyOutfit(avatar->DefaultSuits.Daywear);
    }
    else
    {
        // TS1(sims): FreeSO flag 2 "use temp": the suit number is in Temp[SuitData].
        uint16_t data = operand->SuitData;
        if (context->VM()->TS1 && ((int64_t)operand->Flags & 2) == 2 && operand->SuitData < context->Thread->TempRegisters.size()) data = (uint16_t)(uint8_t)context->Thread->TempRegisters[operand->SuitData];
        auto suit = VMSuitProvider::GetSuit(context, operand->SuitScope, data);
        if (suit.IsNull())
        {
            return VMPrimitiveExitCode::GOTO_TRUE;
        }

        if (suit.Type == VMSuitResult::Kind::AppearanceName) // C#: suit is string
        {
            auto suitFile = suit.AppearanceName;
            auto apr = FSO::Content::Content::Get()->AvatarAppearances->Get(suitFile);
            if ((operand->Flags & VMChangeSuitOrAccessoryFlags::Remove) == (int64_t)VMChangeSuitOrAccessoryFlags::Remove)
            {
                avatar->BoundAppearances.erase(suitFile);
                // STUB(3ds): SimAvatar::RemoveAccessory is a no-op (no Vitaboy renderer); kept for structure.
                if (avatar->Avatar != nullptr) avatar->Avatar->RemoveAccessory(apr);
            }
            else
            {
                avatar->BoundAppearances.insert(suitFile);
                // STUB(3ds): SimAvatar::AddAccessory is a no-op (no Vitaboy renderer); kept for structure.
                if (avatar->Avatar != nullptr) avatar->Avatar->AddAccessory(apr);
            }
        } else if (suit.Type == VMSuitResult::Kind::OutfitID) // C#: suit is ulong
        {
            auto oft = suit.OutfitID;
            avatar->SetPersonData(Model::VMPersonDataVariable::CurrentOutfit, operand->SuitData);
            avatar->BodyOutfit(oft);
        }
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

// region VMPrimitiveOperand Members
void VMChangeSuitOrAccessoryOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    SuitData = io.ReadByte();
    SuitScope = (Scopes::VMSuitScope)io.ReadByte();
    Flags = (VMChangeSuitOrAccessoryFlags)io.ReadUInt16();
}

void VMChangeSuitOrAccessoryOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteByte(SuitData);
    io.WriteByte((uint8_t)SuitScope);
    io.WriteUInt16((uint16_t)Flags);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
