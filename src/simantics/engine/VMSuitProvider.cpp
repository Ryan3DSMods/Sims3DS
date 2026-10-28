/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include <cstdlib>
#include <cstdio>
// FSO.SimAntics.Engine.VMSuitProvider
#include "simantics/engine/VMSuitProvider.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/entities/VMAvatar.h"
#include <algorithm>
#include "simantics/VM.h"
#include "content/ContentManager.h"
#include "content/TS1Clothing.h"
#include "content/TS1JobProvider.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "vitaboy/Outfit.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/OBJD.h"
namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::SimAntics::Engine::Scopes;
using namespace FSO::SimAntics::Model;
using FSO::Files::Formats::IFF::Chunks::STR;

const int64_t VMSuitProvider::JobOutfits[2][5][3] = {
    { //male
        { 0x5870000000DLL, 0x5880000000DLL, 0x5890000000DLL }, //robot
        { 0x58B0000000DLL, 0x58D0000000DLL, 0x58E0000000DLL }, //waiter
        { 0x58A0000000DLL, 0x58C0000000DLL, 0x58F0000000DLL }, //cook
        { 0x5900000000DLL, 0x5920000000DLL, 0x5940000000DLL }, //dj
        { 0x5910000000DLL, 0x5930000000DLL, 0x5950000000DLL }, //dancer
    },
    { //female
        { 0x5780000000DLL, 0x5790000000DLL, 0x57A0000000DLL }, //robot
        { 0x57C0000000DLL, 0x57E0000000DLL, 0x57F0000000DLL }, //waiter
        { 0x57B0000000DLL, 0x57D0000000DLL, 0x5800000000DLL }, //cook
        { 0x5810000000DLL, 0x5830000000DLL, 0x5850000000DLL }, //dj
        { 0x5820000000DLL, 0x5840000000DLL, 0x5860000000DLL }, //dancer
    }
};

VMSuitResult VMSuitProvider::GetSuit(VMStackFrame* context, VMSuitScope scope, uint16_t id) {
    STR* suitTable = nullptr;

    auto avatar = (VMAvatar*)context->Caller;
    switch (scope) {
        case VMSuitScope::Object:
            suitTable = context->CodeOwner->Resource->Get<STR>(304);
            break;
        case VMSuitScope::Global:
            suitTable = context->Global()->Resource->Get<STR>(304);
            break;
        case VMSuitScope::Person: {
            if (context->VM()->TS1) return GetPersonSuitTS1(avatar, id); // TS1(sims): FreeSO
            //get outfit from person
            auto type = (VMPersonSuits)id;
            bool male = (avatar->GetPersonData(VMPersonDataVariable::Gender) == 0);
            switch (type) {
                //todo: Dynamic Daywear
                case VMPersonSuits::DefaultDaywear:
                    return VMSuitResult::Outfit(avatar->DefaultSuits.Daywear);
                case VMPersonSuits::Naked:
                    return VMSuitResult::Outfit((uint64_t)(male ? 0x24E0000000DULL : 0x10000000DULL));
                case VMPersonSuits::DefaultSwimwear:
                    return VMSuitResult::Outfit(avatar->DefaultSuits.Swimwear);
                case VMPersonSuits::JobOutfit: {
                    auto job = avatar->GetPersonData(VMPersonDataVariable::OnlineJobID);
                    if (job < 1 || job > 5) return VMSuitResult();
                    auto level = std::max(0, std::min(3, (avatar->GetPersonData(VMPersonDataVariable::OnlineJobGrade) - 1) / 4));
                    if (level > 2) level = 2; // C# would throw IndexOutOfRange for grade >= 13
                    return VMSuitResult::Outfit((uint64_t)(JobOutfits[male ? 0 : 1][job - 1][level]));
                }
                case VMPersonSuits::DefaultSleepwear:
                    return VMSuitResult::Outfit(avatar->DefaultSuits.Sleepwear);
                case VMPersonSuits::SkeletonPlus:
                    return VMSuitResult::Outfit((uint64_t)(0x5750000000DULL));
                case VMPersonSuits::SkeletonMinus:
                    return VMSuitResult::Outfit((uint64_t)(0x5740000000DULL));
                case VMPersonSuits::TeleporterMishap:
                    return VMSuitResult::Outfit((uint64_t)(male ? 0x2900000000DULL : 0x4A0000000DULL));
                default: break;
            }
            return VMSuitResult();
        }
    }

    if (suitTable != nullptr) {
        auto suitFile = suitTable->GetString(id) + ".apr";
        return VMSuitResult::Appearance(suitFile);
        //var apr = FSO.Content.Content.Get().AvatarAppearances.Get(suitFile);
        //return apr;
    }
    return VMSuitResult();
}

