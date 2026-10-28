/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMAvatar (entities/VMAvatar.cs)
#include "simantics/entities/VMAvatar.h"
#include "hit/HITThread.h"
#include "hit/HITVM.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMTS1MotiveDecay.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "common/utils/DirectionUtils.h"
#include "content/ContentManager.h"
#include "vitaboy/content/TS1AvatarProvider.h" // HOOK(vitaboy)
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

#if __has_include("simantics/primitives/VMFindLocationFor.h")
#include "simantics/primitives/VMFindLocationFor.h"
#define ST_HAVE_FINDLOCATIONFOR 1
#endif

namespace FSO { namespace SimAntics {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Model;
using namespace FSO::LotView::Model;
using FSO::LotView::Components::AvatarComponent;
using FSO::LotView::Components::ObjectComponent;
using FSO::Common::Utils::DirectionUtils;
using FSO::Vitaboy::AppearanceType;

uint32_t VMAvatar::TEMPLATE_PERSON = 0x7FD96B54;
uint32_t VMAvatar::TEMPLATE_NPC = 0x7FD96B56;
uint32_t VMAvatar::NPC_MAID = 0xC61931DC;
uint32_t VMAvatar::NPC_GARDENER = 0xD62B91B4;
uint32_t VMAvatar::DOG_TEMPLATE = 0x3AC5D780;
uint32_t VMAvatar::CAT_TEMPLATE = 0x3762D78D;
const int32_t VMAvatar::FORCE_DELETE_TIMEOUT = 60 * 30;

static bool EqualsIgnoreCase(const std::string& a, const char* b) {
    return FSO::Content::ToLowerInvariant(a) == FSO::Content::ToLowerInvariant(b);
}

static std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t p = s.find(sep, start);
        out.push_back(s.substr(start, (p == std::string::npos) ? std::string::npos : p - start));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

// C# Convert.ToUInt64(str, 16): throws on bad input.
static uint64_t ParseHex64(const std::string& s) {
    if (s.empty()) throw std::invalid_argument("empty");
    char* end = nullptr;
    uint64_t v = strtoull(s.c_str(), &end, 16);
    if (end == nullptr || *end != '\0') throw std::invalid_argument("bad hex");
    return v;
}

void VMAvatar::Message(const std::string& value) {
    m_Message = value;
    SetPersonData(VMPersonDataVariable::ChatBaloonOn, 1);
    MessageTimeout = 150 + (int32_t)value.size() / 2;
}

void VMAvatar::BodyOutfit(uint64_t value) {
    _BodyOutfit = value;
    Avatar->Body = FSO::Content::Content::Get()->AvatarOutfits->Get(value, false);
    if (AvatarType == VMAvatarType::Adult || AvatarType == VMAvatarType::Child) Avatar->Handgroup = Avatar->Body;
}

void VMAvatar::HeadOutfit(uint64_t value) {
    _HeadOutfit = value;
    Avatar->Head = (_HeadOutfit == 0) ? nullptr : FSO::Content::Content::Get()->AvatarOutfits->Get(value, false);
}

void VMAvatar::SkinTone(AppearanceType value) {
    _SkinTone = value;
    Avatar->Appearance = value;
}

st::Vector3 VMAvatar::VisualPosition() const { return (UseWorld && WorldUI) ? WorldUI->Position : st::Vector3(); }
void VMAvatar::VisualPosition(const st::Vector3& value) { if (UseWorld && WorldUI) WorldUI->Position = value; }

void VMAvatar::RadianDirection(float value) {
    _RadianDirection = value;
    if (UseWorld && WorldUI) AvaUI()->RadianDirection = value;
}

VMEntity::DirectionT VMAvatar::Direction() const {
    int midPointDir = (int)DirectionUtils::PosMod(std::nearbyint(_RadianDirection / (M_PI / 4.0)), 8);
    return (DirectionT)(1 << (midPointDir));
}
void VMAvatar::Direction(DirectionT value) {
    RadianDirection(((int)std::nearbyint(std::log2((double)(int)value))) * (float)(M_PI / 4.0));
}

// FIX(sims): The Sims 1 itself stores the species in person data 65 as 0 male, 1 female, 2/3 dog (male/female),
// 4/5 cat (male/female): that is what the shipped NBRS records and every saved house's OBJM hold for the Unleashed pets.
// FreeSO's flags (dog 8, cat 16) are accepted too. Testing only the flags made every pet loaded from a house or a
// neighbour record a person (Old Town cats cooking, dogs flushing toilets, pets eligible for every human interaction).
bool VMAvatar::IsPet() {
    return IsDog() || IsCat();
}
bool VMAvatar::IsDog() {
    auto gender = GetPersonData(VMPersonDataVariable::Gender);
    return (gender & 8) > 0 || gender == 2 || gender == 3;
}
bool VMAvatar::IsCat() {
    auto gender = GetPersonData(VMPersonDataVariable::Gender);
    return (gender & 16) > 0 || gender == 4 || gender == 5;
}

VMAvatar::VMAvatar(FSO::Content::GameObject* obj) : VMEntity(obj) {
    PersonData.fill(0);
    MotiveData.fill(0);
    auto state = std::make_shared<TSOPlatform::VMTSOAvatarState>(); //todo: ts1 switch
    AvatarState = state.get();
    PlatformState = state;
    BodyStrings = Object->Resource->Get<STR>(Object->OBJ->BodyStringID);
    Visitor = false;
    SetAvatarType(BodyStrings);
    SkinTone(AppearanceType::Light);

    if (UseWorld) {
        WorldUI.reset(new AvatarComponent());
        auto avatarc = AvaUI();
        avatarc->Avatar = Avatar.get();
    }

    MotiveDecay.reset(new Entities::VMTS1MotiveDecay());
    for (int i = 0; i < 16; i++) {
        MotiveChanges[i] = VMMotiveChange();
        MotiveChanges[i].Motive = (VMMotive)i;
    }
}

VMAvatar::~VMAvatar() {}

void VMAvatar::SetAvatarType(STR* data) {
    if (data == nullptr) {
        AvatarType = VMAvatarType::Adult;
    } else {
        auto type = data->GetString(0);
        if (type == "adult") AvatarType = VMAvatarType::Adult;
        else if (type == "child") AvatarType = VMAvatarType::Child;
        else if (type == "cat" || type == "kat") AvatarType = VMAvatarType::Cat; // TS1(sims): Unleashed cats are "kat"
        else if (type == "dog") AvatarType = VMAvatarType::Dog;
    }

    auto content = FSO::Content::Content::Get();
    switch (AvatarType) {
        case VMAvatarType::Adult:
        case VMAvatarType::Child: // C#: Child falls through to no case, leaving Avatar null; we keep an adult skeleton so outfits can be bound.
            // HOOK(vitaboy): TS1 children use the child skeleton (same bone layout, child proportions).
            Avatar.reset(new FSO::Vitaboy::SimAvatar(content->AvatarSkeletons->Get(AvatarType == VMAvatarType::Child ? "child.skel" : "adult.skel")));
            Avatar->Head = content->AvatarOutfits->Get(SetAvatarRandomOutfit(1), false);
            Avatar->Body = content->AvatarOutfits->Get(SetAvatarRandomOutfit(0), false);
            Avatar->Handgroup = Avatar->Body;
            break;
        case VMAvatarType::Cat: {
            auto skel = content->AvatarSkeletons->Get("kat.skel"); // TS1(sims): kat_skeleton.cmx (FreeSims "cat.skel")
            if (skel == nullptr || skel->Bones.empty()) skel = content->AvatarSkeletons->Get("cat.skel");
            Avatar.reset(new FSO::Vitaboy::SimAvatar(skel));
            Avatar->Body = content->AvatarOutfits->Get("uaa002cat_calico.oft");
            break;
        }
        case VMAvatarType::Dog:
            Avatar.reset(new FSO::Vitaboy::SimAvatar(content->AvatarSkeletons->Get("dog.skel")));
            Avatar->Body = content->AvatarOutfits->Get("uaa012dog_scottish.oft"); //;)
            break;
    }
}

void VMAvatar::SetAvatarBodyStrings(STR* data, VMContext* context) {
    if (data == nullptr) return;

    auto body = data->GetString(1);
    auto randBody = data->GetString(10);

    auto gender = data->GetString(12);
    auto genVar = (int)VMPersonDataVariable::Gender;

    try {
        if (randBody != "") {
            auto bodySpl = Split(randBody, ';');
            DefaultSuits.Daywear = ParseHex64(bodySpl[context->NextRandom((uint64_t)bodySpl.size() - 1)]);
        } else if (body != "") {
            DefaultSuits.Daywear = ParseHex64(body);
        }

        BodyOutfit(DefaultSuits.Daywear);

        auto head = data->GetString(2);
        auto randHead = data->GetString(9);

        if (randHead != "") {
            auto headSpl = Split(randHead, ';');
            HeadOutfit(ParseHex64(headSpl[context->NextRandom((uint64_t)headSpl.size() - 1)]));
        } else if (head != "") {
            HeadOutfit(ParseHex64(head));
        }
    } catch (...) {
        // HOOK(vitaboy): TS1 body strings are "suit,BODY=texture" (not TSO hex outfit ids). Register them as
        // TS1 outfits (FreeSO: new VMOutfitReference(data, head)) so the Sim gets its real body/head/hands.
        auto vbContent = FSO::Content::TS1::TS1AvatarContent::Get();
        auto ts1Body = vbContent->RegisterOutfitFromBodyStrings(data, false);
        auto ts1Head = vbContent->RegisterOutfitFromBodyStrings(data, true);
        if (ts1Body != 0) {
            DefaultSuits.Daywear = ts1Body;
            BodyOutfit(ts1Body);
            HeadOutfit(ts1Head);
        } else {
        //head or body invalid, resort to default.
        HeadOutfit(SetAvatarRandomOutfit(1));
        BodyOutfit(SetAvatarRandomOutfit(0));
        }
    }

    if (EqualsIgnoreCase(gender, "male")) PersonData[genVar] = 0;
    else if (EqualsIgnoreCase(gender, "female")) PersonData[genVar] = 1;
    // FIX(sims): TS1 encodes pets as 2..5 (see IsPet); FreeSO's 8/9/16/17 never reach a TS1 BHAV or a saved file.
    else if (EqualsIgnoreCase(gender, "dogmale")) PersonData[genVar] = context->VM->TS1 ? 2 : 8;
    else if (EqualsIgnoreCase(gender, "dogfemale")) PersonData[genVar] = context->VM->TS1 ? 3 : 9;
    else if (EqualsIgnoreCase(gender, "catmale")) PersonData[genVar] = context->VM->TS1 ? 4 : 16;
    else if (EqualsIgnoreCase(gender, "catfemale")) PersonData[genVar] = context->VM->TS1 ? 5 : 17;

    auto names = data->GetString(11);
    if (names != "") {
        auto nameSpl = Split(names, ';');
        Name(nameSpl[context->NextRandom((uint64_t)nameSpl.size())]);
    }

    PersonData[(int)VMPersonDataVariable::PersonsAge] = (int16_t)atoi(data->GetString(13).c_str());

    auto skinTone = data->GetString(14);
    if (EqualsIgnoreCase(skinTone, "lgt")) SkinTone(AppearanceType::Light);
    else if (EqualsIgnoreCase(skinTone, "med")) SkinTone(AppearanceType::Medium);
    else if (EqualsIgnoreCase(skinTone, "drk")) SkinTone(AppearanceType::Dark);
}

uint64_t VMAvatar::SetAvatarRandomOutfit(int32_t Type) {
    uint64_t id = 0x3A00000000DULL;
    // C#: new Random().Next(1, 4) (non-deterministic, values 1..3). Uses rand() here; cosmetic only.
    auto value = 1 + (rand() % 3);

    if (Type == 1) {
        switch (value) {
            case 1: id = 0x3A10000000DULL; break;
            case 2: id = 0x3A20000000DULL; break;
            case 3: id = 0x1C50000000DULL; break;
            case 4: id = 0x1C60000000DULL; break;
        }
    } else {
        switch (value) {
            case 1: id = 0x25B0000000DULL; break;
            case 2: id = 0x2600000000DULL; break;
            case 3: id = 0x50000000DULL; break;
            case 4: id = 0x70000000DULL; break;
        }
    }
    return id;
}

void VMAvatar::SetAvatarData(XmlCharacter* charInfo) {
    auto content = FSO::Content::Content::Get();
    if (charInfo != nullptr) {
        PersistID = (uint16_t)atoi(charInfo->Id.c_str());
        AppearanceType type = AppearanceType::Light;
        if (charInfo->Appearance == "Medium") type = AppearanceType::Medium;
        else if (charInfo->Appearance == "Dark") type = AppearanceType::Dark;

        uint64_t headId = 0, bodyId = 0;
        try { headId = ParseHex64(charInfo->Head); } catch (...) {}
        try { bodyId = ParseHex64(charInfo->Body); } catch (...) {}
        auto headPurchasable = content->AvatarPurchasables->Get(headId, false);
        auto bodyPurchasable = content->AvatarPurchasables->Get(bodyId, false);

        Name(charInfo->Name);
        SkinTone(type);
        AvatarType = VMAvatarType::Adult;

        auto Gender = charInfo->Gender;
        if (Gender == "Male")
            SetPersonData(VMPersonDataVariable::Gender, 0);
        else if (Gender == "Female")
            SetPersonData(VMPersonDataVariable::Gender, 1);

        auto HeadOutfitP = content->AvatarOutfits->Get(headPurchasable != nullptr ? headPurchasable->OutfitID : headId, false);
        auto BodyOutfitP = content->AvatarOutfits->Get(bodyPurchasable != nullptr ? bodyPurchasable->OutfitID : bodyId, false);
        Avatar->Head = HeadOutfitP;
        Avatar->Body = BodyOutfitP;
        Avatar->Handgroup = Avatar->Body;
    } else {
        Avatar->Head = content->AvatarOutfits->Get("mah010_baldbeard01.oft"); //default to bob newbie, why not
        Avatar->Body = content->AvatarOutfits->Get("mab002_slob.oft");
        Avatar->Handgroup = Avatar->Body;
    }

    for (size_t i = 0; i < MotiveData.size(); i++) {
        MotiveData[i] = 75;
    }

    if (charInfo != nullptr && charInfo->Data.size() > 1) {
        SetPersonData(VMPersonDataVariable::CookingSkill, (int16_t)(charInfo->Data[0].Value));
        SetPersonData(VMPersonDataVariable::CharismaSkill, (int16_t)(charInfo->Data[1].Value));
    }
}

void VMAvatar::InitBodyData(VMContext* context) {
    //init walking strings
    // TS1(sims): children use the global 'StdChild' (151) / 'PanicChild' (160) tables (c2o-* walks/idles).
    const bool ts1Child = context->VM->TS1 && TS1Appearance.AgeType == "child";
    auto GlobWalk = context->Globals->Resource->Get<STR>(150);
    if (GlobWalk != nullptr)
        for (int i = 0; i < GlobWalk->Length() && i < 50; i++) {
            WalkAnimations[i] = GlobWalk->GetString(i);
        }

    auto GlobSwim = context->Globals->Resource->Get<STR>(158);
    if (GlobSwim != nullptr)
        for (int i = 0; i < GlobSwim->Length() && i < 50; i++) {
            SwimAnimations[i] = GlobSwim->GetString(i);
        }

    if (ts1Child) {
        // the child tables only fill the entries a child has (empty entries keep the adult one)
        auto ChildWalk = context->Globals->Resource->Get<STR>(151);
        if (ChildWalk != nullptr)
            for (int i = 0; i < ChildWalk->Length() && i < 50; i++) { auto str = ChildWalk->GetString(i); if (str != "") WalkAnimations[i] = str; }
        auto ChildSwim = context->Globals->Resource->Get<STR>(160);
        if (ChildSwim != nullptr)
            for (int i = 0; i < ChildSwim->Length() && i < 50; i++) { auto str = ChildSwim->GetString(i); if (str != "") SwimAnimations[i] = str; }
    }

    auto SpecialWalk = Object->Resource->Get<STR>(150);
    if (SpecialWalk != nullptr) {
        for (int i = 0; i < SpecialWalk->Length() && i < 50; i++) {
            auto str = SpecialWalk->GetString(i);
            if (str != "") WalkAnimations[i] = str;
        }
    }
}

void VMAvatar::Init(VMContext* context) {
    if (UseWorld && WorldUI) WorldUI->ObjectID = ObjectID;

    if (context->VM->TS1) {
        // TS1(sims): The Sims 1 person init tree ('Init person', PersonGlobals #8192) reads the person data
        // (age, job, family number...) and sets the motives ('init motives'). So, unlike the FreeSims order below:
        //  1. body strings (gender/age/skin) and the motive defaults are set *before* the init tree runs,
        //  2. a neighbour's persistent person data is inherited from the neighbourhood (NBRS) record of its
        //     GUID, as the game does when it spawns a neighbour (FreeSO: InheritNeighbor),
        //  3. the FreeSims TSO test values (skills 1000, person data 61 = 100, ...) are not applied: in TS1
        //     person data 61 is the family number and 2-18 are the personality/skills of the character.
        Animations.clear();
        SetAvatarBodyStrings(Object->Resource->Get<STR>(Object->OBJ->BodyStringID), context);
        TS1Appearance = Model::VMTS1Appearance::FromBodyStrings(Object->Resource->Get<STR>(Object->OBJ->BodyStringID));
        { // the person's name is its catalog string 0 (CTSS #2000 in User*.iff: "Bob")
            auto ctss = Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(Object->OBJ->CatalogStringsID);
            if (ctss != nullptr && ctss->Length() > 0 && ctss->GetString(0) != "") Name(ctss->GetString(0));
        }
        InitBodyData(context);
        for (size_t i = 0; i < MotiveData.size(); i++) {
            MotiveData[i] = 75;
        }
        SetMotiveData(VMMotive::SleepState, 0); //max all motives except sleep state
        FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::InheritNeighbor(this, context->VM);
        VMEntity::Init(context);
        return;
    }

    VMEntity::Init(context);

    Animations.clear();

    SetAvatarBodyStrings(Object->Resource->Get<STR>(Object->OBJ->BodyStringID), context);
    InitBodyData(context);

    for (size_t i = 0; i < MotiveData.size(); i++) {
        MotiveData[i] = 75;
    }

    SetMotiveData(VMMotive::SleepState, 0); //max all motives except sleep state

    SetPersonData(VMPersonDataVariable::NeatPersonality, 1000); //for testing wash hands after toilet
    SetPersonData(VMPersonDataVariable::OnlineJobID, 1); //for testing wash hands after toilet

    SetPersonData(VMPersonDataVariable::CreativitySkill, 1000);
    SetPersonData(VMPersonDataVariable::CookingSkill, 1000);
    SetPersonData(VMPersonDataVariable::CharismaSkill, 1000);
    SetPersonData(VMPersonDataVariable::LogicSkill, 1000);
    SetPersonData(VMPersonDataVariable::BodySkill, 1000);

    SetPersonData(VMPersonDataVariable::NumOutgoingFriends, 100);
    SetPersonData(VMPersonDataVariable::IncomingFriends, 100);
}

void VMAvatar::Reset(VMContext* context) {
    VMEntity::Reset(context);
    SetPersonData(VMPersonDataVariable::Priority, 0); // TS1(sims): FreeSO
    Animations.clear();
    if (Headline != nullptr && HeadlineRenderer != nullptr) {
        HeadlineRenderer->Dispose();
        Headline.reset();
        HeadlineRenderer.reset();
    }
    for (auto& aprName : BoundAppearances) {
        //remove all appearances, so we don't have stuff stuck to us.
        auto apr = FSO::Content::Content::Get()->AvatarAppearances->Get(aprName);
        Avatar->RemoveAccessory(apr);
    }
    BoundAppearances.clear();
    // STUB(3ds): context.VM.EODHost.ForceDisconnect(this) — no EOD host.
}

void VMAvatar::HandleTimePropsEvent(FSO::Vitaboy::TimePropertyListItem& tp) {
    VMAvatar* avatar = this;
    auto evt = tp.Properties.Get("xevt");
    if (evt != nullptr) {
        auto eventValue = (int16_t)atoi(evt->c_str());
        avatar->CurrentAnimationState()->EventQueue.push_back(eventValue);
        // TS1(sims): FreeSO — only events below 100 count towards the operand's expected event count. Events
        // >= 100 (101 = 'sound' cue) are extra; when the animation ends short of its count, Animate Sim queues the
        // missing ones (0, 1, ...). E.g. 'A2O-tvsmall-turnon' carries only xevt 101 and its tree switches the TV on
        // on the forced event 0: counting 101 meant small TVs never turned on and 'Watch TV' never ran.
        if (eventValue < 100) avatar->CurrentAnimationState()->EventsRun++;
    }
    auto rhevt = tp.Properties.Get("righthand");
    if (rhevt != nullptr) {
        auto eventValue = (int16_t)atoi(rhevt->c_str());
        avatar->Avatar->RightHandGesture = (FSO::Vitaboy::SimHandGesture)eventValue;
    }
    auto lhevt = tp.Properties.Get("lefthand");
    if (lhevt != nullptr) {
        auto eventValue = (int16_t)atoi(lhevt->c_str());
        avatar->Avatar->LeftHandGesture = (FSO::Vitaboy::SimHandGesture)eventValue;
    }

    auto soundevt = tp.Properties.Get("sound");
    auto owner = this;
    if (UseWorld && soundevt != nullptr && FSO::HIT::HITVM::Get() != nullptr) {
        bool playing = false;
        for (auto& x : owner->SoundThreads) if (x.Name == *soundevt) { playing = true; break; }
        if (!playing) {
            auto thread = FSO::HIT::HITVM::Get()->PlaySoundEvent(*soundevt);
            if (thread != nullptr) {
                auto hthread = std::dynamic_pointer_cast<FSO::HIT::HITThread>(thread);
                if (hthread != nullptr) SubmitHITVars(hthread.get());

                if (!thread->AlreadyOwns(owner->ObjectID)) thread->AddOwner(owner->ObjectID);

                Model::Sound::VMSoundEntry entry;
                entry.Sound = thread;
                entry.Pan = true;
                entry.Zoom = true;
                entry.Name = *soundevt; // (C# left Name empty, so the same event could stack up)
                owner->SoundThreads.push_back(entry);
                owner->TickSounds();
            }
        }
    }
}

void VMAvatar::SubmitHITVars(FSO::HIT::HITThread* thread) {
    if (thread == nullptr || thread->ObjectVar.empty()) return;
    // C#: thread.ObjectVar[12] = GetPersonData(VMPersonDataVariable.Gender);  (TSO's field numbering)
    // TS1(sims): The Sims' HIT code reads field 0 with getsrcdatafield and picks the male (0), female (1) or
    // child (anything else) hitlist (generic_fm_smartlists), so Simlish voices match the Sim.
    int16_t gender = GetPersonData(Model::VMPersonDataVariable::Gender);
    int field0 = (AvatarType == VMAvatarType::Child) ? 2 : (gender & 1);
    thread->ObjectVar[0] = field0;
    thread->ObjectVar[1] = GetPersonData(Model::VMPersonDataVariable::PersonsAge);
    thread->ObjectVar[12] = gender;
    SubmitSourceVars(thread, ObjectID);
    thread->WriteVar(2, field0); // arg2 = Gender (SimsGlobals.txt)
}

void VMAvatar::SubmitSourceVars(FSO::HIT::HITThread* thread, int16_t objectID) {
    if (thread == nullptr || thread->ObjectVar.empty()) return;
    // arg1 = Instance (SimsGlobals.txt). Offset so an actor's sequence group never equals GroupMusic (1),
    // GroupDialogMain (2) or GroupDialogOverlay (3).
    thread->WriteVar(1, 0x1000 + objectID);
}

void VMAvatar::Tick() {
    Velocity = st::Vector3(0, 0, 0);
    VMEntity::Tick();

    if (m_Message != "") {
        if (MessageTimeout-- > 0) {
            if (MessageTimeout == 0) {
                SetPersonData(VMPersonDataVariable::ChatBaloonOn, 0);
                m_Message = "";
            }
        }
    }

    if (Thread != nullptr && Thread->ThreadBreak == Engine::VMThreadBreakMode::Pause) return;

    if (PersonData[(int)VMPersonDataVariable::OnlineJobStatusFlags] == 0) PersonData[(int)VMPersonDataVariable::OnlineJobStatusFlags] = 1;
    if (Thread != nullptr) {
        MotiveDecay->Tick(this, Thread->Context);
        SetPersonData(VMPersonDataVariable::OnlineJobGrade, std::max((int16_t)0, Thread->Context->VM->GetGlobalValue(11))); //force job grade to what we expect
        if (Position() == LotTilePos::OUT_OF_WORLD) {
            //uh oh!
            VMEntity* mailbox = nullptr;
            for (auto x : Thread->Context->VM->Entities) {
                if (x->Object->OBJ->GUID == 0xEF121974 || x->Object->OBJ->GUID == 0x1D95C9B0) { mailbox = x; break; }
            }
#ifdef ST_HAVE_FINDLOCATIONFOR
            if (mailbox != nullptr) Primitives::VMFindLocationFor::FindLocationFor(this, mailbox, Thread->Context);
#else
            (void)mailbox; // primitives/VMFindLocationFor not translated yet
#endif
        }
    }

    //animation update for avatars
    VMAvatar* avatar = this;
    float totalWeight = 0.0f;
    for (auto& stateP : Animations) {
        auto state = stateP.get();
        totalWeight += state->Weight;
        if (!state->EndReached && state->Weight != 0) {
            if (state->PlayingBackwards) state->CurrentFrame -= state->Speed;
            else state->CurrentFrame += state->Speed;
            auto currentFrame = state->CurrentFrame;
            auto currentTime = currentFrame * 33.33f;
            auto& timeProps = state->TimePropertyLists;
            if (!state->PlayingBackwards) {
                for (int i = 0; i < (int)timeProps.size(); i++) {
                    auto tp = timeProps[i];
                    if (tp.ID > currentTime) {
                        break;
                    }

                    timeProps.erase(timeProps.begin());
                    i--;

                    HandleTimePropsEvent(tp);
                }
            } else {
                for (int i = (int)timeProps.size() - 1; i >= 0; i--) {
                    auto tp = timeProps[i];
                    if (tp.ID < currentTime) {
                        break;
                    }

                    timeProps.pop_back();
                    HandleTimePropsEvent(tp);
                }
            }

            auto status = FSO::Vitaboy::Animator::RenderFrame(avatar->Avatar.get(), state->Anim, (int)state->CurrentFrame, std::fmod(state->CurrentFrame, 1.0f), state->Weight / totalWeight);
            if (status != FSO::Vitaboy::AnimationStatus::IN_PROGRESS) {
                if (state->Loop) {
                    if (state->PlayingBackwards) state->CurrentFrame += state->Anim->NumFrames();
                    else state->CurrentFrame -= state->Anim->NumFrames();
                }
                else
                    state->EndReached = true;
            }
        }
    }

    if (avatar->CarryAnimationState != nullptr) {
        FSO::Vitaboy::Animator::RenderFrame(avatar->Avatar.get(), avatar->CarryAnimationState->Anim, (int)avatar->CarryAnimationState->CurrentFrame, 0.0f, 1.0f); //currently don't advance frames... I don't think any of them are animated anyways.
    }

    for (int i = 0; i < 16; i++) {
        MotiveChanges[i].Tick(this); //tick over motive changes
    }

    avatar->Avatar->ReloadSkeleton();

    PersonData[(int)VMPersonDataVariable::TickCounter]++;
    if (KillTimeout > -1) {
        if (++KillTimeout > FORCE_DELETE_TIMEOUT) Delete(true, Thread->Context);
        else {
            SetPersonData(VMPersonDataVariable::RenderDisplayFlags, 1);
            SetValue(VMStackObjectVariable::Hidden, (int16_t)((KillTimeout % 30) / 15));
            if (Thread->BlockingState != nullptr) Thread->BlockingState->WaitTime = std::max<int32_t>(Thread->BlockingState->WaitTime, 1000000); //make most things time out
            UserLeaveLot(); //keep forcing
        }
    }
}

void VMAvatar::UserLeaveLot() {
    // STUB(3ds): Thread.Context.VM.EODHost.ForceDisconnect(this) — no EOD host.
    for (size_t i = 0; i < Thread->Queue.size(); i++) {
        auto& x = Thread->Queue[i];
        if (x->ActionRoutine != nullptr && x->ActionRoutine->ID == LEAVE_LOT_TREE && (int)i <= Thread->ActiveQueueBlock + 1) return; //we're already leaving
    }
    auto actions = Thread->Queue;
    for (auto& action : actions) {
        Thread->CancelAction(action->UID);
    }

    auto tree = GetBHAVWithOwner(LEAVE_LOT_TREE, Thread->Context);
    if (tree == nullptr) return;
    auto routine = Thread->Context->VM->Assemble(tree->bhav);

    auto action = std::make_shared<Engine::VMQueuedAction>();
    action->Callee = this;
    action->CodeOwner = tree->owner;
    action->ActionRoutine = routine;
    action->Name = "Leave Lot";
    action->StackObject = this;
    action->Args.assign(4, 0);
    action->InteractionNumber = -1;
    action->Priority = INT16_MAX;
    action->Flags = TTABFlags::Leapfrog | TTABFlags::MustRun;
    Thread->EnqueueAction(action);
    if (KillTimeout == -1) KillTimeout = 0;
}

void VMAvatar::FractionalAnim(float fraction) {
    auto avatar = this;
    float totalWeight = 0.0f;
    for (auto& state : Animations) {
        totalWeight += state->Weight;
        if (!state->EndReached) {
            float visualFrame = state->CurrentFrame;
            if (state->PlayingBackwards) visualFrame -= state->Speed / 2;
            else visualFrame += state->Speed / 2;

            FSO::Vitaboy::Animator::RenderFrame(avatar->Avatar.get(), state->Anim, (int)visualFrame, std::fmod(visualFrame, 1.0f), state->Weight / totalWeight);
        }
    }
    if (avatar->CarryAnimationState != nullptr) FSO::Vitaboy::Animator::RenderFrame(avatar->Avatar.get(), avatar->CarryAnimationState->Anim, (int)avatar->CarryAnimationState->CurrentFrame, 0.0f, 1.0f);

    //TODO: if this gets changed to run at variable framerate need to "remember" visual position
    avatar->Avatar->ReloadSkeleton();
    VisualPosition(VisualPosition() + fraction * Velocity);
}

int16_t VMAvatar::GetPersonData(VMPersonDataVariable variable) {
    if ((uint16_t)variable > 100) throw std::runtime_error("Person Data out of bounds!");
    switch (variable) {
        // TS1(sims): FreeSO — Priority is plain person data (set when an interaction starts/ends, read by the
        // queue). FreeSims mapped it onto Queue[0].Priority.
        case VMPersonDataVariable::IsHousemate: {
            auto level = ((TSOPlatform::VMTSOAvatarState*)TSOState())->Permissions;
            return (int16_t)((level >= TSOPlatform::VMTSOAvatarPermissions::BuildBuyRoommate) ? 2 : ((level >= TSOPlatform::VMTSOAvatarPermissions::Roommate) ? 1 : 0));
        }
        default: break;
    }
    return PersonData[(uint16_t)variable];
}

void VMAvatar::SetMotiveChange(VMMotive motive, int16_t PerHourChange, int16_t MaxValue) {
    auto& temp = MotiveChanges[(int)motive];
    // TS1(sims): apply the translated VMMotiveChange::ScaleRate (dead code in FreeSims). For TS1 it scales
    // positive energy rates by energy span / sleep hours (180 / (24 - 16)), so a night in bed restores the energy
    // a day awake costs; without it Sims slept ~20 game hours and woke up still tired.
    if (Thread != nullptr && Thread->Context->VM->TS1)
        PerHourChange = (int16_t)Model::VMMotiveChange::ScaleRate(Thread->Context->VM, PerHourChange, motive);
    if (temp.Ticked) {
        temp.PerHourChange = PerHourChange;
        temp.MaxValue = MaxValue;
        temp.Ticked = false;
    }
}

bool VMAvatar::HasMotiveChange(VMMotive motive) {
    return MotiveChanges[(int)motive].PerHourChange != 0;
}

void VMAvatar::ClearMotiveChanges() {
    for (int i = 0; i < 16; i++) {
        MotiveChanges[i].Clear();
    }
}

bool VMAvatar::SetPersonData(VMPersonDataVariable variable, int16_t value) {
    if ((uint16_t)variable > 100) throw std::runtime_error("Person Data out of bounds!");
    switch (variable) {
        case VMPersonDataVariable::Priority:
            // TS1(sims): FreeSO — stored person data; changing it re-evaluates the queue.
            if (Thread != nullptr) Thread->QueueDirty = true;
            break;
        case VMPersonDataVariable::CensorshipFlags: // TS1(sims): the mosaic over the Sim (World::DrawCensor)
            if (WorldUI != nullptr) {
                bool female = (GetPersonData(VMPersonDataVariable::Gender) & 1) != 0;
                AvaUI()->CensorFlags = female ? value : (value & ~2);
            }
            break;
        case VMPersonDataVariable::MoneyAmmountOverHead: // FreeSO
            if (value != -32768) ShowMoneyHeadline(value);
            break;
        case VMPersonDataVariable::RenderDisplayFlags:
            if (WorldUI != nullptr) AvaUI()->DisplayFlags = (FSO::LotView::Components::AvatarDisplayFlags)value;
            return true;
        default: break;
    }
    PersonData[(uint16_t)variable] = value;
    return true;
}

int16_t VMAvatar::GetMotiveData(VMMotive variable) {
    if ((uint16_t)variable > 15) throw std::runtime_error("Motive Data out of bounds!");
    return MotiveData[(uint16_t)variable];
}

bool VMAvatar::SetMotiveData(VMMotive variable, int16_t value) {
    if ((uint16_t)variable > 15) throw std::runtime_error("Motive Data out of bounds!");
    MotiveData[(uint16_t)variable] = (int16_t)std::max(std::min((int)value, 100), -100);
    return true;
}

std::string VMAvatar::ToString() {
    return Name();
}

std::unique_ptr<Routing::VMObstacle> VMAvatar::GetObstacle(const LotTilePos& pos, DirectionT /*dir*/) {
    return std::unique_ptr<Routing::VMObstacle>(new Routing::VMObstacle(
        (pos.x - 3),
        (pos.y - 3),
        (pos.x + 3),
        (pos.y + 3)));
}

void VMAvatar::PositionChange(VMContext* context, bool noEntryPoint) {
    if (GhostImage) return;

    auto room = context->GetObjectRoom(this);
    SetRoom(room);

    if (HandObject != nullptr) {
        context->UnregisterObjectPos(HandObject);
        HandObject->Position(Position());
        HandObject->PositionChange(context, noEntryPoint);
    }

    context->RegisterObjectPos(this);
    if (Container != nullptr) return;
    if (Position() == LotTilePos::OUT_OF_WORLD) return;

    VMEntity::PositionChange(context, noEntryPoint);
}

void VMAvatar::PrePositionChange(VMContext* context) {
    Footprint.reset();
    if (GhostImage && UseWorld) {
        if (WorldUI && WorldUI->Container != nullptr) {
            WorldUI->Container = nullptr;
            WorldUI->ContainerSlot = 0;
        }
        return;
    }

    context->UnregisterObjectPos(this);
    if (Container != nullptr) {
        Container->ClearSlot(ContainerSlot);
        return;
    }
    if (Position() == LotTilePos::OUT_OF_WORLD) return;
    VMEntity::PrePositionChange(context);
}

// Begin Container SLOTs interface

int32_t VMAvatar::TotalSlots() {
    return 1;
}

bool VMAvatar::PlaceInSlot(VMEntity* obj, int32_t slot, bool cleanOld, VMContext* context) {
    if (GetSlot(slot) == obj) return true; //already in slot
    if (GetSlot(slot) != nullptr) return false;
    if (cleanOld) obj->PrePositionChange(context);

    if (!obj->GhostImage) {
        HandObject = obj;

        CarryAnimationState = std::make_shared<VMAnimationState>(FSO::Content::Content::Get()->AvatarAnimations->Get("a2o-rarm-carry-loop.anim"), false); //set default carry animation

        obj->Container = this;
        obj->ContainerSlot = (int16_t)slot;
    }
    if (UseWorld && obj->WorldUI) {
        obj->WorldUI->Container = this->WorldUI.get();
        obj->WorldUI->ContainerSlot = slot;
        auto objC = dynamic_cast<ObjectComponent*>(obj->WorldUI.get());
        if (objC != nullptr) {
            objC->ForceDynamic = true;
        }
    }
    obj->Position(Position()); //TODO: is physical position the same as the slot offset position?
    if (cleanOld) obj->PositionChange(context, false);
    return true;
}

int32_t VMAvatar::GetSlotHeight(int32_t /*slot*/) {
    return 5; //in hand
    //TODO: verify
}

VMEntity* VMAvatar::GetSlot(int32_t /*slot*/) {
    return HandObject;
}

void VMAvatar::ClearSlot(int32_t /*slot*/) {
    if (HandObject == nullptr) return;
    HandObject->Container = nullptr;
    HandObject->ContainerSlot = -1;
    CarryAnimationState = nullptr;

    if (UseWorld && HandObject->WorldUI) {
        HandObject->WorldUI->Container = nullptr;
        HandObject->WorldUI->ContainerSlot = 0;

        auto objC = dynamic_cast<ObjectComponent*>(HandObject->WorldUI.get());
        if (objC != nullptr) {
            objC->ForceDynamic = false;
        }
    }

    HandObject = nullptr;
}

// End Container SLOTs interface

void VMAvatar::SetRoom(uint16_t room) {
    VMEntity::SetRoom(room);
    if (VM::UseWorld() && WorldUI) WorldUI->Room = (uint16_t)GetValue(VMStackObjectVariable::Room);
}

void* VMAvatar::GetIcon(void* /*gd*/, int32_t /*store*/) {
    // STUB(3ds): returned the head/body outfit thumbnail (or pet icon) as a Texture2D.
    return nullptr;
}

}}
