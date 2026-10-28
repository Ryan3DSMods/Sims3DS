/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMAvatar (entities/VMAvatar.cs)
// Motives, person data, relationships, routing/animation state: translated.
// STUB(3ds): body mesh / outfit / head rendering, HIT sound events, thumbnails (see STUBS.md).
#pragma once
#include <array>
#include <unordered_set>
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotiveChange.h"
#include "simantics/model/VMAnimationState.h"
#include "simantics/model/TSOPlatform/VMTSOAvatarState.h"
#include "simantics/model/VMTS1Appearance.h"
#include "vitaboy/Appearance.h"

namespace FSO { namespace HIT { class HITThread; } }
namespace FSO { namespace SimAntics {
namespace Entities { class VMTS1MotiveDecay; }

enum class VMAvatarType : uint8_t { Adult, Child, Cat, Dog };

class VMAvatarDefaultSuits {
public:
    uint64_t Daywear;
    uint64_t Swimwear;
    uint64_t Sleepwear;
    explicit VMAvatarDefaultSuits(bool female) {
        Daywear = 0x24C0000000DULL;
        Swimwear = (uint64_t)((female) ? 0x620000000DULL : 0x5470000000DULL);
        Sleepwear = (uint64_t)((female) ? 0x5150000000DULL : 0x5440000000DULL);
    }
};

// STUB(3ds): FSO.LotView.Model.XmlCharacter (FreeSims character xml). Only the fields SetAvatarData reads.
class XmlCharacter {
public:
    struct DataEntry { int32_t Value = 0; };
    std::string Id = "0";
    std::string Name;
    std::string Appearance = "Light";
    std::string Gender = "Male";
    std::string Head = "0";
    std::string Body = "0";
    std::vector<DataEntry> Data;
};

class VMAvatar : public VMEntity {
public:
    static uint32_t TEMPLATE_PERSON;
    static uint32_t TEMPLATE_NPC;
    static uint32_t NPC_MAID;
    static uint32_t NPC_GARDENER;
    static uint32_t DOG_TEMPLATE;
    static uint32_t CAT_TEMPLATE;

    std::unique_ptr<FSO::Vitaboy::SimAvatar> Avatar;

    /** Animation vars **/
    Model::TSOPlatform::VMTSOAvatarState* AvatarState; // owned by PlatformState

    std::vector<std::shared_ptr<Model::VMAnimationState>> Animations;
    Model::VMAnimationState* CurrentAnimationState() const { return (Animations.size() == 0) ? nullptr : Animations[0].get(); }
    std::shared_ptr<Model::VMAnimationState> CarryAnimationState;

    const std::string& Message() const { return m_Message; }
    void Message(const std::string& value);

    int32_t MessageTimeout = 0;
    st::Vector3 Velocity; //used for 60 fps walking animation
    bool Visitor = false;

    /*
        APPEARANCE DATA
    */
    VMAvatarDefaultSuits DefaultSuits = VMAvatarDefaultSuits(false);
    std::unordered_set<std::string> BoundAppearances;
    /// TS1(sims): The Sims 1 body/head/hand mesh + texture names from this person's body strings
    /// (character iff STR# 200), filled in Init. FreeSims' BodyOutfit/HeadOutfit are TSO outfit ids and are
    /// random placeholders for TS1 people; the body (Vitaboy) code should read this instead.
    Model::VMTS1Appearance TS1Appearance;

    uint64_t BodyOutfit() const { return _BodyOutfit; }
    void BodyOutfit(uint64_t value);
    uint64_t HeadOutfit() const { return _HeadOutfit; }
    void HeadOutfit(uint64_t value);
    FSO::Vitaboy::AppearanceType SkinTone() const { return _SkinTone; }
    void SkinTone(FSO::Vitaboy::AppearanceType value);

    st::Vector3 VisualPosition() const override;
    void VisualPosition(const st::Vector3& value) override;
    float RadianDirection() const override { return _RadianDirection; }
    void RadianDirection(float value) override;
    DirectionT Direction() const override;
    void Direction(DirectionT value) override;

    //inferred properties
    std::array<std::string, 50> WalkAnimations;
    std::array<std::string, 50> SwimAnimations;

    /// FreeSims VMAvatar.SubmitHITVars: gives a HIT thread this Sim's data fields (TS1: field 0 = gender, 0 male,
    /// 1 female, 2 child; arg1 = the speaking instance, arg2 = gender).
    void SubmitHITVars(FSO::HIT::HITThread* thread);
    /// TS1(sims): the source fields any object passes (arg1 = instance).
    static void SubmitSourceVars(FSO::HIT::HITThread* thread, int16_t objectID);