// TS1(sims): FreeSO VMSuitProvider.GetPersonSuitTS1. The body string comes from content/TS1Clothing
// (TS1PersonSuitBodyString); it is registered as a TS1 outfit with the Sim's everyday hands (FreeSO copies the daywear
// LiteralHandgroup), so swimwear at the pool, pyjamas in bed, formal wear, uniforms, ... show the real meshes.
std::vector<std::string> VMSuitProvider::BodyStringsOf(VMAvatar* avatar) {
    std::vector<std::string> out;
    if (avatar == nullptr || avatar->Object == nullptr) return out;
    auto str = avatar->Object->Resource->Get<STR>(avatar->Object->OBJ->BodyStringID);
    if (str == nullptr) return out;
    for (int32_t i = 0; i < str->Length(); i++) out.push_back(str->GetString(i));
    return out;
}

uint64_t VMSuitProvider::RegisterTS1Outfit(const std::string& bodyString, const std::vector<std::string>& bodyStrings) {
    if (bodyString.empty()) return 0;
    std::string hands[6];
    for (int i = 0; i < 6; i++) hands[i] = (17 + i < (int)bodyStrings.size()) ? bodyStrings[17 + i] : std::string();
    FSO::Vitaboy::Outfit tmp;
    tmp.Read(bodyString, hands);
    bool withHands = !hands[0].empty();
    return FSO::Content::TS1::TS1AvatarContent::Get()->RegisterOutfit(tmp.TS1AppearanceID, tmp.TS1TextureID, withHands, hands);
}

VMSuitResult VMSuitProvider::GetPersonSuitTS1(VMAvatar* avatar, uint16_t id) {
    if (avatar == nullptr) return VMSuitResult();
    auto bs = BodyStringsOf(avatar);
    if (bs.empty()) return VMSuitResult();
    bool male = (avatar->GetPersonData(VMPersonDataVariable::Gender) == 0);
    auto age = avatar->GetPersonData(VMPersonDataVariable::PersonsAge);
    bool child = (age < 18 && age != 0);
    std::string jm, jf, js;
    if (id == (uint16_t)VMPersonSuits::JobOutfit) {
        auto content = FSO::Content::Content::Get();
        auto job = (content && content->Jobs) ? content->Jobs->GetJob(avatar->GetPersonData(VMPersonDataVariable::JobType)) : nullptr;
        auto level = avatar->GetPersonData(VMPersonDataVariable::JobPromotionLevel);
        if (job != nullptr && level >= 0 && level < (int16_t)job->JobLevels.size()) {
            auto& jl = job->JobLevels[(size_t)level];
            jm = jl.MaleUniformMesh; jf = jl.FemaleUniformMesh; js = jl.UniformSkin;
        }
    }
    auto body = FSO::Content::TS1::TS1PersonSuitBodyString(bs, id, male, child, jm, jf, js);
    if (body.empty()) return VMSuitResult();
    auto oft = RegisterTS1Outfit(body, bs);
    if (oft == 0) return VMSuitResult();
    return VMSuitResult::Outfit(oft);
}

}}}
