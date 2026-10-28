/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1Outfits.h.
#include "simantics/engine/VMTS1Outfits.h"
#include "simantics/engine/VMSuitProvider.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/entities/VMAvatar.h"
#include "content/ContentManager.h"
#include "content/TS1CharacterFiles.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "content/TS1SimDesc.h"
#include "content/TS1SkinCatalog.h"
#include "simantics/VMContext.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Engine { namespace Utils {

using FSO::Content::TS1::TS1Clothing;
using FSO::Files::Formats::IFF::Chunks::STR;

std::string VMTS1Outfits::PetType(VMAvatar* avatar) {
    if (avatar == nullptr) return "";
    if (avatar->IsDog()) return "dog";
    if (avatar->IsCat()) return "cat";
    return "";
}

std::vector<TS1Clothing::Outfit> VMTS1Outfits::ValidOutfits(VMAvatar* avatar, int16_t outfitType) {
    if (avatar == nullptr) return TS1Clothing::Get().ValidOutfits({}, outfitType < 0 ? outfitType : (int16_t)-1);
    return TS1Clothing::Get().ValidOutfits(VMSuitProvider::BodyStringsOf(avatar), outfitType, PetType(avatar));
}

int16_t VMTS1Outfits::GetSuitIndex(VMAvatar* avatar, int16_t outfitType) {
    if (avatar == nullptr) return -1;
    return TS1Clothing::Get().SuitIndex(VMSuitProvider::BodyStringsOf(avatar), outfitType, PetType(avatar));
}

int16_t VMTS1Outfits::CountSuits(VMAvatar* avatar, int16_t outfitType) {
    return (int16_t)ValidOutfits(avatar, outfitType).size();
}

void VMTS1Outfits::SaveBodyStrings(VMAvatar* avatar, const std::vector<std::string>& bs) {
    if (avatar == nullptr || avatar->Object == nullptr) return;
    auto str = avatar->Object->Resource->Get<STR>(avatar->Object->OBJ->BodyStringID);
    if (str != nullptr) {
        for (size_t i = 0; i < bs.size(); i++) {
            if ((int32_t)i < str->Length()) { if (str->GetString((int32_t)i) != bs[i]) str->SetString((int32_t)i, bs[i]); }
            else {
                FSO::Files::Formats::IFF::Chunks::STRItem item;
                item.Value = bs[i];
                str->InsertString((int32_t)i, item);
            }
        }
    }
    auto guid = avatar->Object->OBJ->GUID;
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov != nullptr) prov->DirtyAvatars.insert(guid); // FreeSO Neighborhood.AvatarChanged
    // The game keeps the Sim's wardrobe in its character file; this port writes it right away (the save paths of
    // the front end only rewrite Neighborhood.iff and the houses). Objects.far NPCs / templates are not written.
    auto path = FSO::Content::TS1::TS1CharacterFiles::CharacterFileOf(guid);
    if (!path.empty()) FSO::Content::TS1::TS1CharacterFiles::ReplaceBodyStrings(path, bs);
}

int16_t VMTS1Outfits::SetSuit(VMAvatar* avatar, int16_t outfitType, int16_t index) {
    if (avatar == nullptr) return -2;
    auto bs = VMSuitProvider::BodyStringsOf(avatar);
    auto pet = PetType(avatar);
    auto prev = TS1Clothing::Get().SetSuit(bs, outfitType, index, pet);
    if (prev == -2) return prev;
    SaveBodyStrings(avatar, bs);
    if (!pet.empty() || outfitType == 0) {
        // the everyday outfit (pets: the coat) is what the Sim wears unless the code changes suits
        auto oft = VMSuitProvider::RegisterTS1Outfit(bs.size() > 1 ? bs[1] : std::string(), bs);
        if (oft != 0) {
            avatar->DefaultSuits.Daywear = oft;
            if (!pet.empty() || avatar->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::CurrentOutfit) == 0) avatar->BodyOutfit(oft);
        }
        if (bs.size() > 1) avatar->TS1Appearance.Body = FSO::SimAntics::Model::VMTS1BodyPart::Parse(bs[1]);
    }
    return prev;
}