    bool IsPet();
    /// FreeSO VMAvatar.ShowMoneyHeadline: the amount floating over the Sim's head (person data MoneyAmmountOverHead,
    /// expression scope MoneyOverHead32Bit). STUB(3ds): instead of a Money headline renderer the UI polls these
    /// (MoneyHeadlineSerial changes on every call) and draws the TS1 "+§n" text over the head.
    void ShowMoneyHeadline(int32_t value) { MoneyHeadlineValue = value; MoneyHeadlineSerial++; }
    int32_t MoneyHeadlineValue = 0;
    uint32_t MoneyHeadlineSerial = 0;
    bool IsDog();
    bool IsCat();

    explicit VMAvatar(FSO::Content::GameObject* obj);
    ~VMAvatar() override;

    void SetAvatarType(FSO::Files::Formats::IFF::Chunks::STR* data);
    void SetAvatarBodyStrings(FSO::Files::Formats::IFF::Chunks::STR* data, VMContext* context);
    uint64_t SetAvatarRandomOutfit(int32_t Type);
    void SetAvatarData(XmlCharacter* charInfo);
    void InitBodyData(VMContext* context);
    void Init(VMContext* context) override;
    void Reset(VMContext* context) override;
    void Tick() override;
    void UserLeaveLot();
    void FractionalAnim(float fraction);

    virtual int16_t GetPersonData(Model::VMPersonDataVariable variable);
    virtual void SetMotiveChange(Model::VMMotive motive, int16_t PerHourChange, int16_t MaxValue);
    bool HasMotiveChange(Model::VMMotive motive);
    /// TS1(sims): read-only view for the lot saver (utils/VMTS1LotSaver, OBJM motive deltas).
    const Model::VMMotiveChange& GetMotiveChange(int index) const { return MotiveChanges[(size_t)(index & 15)]; }
    virtual void ClearMotiveChanges();
    virtual bool SetPersonData(Model::VMPersonDataVariable variable, int16_t value);
    virtual int16_t GetMotiveData(Model::VMMotive variable); //needs special conditions for ones like Mood.
    virtual bool SetMotiveData(Model::VMMotive variable, int16_t value);

    std::string ToString() override;
    std::unique_ptr<Model::Routing::VMObstacle> GetObstacle(const LotTilePos& pos, DirectionT dir) override;
    void PositionChange(VMContext* context, bool noEntryPoint) override;
    void PrePositionChange(VMContext* context) override;

    // Begin Container SLOTs interface
    int32_t TotalSlots() override;
    bool PlaceInSlot(VMEntity* obj, int32_t slot, bool cleanOld, VMContext* context) override;
    int32_t GetSlotHeight(int32_t slot) override;
    VMEntity* GetSlot(int32_t slot) override;
    void ClearSlot(int32_t slot) override;
    // End Container SLOTs interface

    void SetRoom(uint16_t room) override;
    void* GetIcon(void* gd, int32_t store) override; // STUB(3ds)

private:
    std::string m_Message = "";
    std::array<Model::VMMotiveChange, 16> MotiveChanges;
    std::unique_ptr<Entities::VMTS1MotiveDecay> MotiveDecay;
    std::array<int16_t, 101> PersonData; // C# allocates 100 but allows index 100 through the bounds check
    std::array<int16_t, 16> MotiveData;
    VMEntity* HandObject = nullptr;
    float _RadianDirection = 0;

    int32_t KillTimeout = -1;
    static const int32_t FORCE_DELETE_TIMEOUT;
    const uint16_t LEAVE_LOT_TREE = 8373;

    uint64_t _BodyOutfit = 0;
    uint64_t _HeadOutfit = 0;
    FSO::Vitaboy::AppearanceType _SkinTone = FSO::Vitaboy::AppearanceType::Light;
    FSO::Files::Formats::IFF::Chunks::STR* BodyStrings = nullptr;
    VMAvatarType AvatarType = VMAvatarType::Adult;

    void HandleTimePropsEvent(FSO::Vitaboy::TimePropertyListItem& tp);
    FSO::LotView::Components::AvatarComponent* AvaUI() const { return (FSO::LotView::Components::AvatarComponent*)WorldUI.get(); }
};
}}
