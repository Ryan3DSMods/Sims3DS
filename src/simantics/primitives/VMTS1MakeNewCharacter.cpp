/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1MakeNewCharacter.h.
#include "simantics/primitives/VMTS1MakeNewCharacter.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSuitProvider.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include "content/TS1CharacterFiles.h"
#include "content/TS1Clothing.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1SimDesc.h"
#include "content/TS1SkinCatalog.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdio>

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
using FSO::Content::TS1::TS1CharacterFiles;
using FSO::Content::TS1::TS1Clothing;
namespace CAS = FSO::Content::TS1::CAS;
using Model::VMPersonDataVariable;

static int16_t LocalAt(VMStackFrame* context, uint8_t i) { return i < context->Locals.size() ? context->Locals[i] : (int16_t)0; }

VMPrimitiveExitCode VMTS1MakeNewCharacter::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    //make the character iff, save it, and return their new neighbour id.
    auto operand = static_cast<VMTS1MakeNewCharacterOperand*>(args);
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr || prov->Neighbors == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    auto vm = context->VM();
    auto rnd = [vm](int n) { return n <= 0 ? 0 : (int)vm->Context->NextRandom((uint64_t)n); };

    auto color = LocalAt(context, operand->ColorLocal);
    auto age = LocalAt(context, operand->AgeLocal);
    auto gender = LocalAt(context, operand->GenderLocal);
    int16_t family = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY);

    TS1CharacterFiles::NewCharacter nc;
    std::vector<int16_t> pd;
    nc.FirstName = context->StackObject ? context->StackObject->Name() : std::string();

    if (operand->AvatarType == 0) {
        // the clone's original: ChemistrySet 'make clone' puts its neighbour id in the stack object (SOID := MyPD[31])
        VMAvatar* source = nullptr;
        auto sid = context->StackObjectID();
        for (auto e : vm->Entities) if (auto a = dynamic_cast<VMAvatar*>(e))
            if (sid > 0 && !a->Dead && a->GetPersonData((VMPersonDataVariable)VMTS1Neighbourhood::PD_NEIGHBOR_ID) == sid) { source = a; break; }
        if (source == nullptr) source = dynamic_cast<VMAvatar*>(context->StackObject);
        if (source == nullptr) source = dynamic_cast<VMAvatar*>(context->Caller);
        if (age < 0 && source != nullptr) {
            // a clone (ChemistrySet 'make clone', Charms 'Send Clone to Work'): the same Sim, new GUID
            nc.BodyStrings = VMSuitProvider::BodyStringsOf(source);
            nc.FirstName = source->Name();
            for (int i = 0; i < FSO::Content::TS1::TS1NeighborhoodProvider::TS1_PERSON_DATA_COUNT; i++)
                pd.push_back(source->GetPersonData((VMPersonDataVariable)i));
            if (pd.size() > (size_t)VMTS1Neighbourhood::PD_NEIGHBOR_ID) pd[(size_t)VMTS1Neighbourhood::PD_NEIGHBOR_ID] = 0;
        } else {
            CAS::CASSimDesc sim;
            sim.Age = (age < 18) ? CAS::CASAge::Child : CAS::CASAge::Adult;
            sim.Gender = (gender > 0) ? CAS::CASGender::Female : CAS::CASGender::Male;
            // person data 60 'skin color' (1 lgt, 2 drk, 3 med); FreeSO read 0 lgt, 1 med, 2 drk
            sim.Skin = (color == 2) ? CAS::CASSkin::Dark : (color == 3) ? CAS::CASSkin::Medium : CAS::CASSkin::Light;
            // a random everyday head and body of this kind: the Create-A-Sim catalogue (content/TS1SkinCatalog skips the
            // costume-trunk and Vacation winter/swim variants the game only puts on by itself). FreeSO picked from
            // the raw "c"/"b" suit collections.
            static CAS::CASCatalog catalog;
            if (!catalog.Built() && !catalog.Build()) return VMPrimitiveExitCode::GOTO_FALSE;
            auto& heads = catalog.Heads(sim.Age, sim.Gender, sim.Skin);
            auto& bodies = catalog.Bodies(sim.Age, sim.Gender, sim.Skin);
            if (heads.empty() || bodies.empty()) return VMPrimitiveExitCode::GOTO_FALSE;
            sim.Head = heads[(size_t)rnd((int)heads.size())];
            sim.Body = bodies[(size_t)rnd((int)bodies.size())];
            for (int t = 0; t < CAS::TraitCount; t++) sim.Traits[t] = 5; // FreeSO: 500 each ("todo: randomize")
            sim.InitTraits(rnd);
            nc.BodyStrings = sim.BodyStrings();
            // the Hot Date+ wardrobe the game gives every new Sim (as the Unleashed families have: 30..33)
            if (sim.Age == CAS::CASAge::Adult) {
                std::string code = std::string(sim.Gender == CAS::CASGender::Female ? "f" : "m") + "a" + sim.BodyType();
                std::string skin = CAS::SkinCode(sim.Skin);
                nc.BodyStrings.resize(34);
                nc.BodyStrings[30] = "f100" + code + "_original,BODY=f100" + code + skin + "_original";
                nc.BodyStrings[31] = "s100" + code + "_original,BODY=s100" + code + skin + "_original";
                nc.BodyStrings[32] = "l100" + code + "_original,BODY=l100" + code + skin + "_original";
                nc.BodyStrings[33] = "w100" + code + "_original,BODY=w100" + code + skin + "_original";
            }
            pd = sim.PersonData(family);
        }
    } else {
        // pets (FreeSO): TemplateDog / TemplateCat with the chosen coat
        bool dog = operand->AvatarType == 1;
        nc.TemplateGUID = dog ? TS1CharacterFiles::TEMPLATE_DOG_GUID : TS1CharacterFiles::TEMPLATE_CAT_GUID;
        auto tpl = FSO::Content::Content::Get()->WorldObjects->Get(nc.TemplateGUID, true);
        if (tpl == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
        auto str = tpl->Resource->Get<FSO::Files::Formats::IFF::Chunks::STR>(200);
        if (str != nullptr) for (int32_t i = 0; i < str->Length(); i++) nc.BodyStrings.push_back(str->GetString(i));
        if (nc.BodyStrings.size() < 14) nc.BodyStrings.resize(14);
        auto coats = TS1Clothing::Get().ValidOutfits({}, dog ? (int16_t)-1 : (int16_t)-2);
        if (coats.empty()) return VMPrimitiveExitCode::GOTO_FALSE;
        int16_t index = context->StackObject ? context->StackObject->GetAttribute(8) : (int16_t)0;
        if (index < 0 || index >= (int16_t)coats.size()) index = 0;
        nc.BodyStrings[0] = dog ? "dog" : "kat";
        nc.BodyStrings[1] = coats[(size_t)index].BodyString;
        nc.BodyStrings[12] = std::string(dog ? "dog" : "cat") + ((gender > 0) ? "female" : "male");
        nc.BodyStrings[13] = "27";
        // as the Unleashed pets (TemplateFamilyUnleashed *.FAM uChr): no head, no wardrobe
        nc.BodyStrings.resize(34);
        nc.BodyStrings[2] = "NONE";
        nc.BodyStrings[14] = "lgt";
        for (int i = 30; i < 34; i++) nc.BodyStrings[(size_t)i] = "NONE";
        pd.assign(80, 0);
        pd[58] = 27;
        pd[61] = family;
        pd[65] = (int16_t)((dog ? 8 : 16) | (gender > 0 ? 1 : 0)); // VMAvatar gender: dogmale 8, dogfemale 9, catmale 16, catfemale 17
    }

    // the character file and the neighbour
    std::set<uint32_t> avoid;
    for (auto& n : prov->Neighbors->Entries) if (n->Unknown1 == 1) avoid.insert(n->GUID);
    nc.GUID = TS1CharacterFiles::NewGUID([vm]() { return (uint32_t)vm->Context->NextRandom(0xFFFFFFFFull); }, avoid);
    if (nc.GUID == 0) return VMPrimitiveExitCode::GOTO_FALSE;
    auto userName = TS1CharacterFiles::NextUserName(prov->UserPath + "/Characters", TS1CharacterFiles::NeighbourNames(prov));
    std::string err;
    if (userName.empty() || TS1CharacterFiles::WriteCharacter(prov->UserPath, nc, userName, &err).empty()) {
        fprintf(stderr, "Make New Character: %s\n", err.c_str());
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
    auto n = TS1CharacterFiles::AddNeighbour(prov, nc.GUID, userName, pd);
    if (n == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;

    // C#: context.StackObjectID = n.NeighbourID (a neighbour id, not an object: see VMStackFrame::StackObjectRawID)
    context->StackObject = nullptr;
    context->StackObjectRawID = n->NeighbourID;
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMTS1MakeNewCharacterOperand::Read(const uint8_t* bytes)
{
    ColorLocal = bytes[0];
    AgeLocal = bytes[1];
    GenderLocal = bytes[2];
    AvatarType = bytes[3];
}

void VMTS1MakeNewCharacterOperand::Write(uint8_t* bytes)
{
    bytes[0] = ColorLocal; bytes[1] = AgeLocal; bytes[2] = GenderLocal; bytes[3] = AvatarType;
}

}}}