bool VMTS1Outfits::ChildToAdult(VMAvatar* avatar, VMContext* context) {
    namespace CAS = FSO::Content::TS1::CAS;
    using FSO::SimAntics::Model::VMPersonDataVariable;
    if (avatar == nullptr || avatar->IsPet() || avatar->Object == nullptr) return false;
    auto age = avatar->GetPersonData(VMPersonDataVariable::PersonsAge);
    if (age >= 18) return false;
    auto old = VMSuitProvider::BodyStringsOf(avatar);
    CAS::CASSimDesc sim;
    sim.Age = CAS::CASAge::Adult;
    sim.Gender = avatar->GetPersonData(VMPersonDataVariable::Gender) == 1 ? CAS::CASGender::Female : CAS::CASGender::Male;
    auto skin = old.size() > 14 ? old[14] : std::string("lgt");
    sim.Skin = skin == "drk" ? CAS::CASSkin::Dark : skin == "med" ? CAS::CASSkin::Medium : CAS::CASSkin::Light;
    static CAS::CASCatalog catalog;
    if (!catalog.Built() && !catalog.Build()) return false;
    auto& heads = catalog.Heads(sim.Age, sim.Gender, sim.Skin);
    auto& bodies = catalog.Bodies(sim.Age, sim.Gender, sim.Skin);
    if (heads.empty() || bodies.empty()) return false;
    sim.Head = heads[(size_t)context->NextRandom(heads.size())];
    sim.Body = bodies[(size_t)context->NextRandom(bodies.size())];
    auto bs = sim.BodyStrings();
    std::string code = std::string(sim.Gender == CAS::CASGender::Female ? "f" : "m") + "a" + sim.BodyType();
    bs.resize(34);
    bs[30] = "f100" + code + "_original,BODY=f100" + code + skin + "_original";
    bs[31] = "s100" + code + "_original,BODY=s100" + code + skin + "_original";
    bs[32] = "l100" + code + "_original,BODY=l100" + code + skin + "_original";
    bs[33] = "w100" + code + "_original,BODY=w100" + code + skin + "_original";
    SaveBodyStrings(avatar, bs);
    auto str = avatar->Object->Resource->Get<STR>(avatar->Object->OBJ->BodyStringID);
    if (str == nullptr) return false;
    // re-dress on the adult skeleton (SetAvatarType makes a new Vitaboy avatar), adult walk/idle tables
    avatar->SetAvatarType(str);
    avatar->SetAvatarBodyStrings(str, context);
    avatar->TS1Appearance = FSO::SimAntics::Model::VMTS1Appearance::FromBodyStrings(str);
    avatar->InitBodyData(context);
    avatar->SetPersonData(VMPersonDataVariable::PersonsAge, 27);
    avatar->SetPersonData(VMPersonDataVariable::JobType, 0);          // children's "school" (-1) ends
    avatar->SetPersonData(VMPersonDataVariable::JobPromotionLevel, 0);
    VMTS1Neighbourhood::SavePersistentData(avatar);
    return true;
}

bool VMTS1Outfits::PetToAdult(VMAvatar* avatar) {
    using FSO::SimAntics::Model::VMPersonDataVariable;
    if (avatar == nullptr || !avatar->IsPet()) return false;
    avatar->SetPersonData(VMPersonDataVariable::PersonsAge, 27);
    auto bs = VMSuitProvider::BodyStringsOf(avatar);
    if (bs.size() > 13 && bs[13] != "27") { bs[13] = "27"; SaveBodyStrings(avatar, bs); }
    VMTS1Neighbourhood::SavePersistentData(avatar);
    return true;
}

}}}}
